/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_UTILS_UTILS_H_
#define _NETSURF_UTILS_UTILS_H_

#include <stdlib.h>
#include "libxml/encoding.h"

void die(const char * const error);
char * strip(char * const s);
int whitespace(const char * str);
void * xcalloc(const size_t n, const size_t size);
void * xrealloc(void * p, const size_t size);
void xfree(void* p);
char * xstrdup(const char * const s);
char * load(const char * const path);
char * squash_whitespace(const char * s);
char * tolat1(xmlChar * s);
char * tolat1_pre(xmlChar * s);
char *squash_tolat1(xmlChar *s);
char *url_join(const char* new, const char* base);
char *get_host_from_url(char* url);

#endif
