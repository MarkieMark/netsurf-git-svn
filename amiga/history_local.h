/*
 * Copyright 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_HISTORY_LOCAL_H
#define AMIGA_HISTORY_LOCAL_H

#include <exec/types.h>
#include <intuition/classusr.h>
#include "amiga/gui.h"

struct history_window {
	struct Window *win;
	Object *objects[OID_LAST];
	struct Gadget *gadgets[GID_LAST]; // not used
	struct nsObject *node;
	struct browser_window *bw;
	ULONG pad[4];
	struct Hook scrollerhook;
};

void ami_history_open(struct browser_window *bw, struct history *history);
void ami_history_close(struct history_window *hw);
#endif