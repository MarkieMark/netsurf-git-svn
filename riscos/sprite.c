/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Content for image/x-riscos-sprite (RISC OS implementation).
 *
 * No conversion is necessary: we can render RISC OS sprites directly under
 * RISC OS.
 *
 * Unfortunately we have to make a copy of the bitmap data, because sprite areas
 * need a length word at the start.
 */

#include <string.h>
#include <stdlib.h>
#include "oslib/osspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/image.h"
#include "netsurf/riscos/sprite.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_SPRITE


/**
 * Create a new CONTENT_SPRITE.
 *
 * A new empty sprite area is allocated.
 */

bool sprite_create(struct content *c, const char *params[])
{
	union content_msg_data msg_data;

	c->data.sprite.data = malloc(4);
	if (!c->data.sprite.data) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
	}
	c->data.sprite.length = 4;
	return true;
}


/**
 * Convert a CONTENT_SPRITE for display.
 *
 * No conversion is necessary. We merely read the sprite dimensions.
 */

bool sprite_convert(struct content *c, int width, int height)
{
	os_error *error;
	int w, h;
	union content_msg_data msg_data;
	char *source_data;
	
	source_data = ((char *)c->source_data) - 4;
	osspriteop_area *area = (osspriteop_area*)source_data;
	c->data.sprite.data = area;

	error = xosspriteop_read_sprite_info(osspriteop_PTR,
			(osspriteop_area *)0x100,
			(osspriteop_id) ((char *) area + area->first),
			&w, &h, NULL, NULL);
	if (error) {
		LOG(("xosspriteop_read_sprite_info: 0x%x: %s",
				error->errnum, error->errmess));
		msg_data.error = error->errmess;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	c->width = w;
	c->height = h;
	c->title = malloc(100);
	if (c->title)
		snprintf(c->title, 100, messages_get("SpriteTitle"), c->width,
				c->height, c->data.sprite.length);
	c->status = CONTENT_STATUS_DONE;
	return true;
}


/**
 * Destroy a CONTENT_SPRITE and free all resources it owns.
 */

void sprite_destroy(struct content *c)
{
	/* do not free c->data.sprite.data at it is simply a pointer to
	 * 4 bytes before the c->data.source_data. */
	free(c->title);
}


/**
 * Redraw a CONTENT_SPRITE.
 */

bool sprite_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour)
{
	return image_redraw(c->data.sprite.data,
			ro_plot_origin_x + x * 2,
			ro_plot_origin_y - y * 2,
			width, height,
			c->width,
			c->height,
			background_colour,
			false, false, false,
			IMAGE_PLOT_OS);
}

#endif
