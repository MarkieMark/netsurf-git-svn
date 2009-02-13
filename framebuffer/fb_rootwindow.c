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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <unistd.h>

#ifdef WITH_HUBBUB
#include <hubbub/hubbub.h>
#endif

#include "desktop/gui.h"
#include "desktop/plotters.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "desktop/history_core.h"

#include "framebuffer/fb_bitmap.h"
#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_frontend.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_schedule.h"
#include "framebuffer/fb_cursor.h"
#include "framebuffer/fb_rootwindow.h"

struct fb_widget_image_s {
  unsigned int 	 width;
  unsigned int 	 height;
  unsigned int 	 bytes_per_pixel; /* 3:RGB, 4:RGBA */ 
  unsigned char	 *pixel_data;
};

typedef struct fb_widget_image_s fb_widget_image_t;

static const fb_widget_image_t left_arrow = {
  22, 25, 4,
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\0\10\0\14\0\10\0\206\0\11\0\350\0\10\0.\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\0\10\0\1\0\10\0]\0\12\0\347\0\10\0\377\0\10\0\377\0\10\0W\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0""5\0\11\0\310\0\10"
  "\0\377\0)\3\375\0|\12\377\0\10\0\377\0\10\0W\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\0\10\0\30\0\10\0\237\0\10\0\376\0\23\1\375\0\215\14\377\0\371\25\377"
  "\0\301\20\377\0\10\0\377\0\10\0W\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0\6\0\10\0u\0\12\0\364\0"
  "\11\0\377\0d\10\377\0\350\24\377\0\377\26\377\0\377\26\377\0\301\20\377\0"
  "\10\0\377\0\10\0V\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\0\10\0L\0\12\0\334\0\10\0\377\0<\5\376\0\313\21\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\301\20\377\0\10\0\377\0\10\0V\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\0\10\0(\0\10\0\267\0\10\0\377\0\36\2\375"
  "\0\244\16\377\0\376\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\301\20\377\0\10\0\377\0\10\0V\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0\17\0\10"
  "\0\216\0\11\0\373\0\15\0\376\0|\12\377\0\363\25\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\301"
  "\20\377\0\10\0\377\0\10\0U\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\0\10\0\2\0\10\0e\0\12\0\354\0\10\0\377\0T\7\377\0\335\23\377\0"
  "\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\301\20\377\0\10\0\377\0\10"
  "\0U\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0<\0\12\0\320\0\10\0\377"
  "\0/\3\375\0\274\20\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\301\20\377\0\10\0\377\0\10\0T\377\377\377\0\0\10\0\35"
  "\0\10\0\247\0\10\0\377\0\26\1\375\0\223\14\377\0\373\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\301"
  "\20\377\0\10\0\377\0\10\0T\0\10\0m\0\12\0\367\0\11\0\376\0l\11\377\0\354"
  "\24\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\301\20\377\0\10\0\377\0\10"
  "\0T\0\11\0\331\0\10\0\377\0=\5\377\0\340\23\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377"
  "\26\377\0\377\26\377\0\301\20\377\0\10\0\377\0\10\0S\0\10\0\21\0\10\0\222"
  "\0\11\0\374\0\16\1\376\0\200\13\377\0\364\25\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\301"
  "\20\377\0\10\0\377\0\10\0S\377\377\377\0\377\377\377\0\0\10\0+\0\11\0\275"
  "\0\10\0\377\0\40\2\375\0\250\16\377\0\377\26\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\301\20\377\0\10\0\377\0\10"
  "\0S\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0Q\0\12"
  "\0\340\0\10\0\377\0@\5\376\0\316\22\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377"
  "\26\377\0\377\26\377\0\301\20\377\0\10\0\377\0\10\0R\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0\10\0\10\0{\0\12\0"
  "\366\0\11\0\376\0h\11\377\0\352\24\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\301"
  "\20\377\0\10\0\377\0\10\0R\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0\33\0\10\0\245"
  "\0\10\0\377\0\24\1\375\0\221\14\377\0\372\26\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\301\20\377\0\10\0"
  "\377\0\10\0R\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0"
  ":\0\12\0\315\0\10\0\377\0,\3\375\0\270\20\377\0\377\26\377\0\377\26\377\0"
  "\377\26\377\0\377\26\377\0\377\26\377\0\301\20\377\0\10\0\377\0\10\0Q\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0\2\0"
  "\10\0c\0\12\0\353\0\10\0\377\0P\6\377\0\332\23\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\301\20\377\0\10\0\377\0\10\0Q\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0"
  "\10\0\17\0\10\0\215\0\11\0\373\0\15\0\376\0x\12\377\0\361\25\377\0\377\26"
  "\377\0\301\20\377\0\10\0\377\0\10\0Q\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\0\10\0'\0\10\0\267\0\10\0\377\0\34\2\375\0\241\16\377\0\300"
  "\20\377\0\10\0\377\0\10\0P\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\0\10\0L\0\12\0\334\0\10\0\377\0!\2\376\0\10"
  "\0\377\0\10\0P\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\0\10\0\6\0\10\0v\0\12\0\364\0\10\0\377\0\10"
  "\0N\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0\30\0\10\0g\0\10\0"
  "\6",
};

static const fb_widget_image_t right_arrow = {
  22, 25, 4,
  "\0\10\0""0\0\11\0\350\0\10\0\206\0\10\0\14\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\0\10\0W\0\10\0\377\0\10\0\377\0\13\0\347\0\10\0]\0\10\0\1\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0W\0"
  "\10\0\377\0|\12\377\0)\3\375\0\10\0\377\0\12\0\311\0\10\0""6\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0W\0\10\0\377\0"
  "\301\20\377\0\371\25\377\0\215\14\377\0\23\1\375\0\10\0\376\0\10\0\240\0"
  "\10\0\31\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0V\0\10\0\377\0\301\20"
  "\377\0\377\26\377\0\377\26\377\0\350\24\377\0d\10\377\0\11\0\377\0\12\0\364"
  "\0\10\0w\0\10\0\6\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\0\10\0V\0\10\0\377\0\301\20\377\0\377\26\377\0\377"
  "\26\377\0\377\26\377\0\377\26\377\0\313\21\377\0<\5\376\0\10\0\377\0\12\0"
  "\336\0\10\0N\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\0\10\0V\0\10\0\377\0\301\20\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\376\26\377\0\244\16\377\0\36\2\375\0\10"
  "\0\377\0\11\0\273\0\10\0*\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0U\0"
  "\10\0\377\0\301\20\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\363\25\377\0|\12\377\0\15\0\376"
  "\0\11\0\374\0\10\0\221\0\10\0\21\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0U\0\10\0\377\0\301\20"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377"
  "\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\335\23\377\0T\7\377\0\10"
  "\0\377\0\12\0\355\0\10\0h\0\10\0\3\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\0\10\0T\0\10\0\377\0\301\20\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\274\20\377\0/\4\375\0\10\0"
  "\377\0\12\0\322\0\10\0@\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0"
  "T\0\10\0\377\0\301\20\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377"
  "\26\377\0\377\26\377\0\377\26\377\0\373\26\377\0\223\14\377\0\26\1\375\0"
  "\10\0\377\0\10\0\253\0\10\0\37\377\377\377\0\0\10\0T\0\10\0\377\0\301\20"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377"
  "\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\354\24\377\0l\11\377\0\11\0\376"
  "\0\11\0\370\0\10\0u\0\10\0S\0\10\0\377\0\301\20\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377"
  "\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\340\23\377\0=\5\377\0\10\0\377\0\11\0\327\0"
  "\10\0S\0\10\0\377\0\301\20\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377"
  "\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\364\25\377\0\200\13"
  "\377\0\16\1\376\0\11\0\375\0\10\0\227\0\10\0\23\0\10\0S\0\10\0\377\0\301"
  "\20\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\250\16\377\0\40\2\375\0\10\0\377\0\11\0\301\0\10\0/\377"
  "\377\377\0\377\377\377\0\0\10\0R\0\10\0\377\0\301\20\377\0\377\26\377\0\377"
  "\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\316\22\377\0@\5\376\0\10\0\377"
  "\0\12\0\342\0\10\0U\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\0\10\0R\0\10\0\377\0\301\20\377\0\377\26\377\0\377\26\377\0\377\26\377"
  "\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\352\24"
  "\377\0h\11\377\0\11\0\376\0\12\0\367\0\10\0~\0\10\0\11\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0R\0\10\0\377\0"
  "\301\20\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\372\26\377\0\221\14\377\0\24\1\375\0\10\0\377\0\10\0"
  "\247\0\10\0\35\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\0\10\0Q\0\10\0\377\0\301\20\377\0"
  "\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\377\26\377\0\270\20"
  "\377\0,\3\375\0\10\0\377\0\12\0\317\0\10\0=\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\0\10\0Q\0\10\0\377\0\301\20\377\0\377\26\377\0\377\26"
  "\377\0\377\26\377\0\332\23\377\0P\6\377\0\10\0\377\0\12\0\354\0\10\0e\0\10"
  "\0\2\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\0\10\0Q\0\10\0\377\0\301\20\377\0\377\26\377\0\361\25\377\0x\12\377\0\15"
  "\0\376\0\11\0\374\0\10\0\217\0\10\0\17\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0P\0\10\0"
  "\377\0\300\20\377\0\241\16\377\0\34\2\375\0\10\0\377\0\10\0\270\0\10\0(\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0P\0\10\0\377\0!\2\376"
  "\0\10\0\377\0\12\0\335\0\10\0M\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\0\10\0K\0\10\0\377\0\12\0\364\0\10\0v"
  "\0\10\0\6\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\0\10\0\10\0\10\0j\0\10\0\30\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0",
};

static const fb_widget_image_t reload = {
  24, 25, 4,
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0"
  "\36\0\10\0O\0\10\0H\0\10\0!\0\10\0\2\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\0\10\0\5\0\10\0r\0\10\0\271\0\10\0\353\0\10"
  "\0\377\0\10\0\377\0\10\0\377\0\10\0\377\0\10\0\370\0\10\0\263\0\10\0""6\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\0\10\0.\0\10\0\317\0\10\0\377\0\10\0\341\0\10\0\255\0\10"
  "\0z\0\10\0I\0\10\0Q\0\10\0v\0\10\0\241\0\10\0\366\0\10\0\377\0\10\0\267\0"
  "\10\0\40\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0|\0"
  "\10\0\372\0\10\0\361\0\10\0e\0\10\0\0\0\10\0\0\0\10\0\0\0\10\0\0\0\10\0\0"
  "\0\10\0\0\0\10\0\0\0\10\0\27\0\10\0\214\0\10\0\370\0\10\0\343\0\10\0&\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\0\10\0i\0\10\0\377\0\10\0\272\0\10\0\36\0\10\0\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\0\10\0\0\0\10\0\0\0\10\0D\0\10\0\365\0\10\0\350\0\10"
  "\0)\377\377\377\0\377\377\377\0\0\0\0\"\377\377\377\0\377\377\377\0\0\10"
  "\0\"\0\10\0\364\0\10\0\312\0\10\0\3\0\10\0\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\0\10\0\0\0\10\0>\0\10\0\366\0\7\0"
  "\306\0\0\0n\0\0\0\330\0\0\0\366\377\377\377\0\0\10\0\2\0\10\0\301\0\10\0"
  "\367\0\10\0)\0\10\0\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\0\0\0\6\0\0\0S\0\1\0\342\0\1\0\377\0\0\0\373"
  "\0\0\0\377\0\0\0\333\377\377\377\0\0\10\0O\0\10\0\377\0\10\0u\0\10\0\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\0\0\0\2\0\0\0\267\0\0\0\366\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0"
  "\0\0\260\377\377\377\0\0\10\0\207\0\10\0\377\0\10\0\25\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\0\0\0\0\0\0\0\7\0\0\0\261\0\0\0\357\0\0\0\377\0\0\0\377\0\0\0\375\0\0"
  "\0t\377\377\377\0\0\10\0\272\0\10\0\341\0\10\0\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\0\0\0\0\0\0\0\0\0\0\0\204\0\0\0\354\0\0\0\377\0\0\0\355\0\0\0"
  "C\377\377\377\0\0\10\0\354\0\10\0\254\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\0\0\0\0\0\0\0U\0\0\0\352\0\0\0"
  "\356\0\0\0\26\0\10\0\40\0\10\0\377\0\10\0x\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\0\0\0\0\0\0"
  """1\0\0\0\324\0\0\0\0\0\10\0\7\0\10\0C\0\10\0\"\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\0\0\0\0\0\0\0\10\377\377\377\0\0\10\0\0\0\0\0""8\0\0\0\6\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\0\10\0\13\0\10\0\266\0\10\0I\377\377\377\0\0\0\0\273"
  "\0\0\0\322\0\0\0\24\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\0\10\0;\0\10\0\377\0\10\0\\\377\377"
  "\377\0\0\0\0\340\0\0\0\375\0\0\0\341\0\0\0+\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0p\0\10\0\377\0\10\0*\0"
  "\0\0\10\0\0\0\347\0\0\0\377\0\0\0\377\0\0\0\350\0\0\0L\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0\244\0\10\0\364\0\10\0"
  "\2\0\0\0""1\0\0\0\351\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\353\0\0\0x\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0\333\0\10\0\304\0\10\0"
  "\0\0\0\0_\0\0\0\370\0\0\0\377\0\0\0\377\0\0\0\374\0\0\0\352\0\0\0\301\0\0"
  "\0\35\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\0\10\0d\0\10\0\377\0\10\0w\377\377\377\0\0\0"
  "\0\232\0\0\0\376\0\0\0\353\0\1\0\367\0\4\0\367\0\4\0)\0\0\0\0\0\0\0\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\0\10\0\37\0\10\0\362\0\10\0\315\0\10\0\4\377\377\377\0\0\0\0\236\0\0\0"
  "\217\0\0\0&\0\10\0>\0\10\0\376\0\10\0\320\0\10\0\25\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0\12\0\10"
  "\0\277\0\10\0\371\0\10\0,\0\10\0\0\377\377\377\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\10\0\0\0\10\0c\0\10\0\375\0\10\0\325\0\10\0\36\377\377\377\0\377\377\377"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\377\377\377\0\0\10\0<\0\10\0\333\0\10\0\377\0\10\0s\0\10\0"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0"
  "\377\377\377\0\0\10\0\0\0\10\0Z\0\10\0\373\0\10\0\370\0\10\0\226\0\10\0\35"
  "\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\0\10\0\5\0\10\0"
  """0\0\10\0\220\0\10\0\375\0\10\0\351\0\10\0S\0\10\0\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377"
  "\0\377\377\377\0\0\10\0\0\0\10\0/\0\10\0\256\0\10\0\376\0\10\0\376\0\10\0"
  "\341\0\10\0\274\0\10\0\242\0\10\0\312\0\10\0\370\0\10\0\377\0\10\0\377\0"
  "\10\0\252\0\10\0\24\0\10\0\0\377\377\377\0\377\377\377\0\377\377\377\0\377"
  "\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377"
  "\377\0\377\377\377\0\0\10\0\0\0\10\0\0\0\10\0.\0\10\0\215\0\10\0\265\0\10"
  "\0\334\0\10\0\366\0\10\0\320\0\10\0\234\0\10\0i\0\10\0""1\0\10\0\0\0\10\0"
  "\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0\377\377\377\0",
};



struct fb_widget {
        struct fb_widget *next;
        int x;
        int y;
        int width;
        int height;
        fb_widget_mouseclick_t click;
        struct bitmap *bitmap; 
        struct gui_window *g;
};

static struct fb_widget *widget_list;

static struct fb_widget *
fb_add_button_widget(int x, 
              int y, 
              const fb_widget_image_t *widget_image, 
              fb_widget_mouseclick_t click_rtn)
{
        struct fb_widget *new_widget;
        new_widget = malloc(sizeof(struct fb_widget));
        if (new_widget == NULL)
                return NULL;

        new_widget->x = x;
        new_widget->y = y;
        new_widget->click = click_rtn;
        new_widget->width = widget_image->width;
        new_widget->height = widget_image->height;

        new_widget->bitmap = bitmap_create(widget_image->width, 
                                           widget_image->height, 
                                           0);

        memcpy(new_widget->bitmap->pixdata, 
               widget_image->pixel_data, 
               widget_image->width * 
               widget_image->height * 
               widget_image->bytes_per_pixel);


        new_widget->next = widget_list;
        widget_list = new_widget;

        /* plot the image */
        plot.bitmap(x, y, 
                    new_widget->width, 
                    new_widget->height, 
                    new_widget->bitmap, 
                    0, NULL);

        return new_widget;
}

struct fb_widget *
fb_add_window_widget(struct gui_window *g,
                     fb_widget_mouseclick_t click_rtn)
{
        struct fb_widget *new_widget;
        new_widget = malloc(sizeof(struct fb_widget));
        if (new_widget == NULL)
                return NULL;

        new_widget->x = g->x;
        new_widget->y = g->y;
        new_widget->width = g->width;
        new_widget->height = g->height;
        new_widget->click = click_rtn;

        new_widget->bitmap = NULL;
        new_widget->g = g;

        new_widget->next = widget_list;
        widget_list = new_widget;

        return new_widget;
}

/* left icon click routine */
static int 
fb_widget_leftarrow_click(struct gui_window *g, browser_mouse_state st, int x, int y)
{
        if (history_back_available(g->bw->history))
                history_back(g->bw, g->bw->history);
        return 0;

}

/* right arrow icon click routine */
static int 
fb_widget_rightarrow_click(struct gui_window *g, browser_mouse_state st, int x, int y)
{
        if (history_forward_available(g->bw->history))
                history_forward(g->bw, g->bw->history);
        return 0;

}

void fb_rootwindow_create(framebuffer_t *fb)
{
        bbox_t saved_plot_ctx;
        struct fb_widget *newwidget;

        widget_list = NULL;

        /* enlarge the clipping rectangle to the whole screen for plotting the
         * root window 
         */
        saved_plot_ctx = fb_plot_ctx;

        fb_plot_ctx.x0 = 0;
        fb_plot_ctx.y0 = 0;
        fb_plot_ctx.x1 = fb->width;
        fb_plot_ctx.y1 = fb->height;

        /* do our drawing etc. */
        plot.fill(0, 0, fb->width, fb->height, 0xFFFFFFFF);

        newwidget = fb_add_button_widget(2, 2, 
                                         &left_arrow, 
                                         fb_widget_leftarrow_click);

        newwidget = fb_add_button_widget(newwidget->x + newwidget->width + 2, 
                                         2, 
                                         &right_arrow, 
                                         fb_widget_rightarrow_click);

        fb_os_redraw(&fb_plot_ctx);

        /* restore clipping rectangle */
        fb_plot_ctx = saved_plot_ctx;
}

void 
fb_rootwindow_click(struct gui_window *g, browser_mouse_state st, int x, int y)
{
        struct fb_widget *widget;
        LOG(("Click in root window"));

        widget = widget_list;
        while (widget != NULL) {
                if ((widget->click != NULL) &&
                    (x > widget->x) && 
                    (y > widget->y) && 
                    (x < widget->x + widget->width) && 
                    (y < widget->y + widget->height)) {
                        widget->click(g, st, x, y);
                        break;
                }
                widget = widget->next;
        }
}



/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
