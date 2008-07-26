/*
 * Copyright 2008 Adam Blokus <adamblokus@gmail.com>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Output-in-pages implementation
*/

#ifdef WITH_PDF_EXPORT
#include "desktop/print.h"
#include "desktop/printer.h"

#include "content/content.h"

#include "utils/log.h"
#include "utils/talloc.h"

#include "render/loosen.h"
#include "render/box.h"

#include "pdf/font_haru.h"

static struct content *print_init(struct content *, struct print_settings *);
static bool print_apply_settings(struct content *, struct print_settings *);


/*TODO:	should these be passed as parameters in order to allow simultaneous
	printings?
*/
static float page_content_width, page_content_height;
static float text_margin_height;
static struct content *printed_content;
static float done_height;

/**
 * This function calls print setup, prints page after page until the whole
 * content is printed calls cleaning up afterwise.
 * \param content The content to be printed
 * \param printer The printer interface for the printer to be used
 * \param settings The settings for printing to use or NULL for DEFAULT
 * \return true if successful, false otherwise
*/
bool print_basic_run(struct content *content,
		const struct printer *printer,
		struct print_settings *settings)
{
	bool ret = true;

	if (settings == NULL)
		settings = print_make_settings(DEFAULT);

	if (!print_set_up(content, printer, settings, NULL))
		ret = false;
	
	while (ret && (done_height < printed_content->height) )
		ret = print_draw_next_page(printer, settings);

	print_cleanup(content, printer);
	
	return ret;
}

/**
 * This function prepares the content to be printed. The current browser content
 * is duplicated and resized, printer initialization is called.
 * \param content The content to be printed
 * \param printer The printer interface for the printer to be used
 * \param settings The settings for printing to use
 * \param height updated to the height of the printed content
 * \return true if successful, false otherwise
*/
bool print_set_up(struct content *content,
		const struct printer *printer, struct print_settings *settings,
		double *height)
{

	printed_content = print_init(content, settings);
	
	if (!printed_content)
		return false;
	
	print_apply_settings(printed_content, settings);

	if (height)
		*height = printed_content->height;
	
	printer->print_begin(settings);

	done_height = 0;
	
	return true;	
}

/**
 * This function draws one page, beginning with the height offset of done_height
 * \param printer The printer interface for the printer to be used
 * \param settings The settings for printing to use
 * \return true if successful, false otherwise
 */
bool print_draw_next_page(const struct printer *printer,
			struct print_settings *settings)
{
	
	/*TODO:Plotter will have to be duplicated and passed
	as an argument - to allow simultaneous screen and
	page plotting*/
	plot = *(printer->plotter);

	printer->print_next_page();
	if( !content_redraw(printed_content,
			0,
			-done_height,
			0,0,
			0,
			0,
			page_content_width * settings->scale,
   			page_content_height  * settings->scale,
			settings->scale, 0xffffff))
		return false;
	done_height += page_content_height - text_margin_height;
	
	return true;
}

/**
 * The content passed to the function is duplicated with its boxes, font
 * measuring functions are being set.
 * \param content The content to be printed
 * \param settings The settings for printing to use
 * \return true if successful, false otherwise
 */
struct content *print_init(struct content *content,
		struct print_settings *settings)
{
	struct content* printed_content;
	struct content_user *user_sentinel;
	
	content_add_user(content, NULL, (intptr_t)print_init, 0);
	
	printed_content = talloc_memdup(content, content, sizeof *content);
	
	if (!printed_content)
		return NULL;
	
	printed_content->data.html.bw = 0;
	
	user_sentinel = talloc(printed_content, struct content_user);
	user_sentinel->callback = 0;
	user_sentinel->p1 = user_sentinel->p2 = 0;
	user_sentinel->next = 0;
	printed_content->user_list = user_sentinel;
	content_add_user(printed_content, NULL, (intptr_t)print_init, 0);
	
	printed_content->data.html.layout =
			box_duplicate_tree(content->data.html.layout,
					   printed_content);
	
	if (!printed_content->data.html.layout)
		return NULL;
	
	if (settings->font_func == NULL)
 		printed_content->data.html.font_func = &haru_nsfont;
	else
		printed_content->data.html.font_func = settings->font_func;
	
	return printed_content;
}

/**
 * The content is resized to fit page width. In case it is to wide, it is
 * loosened.
 * \param content The content to be printed
 * \param settings The settings for printing to use
 * \return true if successful, false otherwise
 */
bool print_apply_settings(struct content *content,
			  struct print_settings *settings)
{
	if (settings == NULL)
		return false;
	
	/*Apply settings - adjust page size etc*/
	
	text_margin_height = settings->margins[MARGINTEXT];
	
	page_content_width = (settings->page_width - settings->margins[MARGINLEFT] - 
			settings->margins[MARGINRIGHT]) / settings->scale;
	
	page_content_height = (settings->page_height - settings->margins[MARGINTOP] - 
			settings->margins[MARGINBOTTOM]) / settings->scale;
	
	content_reformat(content, page_content_width, 0);
	LOG(("New layout applied.New height = %d ; New width = %d ",
	     content->height, content->width));
	
	if (content->width > page_content_width)
		return loosen_document_layout(content, content->data.html.layout,
				page_content_width, page_content_height);
			
	return true;	
}

/**
 * Memory allocated during printing is being freed here.
 * \param content The original content
 * \param printer The printer interface for the printer to be used
 * \return true if successful, false otherwise
 */
bool print_cleanup(struct content *content,
		   const struct printer *printer)
{
	printer->print_end();
	
	if (printed_content) {
		content_remove_user(printed_content, NULL, (intptr_t)print_init, 0);
		talloc_free(printed_content);
	}
	
	content_remove_user(content, NULL, (intptr_t)print_init, 0);
	
	return true;
}

/**
 * Generates one of the predefined print settings sets.
 * \return print_settings in case if successful, NULL if unknown configuration \
 * 	or lack of memory.
 */
struct print_settings *print_make_settings(print_configuration configuration)
{
	
	struct print_settings *settings;
	
	switch (configuration){
		case DEFAULT:	
			settings = (struct print_settings*) 
					malloc(sizeof (struct print_settings) );
			
			if (settings == NULL)
				return NULL;
			
			settings->page_width  = 595;
			settings->page_height = 840;
			settings->copies = 1;
			/*with 0.7 the pages look the best, the value in 
			haru_nsfont_apply_style should be kept the same as this
			*/
			settings->scale = 0.7;
			
			settings->margins[MARGINLEFT] = 30;
			settings->margins[MARGINRIGHT] = 30;
			settings->margins[MARGINTOP] = 30;
			settings->margins[MARGINBOTTOM] = 30;
			
			settings->margins[MARGINTEXT] = 10;
			
			settings->output = "out.pdf";
			settings->font_func = &haru_nsfont;
			
			break;
		
		default:
			return NULL;
	}
	
	return settings;	
}

#endif /* WITH_PDF_EXPORT */