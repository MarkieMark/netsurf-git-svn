/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "libxml/HTMLparser.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/css/css.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/html.h"
#ifdef riscos
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/plugin.h"
#endif
#define NDEBUG
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


/* status for box tree construction */
struct status {
	struct content *content;
	char *href;
	char *title;
	struct form* current_form;
	struct page_elements* elements;
};

/* result of converting a special case element */
struct result {
	struct box *box;       /* box for element, if any, 0 otherwise */
	int convert_children;  /* children should be converted */
};

static void box_add_child(struct box * parent, struct box * child);
static struct box * convert_xml_to_box(xmlNode * n, struct content *content,
		struct css_style * parent_style,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box *inline_container,
		struct status status);
static struct css_style * box_get_style(struct content ** stylesheet,
		unsigned int stylesheet_count, struct css_style * parent_style,
		xmlNode * n, struct css_selector * selector, unsigned int depth);
static struct result box_a(xmlNode *n, struct status *status,
		struct css_style *style);
static struct result box_body(xmlNode *n, struct status *status,
		struct css_style *style);
static struct result box_image(xmlNode *n, struct status *status,
		struct css_style *style);
static struct result box_form(xmlNode *n, struct status *status,
		struct css_style *style);
static struct result box_textarea(xmlNode *n, struct status *status,
		struct css_style *style);
static struct result box_select(xmlNode *n, struct status *status,
		struct css_style *style);
static struct result box_input(xmlNode *n, struct status *status,
		struct css_style *style);
static struct box *box_input_text(xmlNode *n, struct status *status,
		struct css_style *style, bool password);
static struct result box_button(xmlNode *n, struct status *status,
		struct css_style *style);
static void add_option(xmlNode* n, struct gui_gadget* current_select, char *text);
static void box_normalise_block(struct box *block);
static void box_normalise_table(struct box *table);
void box_normalise_table_row_group(struct box *row_group,
		unsigned int **row_span, unsigned int *table_columns);
void box_normalise_table_row(struct box *row,
		unsigned int **row_span, unsigned int *table_columns);
static void box_normalise_inline_container(struct box *cont);
static void gadget_free(struct gui_gadget* g);
static void box_free_box(struct box *box);
static struct result box_object(xmlNode *n, struct status *status,
		struct css_style *style);
static struct result box_embed(xmlNode *n, struct status *status,
		struct css_style *style);
static struct result box_applet(xmlNode *n, struct status *status,
		struct css_style *style);
static struct result box_iframe(xmlNode *n, struct status *status,
		struct css_style *style);
static struct form* create_form(xmlNode* n);
static void add_form_element(struct page_elements* pe, struct form* f);
static void add_gadget_element(struct page_elements* pe, struct gui_gadget* g);
static bool plugin_decode(struct content* content, char* url, struct box* box,
                  struct object_params* po);

/* element_table must be sorted by name */
struct element_entry {
	char name[10];   /* element type */
	struct result (*convert)(xmlNode *n, struct status *status,
			struct css_style *style);
};
static const struct element_entry element_table[] = {
	{"a", box_a},
	{"applet", box_applet},
	{"body", box_body},
	{"button", box_button},
	{"embed", box_embed},
	{"form", box_form},
	{"iframe", box_iframe},
	{"img", box_image},
	{"input", box_input},
	{"object", box_object},
	{"select", box_select},
	{"textarea", box_textarea}
};
#define ELEMENT_TABLE_COUNT (sizeof(element_table) / sizeof(element_table[0]))


/**
 * add a child to a box tree node
 */

void box_add_child(struct box * parent, struct box * child)
{
	if (parent->children != 0) {	/* has children already */
		parent->last->next = child;
		child->prev = parent->last;
	} else {			/* this is the first child */
		parent->children = child;
		child->prev = 0;
	}

	parent->last = child;
	child->parent = parent;
}

/**
 * create a box tree node
 */

struct box * box_create(struct css_style * style,
		char *href, char *title)
{
	struct box * box = xcalloc(1, sizeof(struct box));
	box->type = BOX_INLINE;
	box->style = style;
	box->width = UNKNOWN_WIDTH;
	box->max_width = UNKNOWN_MAX_WIDTH;
	box->href = href ? xstrdup(href) : 0;
	box->title = title ? xstrdup(title) : 0;
	box->columns = 1;
	box->rows = 1;
#ifndef riscos
	/* under RISC OS, xcalloc makes these unnecessary */
	box->text = 0;
	box->space = 0;
	box->clone = 0;
	box->style_clone = 0;
	box->length = 0;
	box->start_column = 0;
	box->next = 0;
	box->prev = 0;
	box->children = 0;
	box->last = 0;
	box->parent = 0;
	box->float_children = 0;
	box->next_float = 0;
	box->col = 0;
	box->font = 0;
	box->gadget = 0;
	box->object = 0;
	box->object_params = 0;
	box->object_state = 0;
#endif
	return box;
}


/**
 * make a box tree with style data from an xml tree
 */

void xml_to_box(xmlNode *n, struct content *c)
{
	struct css_selector* selector = xcalloc(1, sizeof(struct css_selector));
	struct status status = {c, 0, 0, 0, &c->data.html.elements};

	LOG(("node %p", n));
	assert(c->type == CONTENT_HTML);

	c->data.html.layout = box_create(0, 0, 0);
	c->data.html.layout->type = BOX_BLOCK;

	c->data.html.style = xcalloc(1, sizeof(struct css_style));
	memcpy(c->data.html.style, &css_base_style, sizeof(struct css_style));
	c->data.html.fonts = font_new_set();

	c->data.html.object_count = 0;
	c->data.html.object = xcalloc(0, sizeof(*c->data.html.object));

	convert_xml_to_box(n, c, c->data.html.style,
			&selector, 0, c->data.html.layout, 0, status);
	LOG(("normalising"));
	box_normalise_block(c->data.html.layout->children);
}


/**
 * make a box tree with style data from an xml tree
 *
 * arguments:
 * 	n		xml tree
 * 	content		content structure
 * 	parent_style	style at this point in xml tree
 * 	selector	element selector hierachy to this point
 * 	depth		depth in xml tree
 * 	parent		parent in box tree
 * 	inline_container	current inline container box, or 0
 * 	status		status for forms etc.
 *
 * returns:
 * 	updated current inline container
 */

/* mapping from CSS display to box type
 * this table must be in sync with css/css_enums */
static box_type box_map[] = {
	0, /*CSS_DISPLAY_INHERIT,*/
	BOX_INLINE, /*CSS_DISPLAY_INLINE,*/
	BOX_BLOCK, /*CSS_DISPLAY_BLOCK,*/
	BOX_BLOCK, /*CSS_DISPLAY_LIST_ITEM,*/
	BOX_INLINE, /*CSS_DISPLAY_RUN_IN,*/
	BOX_INLINE_BLOCK, /*CSS_DISPLAY_INLINE_BLOCK,*/
	BOX_TABLE, /*CSS_DISPLAY_TABLE,*/
	BOX_TABLE, /*CSS_DISPLAY_INLINE_TABLE,*/
	BOX_TABLE_ROW_GROUP, /*CSS_DISPLAY_TABLE_ROW_GROUP,*/
	BOX_TABLE_ROW_GROUP, /*CSS_DISPLAY_TABLE_HEADER_GROUP,*/
	BOX_TABLE_ROW_GROUP, /*CSS_DISPLAY_TABLE_FOOTER_GROUP,*/
	BOX_TABLE_ROW, /*CSS_DISPLAY_TABLE_ROW,*/
	BOX_INLINE, /*CSS_DISPLAY_TABLE_COLUMN_GROUP,*/
	BOX_INLINE, /*CSS_DISPLAY_TABLE_COLUMN,*/
	BOX_TABLE_CELL, /*CSS_DISPLAY_TABLE_CELL,*/
	BOX_INLINE /*CSS_DISPLAY_TABLE_CAPTION,*/
};

struct box * convert_xml_to_box(xmlNode * n, struct content *content,
		struct css_style * parent_style,
		struct css_selector ** selector, unsigned int depth,
		struct box * parent, struct box *inline_container,
		struct status status)
{
	struct box * box = 0;
	struct box * inline_container_c;
	struct css_style * style = 0;
	xmlNode * c;
	char * s;
	char * text = 0;
	xmlChar * title0;
	char * title = 0;
	int convert_children = 1;
	char *href_in = status.href;

	assert(n != 0 && parent_style != 0 && selector != 0 && parent != 0);
	LOG(("depth %i, node %p, node type %i", depth, n, n->type));
	gui_multitask();

	if (n->type == XML_ELEMENT_NODE) {
		struct element_entry *element;

		/* work out the style for this element */
		*selector = xrealloc(*selector, (depth + 1) * sizeof(struct css_selector));
		(*selector)[depth].element = (const char *) n->name;
		(*selector)[depth].class = (*selector)[depth].id = 0;
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "class")))
			(*selector)[depth].class = s;
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "id")))
			(*selector)[depth].id = s;
		style = box_get_style(content->data.html.stylesheet_content,
				content->data.html.stylesheet_count, parent_style, n,
				*selector, depth + 1);
		LOG(("display: %s", css_display_name[style->display]));
		if (style->display == CSS_DISPLAY_NONE) {
			free(style);
			LOG(("depth %i, node %p, node type %i END", depth, n, n->type));
			goto end;
		}
		/* floats are treated as blocks */
		if (style->float_ == CSS_FLOAT_LEFT || style->float_ == CSS_FLOAT_RIGHT)
			if (style->display == CSS_DISPLAY_INLINE)
				style->display = CSS_DISPLAY_BLOCK;

		/* extract title attribute, if present */
		if ((title0 = xmlGetProp(n, (const xmlChar *) "title"))) {
			status.title = title = squash_tolat1(title0);
			xfree(title0);
		}

		/* special elements */
		element = bsearch((const char *) n->name, element_table,
				ELEMENT_TABLE_COUNT, sizeof(element_table[0]),
				(int (*)(const void *, const void *)) strcmp);
		if (element != 0) {
			/* a special convert function exists for this element */
			struct result res = element->convert(n, &status, style);
			box = res.box;
			convert_children = res.convert_children;
			if (box == 0) {
				/* no box for this element */
				assert(convert_children == 0);
				free(style);
				LOG(("depth %i, node %p, node type %i END", depth, n, n->type));
				goto end;
			}
		} else {
			/* general element */
			box = box_create(style, status.href, title);
		}
		box->type = box_map[style->display];

	} else if (n->type == XML_TEXT_NODE) {
		text = squash_tolat1(n->content);

		/* if the text is just a space, combine it with the preceding
		 * text node, if any */
		if (text[0] == ' ' && text[1] == 0) {
			if (inline_container != 0) {
				assert(inline_container->last != 0);
				inline_container->last->space = 1;
			}
			xfree(text);
			LOG(("depth %i, node %p, node type %i END", depth, n, n->type));
			goto end;
		}

		/* text nodes are converted to inline boxes */
		box = box_create(parent_style, status.href, title);
		box->style_clone = 1;
		box->length = strlen(text);
		if (text[box->length - 1] == ' ') {
			box->space = 1;
			box->length--;
		}
		box->text = text;
		box->font = font_open(content->data.html.fonts, box->style);

	} else {
		/* not an element or text node: ignore it (eg. comment) */
		LOG(("depth %i, node %p, node type %i END", depth, n, n->type));
		goto end;
	}

	content->size += sizeof(struct box) + sizeof(struct css_style);
	assert(box != 0);

	if (text != 0 ||
			box->type == BOX_INLINE ||
			box->type == BOX_INLINE_BLOCK ||
			style->float_ == CSS_FLOAT_LEFT ||
			style->float_ == CSS_FLOAT_RIGHT) {
		/* this is an inline box */
		if (inline_container == 0) {
			/* this is the first inline node: make a container */
			inline_container = box_create(0, 0, 0);
			inline_container->type = BOX_INLINE_CONTAINER;
			box_add_child(parent, inline_container);
		}

		if (text != 0) {
			/* text box */
			box_add_child(inline_container, box);
			if (text[0] == ' ') {
				box->length--;
				memmove(text, text + 1, box->length);
				if (box->prev != 0)
					box->prev->space = 1;
			}
			LOG(("depth %i, node %p, node type %i END", depth, n, n->type));
			goto end;
		} else if (box->type == BOX_INLINE) {
			/* inline box: add to tree and recurse */
			box_add_child(inline_container, box);
			if (convert_children) {
				for (c = n->children; c != 0; c = c->next)
					inline_container = convert_xml_to_box(c, content, style,
							selector, depth + 1, parent, inline_container,
							status);
			}
			LOG(("depth %i, node %p, node type %i END", depth, n, n->type));
			goto end;
		} else if (box->type == BOX_INLINE_BLOCK) {
			/* inline block box: add to tree and recurse */
			box_add_child(inline_container, box);
			if (convert_children) {
				inline_container_c = 0;
				for (c = n->children; c != 0; c = c->next)
					inline_container_c = convert_xml_to_box(c, content, style,
							selector, depth + 1, box, inline_container_c,
							status);
			}
			LOG(("depth %i, node %p, node type %i END", depth, n, n->type));
			goto end;
		} else {
			/* float: insert a float box between the parent and current node */
			assert(style->float_ == CSS_FLOAT_LEFT || style->float_ == CSS_FLOAT_RIGHT);
			LOG(("float"));
			parent = box_create(0, status.href, title);
			if (style->float_ == CSS_FLOAT_LEFT)
				parent->type = BOX_FLOAT_LEFT;
			else
				parent->type = BOX_FLOAT_RIGHT;
			box_add_child(inline_container, parent);
			if (box->type == BOX_INLINE || box->type == BOX_INLINE_BLOCK)
				box->type = BOX_BLOCK;
		}
	}

	assert(n->type == XML_ELEMENT_NODE);
	assert(CSS_DISPLAY_INLINE < style->display &&
			style->display < CSS_DISPLAY_NONE);

	/* non-inline box: add to tree and recurse */
	box_add_child(parent, box);
	if (convert_children) {
		inline_container_c = 0;
		for (c = n->children; c != 0; c = c->next)
			inline_container_c = convert_xml_to_box(c, content, style,
					selector, depth + 1, box, inline_container_c,
					status);
	}
	if (style->float_ == CSS_FLOAT_NONE)
		/* new inline container unless this is a float */
		inline_container = 0;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "colspan"))) {
		if ((box->columns = strtol(s, 0, 10)) == 0)
			box->columns = 1;
		xmlFree(s);
	}
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "rowspan"))) {
		if ((box->rows = strtol(s, 0, 10)) == 0)
			box->rows = 1;
		xmlFree(s);
	}

end:
	free(title);
	if (!href_in)
		xmlFree(status.href);
	if (n->type == XML_ELEMENT_NODE) {
		free((*selector)[depth].class);
		free((*selector)[depth].id);
	}

	LOG(("depth %i, node %p, node type %i END", depth, n, n->type));
	return inline_container;
}


/**
 * Get the style for an element
 *
 * The style is collected from three sources:
 *  1. any styles for this element in the document stylesheet(s)
 *  2. non-CSS HTML attributes
 *  3. the 'style' attribute
 */

struct css_style * box_get_style(struct content ** stylesheet,
		unsigned int stylesheet_count, struct css_style * parent_style,
		xmlNode * n, struct css_selector * selector, unsigned int depth)
{
	struct css_style * style = xcalloc(1, sizeof(struct css_style));
	struct css_style * style_new = xcalloc(1, sizeof(struct css_style));
	char * s;
	unsigned int i;

	memcpy(style, parent_style, sizeof(struct css_style));
	memcpy(style_new, &css_blank_style, sizeof(struct css_style));
	for (i = 0; i != stylesheet_count; i++) {
		if (stylesheet[i] != 0) {
			assert(stylesheet[i]->type == CONTENT_CSS);
			css_get_style(stylesheet[i], n, style_new);
		}
	}
	css_cascade(style, style_new);
	free(style_new);

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "bgcolor"))) {
		unsigned int r, g, b;
		if (s[0] == '#' && sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
			style->background_color = (b << 16) | (g << 8) | r;
		else if (s[0] != '#')
			style->background_color = named_colour(s);
		xmlFree(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "color"))) {
		unsigned int r, g, b;
		if (s[0] == '#' && sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
			style->color = (b << 16) | (g << 8) | r;
		else if (s[0] != '#')
			style->color = named_colour(s);
		xmlFree(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "height"))) {
		float value = atof(s);
		if (value < 0) {
			/* ignore negative values */
		} else if (strrchr(s, '%')) {
	                /*the specification doesn't make clear what
	                 * percentage heights mean, so ignore them */
	        } else {
			style->height.height = CSS_HEIGHT_LENGTH;
			style->height.length.unit = CSS_UNIT_PX;
			style->height.length.value = value;
		}
		xmlFree(s);
	}

	if (strcmp((const char *) n->name, "input") == 0) {
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "size"))) {
			int size = atoi(s);
			if (0 < size) {
				char *type = (char *) xmlGetProp(n, (const xmlChar *) "type");
				style->width.width = CSS_WIDTH_LENGTH;
				if (!type || stricmp(type, "text") == 0 ||
						stricmp(type, "password") == 0)
					/* in characters for text or password */
					style->width.value.length.unit = CSS_UNIT_EX;
				else
					/* in pixels otherwise */
					style->width.value.length.unit = CSS_UNIT_PX;
				style->width.value.length.value = size;
				if (type)
					xmlFree(type);
			}
			xmlFree(s);
		}
	}

	if (strcmp((const char *) n->name, "body") == 0) {
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "text"))) {
			unsigned int r, g, b;
			if (s[0] == '#' && sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
				style->color = (b << 16) | (g << 8) | r;
			else if (s[0] != '#')
				style->color = named_colour(s);
			xmlFree(s);
		}
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "width"))) {
		float value = atof(s);
		if (value < 0) {
			/* ignore negative values */
		} else if (strrchr(s, '%')) {
			style->width.width = CSS_WIDTH_PERCENT;
			style->width.value.percent = value;
		} else {
			style->width.width = CSS_WIDTH_LENGTH;
			style->width.value.length.unit = CSS_UNIT_PX;
			style->width.value.length.value = value;
		}
		xmlFree(s);
	}

	if (strcmp((const char *) n->name, "textarea") == 0) {
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "rows"))) {
			int value = atoi(s);
			if (0 < value) {
				style->height.height = CSS_HEIGHT_LENGTH;
				style->height.length.unit = CSS_UNIT_EM;
				style->height.length.value = value;
			}
			xmlFree(s);
		}
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "cols"))) {
			int value = atoi(s);
			if (0 < value) {
				style->width.width = CSS_WIDTH_LENGTH;
				style->width.value.length.unit = CSS_UNIT_EX;
				style->width.value.length.value = value;
			}
			xmlFree(s);
		}
	}
	
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "style"))) {
		struct css_style * astyle = xcalloc(1, sizeof(struct css_style));
		memcpy(astyle, &css_empty_style, sizeof(struct css_style));
		css_parse_property_list(astyle, s);
		css_cascade(style, astyle);
		free(astyle);
		xmlFree(s);
	}

	return style;
}


/**
 * Special case elements
 *
 * These functions are called by convert_xml_to_box when an element is being
 * converted, according to the entries in element_table (top of file).
 *
 * The parameters are the xmlNode, a status structure for the conversion, and
 * the style found for the element.
 *
 * If a box is created, it is returned in the result structure. The
 * convert_children field should be 1 if convert_xml_to_box should convert the
 * node's children recursively, 0 if it should ignore them (presumably they
 * have been processed in some way by the function). If box is 0, no box will
 * be created for that element, and convert_children must be 0.
 */

struct result box_a(xmlNode *n, struct status *status,
		struct css_style *style)
{
	struct box *box;
	char *s;
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "href")))
		status->href = s;
	box = box_create(style, status->href, status->title);
	return (struct result) {box, 1};
}

struct result box_body(xmlNode *n, struct status *status,
		struct css_style *style)
{
	struct box *box;
	status->content->data.html.background_colour = style->background_color;
	box = box_create(style, status->href, status->title);
	return (struct result) {box, 1};
}

struct result box_image(xmlNode *n, struct status *status,
		struct css_style *style)
{
	struct box *box;
	char *s, *url;
	xmlChar *s2;

	box = box_create(style, status->href, status->title);

	/* handle alt text */
	if ((s2 = xmlGetProp(n, (const xmlChar *) "alt"))) {
		box->text = squash_tolat1(s2);
		box->length = strlen(box->text);
		box->font = font_open(status->content->data.html.fonts, style);
		free(s2);
	}

	/* img without src is an error */
	if (!(s = (char *) xmlGetProp(n, (const xmlChar *) "src")))
		return (struct result) {box, 0};

	url = url_join(s, status->content->url);
	LOG(("image '%s'", url));
	xmlFree(s);

	/* start fetch */
	html_fetch_object(status->content, url, box);

	return (struct result) {box, 0};
}

struct result box_form(xmlNode *n, struct status *status,
		struct css_style *style)
{
	struct box *box;
	box = box_create(style, status->href, status->title);
	status->current_form = create_form(n);
	add_form_element(status->elements, status->current_form);
	return (struct result) {box, 1};
}

struct result box_textarea(xmlNode *n, struct status *status,
		struct css_style *style)
{
	xmlChar *content, *current;
	struct box *box, *inline_container, *inline_box;
	char* s;

	box = box_create(style, NULL, 0);
	box->gadget = xcalloc(1, sizeof(struct gui_gadget));
	box->gadget->type = GADGET_TEXTAREA;
	box->gadget->form = status->current_form;
	style->display = CSS_DISPLAY_INLINE_BLOCK;

	/* split the content at newlines and make an inline container with an
	 * inline box for each line */
	current = content = xmlNodeGetContent(n);
	do {
		size_t len = strcspn(current, "\r\n");
		char old = current[len];
		current[len] = 0;
		inline_container = box_create(0, 0, 0);
		inline_container->type = BOX_INLINE_CONTAINER;
		inline_box = box_create(style, 0, 0);
		inline_box->type = BOX_INLINE;
		inline_box->style_clone = 1;
		inline_box->text = tolat1(current);
		inline_box->length = strlen(inline_box->text);
		inline_box->font = font_open(status->content->data.html.fonts, style);
		box_add_child(inline_container, inline_box);
		box_add_child(box, inline_container);
		current[len] = old;
		current += len;
		current += strspn(current, "\r\n");
	} while (*current);
	xmlFree(content);

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name")))
	{
		box->gadget->name = s;
	}

	add_gadget_element(status->elements, box->gadget);

	return (struct result) {box, 0};
}

struct result box_select(xmlNode *n, struct status *status,
		struct css_style *style)
{
	struct box* box = 0;
	char* s;
	xmlNode *c;

	box = box_create(style, NULL, 0);
	box->gadget = xcalloc(1, sizeof(struct gui_gadget));
	box->gadget->type = GADGET_SELECT;
	box->gadget->form = status->current_form;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "size")))
	{
		box->gadget->data.select.size = atoi(s);
		xmlFree(s);
	}
	else
		box->gadget->data.select.size = 1;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "multiple"))) {
		box->gadget->data.select.multiple = 1;
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name"))) {
		box->gadget->name = s;
	}

	box->gadget->data.select.items = NULL;
	box->gadget->data.select.numitems = 0;
	/* TODO: multiple */

	for (c = n->children; c != 0; c = c->next) {
		if (strcmp((const char *) c->name, "option") == 0) {
			xmlChar *content = xmlNodeGetContent(c);
			add_option(c, box->gadget, squash_tolat1(content));
			xmlFree(content);
		}
	}
	add_gadget_element(status->elements, box->gadget);

	return (struct result) {box, 0};
}

void add_option(xmlNode* n, struct gui_gadget* current_select, char *text)
{
	struct formoption* option;
	char* s;
	assert(current_select != 0);

	if (current_select->data.select.items == 0)
	{
		option = xcalloc(1, sizeof(struct formoption));
		current_select->data.select.items = option;
	}
	else
	{
		struct formoption* current;
		option = xcalloc(1, sizeof(struct formoption));
		current = current_select->data.select.items;
		/* TODO: make appending constant time */
		while (current->next != 0)
			current = current->next;
		current->next = option;
	}

	option->text = text;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "selected"))) {
		option->selected = -1;
		xmlFree(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
		option->value = s;
	}
}

struct result box_input(xmlNode *n, struct status *status,
		struct css_style *style)
{
	struct box* box = 0;
	struct gui_gadget *gadget = 0;
	char *s, *type, *url;

	type = (char *) xmlGetProp(n, (const xmlChar *) "type");

	/* the default type is "text" */
	if (type == 0 || stricmp(type, "text") == 0)
	{
		box = box_input_text(n, status, style, false);
		gadget = box->gadget;
	}
	else if (stricmp(type, "password") == 0)
	{
		box = box_input_text(n, status, style, true);
		gadget = box->gadget;
	}
	else if (stricmp(type, "hidden") == 0)
	{
		/* no box for hidden inputs */
		gadget = xcalloc(1, sizeof(struct gui_gadget));
		gadget->type = GADGET_HIDDEN;

		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value")))
			gadget->data.hidden.value = s;
	}
	else if (stricmp(type, "checkbox") == 0 || stricmp(type, "radio") == 0)
	{
		box = box_create(style, NULL, 0);
		box->gadget = gadget = xcalloc(1, sizeof(struct gui_gadget));
		if (type[0] == 'c' || type[0] == 'C')
			gadget->type = GADGET_CHECKBOX;
		else
			gadget->type = GADGET_RADIO;

		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "checked"))) {
			if (gadget->type == GADGET_CHECKBOX)
				gadget->data.checkbox.selected = -1;
			else
				gadget->data.radio.selected = -1;
			xmlFree(s);
		}

		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
			if (gadget->type == GADGET_CHECKBOX)
				gadget->data.checkbox.value = s;
			else
				gadget->data.radio.value = s;
		}
	}
	else if (stricmp(type, "submit") == 0 || stricmp(type, "reset") == 0)
	{
		struct result result = box_button(n, status, style);
		struct box *inline_container, *inline_box;
		box = result.box;
		inline_container = box_create(0, 0, 0);
		inline_container->type = BOX_INLINE_CONTAINER;
		inline_box = box_create(style, 0, 0);
		inline_box->type = BOX_INLINE;
		inline_box->style_clone = 1;
		if (box->gadget->value)
			inline_box->text = tolat1(box->gadget->value);
		else if (box->gadget->type == GADGET_SUBMIT)
			inline_box->text = xstrdup(messages_get("Form_Submit"));
		else
			inline_box->text = xstrdup(messages_get("Form_Reset"));
		inline_box->length = strlen(inline_box->text);
		inline_box->font = font_open(status->content->data.html.fonts, style);
		box_add_child(inline_container, inline_box);
		box_add_child(box, inline_container);
	}
	else if (stricmp(type, "image") == 0)
	{
	        box = box_create(style, NULL, 0);
	        box->gadget = gadget = xcalloc(1, sizeof(struct gui_gadget));
	        gadget->type = GADGET_IMAGE;
	        if ((s = (char *) xmlGetProp(n, (const xmlChar*) "name"))) {
	                gadget->data.image.n = s;
	        }
	        if ((s = (char *) xmlGetProp(n, (const xmlChar*) "width"))) {
	                gadget->data.image.width = (atoi(s));
	        }
	        if ((s = (char *) xmlGetProp(n, (const xmlChar*) "height"))) {
                        gadget->data.image.height = (atoi(s));
	        }
	        if ((s = (char *) xmlGetProp(n, (const xmlChar*) "src"))) {
	                url = url_join(strdup(s), status->content->url);
	                html_fetch_object(status->content, url, box);
	        }
	        gadget->data.image.name =
	               xcalloc(strlen(gadget->data.image.n) + 5, sizeof(char));
	        gadget->data.image.value =
                       xcalloc(strlen(gadget->data.image.n) + 20, sizeof(char));
	}

	if (type != 0)
		xmlFree(type);

	if (gadget != 0) {
		gadget->form = status->current_form;
		gadget->name = (char *) xmlGetProp(n, (const xmlChar *) "name");
		add_gadget_element(status->elements, gadget);
	}

	return (struct result) {box, 0};
}

struct box *box_input_text(xmlNode *n, struct status *status,
		struct css_style *style, bool password)
{
	char *s;
	unsigned int i;
	struct box *box = box_create(style, 0, 0);
	struct box *inline_container, *inline_box;
	style->display = CSS_DISPLAY_INLINE_BLOCK;

	box->gadget = xcalloc(1, sizeof(struct gui_gadget));

	box->gadget->maxlength = 100;
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "maxlength"))) {
		box->gadget->maxlength = atoi(s);
		xmlFree(s);
	}

	s = (char *) xmlGetProp(n, (const xmlChar *) "value");
	box->gadget->value = s ? tolat1(s) : xstrdup("");
	box->gadget->initial_value = xstrdup(box->gadget->value);
	if (s)
		xmlFree(s);

	inline_container = box_create(0, 0, 0);
	inline_container->type = BOX_INLINE_CONTAINER;
	inline_box = box_create(style, 0, 0);
	inline_box->type = BOX_INLINE;
	inline_box->style_clone = 1;
	inline_box->length = strlen(box->gadget->value);
	if (password) {
		box->gadget->type = GADGET_PASSWORD;
		inline_box->text = xcalloc(inline_box->length + 1, 1);
		for (i = 0; i != inline_box->length; i++)
			inline_box->text[i] = '*';
	} else {
		box->gadget->type = GADGET_TEXTBOX;
		inline_box->text = xstrdup(box->gadget->value);
		/* replace spaces with hard spaces to prevent line wrapping */
		for (i = 0; i != inline_box->length; i++)
			if (inline_box->text[i] == ' ')
				inline_box->text[i] = 160;
	}
	inline_box->font = font_open(status->content->data.html.fonts, style);
	box_add_child(inline_container, inline_box);
	box_add_child(box, inline_container);

	return box;
}

struct result box_button(xmlNode *n, struct status *status,
		struct css_style *style)
{
	char *type = (char *) xmlGetProp(n, (const xmlChar *) "type");
	struct box *box = box_create(style, 0, 0);
	style->display = CSS_DISPLAY_INLINE_BLOCK;

	if (!type || strcasecmp(type, "submit") == 0) {
		box->gadget = xcalloc(1, sizeof(struct gui_gadget));
		box->gadget->type = GADGET_SUBMIT;
	} else if (strcasecmp(type, "reset") == 0) {
		box->gadget = xcalloc(1, sizeof(struct gui_gadget));
		box->gadget->type = GADGET_RESET;
	} else {
		/* type="button" or unknown: just render the contents */
		xmlFree(type);
		return (struct result) {box, 1};
	}

	if (type)
		xmlFree(type);

	box->gadget->form = status->current_form;
	box->gadget->name = (char *) xmlGetProp(n, (const xmlChar *) "name");
	box->gadget->value = (char *) xmlGetProp(n, (const xmlChar *) "value");

	return (struct result) {box, 1};
}


/**
 * print a box tree to standard output
 */

void box_dump(struct box * box, unsigned int depth)
{
	unsigned int i;
	struct box * c;

	for (i = 0; i < depth; i++)
		fprintf(stderr, "  ");

	fprintf(stderr, "x%li y%li w%li h%li ", box->x, box->y, box->width, box->height);
	if (box->max_width != UNKNOWN_MAX_WIDTH)
		fprintf(stderr, "min%lu max%lu ", box->min_width, box->max_width);

	switch (box->type) {
		case BOX_BLOCK:            fprintf(stderr, "BOX_BLOCK "); break;
		case BOX_INLINE_CONTAINER: fprintf(stderr, "BOX_INLINE_CONTAINER "); break;
		case BOX_INLINE:           fprintf(stderr, "BOX_INLINE "); break;
		case BOX_INLINE_BLOCK:     fprintf(stderr, "BOX_INLINE_BLOCK "); break;
		case BOX_TABLE:            fprintf(stderr, "BOX_TABLE "); break;
		case BOX_TABLE_ROW:        fprintf(stderr, "BOX_TABLE_ROW "); break;
		case BOX_TABLE_CELL:       fprintf(stderr, "BOX_TABLE_CELL [columns %i] ",
		                                   box->columns); break;
		case BOX_TABLE_ROW_GROUP:  fprintf(stderr, "BOX_TABLE_ROW_GROUP "); break;
		case BOX_FLOAT_LEFT:       fprintf(stderr, "BOX_FLOAT_LEFT "); break;
		case BOX_FLOAT_RIGHT:      fprintf(stderr, "BOX_FLOAT_RIGHT "); break;
		default:                   fprintf(stderr, "Unknown box type ");
	}
	if (box->text)
		fprintf(stderr, "'%.*s' ", (int) box->length, box->text);
	if (box->object)
		fprintf(stderr, "(object '%s') ", box->object->url);
	if (box->style)
		css_dump_style(box->style);
	if (box->href != 0)
		fprintf(stderr, " -> '%s' ", box->href);
	if (box->title != 0)
		fprintf(stderr, "[%s]", box->title);
	fprintf(stderr, "\n");

	for (c = box->children; c != 0; c = c->next)
		box_dump(c, depth + 1);
}


/**
 * ensure the box tree is correctly nested
 *
 * parent		permitted child nodes
 * BLOCK, INLINE_BLOCK	BLOCK, INLINE_CONTAINER, TABLE
 * INLINE_CONTAINER	INLINE, INLINE_BLOCK, FLOAT_LEFT, FLOAT_RIGHT
 * INLINE		none
 * TABLE		at least 1 TABLE_ROW_GROUP
 * TABLE_ROW_GROUP	at least 1 TABLE_ROW
 * TABLE_ROW		at least 1 TABLE_CELL
 * TABLE_CELL		BLOCK, INLINE_CONTAINER, TABLE (same as BLOCK)
 * FLOAT_(LEFT|RIGHT)	exactly 1 BLOCK or TABLE
 */

void box_normalise_block(struct box *block)
{
	struct box *child;
	struct box *next_child;
	struct box *table;
	struct css_style *style;

	assert(block != 0);
	assert(block->type == BOX_BLOCK || block->type == BOX_INLINE_BLOCK ||
			block->type == BOX_TABLE_CELL);
	LOG(("block %p, block->type %u", block, block->type));
	gui_multitask();

	for (child = block->children; child != 0; child = next_child) {
		LOG(("child %p, child->type = %d", child, child->type));
		next_child = child->next;	/* child may be destroyed */
		switch (child->type) {
			case BOX_BLOCK:
				/* ok */
				box_normalise_block(child);
				break;
			case BOX_INLINE_CONTAINER:
				box_normalise_inline_container(child);
				break;
			case BOX_TABLE:
				box_normalise_table(child);
				break;
			case BOX_INLINE:
			case BOX_INLINE_BLOCK:
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				/* should have been wrapped in inline
				   container by convert_xml_to_box() */
				assert(0);
				break;
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_ROW:
			case BOX_TABLE_CELL:
				/* insert implied table */
				style = xcalloc(1, sizeof(struct css_style));
				memcpy(style, block->style, sizeof(struct css_style));
				css_cascade(style, &css_blank_style);
				table = box_create(style, block->href, 0);
				table->type = BOX_TABLE;
				if (child->prev == 0)
					block->children = table;
				else
					child->prev->next = table;
				table->prev = child->prev;
				while (child != 0 && (
						child->type == BOX_TABLE_ROW_GROUP ||
						child->type == BOX_TABLE_ROW ||
						child->type == BOX_TABLE_CELL)) {
					box_add_child(table, child);
					child = child->next;
				}
				table->last->next = 0;
				table->next = next_child = child;
				table->parent = block;
				box_normalise_table(table);
				break;
			default:
				assert(0);
		}
	}
	LOG(("block %p done", block));
}


void box_normalise_table(struct box *table)
{
	struct box *child;
	struct box *next_child;
	struct box *row_group;
	struct css_style *style;
	unsigned int *row_span = xcalloc(2, sizeof(row_span[0]));
	unsigned int table_columns = 1;

	assert(table != 0);
	assert(table->type == BOX_TABLE);
	LOG(("table %p", table));
	row_span[0] = row_span[1] = 0;

	for (child = table->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
			case BOX_TABLE_ROW_GROUP:
				/* ok */
				box_normalise_table_row_group(child, &row_span,
						&table_columns);
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW:
			case BOX_TABLE_CELL:
				/* insert implied table row group */
				style = xcalloc(1, sizeof(struct css_style));
				memcpy(style, table->style, sizeof(struct css_style));
				css_cascade(style, &css_blank_style);
				row_group = box_create(style, table->href, 0);
				row_group->type = BOX_TABLE_ROW_GROUP;
				if (child->prev == 0)
					table->children = row_group;
				else
					child->prev->next = row_group;
				row_group->prev = child->prev;
				while (child != 0 && (
						child->type == BOX_BLOCK ||
						child->type == BOX_INLINE_CONTAINER ||
						child->type == BOX_TABLE ||
						child->type == BOX_TABLE_ROW ||
						child->type == BOX_TABLE_CELL)) {
					box_add_child(row_group, child);
					child = child->next;
				}
				row_group->last->next = 0;
				row_group->next = next_child = child;
				row_group->parent = table;
				box_normalise_table_row_group(row_group, &row_span,
						&table_columns);
				break;
			case BOX_INLINE:
			case BOX_INLINE_BLOCK:
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				/* should have been wrapped in inline
				   container by convert_xml_to_box() */
				assert(0);
				break;
			default:
				fprintf(stderr, "%i\n", child->type);
				assert(0);
		}
	}

	table->columns = table_columns;
	xfree(row_span);

	if (table->children == 0) {
		LOG(("table->children == 0, removing"));
		if (table->prev == 0)
			table->parent->children = table->next;
		else
			table->prev->next = table->next;
		if (table->next != 0)
			table->next->prev = table->prev;
		box_free(table);
	}

	LOG(("table %p done", table));
}


void box_normalise_table_row_group(struct box *row_group,
		unsigned int **row_span, unsigned int *table_columns)
{
	struct box *child;
	struct box *next_child;
	struct box *row;
	struct css_style *style;

	assert(row_group != 0);
	assert(row_group->type == BOX_TABLE_ROW_GROUP);
	LOG(("row_group %p", row_group));

	for (child = row_group->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
			case BOX_TABLE_ROW:
				/* ok */
				box_normalise_table_row(child, row_span, table_columns);
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_CELL:
				/* insert implied table row */
				style = xcalloc(1, sizeof(struct css_style));
				memcpy(style, row_group->style, sizeof(struct css_style));
				css_cascade(style, &css_blank_style);
				row = box_create(style, row_group->href, 0);
				row->type = BOX_TABLE_ROW;
				if (child->prev == 0)
					row_group->children = row;
				else
					child->prev->next = row;
				row->prev = child->prev;
				while (child != 0 && (
						child->type == BOX_BLOCK ||
						child->type == BOX_INLINE_CONTAINER ||
						child->type == BOX_TABLE ||
						child->type == BOX_TABLE_ROW_GROUP ||
						child->type == BOX_TABLE_CELL)) {
					box_add_child(row, child);
					child = child->next;
				}
				row->last->next = 0;
				row->next = next_child = child;
				row->parent = row_group;
				box_normalise_table_row(row, row_span, table_columns);
				break;
			case BOX_INLINE:
			case BOX_INLINE_BLOCK:
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				/* should have been wrapped in inline
				   container by convert_xml_to_box() */
				assert(0);
				break;
			default:
				assert(0);
		}
	}

	if (row_group->children == 0) {
		LOG(("row_group->children == 0, removing"));
		if (row_group->prev == 0)
			row_group->parent->children = row_group->next;
		else
			row_group->prev->next = row_group->next;
		if (row_group->next != 0)
			row_group->next->prev = row_group->prev;
		box_free(row_group);
	}

	LOG(("row_group %p done", row_group));
}


void box_normalise_table_row(struct box *row,
		unsigned int **row_span, unsigned int *table_columns)
{
	struct box *child;
	struct box *next_child;
	struct box *cell;
	struct css_style *style;
	unsigned int columns = 0, i, min;

	assert(row != 0);
	assert(row->type == BOX_TABLE_ROW);
	LOG(("row %p", row));

	for (child = row->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
			case BOX_TABLE_CELL:
				/* ok */
				box_normalise_block(child);
				cell = child;
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_ROW:
				/* insert implied table cell */
				style = xcalloc(1, sizeof(struct css_style));
				memcpy(style, row->style, sizeof(struct css_style));
				css_cascade(style, &css_blank_style);
				cell = box_create(style, row->href, 0);
				cell->type = BOX_TABLE_CELL;
				if (child->prev == 0)
					row->children = cell;
				else
					child->prev->next = cell;
				cell->prev = child->prev;
				while (child != 0 && (
						child->type == BOX_BLOCK ||
						child->type == BOX_INLINE_CONTAINER ||
						child->type == BOX_TABLE ||
						child->type == BOX_TABLE_ROW_GROUP ||
						child->type == BOX_TABLE_ROW)) {
					box_add_child(cell, child);
					child = child->next;
				}
				cell->last->next = 0;
				cell->next = next_child = child;
				cell->parent = row;
				box_normalise_block(cell);
				break;
			case BOX_INLINE:
			case BOX_INLINE_BLOCK:
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				/* should have been wrapped in inline
				   container by convert_xml_to_box() */
				assert(0);
				break;
			default:
				assert(0);
		}

		/* skip columns with cells spanning from above */
		while ((*row_span)[columns] != 0)
			columns++;
		cell->start_column = columns;
		if (*table_columns < columns + cell->columns) {
			*table_columns = columns + cell->columns;
			*row_span = xrealloc(*row_span,
					sizeof((*row_span)[0]) *
					(*table_columns + 1));
			(*row_span)[*table_columns] = 0;  /* sentinel */
		}
		for (i = 0; i != cell->columns; i++)
			(*row_span)[columns + i] = cell->rows;
		columns += cell->columns;
	}

	for (i = 0; i != *table_columns; i++)
		if ((*row_span)[i] != 0)
			(*row_span)[i]--;

	if (row->children == 0) {
		LOG(("row->children == 0, removing"));
		if (row->prev == 0)
			row->parent->children = row->next;
		else
			row->prev->next = row->next;
		if (row->next != 0)
			row->next->prev = row->prev;
		box_free(row);
	}

	LOG(("row %p done", row));
}


void box_normalise_inline_container(struct box *cont)
{
	struct box *child;
	struct box *next_child;

	assert(cont != 0);
	assert(cont->type == BOX_INLINE_CONTAINER);
	LOG(("cont %p", cont));

	for (child = cont->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
			case BOX_INLINE:
				/* ok */
				break;
			case BOX_INLINE_BLOCK:
				/* ok */
				box_normalise_block(child);
				break;
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				/* ok */
				assert(child->children != 0);
				switch (child->children->type) {
					case BOX_BLOCK:
						box_normalise_block(child->children);
						break;
					case BOX_TABLE:
						box_normalise_table(child->children);
						break;
					default:
						assert(0);
				}
				if (child->children == 0) {
					/* the child has destroyed itself: remove float */
					if (child->prev == 0)
						child->parent->children = child->next;
					else
						child->prev->next = child->next;
					if (child->next != 0)
						child->next->prev = child->prev;
					box_free(child);
				}
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_ROW:
			case BOX_TABLE_CELL:
			default:
				assert(0);
		}
	}
	LOG(("cont %p done", cont));
}


void gadget_free(struct gui_gadget* g)
{
	struct formoption *o, *o1;

	if (g->name != 0)
		xmlFree(g->name);
	free(g->value);
	free(g->initial_value);

	switch (g->type)
	{
		case GADGET_HIDDEN:
			if (g->data.hidden.value != 0)
				xmlFree(g->data.hidden.value);
			break;
		case GADGET_RADIO:
			if (g->data.checkbox.value != 0)
				xmlFree(g->data.radio.value);
			break;
		case GADGET_CHECKBOX:
			if (g->data.checkbox.value != 0)
				xmlFree(g->data.checkbox.value);
			break;
		case GADGET_IMAGE:
		        if (g->data.image.n != 0)
		                xmlFree(g->data.image.n);
		        if (g->data.image.name != 0)
                                xfree(g->data.image.name);
                        if (g->data.image.value != 0)
                                xfree(g->data.image.value);
		        break;
		case GADGET_SELECT:
			o = g->data.select.items;
			while (o != NULL)
			{
				if (o->text != 0)
					xmlFree(o->text);
				if (o->value != 0)
					xmlFree(o->value);
				o1 = o->next;
				xfree(o);
				o = o1;
			}
			break;
	}
}

/**
 * free a box tree recursively
 */

void box_free(struct box *box)
{
	struct box *child, *next;

	/* free children first */
	for (child = box->children; child; child = next) {
		next = child->next;
		box_free(child);
	}

	/* last this box */
	box_free_box(box);
}

void box_free_box(struct box *box)
{
	if (!box->clone) {
		if (box->gadget) {
			gadget_free(box->gadget);
			free(box->gadget);
		}
		free(box->href);
		free(box->title);
		free(box->col);
		if (!box->style_clone)
			free(box->style);
	}

	free(box->text);
	/* TODO: free object_params */
}


/**
 * form helper functions
 */

struct form* create_form(xmlNode* n)
{
	struct form* form;
	char* s;

	form = xcalloc(1, sizeof(*form));

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "action"))) {
		form->action = s;
	}

	form->method = method_GET;
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "method"))) {
		if (stricmp(s, "post") == 0)
			form->method = method_POST;
		xmlFree(s);
	}

	return form;
}

void add_form_element(struct page_elements* pe, struct form* f)
{
	pe->forms = xrealloc(pe->forms, (pe->numForms + 1) * sizeof(struct form*));
	pe->forms[pe->numForms] = f;
	pe->numForms++;
}

void add_gadget_element(struct page_elements* pe, struct gui_gadget* g)
{
	pe->gadgets = xrealloc(pe->gadgets, (pe->numGadgets + 1) * sizeof(struct gui_gadget*));
	pe->gadgets[pe->numGadgets] = g;
	pe->numGadgets++;
}


/**
 * add an object to the box tree
 */
struct result box_object(xmlNode *n, struct status *status,
		struct css_style *style)
{
	struct box *box;
	struct object_params *po;
	struct plugin_params* pp;
	char *s, *url;
	xmlNode *c;

	box = box_create(style, status->href, 0);

        po = xcalloc(1, sizeof(*po));

        /* initialise po struct */
        po->data = 0;
        po->type = 0;
        po->codetype = 0;
        po->codebase = 0;
        po->classid = 0;
        po->params = 0;

        /* object data */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "data"))) {

                po->data = strdup(s);
	        url = url_join(strdup(s), status->content->url);
	        LOG(("object '%s'", po->data));
	        xmlFree(s);
	}

        /* object type */
        if ((s = (char *) xmlGetProp(n, (const xmlChar *) "type"))) {

                po->type = strdup(s);
                LOG(("type: %s", s));
                xmlFree(s);
        }

        /* object codetype */
        if ((s = (char *) xmlGetProp(n, (const xmlChar *) "codetype"))) {

                po->codetype = strdup(s);
                LOG(("codetype: %s", s));
                xmlFree(s);
        }

        /* object codebase */
        if ((s = (char *) xmlGetProp(n, (const xmlChar *) "codebase"))) {

                po->codebase = strdup(s);
                LOG(("codebase: %s", s));
                xmlFree(s);
        }

        /* object classid */
        if ((s = (char *) xmlGetProp(n, (const xmlChar *) "classid"))) {

                po->classid = strdup(s);
                LOG(("classid: %s", s));
                xmlFree(s);
        }

        /* parameters
         * parameter data is stored in a singly linked list.
         * po->params points to the head of the list.
         * new parameters are added to the head of the list.
         */
        for (c = n->children; c != 0; c = c->next) {
	    if (strcmp((const char *) c->name, "param") == 0) {

               pp = xcalloc(1, sizeof(*pp));

               /* initialise pp struct */
               pp->name = 0;
               pp->value = 0;
               pp->valuetype = 0;
               pp->type = 0;
               pp->next = 0;

               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "name"))) {
                   pp->name = strdup(s);
                   xmlFree(s);
               }
               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "value"))) {
                   pp->value = strdup(s);
                   xmlFree(s);
               }
               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "type"))) {
                   pp->type = strdup(s);
                   xmlFree(s);
               }
               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "valuetype"))) {
                   pp->valuetype = strdup(s);
                   xmlFree(s);
               }
               else {

                   pp->valuetype = strdup("data");
               }

               pp->next = po->params;
               po->params = pp;
	    }
	    else {
	            /* The first non-param child is the start of the
	             * alt html. Therefore, we should break out of this loop.
	             */
	             continue;
	    }
	}

	box->object_params = po;

	/* start fetch */
	if (plugin_decode(status->content, url, box, po))
		return (struct result) {box, 0};

	return (struct result) {box, 1};
}

/**
 * add an embed to the box tree
 */

struct result box_embed(xmlNode *n, struct status *status,
		struct css_style *style)
{
	struct box *box;
	struct object_params *po;
	struct plugin_params *pp;
	char *s, *url;
	xmlAttr *a;

	box = box_create(style, status->href, 0);

	po = xcalloc(1, sizeof(*po));

	/* initialise po struct */
        po->data = 0;
        po->type = 0;
        po->codetype = 0;
        po->codebase = 0;
        po->classid = 0;
        po->params = 0;

	/* embed src */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "src"))) {

                po->data = strdup(s);
	        url = url_join(strdup(s), status->content->url);
	        LOG(("embed '%s'", url));
	        xmlFree(s);
        }

        /**
         * we munge all other attributes into a plugin_parameter structure
         */
         for(a=n->properties; a!=0; a=a->next) {

                pp = xcalloc(1, sizeof(*pp));

                /* initialise pp struct */
                pp->name = 0;
                pp->value = 0;
                pp->valuetype = 0;
                pp->type = 0;
                pp->next = 0;

                if(strcasecmp((char*)a->name, "src") != 0) {
                        pp->name = strdup((char*)a->name);
                        pp->value = strdup((char*)a->children->content);
                        pp->valuetype = strdup("data");

                        pp->next = po->params;
                        po->params = pp;
                }
         }

	box->object_params = po;

        /* start fetch */
	plugin_decode(status->content, url, box, po);

	return (struct result) {box,0};
}

/**
 * add an applet to the box tree
 */

struct result box_applet(xmlNode *n, struct status *status,
		struct css_style *style)
{
	struct box *box;
	struct object_params *po;
	struct plugin_params *pp;
	char *s, *url;
	xmlNode *c;

	box = box_create(style, status->href, 0);

	po = xcalloc(1, sizeof(*po));

	/* initialise po struct */
        po->data = 0;
        po->type = 0;
        po->codetype = 0;
        po->codebase = 0;
        po->classid = 0;
        po->params = 0;

        /* code */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "code"))) {

                po->classid = strdup(s);
                url = url_join(strdup(s), status->content->url);
                LOG(("applet '%s'", url));
                xmlFree(s);
        }

        /* object codebase */
        if ((s = (char *) xmlGetProp(n, (const xmlChar *) "codebase"))) {

                po->codebase = strdup(s);
                LOG(("codebase: %s", s));
                xmlFree(s);
        }

        /* parameters
         * parameter data is stored in a singly linked list.
         * po->params points to the head of the list.
         * new parameters are added to the head of the list.
         */
        for (c = n->children; c != 0; c = c->next) {
	    if (strcmp((const char *) c->name, "param") == 0) {

               pp = xcalloc(1, sizeof(*pp));

               /* initialise pp struct */
               pp->name = 0;
               pp->value = 0;
               pp->valuetype = 0;
               pp->type = 0;
               pp->next = 0;

               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "name"))) {
                   pp->name = strdup(s);
                   xmlFree(s);
               }
               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "value"))) {
                   pp->value = strdup(s);
                   xmlFree(s);
               }
               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "type"))) {
                   pp->type = strdup(s);
                   xmlFree(s);
               }
               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "valuetype"))) {
                   pp->valuetype = strdup(s);
                   xmlFree(s);
               }
               else {

                   pp->valuetype = strdup("data");
               }

               pp->next = po->params;
               po->params = pp;
	    }
	    else {
	            /* The first non-param child is the start of the
	             * alt html. Therefore, we should break out of this loop.
	             */
	             continue;
	    }
	}

        box->object_params = po;

	/* start fetch */
	if(plugin_decode(status->content, url, box, po))
        	return (struct result) {box,0};

        return (struct result) {box,1};
}

/**
 * box_iframe
 * add an iframe to the box tree
 * TODO - implement GUI nested wimp stuff 'cos this looks naff atm. (16_5)
 */
struct result box_iframe(xmlNode *n, struct status *status,
		struct css_style *style)
{
        struct box *box;
	struct object_params *po;
	char *s, *url;

	box = box_create(style, status->href, 0);

	po = xcalloc(1, sizeof(*po));

	/* initialise po struct */
        po->data = 0;
        po->type = 0;
        po->codetype = 0;
        po->codebase = 0;
        po->classid = 0;
        po->params = 0;

	/* iframe src */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "src"))) {

                po->data = strdup(s);
	        url = url_join(strdup(s), status->content->url);
	        LOG(("embed '%s'", url));
	        xmlFree(s);
        }

	box->object_params = po;

        /* start fetch */
	plugin_decode(status->content, url, box, po);

	return (struct result) {box,0};
}

/**
 * plugin_decode
 * This function checks that the contents of the plugin_object struct
 * are valid. If they are, it initiates the fetch process. If they are
 * not, it exits, leaving the box structure as it was on entry. This is
 * necessary as there are multiple ways of declaring an object's attributes.
 *
 * Returns false if the object could not be handled.
 */
bool plugin_decode(struct content* content, char* url, struct box* box,
                  struct object_params* po)
{
  struct plugin_params * pp;

  /* Set basehref */
  po->basehref = strdup(content->url);

  /* Check if the codebase attribute is defined.
   * If it is not, set it to the codebase of the current document.
   */
   if(po->codebase == 0)
           po->codebase = url_join("./", content->url);
   else
           po->codebase = url_join(po->codebase, content->url);

  /* Check that we have some data specified.
   * First, check the data attribute.
   * Second, check the classid attribute.
   * The data attribute takes precedence.
   * If neither are specified or if classid begins "clsid:",
   * we can't handle this object.
   */
   if(po->data == 0 && po->classid == 0) {
           return false;
   }
   if(po->data == 0 && po->classid != 0) {
           if(strncasecmp(po->classid, "clsid:", 6) == 0) {
                   /* Flash */
                   if(strcasecmp(po->classid, "clsid:D27CDB6E-AE6D-11cf-96B8-444553540000") == 0) {
                           for(pp = po->params; pp != 0 &&
                               (strcasecmp(pp->name, "movie") != 0);
                               pp = pp->next);
                           if(pp != 0)
                                   url = url_join(pp->value, po->basehref);
                           else return false;
                           /* munge the codebase */
                           po->codebase = url_join("./", content->url);
                   }
                   else {
                           LOG(("ActiveX object - n0"));
                           return false;
                   }
           }
           else {
                   url = url_join(po->classid, po->codebase);

                   /* The java plugin doesn't need the .class extension
                    * so we strip it.
                    */
                   if(stricmp((&po->classid[strlen(po->classid)-6]),
                                                            ".class") == 0)
                           po->classid[strlen(po->classid)-6] = 0;
           }
   }
   else {
           url = url_join(po->data, po->codebase);
   }

   /* Check if the declared mime type is understandable.
    * Checks type and codetype attributes.
    */
    if(po->type != 0) {
           if (content_lookup(po->type) == CONTENT_OTHER)
                  return false;
    }
    if(po->codetype != 0) {
           if (content_lookup(po->codetype) == CONTENT_OTHER)
                  return false;
    }

  /* If we've got to here, the object declaration has provided us with
   * enough data to enable us to have a go at downloading and displaying it.
   *
   * We may still find that the object has a MIME type that we can't handle
   * when we fetch it (if the type was not specified or is different to that
   * given in the attributes).
   */
   html_fetch_object(content, url, box);

   return true;
}


/**
 * Find the absolute coordinates of a box.
 */

void box_coords(struct box *box, unsigned long *x, unsigned long *y)
{
	*x = box->x;
	*y = box->y;
	while (box->parent != 0) {
		box = box->parent;
		*x += box->x;
		*y += box->y;
	}
}

