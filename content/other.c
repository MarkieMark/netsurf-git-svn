/**
 * $Id: other.c,v 1.1 2003/06/17 19:24:20 bursa Exp $
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "netsurf/content/other.h"
#include "netsurf/utils/utils.h"


void other_create(struct content *c)
{
	c->data.other.data = xcalloc(0, 1);
	c->data.other.length = 0;
}


void other_process_data(struct content *c, char *data, unsigned long size)
{
	c->data.other.data = xrealloc(c->data.other.data, c->data.other.length + size);
	memcpy(c->data.other.data + c->data.other.length, data, size);
	c->data.other.length += size;
	c->size += size;
}


int other_convert(struct content *c, unsigned int width, unsigned int height)
{
	c->status = CONTENT_STATUS_DONE;
	return 0;
}


void other_revive(struct content *c, unsigned int width, unsigned int height)
{
	assert(0);
}


void other_reformat(struct content *c, unsigned int width, unsigned int height)
{
	assert(0);
}


void other_destroy(struct content *c)
{
	assert(0);
}
