/**
 * $Id: css.c,v 1.3 2002/05/18 08:23:39 bursa Exp $
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "css.h"
#include "utils.h"

/**
 * internal structures
 */

struct rule {
	struct css_selector * selector;
	unsigned int selectors;
	struct css_style * style;
	struct rule * next;
};

struct decl {
	unsigned long score;
	struct rule * rule;
};

#define HASH_SIZE 13

struct css_stylesheet {
	struct rule * hash[HASH_SIZE];
};

static int parse_length(struct css_length * const length, const char *s);
static void parse_display(struct css_style * const style, const char * const value);
static void parse_float(struct css_style * const style, const char * const value);
static void parse_font_size(struct css_style * const style, const char * const value);
static void parse_height(struct css_style * const style, const char * const value);
static void parse_width(struct css_style * const style, const char * const value);
static void parse_selector(struct css_selector * sel, char * const str);
static unsigned int hash_str(const char * str);
static int seleq(const struct css_selector * const s1, const struct css_selector * const s2);
static unsigned long selmatch(const struct css_selector * const s, const struct css_selector * const sr);
static struct rule * find_rule(struct css_stylesheet * stylesheet,
			struct css_selector * selector, unsigned int selectors);
static int cmpdecl(const struct decl * d0, const struct decl * d1);
static void update_style(struct css_stylesheet * stylesheet, struct css_selector * selector,
			unsigned int selectors, char * str);
static void dump_length(const struct css_length * const length);
static void dump_selector(const struct css_selector * const sel);
static void dump_rule(const struct rule * rule);
static void css_dump_stylesheet(const struct css_stylesheet * stylesheet);

const struct css_style css_base_style = {
	CSS_DISPLAY_BLOCK,
	CSS_FLOAT_NONE,
	{ CSS_FONT_SIZE_ABSOLUTE, 10.0 },
	{ CSS_HEIGHT_AUTO },
	{ CSS_WIDTH_AUTO }
};

const struct css_style css_empty_style = {
	CSS_DISPLAY_INHERIT,
	CSS_FLOAT_INHERIT,
	{ CSS_FONT_SIZE_INHERIT },
	{ CSS_HEIGHT_AUTO },
	{ CSS_WIDTH_AUTO }
};

const struct css_style css_blank_style = {
	CSS_DISPLAY_BLOCK,
	CSS_FLOAT_NONE,
	{ CSS_FONT_SIZE_INHERIT },
	{ CSS_HEIGHT_AUTO },
	{ CSS_WIDTH_AUTO }
};



/**
 * property parsers
 */

static int parse_length(struct css_length * const length, const char *s)
{
	length->unit = css_unit_parse(s + strspn(s, "0123456789+-."));
	if (length->unit == CSS_UNIT_UNKNOWN) return 1;
	length->value = atof(s);
	return 0;
}

static void parse_display(struct css_style * const style, const char * const value)
{
	style->display = css_display_parse(value);
}

static void parse_float(struct css_style * const style, const char * const value)
{
	style->float_ = css_float_parse(value);
}

#define SIZE_FACTOR 1.2

static struct font_size {
	const char * keyword;
	float size;
} const font_size[] = {
	{ "xx-small", 1.0 / (SIZE_FACTOR * SIZE_FACTOR * SIZE_FACTOR) },
	{ "x-small", 1.0 / (SIZE_FACTOR * SIZE_FACTOR) },
	{ "small", 1.0 / SIZE_FACTOR },
	{ "medium", 1.0 },
	{ "large", 1.0 * SIZE_FACTOR },
	{ "x-large", 1.0 * SIZE_FACTOR * SIZE_FACTOR },
	{ "xx-large", 1.0 * SIZE_FACTOR * SIZE_FACTOR * SIZE_FACTOR }
};

static void parse_font_size(struct css_style * const style, const char * const value)
{
	unsigned int i;
	for (i = 0; i < sizeof(font_size) / sizeof(struct font_size); i++) {
		if (strcmp(value, font_size[i].keyword) == 0) {
			style->font_size.size = CSS_FONT_SIZE_ABSOLUTE;
			style->font_size.value.absolute = font_size[i].size;
			return;
		}
	}
	if (strcmp(value, "larger") == 0)
		style->font_size.size = CSS_FONT_SIZE_PERCENT,
		style->font_size.value.percent = SIZE_FACTOR * 100;
	else if (strcmp(value, "smaller") == 0)
		style->font_size.size = CSS_FONT_SIZE_PERCENT,
		style->font_size.value.percent = 1 / SIZE_FACTOR * 100;
	else if (strrchr(value, '%'))
		style->font_size.size = CSS_FONT_SIZE_PERCENT,
		style->font_size.value.percent = atof(value);
	else if (parse_length(&style->font_size.value.length, value) == 0)
		style->font_size.size = CSS_FONT_SIZE_LENGTH;
}

static void parse_height(struct css_style * const style, const char * const value)
{
	if (strcmp(value, "auto") == 0)
		style->height.height = CSS_HEIGHT_AUTO;
	else if (parse_length(&style->height.length, value) == 0)
		style->height.height = CSS_HEIGHT_LENGTH;
}

static void parse_width(struct css_style * const style, const char * const value)
{
	if (strcmp(value, "auto") == 0)
		style->width.width = CSS_WIDTH_AUTO;
	else if (strrchr(value, '%'))
		style->width.width = CSS_WIDTH_PERCENT,
		style->width.value.percent = atof(value);
	else if (parse_length(&style->width.value.length, value) == 0)
		style->width.width = CSS_WIDTH_LENGTH;
}

static struct property {
	const char * const name;
	void (*parse) (struct css_style * const s, const char * const value);
} const property[] = {
	{ "display", parse_display },
	{ "float", parse_float },
	{ "font-size", parse_font_size },
	{ "height", parse_height },
	{ "width", parse_width },
};

/**
 * parse a property list
 */

void css_parse_property_list(struct css_style * style, char * str)
{
	char * end;
	for (; str != 0; str = end) {
		char * prop;
		char * value;
		unsigned int i;

		if ((end = strchr(str, ';')) != 0) *end = 0, end++;
		if ((value = strchr(str, ':')) == 0) continue;
		*value = 0; value++;
		prop = strip(str);
		value = strip(value);
		/*printf("css_parse: '%s' => '%s'\n", prop, value);*/

		for (i = 0; i < sizeof(property) / sizeof(struct property); i++) {
			if (strcmp(prop, property[i].name) == 0) {
				property[i].parse(style, value);
				break;
			}
		}
	}
}

/**
 * selectors
 */

static void parse_selector(struct css_selector * sel, char * const str)
{
 	char * dot = strchr(str, '.');
 	char * hash = strchr(str, '#');

	if ((dot == 0) && (hash == 0)) {
		sel->element = xstrdup(str);
		sel->class = sel->id = 0;
	} else if (dot != 0) {
		*dot = 0;
		sel->element = xstrdup(str);
		sel->class = xstrdup(dot + 1);
		sel->id = 0;
	} else if (hash != 0) {
		*hash = 0;
		sel->element = xstrdup(str);
		sel->class = 0;
		sel->id = xstrdup(hash + 1);
	}
}

/**
 * stylesheet structure
 */

static unsigned int hash_str(const char * str)
{
	unsigned int s = 0;
	for (; *str != 0; str++)
		s += *str;
	return s % HASH_SIZE;
}

static int seleq(const struct css_selector * const s1, const struct css_selector * const s2)
{
	return strcmp(s1->element, s2->element) == 0 &&
	       ((s1->class == 0 && s2->class == 0) ||
	        (s1->class != 0 && s2->class != 0 && strcmp(s1->class, s2->class) == 0)) &&
	       ((s1->id == 0 && s2->id == 0) ||
	        (s1->id != 0 && s2->id != 0 && strcmp(s1->id, s2->id) == 0));
}

static unsigned long selmatch(const struct css_selector * const s, const struct css_selector * const sr)
{
	unsigned int c;
	if (strcmp(s->element, sr->element) != 0) return 0;
	c = s->element[0] == 0 ? 0 : 1;
	if (sr->class != 0) {
		if (s->class != 0 && strcmp(s->class, sr->class) == 0) return 0x100 + c;
		return 0;
	}
	if (sr->id != 0) {
		if (s->id != 0 && strcmp(s->id, sr->id) == 0) return 0x10000 + c;
		return 0;
	}
	return 1;
}

struct css_stylesheet * css_new_stylesheet(void)
{
	struct css_stylesheet * stylesheet = xcalloc(1, sizeof(struct css_stylesheet));
	return stylesheet;
}

static struct rule * find_rule(struct css_stylesheet * stylesheet,
			struct css_selector * selector, unsigned int selectors)
{
	struct rule * rule;
	for (rule = stylesheet->hash[hash_str(selector[selectors - 1].element)];
	     rule != 0; rule = rule->next) {
		unsigned int i;
		if (selectors != rule->selectors)
			continue;
		/* inv: selector[0..i) == rule->selector[0..i) */
		for (i = 0; i != selectors && seleq(&selector[i], &rule->selector[i]); i++)
			;
		if (i == selectors)
			return rule;
	}
	return 0;
}

static int cmpdecl(const struct decl * d0, const struct decl * d1)
{
	if (d0->score < d1->score) return -1;
	else if (d0->score == d1->score) return 0;
	return 1;
}

void css_get_style(struct css_stylesheet * stylesheet, struct css_selector * selector,
			unsigned int selectors, struct css_style * style)
{
	struct rule * rule;
	struct decl * decl = xcalloc(0, sizeof(struct decl));
	unsigned int d, decls = 0;

	for (rule = stylesheet->hash[hash_str(selector[selectors - 1].element)];
	     rule != 0; rule = rule->next) {
		unsigned int i = selectors - 1;
		unsigned int j;
		unsigned int score, s;

		if ((score = selmatch(&selector[i], &rule->selector[rule->selectors - 1])) == 0)
			continue;
		if (selectors < rule->selectors)
			continue;

		for (j = rule->selectors - 1; j != 0; j--) {
			for (; i != 0 && (s = selmatch(&selector[i - 1], &rule->selector[j - 1])) == 0; i--)
				;
			if (i == 0)
				break;
			score += s;
		}
		if (j == 0) {
			decl = xrealloc(decl, (decls + 1) * sizeof(struct decl));
			decl[decls].score = score;
			decl[decls].rule = rule;
			decls++;
		}
	}

	if (decls == 0) {
		css_cascade(style, &css_blank_style);

	} else {
		qsort(decl, decls, sizeof(struct decl), (int (*) (const void *, const void *)) cmpdecl);
	
		for (d = 0; d < decls; d++) {
/* 			printf("%i: 0x%lx\n", d, decl[d].score); */
/*	 		css_dump_rule(decl[d].rule); */
			css_cascade(style, decl[d].rule->style);
		}
	}
}

static void update_style(struct css_stylesheet * stylesheet, struct css_selector * selector,
			unsigned int selectors, char * str)
{
 	struct rule * rule = find_rule(stylesheet, selector, selectors);
 	if (rule == 0) {
	 	unsigned int h = hash_str(selector[selectors - 1].element);
	 	/*printf("update_style: not present - adding\n");*/
 		rule = xcalloc(1, sizeof(struct rule));
		rule->selector = selector;
		rule->selectors = selectors;
		rule->style = xcalloc(1, sizeof(struct css_style));
		memcpy(rule->style, &css_empty_style, sizeof(struct css_style));
		css_parse_property_list(rule->style, str);
		rule->next = stylesheet->hash[h];
		stylesheet->hash[h] = rule;
 	} else {
	 	/*printf("update_style: already present - updating\n");*/
		css_parse_property_list(rule->style, str);
		free(selector);
 	}
}

/**
 * parse an entire css file or block
 */

void css_parse_stylesheet(struct css_stylesheet * stylesheet, char * str)
{
	/* overwrite comments with spaces */
	char * copen = strstr(str, "/*");
	while (copen != 0) {
		char * cclose = strstr(copen + 2, "*/");
		if (cclose == 0) { *copen = 0; break; }
		memset(copen, ' ', (size_t) (cclose - copen + 2));
		copen = strstr(cclose + 2, "/*");
	}

	while (*str != 0) {
		char * open = strchr(str, '{');
		char * close = strchr(str, '}');
		char * sels_str;
		char * comma;

                if ((open == 0) || (close == 0)) break;
		*open = 0; *close = 0;

		sels_str = strip(str);
		do {
			char * sel_str;
			char * space;
			char * style_str;
			unsigned int selectors = 0;
			struct css_selector * selector = xcalloc(0, sizeof(struct css_selector));

			comma = strchr(sels_str, ',');
			if (comma != 0) *comma = 0;

			sel_str = strip(sels_str);
			/*printf("css_parse_stylesheet: %s\n", sel_str);*/
			do {
				space = strchr(sel_str, ' ');
				if (space != 0) *space = 0;
				selector = xrealloc(selector, sizeof(struct css_selector) * (selectors + 1));
				parse_selector(&selector[selectors++], sel_str);
				if (space != 0) sel_str = strip(space + 1);
			} while (space != 0);

			style_str = xstrdup(open + 1);
			update_style(stylesheet, selector, selectors, style_str);
			free(style_str);

			if (comma != 0) sels_str = strip(comma + 1);
		} while (comma != 0);

		str = close + 1;
	}
}

/**
 * dump a style
 */

static void dump_length(const struct css_length * const length)
{
	printf("%g%s", length->value,
	               css_unit_name[length->unit]);
}

void css_dump_style(const struct css_style * const style)
{
	puts("{");
	printf("\tdisplay: %s;\n", css_display_name[style->display]);
	printf("\tfloat: %s;\n", css_float_name[style->float_]);
	printf("\tfont-size: ");
	switch (style->font_size.size) {
		case CSS_FONT_SIZE_ABSOLUTE: printf("[%g]", style->font_size.value.absolute); break;
		case CSS_FONT_SIZE_LENGTH:   dump_length(&style->font_size.value.length); break;
		case CSS_FONT_SIZE_PERCENT:  printf("%g%%", style->font_size.value.percent); break;
		case CSS_FONT_SIZE_INHERIT:  printf("inherit"); break;
		default:                     printf("UNKNOWN"); break;
	}
	puts(";");
	printf("\theight: ");
	switch (style->height.height) {
		case CSS_HEIGHT_AUTO:   printf("auto"); break;
		case CSS_HEIGHT_LENGTH: dump_length(&style->height.length); break;
		default:                printf("UNKNOWN"); break;
	}
	puts(";");
	printf("\twidth: ");
	switch (style->width.width) {
		case CSS_WIDTH_AUTO:    printf("auto"); break;
		case CSS_WIDTH_LENGTH:  dump_length(&style->width.value.length); break;
		case CSS_WIDTH_PERCENT: printf("%g%%", style->width.value.percent); break;
		default:                printf("UNKNOWN"); break;
	}
	puts(";");
	puts("}");
}

static void dump_selector(const struct css_selector * const sel)
{
	if (sel->class != 0)
		printf("%s.%s ", sel->element, sel->class);
	else if (sel->id != 0)
		printf("%s#%s ", sel->element, sel->id);
	else
		printf("%s ", sel->element);
}

static void dump_rule(const struct rule * rule)
{
	unsigned int i;
	for (i = 0; i < rule->selectors; i++)
		dump_selector(&rule->selector[i]);
	css_dump_style(rule->style);
}

static void css_dump_stylesheet(const struct css_stylesheet * stylesheet)
{
	unsigned int i;
	for (i = 0; i < HASH_SIZE; i++) {
		struct rule * rule;
		printf("hash %i:\n", i);
		for (rule = stylesheet->hash[i]; rule != 0; rule = rule->next)
			dump_rule(rule);
	}
}

/**
 * cascade styles
 */

void css_cascade(struct css_style * const style, const struct css_style * const apply)
{
	float f;
	if (apply->display != CSS_DISPLAY_INHERIT) style->display = apply->display;
	if (apply->float_ != CSS_FLOAT_INHERIT) style->float_ = apply->float_;
	style->height = apply->height;
	style->width = apply->width;

	/* font-size */
	f = apply->font_size.value.percent / 100;
	switch (apply->font_size.size) {
		case CSS_FONT_SIZE_ABSOLUTE: style->font_size = apply->font_size; break;
		case CSS_FONT_SIZE_LENGTH:
			switch (apply->font_size.value.length.unit) {
				case CSS_UNIT_EM: f = apply->font_size.value.length.value; break;
				case CSS_UNIT_EX: f = apply->font_size.value.length.value * 0.6 /*?*/; break;
				default: style->font_size = apply->font_size;
			}
			if ((apply->font_size.value.length.unit != CSS_UNIT_EM) &&
			    (apply->font_size.value.length.unit != CSS_UNIT_EX)) break;
			/* drop through if EM or EX */
		case CSS_FONT_SIZE_PERCENT:
			switch (style->font_size.size) {
				case CSS_FONT_SIZE_ABSOLUTE: style->font_size.value.absolute *= f; break;
				case CSS_FONT_SIZE_LENGTH: style->font_size.value.length.value *= f; break;
				default: die("attempting percentage of unknown font-size");
			}
			break;
		case CSS_FONT_SIZE_INHERIT:
		default:                     /* leave unchanged */
	}
}

/**
 * testing
 */
/*
int main(int argv, char *argc[])
{
	int i;
	struct rule * r;
	struct css_stylesheet * s;
	struct css_selector sel = { "h1", 0, 0 };
	struct css_selector con[] = {{ "html", 0, 0 }, {"body", 0, 0}, {"h1", "foo", 0}, {"b", 0, "bar"}};
	struct css_style * style = xcalloc(1, sizeof(struct css_style));

	s = css_new_stylesheet();
	css_parse_stylesheet(s, argc[1]);
	css_dump_stylesheet(s);

/* 	r->selectors = 1; */
/* 	css_stylesheet_add_rule(s, r); */
/*	puts("********** finding h1:");
	r = find_rule(s, &sel, 1);
	if (r)
		dump_rule(r);
	else
		puts("not found");

	puts("********** finding html body h1.foo b#bar:");
	for (i = 1; i <= sizeof(con) / sizeof(con[0]); i++) {
		css_get_style(s, con, i, style);
		css_dump_style(style);
	}

/* 	printf("%x %x\n", r, r2); */

/*	struct css_style *s;
	struct css_selector *sel;

	s = parse_property_list(argc[1]);
	css_dump_style(s);

	for (i = 2; i < argv; i++) {
 		css_cascade(s, parse_property_list(argc[i]));
		css_dump_style(s);
	}*/

/*	for (i = 1; i < argv; i++) {
		sel = parse_selector(argc[i]);
		css_dump_selector(sel);
		puts("");
	}*/
/*
	return 0;
}
*/
