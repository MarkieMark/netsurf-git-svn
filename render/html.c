/**
 * $Id: html.c,v 1.16 2003/04/25 08:03:15 bursa Exp $
 */

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/html.h"
#include "netsurf/render/layout.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"


struct fetch_data {
	struct content *c;
	unsigned int i;
};


static void html_convert_css_callback(fetchcache_msg msg, struct content *css,
		void *p, const char *error);
static void html_title(struct content *c, xmlNode *head);
static void html_find_stylesheets(struct content *c, xmlNode *head);
static void html_image_callback(fetchcache_msg msg, struct content *image,
		void *p, const char *error);


void html_create(struct content *c)
{
	c->data.html.parser = htmlCreatePushParserCtxt(0, 0, "", 0, 0, XML_CHAR_ENCODING_8859_1);
	c->data.html.layout = NULL;
	c->data.html.style = NULL;
	c->data.html.fonts = NULL;
}


#define CHUNK 4096

void html_process_data(struct content *c, char *data, unsigned long size)
{
	unsigned long x;
	for (x = 0; x + CHUNK <= size; x += CHUNK) {
		htmlParseChunk(c->data.html.parser, data + x, CHUNK, 0);
		gui_multitask();
	}
	htmlParseChunk(c->data.html.parser, data + x, (int) (size - x), 0);
}


int html_convert(struct content *c, unsigned int width, unsigned int height)
{
	unsigned int i;
	xmlDoc *document;
	xmlNode *html, *head;

	/* finish parsing */
	htmlParseChunk(c->data.html.parser, "", 0, 1);
	document = c->data.html.parser->myDoc;
	/*xmlDebugDumpDocument(stderr, c->data.html.parser->myDoc);*/
	htmlFreeParserCtxt(c->data.html.parser);
	if (document == NULL) {
		LOG(("Parsing failed"));
		return 1;
	}

	/* locate html and head elements */
	for (html = document->children;
			html != 0 && html->type != XML_ELEMENT_NODE;
			html = html->next)
		;
	if (html == 0 || strcmp((const char *) html->name, "html") != 0) {
		LOG(("html element not found"));
		xmlFreeDoc(document);
		return 1;
	}
	for (head = html->children;
			head != 0 && head->type != XML_ELEMENT_NODE;
			head = head->next)
		;
	if (strcmp((const char *) head->name, "head") != 0) {
		head = 0;
		LOG(("head element not found"));
	}

	if (head != 0)
		html_title(c, head);

	/* get stylesheets */
	html_find_stylesheets(c, head);

	/* convert xml tree to box tree */
	LOG(("XML to box"));
	xml_to_box(html, c);
	/*box_dump(c->data.html.layout->children, 0);*/

	/* XML tree and stylesheets not required past this point */
	xmlFreeDoc(document);

	cache_free(c->data.html.stylesheet_content[0]);
	if (c->data.html.stylesheet_content[1] != 0)
		content_destroy(c->data.html.stylesheet_content[1]);
	for (i = 2; i != c->data.html.stylesheet_count; i++)
		if (c->data.html.stylesheet_content[i] != 0)
			cache_free(c->data.html.stylesheet_content[i]);
	xfree(c->data.html.stylesheet_content);

	/* layout the box tree */
	c->status_callback(c->status_p, "Formatting document");
	LOG(("Layout document"));
	layout_document(c->data.html.layout->children, width);
	/*box_dump(c->data.html.layout->children, 0);*/

	c->width = c->data.html.layout->children->width;
	c->height = c->data.html.layout->children->height;

	if (c->active != 0)
		c->status = CONTENT_PENDING;
	
	return 0;
}


void html_convert_css_callback(fetchcache_msg msg, struct content *css,
		void *p, const char *error)
{
	struct fetch_data *data = p;
	struct content *c = data->c;
	unsigned int i = data->i;
	switch (msg) {
		case FETCHCACHE_OK:
			free(data);
			LOG(("got stylesheet '%s'", css->url));
			c->data.html.stylesheet_content[i] = css;
			/*css_dump_stylesheet(css->data.css);*/
			c->active--;
			break;
		case FETCHCACHE_BADTYPE:
		case FETCHCACHE_ERROR:
			free(data);
			c->data.html.stylesheet_content[i] = 0;
			c->active--;
			c->error = 1;
			break;
		case FETCHCACHE_STATUS:
			/* TODO: need to add a way of sending status to the
			 * owning window */
			break;
		default:
			assert(0);
	}
}


void html_title(struct content *c, xmlNode *head)
{
	xmlNode *node;
	xmlChar *title;

	c->title = 0;

	for (node = head->children; node != 0; node = node->next) {
		if (strcmp(node->name, "title") == 0) {
			title = xmlNodeGetContent(node);
			c->title = squash_tolat1(title);
			xmlFree(title);
			return;
		}
	}
}


void html_find_stylesheets(struct content *c, xmlNode *head)
{
	xmlNode *node, *node2;
	char *rel, *type, *media, *href, *data, *url;
	char status[80];
	unsigned int i = 2;
	struct fetch_data *fetch_data;

	/* stylesheet 0 is the base style sheet, stylesheet 1 is any <style> elements */
	c->data.html.stylesheet_content = xcalloc(2, sizeof(*c->data.html.stylesheet_content));
	c->data.html.stylesheet_content[1] = 0;
	c->data.html.stylesheet_count = 2;

	c->error = 0;
	c->active = 0;

	fetch_data = xcalloc(1, sizeof(*fetch_data));
	fetch_data->c = c;
	fetch_data->i = 0;
	c->active++;
	fetchcache("file:///%3CNetSurf$Dir%3E/Resources/CSS", c->url,
			html_convert_css_callback,
			fetch_data, c->width, c->height, 1 << CONTENT_CSS);

	for (node = head == 0 ? 0 : head->children; node != 0; node = node->next) {
		if (strcmp(node->name, "link") == 0) {
			/* rel='stylesheet' */
			if (!(rel = (char *) xmlGetProp(node, (const xmlChar *) "rel")))
				continue;
			if (strcasecmp(rel, "stylesheet") != 0) {
				xmlFree(rel);
				continue;
			}
			xmlFree(rel);

			/* type='text/css' or not present */
			if ((type = (char *) xmlGetProp(node, (const xmlChar *) "type"))) {
				if (strcmp(type, "text/css") != 0) {
					xmlFree(type);
					continue;
				}
				xmlFree(type);
			}

			/* media contains 'screen' or 'all' or not present */
			if ((media = (char *) xmlGetProp(node, (const xmlChar *) "media"))) {
				if (strstr(media, "screen") == 0 &&
						strstr(media, "all") == 0) {
					xmlFree(media);
					continue;
				}
				xmlFree(media);
			}
			
			/* href='...' */
			if (!(href = (char *) xmlGetProp(node, (const xmlChar *) "href")))
				continue;

			url = url_join(href, c->url);
			LOG(("linked stylesheet %i '%s'", i, url));
			xmlFree(href);

			/* start fetch */
			c->data.html.stylesheet_content = xrealloc(c->data.html.stylesheet_content,
					(i + 1) * sizeof(*c->data.html.stylesheet_content));
			fetch_data = xcalloc(1, sizeof(*fetch_data));
			fetch_data->c = c;
			fetch_data->i = i;
			c->active++;
			fetchcache(url, c->url, html_convert_css_callback,
					fetch_data, c->width, c->height, 1 << CONTENT_CSS);
			free(url);
			i++;

		} else if (strcmp(node->name, "style") == 0) {
			/* type='text/css' */
			if (!(type = (char *) xmlGetProp(node, (const xmlChar *) "type")))
				continue;
			if (strcmp(type, "text/css") != 0) {
				xmlFree(type);
				continue;
			}
			xmlFree(type);

			/* media contains 'screen' or 'all' or not present */
			if ((media = (char *) xmlGetProp(node, (const xmlChar *) "media"))) {
				if (strstr(media, "screen") == 0 &&
						strstr(media, "all") == 0) {
					xmlFree(media);
					continue;
				}
				xmlFree(media);
			}

			/* create stylesheet */
			LOG(("style element"));
			if (c->data.html.stylesheet_content[1] == 0)
				c->data.html.stylesheet_content[1] = content_create(CONTENT_CSS, c->url);

			/* can't just use xmlNodeGetContent(node), because that won't give
			 * the content of comments which may be used to 'hide' the content */
			for (node2 = node->children; node2 != 0; node2 = node2->next) {
				data = xmlNodeGetContent(node2);
				content_process_data(c->data.html.stylesheet_content[1],
						data, strlen(data));
				xmlFree(data);
			}
		}
	}

	c->data.html.stylesheet_count = i;

	if (c->data.html.stylesheet_content[1] != 0)
		content_convert(c->data.html.stylesheet_content[1], c->width, c->height);

	/* complete the fetches */
	while (c->active != 0) {
		if (c->status_callback != 0) {
			sprintf(status, "Loading %u stylesheets", c->active);
			c->status_callback(c->status_p, status);
		}
		fetch_poll();
		gui_multitask();
	}

	if (c->error) {
		c->status_callback(c->status_p, "Warning: some stylesheets failed to load");
	}
}


void html_fetch_image(struct content *c, char *url, struct box *box)
{
	struct fetch_data *fetch_data;
	
	/* add to object list */
	c->data.html.object = xrealloc(c->data.html.object,
			(c->data.html.object_count + 1) *
			sizeof(*c->data.html.object));
	c->data.html.object[c->data.html.object_count].url = url;
	c->data.html.object[c->data.html.object_count].content = 0;
	c->data.html.object[c->data.html.object_count].box = box;

	/* start fetch */
	fetch_data = xcalloc(1, sizeof(*fetch_data));
	fetch_data->c = c;
	fetch_data->i = c->data.html.object_count;
	c->data.html.object_count++;
	c->active++;
	fetchcache(url, c->url,
			html_image_callback,
			fetch_data, 0, 0, 1 << CONTENT_JPEG);
}


void html_image_callback(fetchcache_msg msg, struct content *image,
		void *p, const char *error)
{
	struct fetch_data *data = p;
	struct content *c = data->c;
	unsigned int i = data->i;
	struct box *box = c->data.html.object[i].box;
	switch (msg) {
		case FETCHCACHE_OK:
			LOG(("got image '%s'", image->url));
			box->object = image;
			c->data.html.object[i].content = image;
			/* set dimensions to object dimensions if auto */
			if (box->style->width.width == CSS_WIDTH_AUTO) {
				box->style->width.width = CSS_WIDTH_LENGTH;
				box->style->width.value.length.unit = CSS_UNIT_PX;
				box->style->width.value.length.value = image->width;
				box->min_width = box->max_width = image->width;
				/* invalidate parent min, max widths */
				if (box->parent->max_width != UNKNOWN_MAX_WIDTH) {
					struct box *b = box->parent;
					if (b->min_width < image->width)
						b->min_width = image->width;
					if (b->max_width < image->width)
						b->max_width = image->width;
					for (b = b->parent;
							b != 0 && b->max_width != UNKNOWN_MAX_WIDTH;
							b = b->parent)
						b->max_width = UNKNOWN_MAX_WIDTH;
				}
			}
			if (box->style->height.height == CSS_HEIGHT_AUTO) {
				box->style->height.height = CSS_HEIGHT_LENGTH;
				box->style->height.length.unit = CSS_UNIT_PX;
				box->style->height.length.value = image->height;
			}
			/* remove alt text */
			if (box->text != 0) {
				free(box->text);
				box->text = 0;
				box->length = 0;
			}
			/*if (box->children != 0) {
				box_free(box->children);
				box->children = 0;
			}*/
			/* TODO: recalculate min, max width */
			/* drop through */
		case FETCHCACHE_BADTYPE:
		case FETCHCACHE_ERROR:
			if (c->active == 1 && c->status == CONTENT_PENDING) {
				/* all images have arrived */
				content_reformat(c, c->available_width, 0);
			}
			c->active--;
			if (c->active == 0)
				sprintf(c->status_message, "Document done");
			else
				sprintf(c->status_message, "Loading %i images", c->active);
			free(data);
			break;
		case FETCHCACHE_STATUS:
			/* TODO: need to add a way of sending status to the
			 * owning window */
			break;
		default:
			assert(0);
	}
}


void html_revive(struct content *c, unsigned int width, unsigned int height)
{
	unsigned int i;
	struct fetch_data *fetch_data;

	/* reload images and fix pointers */
	for (i = 0; i != c->data.html.object_count; i++) {
		if (c->data.html.object[i].content != 0) {
			fetch_data = xcalloc(1, sizeof(*fetch_data));
			fetch_data->c = c;
			fetch_data->i = i;
			c->active++;
			fetchcache(c->data.html.object[i].url, c->url,
					html_image_callback,
					fetch_data, 0, 0, 1 << CONTENT_JPEG);
		}
	}

	layout_document(c->data.html.layout->children, width);
	c->width = c->data.html.layout->children->width;
	c->height = c->data.html.layout->children->height;

	if (c->active != 0)
		c->status = CONTENT_PENDING;
}


void html_reformat(struct content *c, unsigned int width, unsigned int height)
{
	layout_document(c->data.html.layout->children, width);
	c->width = c->data.html.layout->children->width;
	c->height = c->data.html.layout->children->height;
}


void html_destroy(struct content *c)
{
	unsigned int i;
	LOG(("content %p", c));

	LOG(("layout %p", c->data.html.layout));
	if (c->data.html.layout != 0)
		box_free(c->data.html.layout);
	LOG(("fonts %p", c->data.html.fonts));
	if (c->data.html.fonts != 0)
		font_free_set(c->data.html.fonts);
	LOG(("title %p", c->title));
	if (c->title != 0)
		xfree(c->title);

	for (i = 0; i != c->data.html.object_count; i++) {
		LOG(("object %i %p", i, c->data.html.object[i].content));
		if (c->data.html.object[i].content != 0)
			cache_free(c->data.html.object[i].content);
		free(c->data.html.object[i].url);
	}
	free(c->data.html.object);
}

