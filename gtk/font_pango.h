/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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
 * Font handling (GTK interface).
 */

#ifndef _NETSURF_GTK_FONT_PANGO_H_
#define _NETSURF_GTK_FONT_PANGO_H_

#include <stdbool.h>


struct css_style;

bool nsfont_paint(const struct css_style *style,
		const char *string, size_t length,
		int x, int y, colour c);

PangoFontDescription *nsfont_style_to_description(
		const struct css_style *style);
		
		
#endif
