/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_BITMAP_H_
#define _NETSURF_RISCOS_BITMAP_H_

#include "oslib/osspriteop.h"

struct osspriteop_area;

typedef enum {
	BITMAP_INITIALISE_DONE,		/** Initialisation has been done */
	BITMAP_INITIALISE_QUICK,	/** Just allocate memory */
	BITMAP_INITIALISE_FULL		/** Clear the sprite buffer */
} bitmap_initialisation;

struct bitmap {
	int width;
	int height;
	bool opaque;
	bool modified;
	bool persistent;
	bitmap_initialisation init;

	osspriteop_area *sprite_area;	/** Uncompressed data, or NULL */
	char *compressed;		/** Compressed data, or NULL */
	char filename[12];		/** Data filename, or '/0' */

	struct bitmap *previous;	/** Previous bitmap */
	struct bitmap *next;		/** Next bitmap */

};

struct bitmap *bitmap_create_file(char *file);
void bitmap_initialise_memory(void);
void bitmap_quit(void);
void bitmap_maintain(void);

/** Whether maintenance of the pool states is needed
*/
extern bool bitmap_maintenance;

/** Whether maintenance of the pool is high priority
*/
extern bool bitmap_maintenance_priority;

/** Maximum amount of memory for direct images
*/
extern unsigned int bitmap_direct_size;

/** Total size of compressed area
*/
extern unsigned int bitmap_compressed_size;


#endif
