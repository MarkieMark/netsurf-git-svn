/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Save dialog and drag and drop saving (implementation).
 */

#include <stdlib.h>
#include <string.h>
#include "oslib/dragasprite.h"
#include "oslib/wimp.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/save_draw.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

gui_save_type gui_current_save_type;


/**
 * Handle clicks in the save dialog.
 */

void ro_gui_save_click(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_SAVE_ICON:
			if (pointer->buttons == wimp_DRAG_SELECT) {
				gui_current_drag_type = GUI_DRAG_SAVE;
				ro_gui_drag_icon(pointer);
			}
			break;
	}
}


/**
 * Start drag of icon under the pointer.
 */

void ro_gui_drag_icon(wimp_pointer *pointer)
{
	char *sprite;
	os_box box = { pointer->pos.x - 34, pointer->pos.y - 34,
			pointer->pos.x + 34, pointer->pos.y + 34 };
	os_error *error;

	if (pointer->i == -1)
		return;

	sprite = ro_gui_get_icon_string(pointer->w, pointer->i);

	error = xdragasprite_start(dragasprite_HPOS_CENTRE |
			dragasprite_VPOS_CENTRE |
			dragasprite_BOUND_POINTER |
			dragasprite_DROP_SHADOW,
			(osspriteop_area *) 1, sprite, &box, 0);
	if (error) {
		LOG(("0x%x: %s", error->errnum, error->errmess));
		warn_user(error->errmess);
	}
}


/**
 * Handle User_Drag_Box event for a drag from the save dialog.
 */

void ro_gui_save_drag_end(wimp_dragged *drag)
{
	wimp_pointer pointer;
	wimp_message message;

	wimp_get_pointer_info(&pointer);

	message.your_ref = 0;
	message.action = message_DATA_SAVE;
	message.data.data_xfer.w = pointer.w;
	message.data.data_xfer.i = pointer.i;
	message.data.data_xfer.pos.x = pointer.pos.x;
	message.data.data_xfer.pos.y = pointer.pos.y;
	message.data.data_xfer.est_size = 1000;
	message.data.data_xfer.file_type = 0xfaf;
	if (gui_current_save_type == GUI_SAVE_DRAW)
		message.data.data_xfer.file_type = 0xaff;
	strncpy(message.data.data_xfer.file_name,
			ro_gui_get_icon_string(dialog_saveas, ICON_SAVE_PATH),
			212);
	message.size = 44 + ((strlen(message.data.data_xfer.file_name) + 4) &
			(~3u));

	wimp_send_message_to_window(wimp_USER_MESSAGE, &message,
			pointer.w, pointer.i);
}


/**
 * Handle Message_DataSaveAck for a drag from the save dialog.
 */

void ro_gui_save_datasave_ack(wimp_message *message)
{
	char *path = message->data.data_xfer.file_name;
	struct content *c = current_gui->data.browser.bw->current_content;

	ro_gui_set_icon_string(dialog_saveas, ICON_SAVE_PATH, path);

	switch (gui_current_save_type) {
		case GUI_SAVE_SOURCE:
		        if (!c)
		                return;
	                xosfile_save_stamped(path, ro_content_filetype(c),
					c->source_data,
					c->source_data + c->source_size);
			break;

		case GUI_SAVE_DRAW:
			if (!c)
				return;
			save_as_draw(c, path);
			break;
	}

	wimp_create_menu(wimp_CLOSE_MENU, 0, 0);
}
