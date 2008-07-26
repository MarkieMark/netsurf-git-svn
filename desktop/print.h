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
 * Conception:
 * 	Generalized output-in-pages. Allows the current content to be 'printed':
 * either into a pdf file (any other later?) or to any kind of other output
 * destination that divides the website into pages - as a printer.
 * 	The basic usage is calling print_basic_run which sets everything up,
 * prints page after page until the whole content is printed and cleans
 * everyting up.
 * 	If there are any other, printer specific routines to be performed in the
 * meantime - there can be set up any other printing funcion, which can use
 * print_set_up, print_draw_next_page and print_cleanup directly.
*/

#ifndef NETSURF_DESKTOP_PRINT_H
#define NETSURF_DESKTOP_PRINT_H

#include <stdbool.h>

struct content;
struct printer;

enum { MARGINLEFT = 0, MARGINRIGHT = 1, MARGINTOP = 2, MARGINBOTTOM = 3,
       MARGINTEXT = 4};

/** Predefined printing configuration names*/
typedef enum {DEFAULT} print_configuration;

/** Settings for a print - filled in by print_make_settings or
 * 'manually' by the caller
*/
struct print_settings{
	/*Standard parameters*/
	float page_width, page_height;
	float margins[5];
	
	float scale;

	unsigned int copies;

	/*Output destinations - file/printer name*/
	const char *output;

	/*TODO: more options?*/
	const struct font_functions *font_func;
};

bool print_basic_run(struct content *, const struct printer *, struct print_settings *);
bool print_set_up(struct content *content,
		const struct printer *printer, struct print_settings *settings,
		double *height);
bool print_draw_next_page(const struct printer *printer,
		struct print_settings *settings);
bool print_cleanup(struct content *, const struct printer *);

struct print_settings *print_make_settings(print_configuration configuration);

#endif