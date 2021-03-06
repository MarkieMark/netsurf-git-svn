/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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

/** \file
 * Page thumbnail creation (implementation).
 *
 * Thumbnails are created by setting the current drawing contexts to the
 * bitmap (a gdk pixbuf) we are passed, and plotting the page at a small
 * scale.
 */

#include <assert.h>
#include <gtk/gtk.h>
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/plotters.h"
#include "desktop/browser.h"
#include "gtk/gtk_scaffolding.h"
#include "gtk/gtk_plotters.h"
#include "gtk/gtk_bitmap.h"
#include "image/bitmap.h"
#include "render/font.h"
#include "utils/log.h"
#include "utils/utils.h"

/**
 * Create a thumbnail of a page.
 *
 * \param  content  content structure to thumbnail
 * \param  bitmap   the bitmap to draw to
 * \param  url      the URL the thumnail belongs to, or NULL
 */
bool thumbnail_create(hlcache_handle *content, struct bitmap *bitmap,
		const char *url)
{
        GdkPixbuf *pixbuf;
	int cwidth, cheight;
	gint width;
	gint height;
	gint depth;
	GdkPixmap *pixmap;
	GdkPixbuf *big;

	assert(content);
	assert(bitmap);

	cwidth = min(content_get_width(content), 1024);
	cheight = min(content_get_height(content), 768);

	pixbuf = gtk_bitmap_get_primary(bitmap);
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	depth = (gdk_screen_get_system_visual(gdk_screen_get_default()))->depth;

	LOG(("Trying to create a thumbnail pixmap for a content of %dx%d@%d",
		content_get_width(content), content_get_height(content), 
		depth));

	pixmap = gdk_pixmap_new(NULL, cwidth, cwidth, depth);
	
	if (pixmap == NULL) {
		/* the creation failed for some reason: most likely because
		 * we've been asked to create with with at least one dimention
		 * as zero.  The RISC OS thumbnail generator returns false
		 * from here when it can't create a bitmap, so we assume it's
		 * safe to do so here too.
		 */
		return false;
	}

	gdk_drawable_set_colormap(pixmap, gdk_colormap_get_system());

	/* set the plotting functions up */
	plot = nsgtk_plotters;

	nsgtk_plot_set_scale((double) cwidth / 
			(double) content_get_width(content));

	/* set to plot to pixmap */
	current_drawable = pixmap;
	current_gc = gdk_gc_new(current_drawable);
#ifdef CAIRO_VERSION
	current_cr = gdk_cairo_create(current_drawable);
#endif
	plot.rectangle(0, 0, cwidth, cwidth, plot_style_fill_white);

	/* render the content */
	content_redraw(content, 0, 0, content_get_width(content), 
			content_get_width(content),
			0, 0, content_get_width(content), 
			content_get_width(content), 1.0, 0xFFFFFF);

	/* resample the large plot down to the size of our thumbnail */
	big = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL, 0, 0, 0, 0,
			cwidth, cwidth);

	gdk_pixbuf_scale(big, pixbuf, 0, 0, width, height, 0, 0,
			(double)width / (double)cwidth,
			(double)height / (double)cwidth,
			GDK_INTERP_TILES);

	/* As a debugging aid, try this to dump out a copy of the thumbnail as
	 * a PNG: gdk_pixbuf_save(pixbuf, "thumbnail.png", "png", NULL, NULL);
	 */

	/* register the thumbnail with the URL */
	if (url)
	  urldb_set_thumbnail(url, bitmap);

	bitmap_modified(bitmap);

	g_object_unref(current_gc);
#ifdef CAIRO_VERSION
	cairo_destroy(current_cr);
#endif
	g_object_unref(pixmap);
	g_object_unref(big);

	return true;
}
