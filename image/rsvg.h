/*
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
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
 * Content handler for image/svg using librsvg (interface).
 */

#ifndef _NETSURF_IMAGE_RSVG_H_
#define _NETSURF_IMAGE_RSVG_H_

#include "utils/config.h"
#ifdef WITH_RSVG

#include <stdbool.h>
#include <librsvg/rsvg.h>
#include <cairo.h>
#include "desktop/plot_style.h"
#include "image/bitmap.h"

struct content;
struct http_parameter;

struct content_rsvg_data {
	RsvgHandle *rsvgh;	/**< Context handle for RSVG renderer */
	cairo_surface_t *cs;	/**< The surface built inside a nsbitmap */
	cairo_t *ct;		/**< Cairo drawing context */
	struct bitmap *bitmap;	/**< Created NetSurf bitmap */
};

bool rsvg_create(struct content *c, const struct http_parameter *params);
bool rsvg_process_data(struct content *c, const char *data, unsigned int size);
bool rsvg_convert(struct content *c);
void rsvg_destroy(struct content *c);
bool rsvg_redraw(struct content *c, int x, int y,
                int width, int height,
                int clip_x0, int clip_y0, int clip_x1, int clip_y1,
                float scale, colour background_colour);
bool rsvg_redraw_tiled(struct content *c, int x, int y,
                int width, int height,
                int clip_x0, int clip_y0, int clip_x1, int clip_y1,
                float scale, colour background_colour,
                bool repeat_x, bool repeat_y);
bool rsvg_clone(const struct content *old, struct content *new_content);

#endif /* WITH_RSVG */

#endif
