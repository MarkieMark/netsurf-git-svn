/*
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
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
 * Content for image/x-drawfile (RISC OS implementation).
 *
 * The DrawFile module is used to plot the DrawFile.
 */

#include "utils/config.h"
#ifdef WITH_DRAW

#include <string.h>
#include <stdlib.h>
#include "oslib/drawfile.h"
#include "utils/config.h"
#include "desktop/plotters.h"
#include "content/content_protected.h"
#include "riscos/draw.h"
#include "riscos/gui.h"
#include "utils/utils.h"
#include "utils/messages.h"
#include "utils/log.h"

/**
 * Convert a CONTENT_DRAW for display.
 *
 * No conversion is necessary. We merely read the DrawFile dimensions and
 * bounding box bottom-left.
 */

bool draw_convert(struct content *c)
{
	union content_msg_data msg_data;
	const char *source_data;
	unsigned long source_size;
	const void *data;
	os_box bbox;
	os_error *error;
	char title[100];

	source_data = content__get_source_data(c, &source_size);
	data = source_data;

	/* BBox contents in Draw units (256*OS unit) */
	error = xdrawfile_bbox(0, (drawfile_diagram *) data,
			(int) source_size, 0, &bbox);
	if (error) {
		LOG(("xdrawfile_bbox: 0x%x: %s",
				error->errnum, error->errmess));
		msg_data.error = error->errmess;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	if (bbox.x1 > bbox.x0 && bbox.y1 > bbox.y0) {
		/* c->width & c->height stored as (OS units/2)
		   => divide by 512 to convert from draw units */
		c->width = ((bbox.x1 - bbox.x0) / 512);
		c->height = ((bbox.y1 - bbox.y0) / 512);
	}
	else
		/* invalid/undefined bounding box */
		c->height = c->width = 0;

	c->data.draw.x0 = bbox.x0;
	c->data.draw.y0 = bbox.y0;
	snprintf(title, sizeof(title), messages_get("DrawTitle"), c->width,
			c->height, source_size);
	content__set_title(c, title);

	c->status = CONTENT_STATUS_DONE;
	/* Done: update status bar */
	content_set_status(c, "");
	return true;
}


/**
 * Destroy a CONTENT_DRAW and free all resources it owns.
 */

void draw_destroy(struct content *c)
{
}


/**
 * Redraw a CONTENT_DRAW.
 */

bool draw_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour)
{
	os_trfm matrix;
	const char *source_data;
	unsigned long source_size;
	const void *data;
	os_error *error;

	if (plot.flush && !plot.flush())
		return false;

	if (!c->width || !c->height)
		return false;

	source_data = content__get_source_data(c, &source_size);
	data = source_data;

	/* Scaled image. Transform units (65536*OS units) */
	matrix.entries[0][0] = width * 65536 / c->width;
	matrix.entries[0][1] = 0;
	matrix.entries[1][0] = 0;
	matrix.entries[1][1] = height * 65536 / c->height;
	/* Draw units. (x,y) = bottom left */
	matrix.entries[2][0] = ro_plot_origin_x * 256 + x * 512 -
			c->data.draw.x0 * width / c->width;
	matrix.entries[2][1] = ro_plot_origin_y * 256 - (y + height) * 512 -
			c->data.draw.y0 * height / c->height;

	error = xdrawfile_render(0, (drawfile_diagram *) data,
			(int) source_size, &matrix, 0, 0);
	if (error) {
		LOG(("xdrawfile_render: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	return true;
}

/**
 * Clone a CONTENT_DRAW
 */

bool draw_clone(const struct content *old, struct content *new_content)
{
	/* Simply rerun convert */
	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (draw_convert(new_content) == false)
			return false;
	}

	return true;
}

#endif
