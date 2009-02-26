/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#ifndef NETSURF_FB_FONT_H
#define NETSURF_FB_FONT_H

bool fb_font_init(void);
bool fb_font_finalise(void);

#ifdef FB_USE_FREETYPE

#include <ft2build.h>  
#include FT_FREETYPE_H 
#include <freetype/ftglyph.h>

FT_Glyph fb_getglyph(const struct css_style *style, uint32_t ucs4);

extern int ft_load_type;

#else

#include "utils/utf8.h"

struct fb_font_desc {
    const char *name;
    int width, height;
    const char *encoding;
    const uint32_t *data;
};

extern const struct fb_font_desc font_vga_8x16;

extern const struct fb_font_desc* fb_get_font(const struct css_style *style);

extern utf8_convert_ret utf8_to_font_encoding(const struct fb_font_desc* font,
				       const char *string, 
				       size_t len,
				       char **result);
#endif

#endif /* NETSURF_FB_FONT_H */

