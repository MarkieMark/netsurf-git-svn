/**
 * $Id: ruleset.c,v 1.1 2003/04/04 15:19:31 bursa Exp $
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define CSS_INTERNALS
#include "netsurf/css/css.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


struct property_entry {
	const char name[20];
	void (*parse) (struct css_style * const s, const struct node * const v);
};

struct colour_entry {
	const char name[12];
	colour col;
};

struct font_size_entry {
	const char name[10];
	float size;
};


static int parse_length(struct css_length * const length, const struct node * const v);
static colour parse_colour(const struct node * const v);
static void parse_background_color(struct css_style * const s, const struct node * const v);
static void parse_clear(struct css_style * const s, const struct node * const v);
static void parse_color(struct css_style * const s, const struct node * const v);
static void parse_display(struct css_style * const s, const struct node * const v);
static void parse_float(struct css_style * const s, const struct node * const v);
static void parse_font_size(struct css_style * const s, const struct node * const v);
static void parse_font_style(struct css_style * const s, const struct node * const v);
static void parse_font_weight(struct css_style * const s, const struct node * const v);
static void parse_height(struct css_style * const s, const struct node * const v);
static void parse_line_height(struct css_style * const s, const struct node * const v);
static void parse_text_align(struct css_style * const s, const struct node * const v);
static void parse_width(struct css_style * const s, const struct node * const v);


/* table of property parsers: MUST be sorted by property name */
static const struct property_entry property_table[] = {
	{ "background-color", parse_background_color },
	{ "clear",            parse_clear },
	{ "color",            parse_color },
	{ "display",          parse_display },
	{ "float",            parse_float },
	{ "font-size",        parse_font_size },
	{ "font-style",       parse_font_style },
	{ "font-weight",      parse_font_weight },
	{ "height",           parse_height },
	{ "line-height",      parse_line_height },
	{ "text-align",       parse_text_align },
	{ "width",            parse_width },
};

/* table of standard colour names: MUST be sorted by colour name */
static const struct colour_entry colour_table[] = {
	{ "aqua",    0x00ffff },
	{ "black",   0x000000 },
	{ "blue",    0x0000ff },
	{ "fuchsia", 0xff00ff },
	{ "gray",    0x808080 },
	{ "green",   0x008000 },
	{ "lime",    0x00ff00 },
	{ "maroon",  0x800000 },
	{ "navy",    0x000080 },
	{ "olive",   0x808000 },
	{ "purple",  0x800080 },
	{ "red",     0xff0000 },
	{ "silver",  0xc0c0c0 },
	{ "teal",    0x008080 },
	{ "transparent", TRANSPARENT },
	{ "white",   0xffffff },
	{ "yellow",  0xffff00 },
};

/* table of font sizes: MUST be sorted by name */
#define SIZE_FACTOR 1.2
static const struct font_size_entry font_size_table[] = {
	{ "large", 1.0 * SIZE_FACTOR },
	{ "medium", 1.0 },
	{ "small", 1.0 / SIZE_FACTOR },
	{ "x-large", 1.0 * SIZE_FACTOR * SIZE_FACTOR },
	{ "x-small", 1.0 / (SIZE_FACTOR * SIZE_FACTOR) },
	{ "xx-large", 1.0 * SIZE_FACTOR * SIZE_FACTOR * SIZE_FACTOR },
	{ "xx-small", 1.0 / (SIZE_FACTOR * SIZE_FACTOR * SIZE_FACTOR) },
};


/**
 * css_add_ruleset -- add a ruleset to a stylesheet
 */

void css_add_ruleset(struct css_stylesheet *stylesheet,
		struct node *selector,
		struct node *declaration)
{
	struct node *n, *last;
	struct css_style *style;

	/* construct the struct css_style */
	style = xcalloc(1, sizeof(*style));
	memcpy(style, &css_blank_style, sizeof(*style));

	for (n = declaration; n != 0; n = n->next) {
		struct property_entry *p;
		assert(n->type == NODE_DECLARATION && n->data != 0 && n->left != 0);
		p = bsearch(n->data, property_table,
				sizeof(property_table) / sizeof(property_table[0]),
				sizeof(property_table[0]), strcasecmp);
		if (p == 0)
			continue;
		p->parse(style, n->left);
	}

	/*css_dump_style(style);*/

	/* add selectors to the stylesheet */
	/* TODO: merge with identical selector */
	for (n = selector; n != 0; n = n->next) {
		n->style = style;
		last = n;
	}

	if (stylesheet->rule == 0)
		stylesheet->rule = selector;
	else
		stylesheet->last_rule->next = selector;
	stylesheet->last_rule = last;
}




/**
 * property parsers
 */

int parse_length(struct css_length * const length, const struct node * const v)
{
	css_unit u;
	if (v->type != NODE_DIMENSION)
		return 1;
	u = css_unit_parse(v->data + strspn(v->data, "0123456789+-."));
	if (u == CSS_UNIT_UNKNOWN)
		return 1;
	length->unit = u;
	length->value = atof(v->data);
	return 0;
}


colour parse_colour(const struct node * const v)
{
	colour c = TRANSPARENT;
	int len;
	unsigned int r, g, b;
	struct colour_entry *col;

	switch (v->type) {
		case NODE_HASH:
			len = strlen(v->data);
			if (len == 4) {
				if (sscanf(v->data + 1, "%1x%1x%1x", &r, &g, &b) == 3)
					c = (b << 20) | (b << 16) | (g << 12) | (g << 8) | (r << 4) | r;
			} else if (len == 7) {
				if (sscanf(v->data + 1, "%2x%2x%2x", &r, &g, &b) == 3)
					c = (b << 16) | (g << 8) | r;
			}
			break;

		case NODE_FUNCTION:
			/* TODO: rgb(r, g, b) */
			break;

		case NODE_IDENT:
			col = bsearch(v->data, colour_table,
					sizeof(colour_table) / sizeof(colour_table[0]),
					sizeof(colour_table[0]), strcasecmp);
			if (col != 0)
				c = col->col;
			break;

		default:
			break;
	}

	return c;
}


void parse_background_color(struct css_style * const s, const struct node * const v)
{
	s->background_color = parse_colour(v);
}

void parse_clear(struct css_style * const s, const struct node * const v)
{
	css_clear z;
	if (v->type != NODE_IDENT || v->next != 0)
		return;
	z = css_clear_parse(v->data);
	if (z != CSS_CLEAR_UNKNOWN)
		s->clear = z;
}

void parse_color(struct css_style * const s, const struct node * const v)
{
	s->color = parse_colour(v);
}

void parse_display(struct css_style * const s, const struct node * const v)
{
	css_display z;
	if (v->type != NODE_IDENT || v->next != 0)
		return;
	z = css_display_parse(v->data);
	if (z != CSS_DISPLAY_UNKNOWN)
		s->display = z;
}

void parse_float(struct css_style * const s, const struct node * const v)
{
	css_float z;
	if (v->type != NODE_IDENT || v->next != 0)
		return;
	z = css_float_parse(v->data);
	if (z != CSS_FLOAT_UNKNOWN)
		s->float_ = z;
}

void parse_font_size(struct css_style * const s, const struct node * const v)
{
	struct font_size_entry *fs;
	switch (v->type) {
		case NODE_IDENT:
			fs = bsearch(v->data, font_size_table,
					sizeof(font_size_table) / sizeof(font_size_table[0]),
					sizeof(font_size_table[0]), strcasecmp);
			if (fs != 0) {
				s->font_size.size = CSS_FONT_SIZE_LENGTH;
				s->font_size.value.length.unit = CSS_UNIT_PT;
				s->font_size.value.length.value = fs->size * 12;
			} else if (strcasecmp(v->data, "larger") == 0) {
				s->font_size.size = CSS_FONT_SIZE_PERCENT;
				s->font_size.value.percent = SIZE_FACTOR * 100;
			} else if (strcmp(v->data, "smaller") == 0) {
				s->font_size.size = CSS_FONT_SIZE_PERCENT;
				s->font_size.value.percent = 1 / SIZE_FACTOR * 100;
		        }
		        break;

		case NODE_PERCENTAGE:
			s->font_size.size = CSS_FONT_SIZE_PERCENT;
			s->font_size.value.percent = atof(v->data);
			break;

		case NODE_DIMENSION:
			if (parse_length(&s->font_size.value.length, v) == 0)
				s->font_size.size = CSS_FONT_SIZE_LENGTH;
			break;

		default:
			break;
	}
}

void parse_font_style(struct css_style * const s, const struct node * const v)
{
	css_font_style z;
	if (v->type != NODE_IDENT || v->next != 0)
		return;
	z = css_font_style_parse(v->data);
	if (z != CSS_FONT_STYLE_UNKNOWN)
		s->font_style = z;
}

void parse_font_weight(struct css_style * const s, const struct node * const v)
{
	css_font_weight z;
	if (v->type != NODE_IDENT || v->next != 0)
		return;
	z = css_font_weight_parse(v->data);
	if (z != CSS_FONT_WEIGHT_UNKNOWN)
		s->font_weight = z;
}
void parse_height(struct css_style * const s, const struct node * const v)
{
	if (v->type == NODE_IDENT && strcasecmp(v->data, "auto") == 0)
		s->height.height = CSS_HEIGHT_AUTO;
	else if (v->type == NODE_DIMENSION && parse_length(&s->height.length, v) == 0)
		s->height.height = CSS_HEIGHT_LENGTH;
}

void parse_line_height(struct css_style * const s, const struct node * const v)
{
	if (v->type == NODE_IDENT && strcasecmp(v->data, "normal") == 0) {
		s->line_height.size = CSS_LINE_HEIGHT_ABSOLUTE;
		s->line_height.value.absolute = 1.0;
	} else if (v->type == NODE_PERCENTAGE) {
		s->line_height.size = CSS_LINE_HEIGHT_PERCENT;
		s->line_height.value.percent = atof(v->data);
	} else if (v->type == NODE_DIMENSION &&
			parse_length(&s->line_height.value.length, v) == 0) {
		s->line_height.size = CSS_LINE_HEIGHT_LENGTH;
	}
}

void parse_text_align(struct css_style * const s, const struct node * const v)
{
	css_text_align z;
	if (v->type != NODE_IDENT || v->next != 0)
		return;
	z = css_text_align_parse(v->data);
	if (z != CSS_TEXT_ALIGN_UNKNOWN)
		s->text_align = z;
}

void parse_width(struct css_style * const s, const struct node * const v)
{
	if (v->type == NODE_IDENT && strcasecmp(v->data, "auto") == 0)
		s->width.width = CSS_WIDTH_AUTO;
	else if (v->type == NODE_PERCENTAGE) {
		s->width.width = CSS_WIDTH_PERCENT;
		s->width.value.percent = atof(v->data);
	} else if (v->type == NODE_DIMENSION &&
			parse_length(&s->width.value.length, v) == 0)
		s->width.width = CSS_WIDTH_LENGTH;
}
