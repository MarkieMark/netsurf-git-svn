/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/image/bmpread.h"
#include "netsurf/image/ico.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_BMP

bool nsico_create(struct content *c, const char *params[]) {
	union content_msg_data msg_data;

	c->data.ico.ico = calloc(sizeof(struct ico_collection), 1);
	if (!c->data.ico.ico) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		warn_user("NoMemory", 0);
		return false;
	}
	return true;
}


bool nsico_convert(struct content *c, int iwidth, int iheight) {
	struct bmp_image *bmp;
	bmp_result res;
	struct ico_collection *ico;
	union content_msg_data msg_data;

	/* set our source data */
	ico = c->data.ico.ico;
	ico->ico_data = c->source_data;
	ico->buffer_size = c->source_size;

	/* analyse the BMP */
	res = ico_analyse(ico);
	switch (res) {
		case BMP_OK:
			break;
		case BMP_INSUFFICIENT_MEMORY:
			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			warn_user("NoMemory", 0);
			return false;
		case BMP_INSUFFICIENT_DATA:
		case BMP_DATA_ERROR:
			msg_data.error = messages_get("BadICO");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
	}

	/*	Store our content width and description
	*/
	c->width = ico->width;
	c->height = ico->height;
	c->title = malloc(100);
	if (c->title)
		snprintf(c->title, 100, messages_get("ICOTitle"), c->width,
				c->height, c->source_size);
	c->size += (ico->width * ico->height * 4) + 16 + 44 + 100;

	/* exit as a success */
	bmp = ico_find(c->data.ico.ico, 255, 255);
	assert(bmp);
	c->bitmap = bmp->bitmap;
	c->status = CONTENT_STATUS_DONE;
	return true;
}

bool nsico_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour) {
	struct bmp_image *bmp = ico_find(c->data.ico.ico, width, height);
	if (!bmp->decoded)
	  	bmp_decode(bmp);
	c->bitmap = bmp->bitmap;
	return plot.bitmap(x, y, width, height, c->bitmap,
			background_colour);
}


bool nsico_redraw_tiled(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour,
		bool repeat_x, bool repeat_y) {
	struct bmp_image *bmp = ico_find(c->data.ico.ico, width, height);
	if (!bmp->decoded)
	  	bmp_decode(bmp);
	c->bitmap = bmp->bitmap;
	return plot.bitmap_tile(x, y, width, height, c->bitmap,
			background_colour, repeat_x, repeat_y);
}


void nsico_destroy(struct content *c)
{
	ico_finalise(c->data.ico.ico);
	free(c->data.ico.ico);
	free(c->title);
}

#endif