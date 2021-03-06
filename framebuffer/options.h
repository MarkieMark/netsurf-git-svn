/*
 * Copyright 2008 Daniel Silverstone <dsilvers@netsurf-browser.org>
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

#ifndef _NETSURF_FRAMEBUFFER_OPTIONS_H_
#define _NETSURF_FRAMEBUFFER_OPTIONS_H_

#include "desktop/options.h"

extern int option_fb_depth;
extern int option_fb_refresh;
extern bool option_fb_font_monochrome; /* render font monochrome */
extern char *option_fb_device;
extern char *option_fb_input_devpath;
extern char *option_fb_input_glob;
extern char *option_fb_face_sans_serif; /* default sans face */
extern char *option_fb_face_sans_serif_bold; /* bold sans face */
extern char *option_fb_face_sans_serif_italic; /* bold sans face */
extern char *option_fb_face_sans_serif_italic_bold; /* bold sans face */
extern char *option_fb_face_monospace; /* monospace face */
extern char *option_fb_face_serif; /* serif face */
extern char *option_fb_face_serif_bold; /* bold serif face */

#define EXTRA_OPTION_DEFINE                     \
  int option_fb_depth = 32;                     \
  int option_fb_refresh = 70;                   \
  bool option_fb_font_monochrome = false;       \
  char *option_fb_device = 0;                   \
  char *option_fb_input_devpath = 0;            \
  char *option_fb_input_glob = 0;               \
  char *option_fb_face_sans_serif;              \
  char *option_fb_face_sans_serif_bold;         \
  char *option_fb_face_sans_serif_italic;       \
  char *option_fb_face_sans_serif_italic_bold;  \
  char *option_fb_face_monospace;               \
  char *option_fb_face_serif;                   \
  char *option_fb_face_serif_bold;              

#define EXTRA_OPTION_TABLE                                              \
    { "fb_depth", OPTION_INTEGER, &option_fb_depth },                   \
    { "fb_refresh", OPTION_INTEGER, &option_fb_refresh },               \
    { "fb_device", OPTION_STRING, &option_fb_device },                  \
    { "fb_input_devpath", OPTION_STRING, &option_fb_input_devpath },    \
    { "fb_input_glob", OPTION_STRING, &option_fb_input_glob },          \
    { "fb_font_monochrome", OPTION_BOOL, &option_fb_font_monochrome },  \
    { "fb_face_sans_serif", OPTION_STRING, &option_fb_face_sans_serif }, \
    { "fb_face_sans_serif_bold", OPTION_STRING, &option_fb_face_sans_serif_bold }, \
    { "fb_face_sans_serif_italic", OPTION_STRING, &option_fb_face_sans_serif_italic }, \
    { "fb_face_sans_serif_italic_bold", OPTION_STRING, &option_fb_face_sans_serif_italic_bold }, \
    { "fb_face_monospace", OPTION_STRING, &option_fb_face_monospace },  \
    { "fb_face_serif", OPTION_STRING, &option_fb_face_serif },          \
    { "fb_serif_bold", OPTION_STRING, &option_fb_face_serif_bold }              

#endif
