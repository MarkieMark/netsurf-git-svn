/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_RENDER_HTML_H_
#define _NETSURF_RENDER_HTML_H_

#include "netsurf/content/content.h"

void html_create(struct content *c);
void html_process_data(struct content *c, char *data, unsigned long size);
int html_convert(struct content *c, unsigned int width, unsigned int height);
void html_revive(struct content *c, unsigned int width, unsigned int height);
void html_reformat(struct content *c, unsigned int width, unsigned int height);
void html_destroy(struct content *c);
void html_fetch_object(struct content *c, char *url, struct box *box);

#endif
