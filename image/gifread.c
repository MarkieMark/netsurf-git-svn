/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */


#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gifread.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/utils/log.h"

/*	READING GIF FILES
	=================

	The functions provided by this file allow for efficient progressive GIF
	decoding. Whilst the initialisation does not ensure that there is
	sufficient image data to complete the entire frame, it does ensure that
	the information provided is valid. Any subsequent attempts to decode an
	initialised GIF are guaranteed to succeed, and any bytes of the image
	not present are assumed to be totally transparent.

	To begin decoding a GIF, the 'gif' structure must be initialised with
	the 'gif_data' and 'buffer_size' set to their initial values. The
	'buffer_position' should initially be 0, and will be internally updated
	as the decoding commences. The caller should then repeatedly call
	gif_initialise() with the structure until the function returns 1, or
	no more data is avaliable.

	Once the initialisation has begun, the decoder completes the variables
	'frame_count' and 'frame_count_partial'. The former being the total
	number of frames that have been successfully initialised, and the
	latter being the number of frames that a partial amount of data is
	available for. This assists the caller in managing the animation whilst
	decoding is continuing.

	To decode a frame, the caller must use gif_decode_frame() which updates
	the current 'frame_image' to reflect the desired frame. The required
	'background_action' is also updated to reflect how the frame should be
	plotted. The caller must not assume that the current 'frame_image' will
	be valid between calls if initialisation is still occuring, and should
	either always request that the frame is decoded (no processing will
	occur if the 'decoded_frame' has not been invalidated by initialisation)
	or perform the check itself.

	It should be noted that gif_finalise() should always be called, even if
	no frames were initialised.

	[rjw] - Fri 2nd April 2004
*/



/*	Internal GIF routines
*/
static int gif_initialise_sprite(struct gif_animation *gif, unsigned int width, unsigned int height);
static int gif_initialise_frame(struct gif_animation *gif);
static unsigned int gif_interlaced_line(int height, int y);



/*	Internal LZW routines
*/
static int gif_next_LZW(struct gif_animation *gif);
static int gif_next_code(struct gif_animation *gif, int code_size);
#define gif_read_LZW(gif) ((stack_pointer > stack) ? *--stack_pointer : gif_next_LZW(gif))

/*	General LZW values. They are shared for all GIFs being decoded, and
	thus we can't handle progressive decoding efficiently without having
	the data for each image which would use an extra 10Kb or so per GIF.
*/
static unsigned char buf[280];
static int maskTbl[16] = {0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f,
			  0x00ff, 0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff};
static int table[2][(1<< GIF_MAX_LZW)];
static int stack[(1 << GIF_MAX_LZW) * 2];
static int *stack_pointer;
static int code_size, set_code_size;
static int max_code, max_code_size;
static int clear_code, end_code;
static int curbit, lastbit, get_done, last_byte;
static int return_clear;
static bool zero_data_block = false;

/*	Whether to clear the decoded image rather than plot
*/
static bool clear_image = false;


/*	Initialises any workspace held by the animation and attempts to decode
	any information that hasn't already been decoded.
	If an error occurs, all previously decoded frames are retained.

	@return GIF_FRAME_DATA_ERROR for GIF frame data error
		GIF_INSUFFICIENT_FRAME_DATA for insufficient data to process
		          any more frames
		GIF_INSUFFICIENT_MEMORY for memory error
		GIF_DATA_ERROR for GIF error
		GIF_INSUFFICIENT_DATA for insufficient data to do anything
		0 for successful decoding
		1 for successful decoding (all frames completely read)
*/
int gif_initialise(struct gif_animation *gif) {
	unsigned char *gif_data;
	unsigned int index;
	int return_value;
	unsigned int frame;

	/*	Check for sufficient data to be a GIF
	*/
	if (gif->buffer_size < 13) return GIF_INSUFFICIENT_DATA;

	/*	Get our current processing position
	*/
	gif_data = gif->gif_data + gif->buffer_position;

	/*	See if we should initialise the GIF
	*/
	if (gif->buffer_position == 0) {

		/*	We want everything to be NULL before we start so we've no chance
			of freeing bad pointers (paranoia)
		*/
		gif->frame_image = NULL;
		gif->frames = NULL;
		gif->local_colour_table = NULL;
		gif->global_colour_table = NULL;

		/*	The caller may have been lazy and not reset any values
		*/
		gif->frame_count = 0;
		gif->frame_count_partial = 0;
		gif->decoded_frame = -1;

		/*	Check we are a GIF
		*/
		if (strncmp(gif_data, "GIF", 3) != 0) {
//			LOG(("Invalid GIF header - should be 'GIF'"));
			return GIF_DATA_ERROR;
		}
		gif_data += 3;

		/*	Check we are a GIF type 87a or 89a
		*/
		if ((strncmp(gif_data, "87a", 3) != 0) &&
				(strncmp(gif_data, "89a", 3) != 0)) {
//			LOG(("Unknown GIF format - proceeding anyway"));
		}
		gif_data += 3;

		/*	Get our GIF data.
		*/
		gif->width = gif_data[0] | (gif_data[1] << 8);
		gif->height = gif_data[2] | (gif_data[3] << 8);
		gif->global_colours = (gif_data[4] & 0x80);
		gif->colour_table_size = (2 << (gif_data[4] & 0x07));
		gif->background_colour = gif_data[5];
		gif->aspect_ratio = gif_data[6];
		gif->dirty_frame = -1;
		gif->loop_count = 1;
		gif_data += 7;

		/*	Some broken GIFs report the size as the screen size they were created in. As
			such, we detect for the common cases and set the sizes as 0 if they are found
			which results in the GIF being the maximum size of the frames.
		*/
		if (((gif->width == 640) && (gif->width == 480)) ||
				((gif->width == 640) && (gif->width == 512)) ||
				((gif->width == 800) && (gif->width == 600)) ||
				((gif->width == 1024) && (gif->width == 768)) ||
				((gif->width == 1280) && (gif->width == 1024)) ||
				((gif->width == 1600) && (gif->width == 1200))) {
			gif->width = 1;
			gif->height = 1;
		}

		/*	Allocate some data irrespective of whether we've got any colour tables. We
			always get the maximum size in case a GIF is lying to us. It's far better
			to give the wrong colours than to trample over some memory somewhere.
		*/
		gif->global_colour_table = (unsigned int *)calloc(GIF_MAX_COLOURS, sizeof(int));
		gif->local_colour_table = (unsigned int *)calloc(GIF_MAX_COLOURS, sizeof(int));
		if ((gif->global_colour_table == NULL) || (gif->local_colour_table == NULL)) {
			gif_finalise(gif);
			return GIF_INSUFFICIENT_MEMORY;
		}

		/*	Set the first colour to a value that will never occur in reality so we
			know if we've processed it
		*/
		gif->global_colour_table[0] = 0xaa000000;

		/*	Initialise enough workspace for 4 frame initially
		*/
		if ((gif->frames = (gif_frame *)malloc(sizeof(gif_frame))) == NULL) {
			gif_finalise(gif);
			return GIF_INSUFFICIENT_MEMORY;
		}
		gif->frame_holders = 1;

		/*	Initialise the sprite header
		*/
		if ((gif->frame_image = bitmap_create(gif->width, gif->height)) == NULL) {
			gif_finalise(gif);
			return GIF_INSUFFICIENT_MEMORY;
		}

		/*	Remember we've done this now
		*/
		gif->buffer_position = gif_data - gif->gif_data;
	}

	/*	Do the colour map if we haven't already. As the top byte is always 0xff or 0x00
		depending on the transparency we know if it's been filled in.
	*/
	if (gif->global_colour_table[0] == 0xaa000000) {
		/*	Check for a global colour map signified by bit 7
		*/
		if (gif->global_colours) {
			if (gif->buffer_size < (gif->colour_table_size * 3 + 12)) {
				return GIF_INSUFFICIENT_DATA;
			}
//			LOG(("Found global colour table with %i entries", gif->colour_table_size));
			for (index = 0; index < gif->colour_table_size; index++) {
				gif->global_colour_table[index] = gif_data[0] | (gif_data[1] << 8) |
					(gif_data[2] << 16) | 0xff000000;
				gif_data += 3;
			}
			gif->buffer_position = (gif_data - gif->gif_data);
		} else {
//			LOG(("No global colour table"));
			/*	Create a default colour table with the first two colours as black and white
			*/
			gif->global_colour_table[0] = 0xff000000;
			gif->global_colour_table[1] = 0xffffffff;
		}
	}

	/*	Repeatedly try to decode frames
	*/
	while ((return_value = gif_initialise_frame(gif)) == 0);

	/*	Update the redraw areas now we know the full data set
	*/
	if (gif->frame_count_partial > 0) {
		/*	We now work backwards to update the redraw characteristics of frames
			with clear codes to stop a snowball effect of the redraw areas. It doesn't
			really make much difference for most images, and will not work as well
			(ie will not optimise as well as for a single-pass call, but still works)
			for multiple calls to this routine when decoding progressively.
		*/
		for (frame = gif->frame_count_partial - 1; frame > 0; frame--) {
		  	if (gif->frames[frame - 1].redraw_required) {
				if (gif->frames[frame].redraw_x > gif->frames[frame - 1].redraw_x) {
					gif->frames[frame].redraw_width +=
							(gif->frames[frame].redraw_x - gif->frames[frame - 1].redraw_x);
					gif->frames[frame].redraw_x = gif->frames[frame - 1].redraw_x;
				}
				if (gif->frames[frame].redraw_y > gif->frames[frame - 1].redraw_y) {
					gif->frames[frame].redraw_height +=
							(gif->frames[frame].redraw_y - gif->frames[frame - 1].redraw_y);
					gif->frames[frame].redraw_y = gif->frames[frame - 1].redraw_y;
				}
				if ((gif->frames[frame - 1].redraw_x + gif->frames[frame - 1].redraw_width) >
						(gif->frames[frame].redraw_x + gif->frames[frame].redraw_width)) {
					gif->frames[frame].redraw_width =
						(gif->frames[frame - 1].redraw_x + gif->frames[frame - 1].redraw_width) -
						gif->frames[frame].redraw_x;
				}
				if ((gif->frames[frame - 1].redraw_y + gif->frames[frame - 1].redraw_height) >
					(gif->frames[frame].redraw_y + gif->frames[frame].redraw_height)) {
					gif->frames[frame].redraw_height =
						(gif->frames[frame - 1].redraw_y + gif->frames[frame - 1].redraw_height) -
						gif->frames[frame].redraw_y;
				}
			}
		}

	}

	/*	If there was a memory error tell the caller
	*/
	if ((return_value == GIF_INSUFFICIENT_MEMORY) ||
			(return_value == GIF_DATA_ERROR)) {
		return return_value;
	}

	/*	If we didn't have some frames then a GIF_INSUFFICIENT_DATA becomes a
		GIF_INSUFFICIENT_FRAME_DATA
	*/
	if ((return_value == GIF_INSUFFICIENT_DATA) && (gif->frame_count_partial > 0)) {
		return_value = GIF_INSUFFICIENT_FRAME_DATA;
	}

	/*	Return how many we got
	*/
	return return_value;
}



/**	Updates the sprite memory size

	@return -3 for a memory error
		0 for success
*/
static int gif_initialise_sprite(struct gif_animation *gif, unsigned int width, unsigned int height) {
	unsigned int max_width;
	unsigned int max_height;
	struct bitmap *buffer;

	/*	Check if we've changed
	*/
	if ((width <= gif->width) && (height <= gif->height)) return 0;

	/*	Get our maximum values
	*/
	max_width = (width > gif->width) ? width : gif->width;
	max_height = (height > gif->height) ? height : gif->height;

	/*	Allocate some more memory
	*/
	if ((buffer = bitmap_create(max_width, max_height)) == NULL) {
		return GIF_INSUFFICIENT_MEMORY;
	}
	bitmap_destroy(gif->frame_image);
	gif->frame_image = buffer;

	/*	Invalidate our currently decoded image
	*/
	gif->decoded_frame = -1;
	return 0;
}


/*	Attempts to initialise the next frame

	@return -4 for insufficient data to process the entire frame
		-3 for a memory error
		-2 for a data error
		-1 for insufficient data to process anything
		0 for success
		1 for success (GIF terminator found)
*/
int gif_initialise_frame(struct gif_animation *gif) {
	int frame;
	gif_frame *temp_buf;

	unsigned char *gif_data, *gif_end;
	int gif_bytes;
	unsigned int flags = 0;
	unsigned int background_action;
	unsigned int width, height, offset_x, offset_y;
	unsigned int extension_size, colour_table_size;
	unsigned int block_size;
	unsigned int more_images;
	unsigned int first_image;

	/*	Get the frame to decode and our data position
	*/
	frame = gif->frame_count;

	/*	Get our buffer position etc.
	*/
	gif_data = (unsigned char *)(gif->gif_data + gif->buffer_position);
	gif_end = (unsigned char *)(gif->gif_data + gif->buffer_size);
	gif_bytes = (gif_end - gif_data);

	/*	Check we have enough data for at least the header, or if we've finished
	*/
	if ((gif_bytes > 0) && (gif_data[0] == 0x3b)) return 1;
	if (gif_bytes < 11) return -1;

	/*	We could theoretically get some junk data that gives us millions of frames, so
		we ensure that we don't have a silly number
	*/
	if (frame > 4096) return GIF_DATA_ERROR;

	/*	Get some memory to store our pointers in etc.
	*/
	if ((int)gif->frame_holders <= frame) {
		/*	Allocate more memory
		*/
		if ((temp_buf = (gif_frame *)realloc(gif->frames,
					(frame + 1) * sizeof(gif_frame))) == NULL) {
			return GIF_INSUFFICIENT_MEMORY;
		}
		gif->frames = temp_buf;

		/*	Remember we allocated it
		*/
		gif->frame_holders = frame + 1;
	}

	/*	Store our frame pointer. We would do it when allocating except we
		start off with one frame allocated so we can always use realloc.
	*/
	gif->frames[frame].frame_pointer = gif->buffer_position;
	gif->frames[frame].virgin = true;
	gif->frames[frame].frame_delay = 100;
	gif->frames[frame].redraw_required = false;

	/*	Invalidate any previous decoding we have of this frame
	*/
	if (gif->decoded_frame == frame) gif->decoded_frame = -1;

	/*	We pretend to initialise the frames, but really we just skip over all
		the data contained within. This is all basically a cut down version of
		gif_decode_frame that doesn't have any of the LZW bits in it.
	*/
	more_images = 1;
	first_image = 1;
	while (more_images != 0) {

		/*	Ensure we have some data
		*/
		if ((gif_end - gif_data) < 10) return GIF_INSUFFICIENT_FRAME_DATA;

		/*	Decode the extensions
		*/
		background_action = 0;
		while (gif_data[0] == 0x21) {
			/*	Get the extension size
			*/
			extension_size = gif_data[2];

			/*	Check we've enough data for the extension then header
			*/
			if ((gif_end - gif_data) < (int)(extension_size + 13)) return GIF_INSUFFICIENT_FRAME_DATA;

			/*	Graphic control extension - store the frame delay.
			*/
			if (gif_data[1] == 0xf9) {
				gif->frames[frame].frame_delay = gif_data[4] | (gif_data[5] << 8);
				background_action = ((gif_data[3] & 0x1c) >> 2);
				more_images = ((gif->frames[frame].frame_delay) == 0);

			/*	Application extension - handle NETSCAPE2.0 looping
			*/
			} else if ((gif_data[1] == 0xff) &&
					(gif_data[2] == 0x0b) &&
					(strncmp(gif_data + 3, "NETSCAPE2.0", 11) == 0) &&
					(gif_data[14] == 0x03) &&
					(gif_data[15] == 0x01)) {
				gif->loop_count = gif_data[16] | (gif_data[17] << 8);
			}

			/*	Move to the first sub-block
			*/
			gif_data += 2;

			/*	Skip all the sub-blocks
			*/
			while (gif_data[0] != 0x00) {
				gif_data += gif_data[0] + 1;
				if ((gif_end - gif_data) < 10) return GIF_INSUFFICIENT_FRAME_DATA;
			}
			gif_data++;
		}

		/*	We must have at least one image descriptor
		*/
		if (gif_data[0] != 0x2c) return GIF_FRAME_DATA_ERROR;

		/*	Do some simple boundary checking
		*/
		offset_x = gif_data[1] | (gif_data[2] << 8);
		offset_y = gif_data[3] | (gif_data[4] << 8);
		width = gif_data[5] | (gif_data[6] << 8);
		height = gif_data[7] | (gif_data[8] << 8);

		/*	Set up the redraw characteristics. We have to check for extending the area
			due to multi-image frames.
		*/
		if (first_image == 0) {
			if (gif->frames[frame].redraw_x > offset_x) {
				gif->frames[frame].redraw_width += (gif->frames[frame].redraw_x - offset_x);
				gif->frames[frame].redraw_x = offset_x;
			}
			if (gif->frames[frame].redraw_y > offset_y) {
				gif->frames[frame].redraw_height += (gif->frames[frame].redraw_y - offset_y);
				gif->frames[frame].redraw_y = offset_y;
			}
			if ((offset_x + width) > (gif->frames[frame].redraw_x + gif->frames[frame].redraw_width)) {
				gif->frames[frame].redraw_width = (offset_x + width) - gif->frames[frame].redraw_x;
			}
			if ((offset_y + height) > (gif->frames[frame].redraw_y + gif->frames[frame].redraw_height)) {
				gif->frames[frame].redraw_height = (offset_y + height) - gif->frames[frame].redraw_y;
			}
		} else {
			first_image = 0;
			gif->frames[frame].redraw_x = offset_x;
			gif->frames[frame].redraw_y = offset_y;
			gif->frames[frame].redraw_width = width;
			gif->frames[frame].redraw_height = height;
		}

		/*	if we are clearing the background then we need to redraw enough to cover the previous
			frame too
		*/
		gif->frames[frame].redraw_required = ((background_action == 2) || (background_action == 3));

		/*	Boundary checking - shouldn't ever happen except with junk data
		*/
		if (gif_initialise_sprite(gif, (offset_x + width), (offset_y + height))) {
			return GIF_INSUFFICIENT_MEMORY;
		}

		/*	Decode the flags
		*/
		flags = gif_data[9];
		colour_table_size = 2 << (flags & 0x07);

		/*	Move our data onwards and remember we've got a bit of this frame
		*/
		gif_data += 10;
		gif_bytes = (gif_end - gif_data);
		gif->frame_count_partial = frame + 1;

		/*	Skip the local colour table
		*/
		if (flags & 0x80) {
			gif_data += 3 * colour_table_size;
			if ((gif_bytes = (gif_end - gif_data)) < 0) return GIF_INSUFFICIENT_FRAME_DATA;
		}

		/*	Ensure we have a correct code size
		*/
		if (gif_data[0] > GIF_MAX_LZW) return GIF_DATA_ERROR;

		/*	Move our data onwards
		*/
		gif_data++;
		if (--gif_bytes < 0) return GIF_INSUFFICIENT_FRAME_DATA;

		/*	Repeatedly skip blocks until we get a zero block or run out of data
		*/
		block_size = 0;
		while (block_size != 1) {
			/*	Skip the block data
			*/
			block_size = gif_data[0] + 1;
			if ((gif_bytes -= block_size) < 0) return GIF_INSUFFICIENT_FRAME_DATA;
			gif_data += block_size;
		}

		/*	Check for end of data
		*/
		if ((gif_bytes < 1) || (gif_data[0] == 0x3b)) more_images = 0;
	}

	/*	Check if we've finished
	*/
	if (gif_bytes < 1) {
		return GIF_INSUFFICIENT_FRAME_DATA;
	} else {
		gif->buffer_position = gif_data - gif->gif_data;
		gif->frame_count = frame + 1;
		if (gif_data[0] == 0x3b) return 1;
	}
	return 0;
}


/**	Decodes a GIF frame.

	@return GIF_FRAME_DATA_ERROR for GIF frame data error
		GIF_INSUFFICIENT_FRAME_DATA for insufficient data to complete the frame
		GIF_DATA_ERROR for GIF error (invalid frame header)
		GIF_INSUFFICIENT_DATA for insufficient data to do anything
		0 for successful decoding
*/
int gif_decode_frame(struct gif_animation *gif, unsigned int frame) {
	unsigned int index = 0;
	unsigned char *gif_data;
	unsigned char *gif_end;
	int gif_bytes;
	unsigned int width, height, offset_x, offset_y;
	unsigned int flags, colour_table_size, interlace;
	unsigned int *colour_table;
	unsigned int *frame_data = 0;	// Set to 0 for no warnings
	unsigned int *frame_scanline;
	unsigned int extension_size;
	unsigned int background_action;
	int transparency_index = -1;
	unsigned int save_buffer_position;
	unsigned int return_value = 0;
	unsigned int x, y, decode_y, burst_bytes;
	unsigned int block_size;
	register int colour;
	unsigned int more_images;

	/*	Ensure we have a frame to decode
	*/
	if (frame > gif->frame_count_partial) return GIF_INSUFFICIENT_DATA;
	if ((!clear_image) && ((int)frame == gif->decoded_frame)) return 0;

	/*	If the previous frame was dirty, remove it
	*/
	if (!clear_image) {
	  	if (frame == 0)
	  		gif->dirty_frame = -1;
		if (gif->decoded_frame == gif->dirty_frame) {
			clear_image = true;
			if (frame != 0) gif_decode_frame(gif, gif->dirty_frame);
			clear_image = false;
		}
		gif->dirty_frame = -1;
	}

	/*	Get the start of our frame data and the end of the GIF data
	*/
	gif_data = gif->gif_data + gif->frames[frame].frame_pointer;
	gif_end = gif->gif_data + gif->buffer_size;
	gif_bytes = (gif_end - gif_data);

	/*	Check we have enough data for the header
	*/
	if (gif_bytes < 9) return GIF_INSUFFICIENT_DATA;

	/*	Clear the previous frame totally. We can't just pretend we've got a smaller
		sprite and clear what we need as some frames have multiple images which would
		produce errors.
	*/
	frame_data = (unsigned int *)bitmap_get_buffer(gif->frame_image);
	if (!clear_image) {
		if ((frame == 0) || (gif->decoded_frame == -1))
			memset((char*)frame_data, 0x00, gif->width * gif->height * sizeof(int));
		gif->decoded_frame = frame;
	}

	/*	Save the buffer position
	*/
	save_buffer_position = gif->buffer_position;
	gif->buffer_position = gif_data - gif->gif_data;

	/*	We've got to do this more than one time if we've got multiple images
	*/
	more_images = 1;
	while (more_images != 0) {
		background_action = 0;

		/*	Ensure we have some data
		*/
		gif_data = gif->gif_data + gif->buffer_position;
		if ((gif_end - gif_data) < 10) {
			return_value = GIF_INSUFFICIENT_FRAME_DATA;
			break;
		}

		/*	Decode the extensions
		*/
		while (gif_data[0] == 0x21) {

			/*	Get the extension size
			*/
			extension_size = gif_data[2];

			/*	Check we've enough data for the extension then header
			*/
			if ((gif_end - gif_data) < (int)(extension_size + 13)) {
				return_value = GIF_INSUFFICIENT_FRAME_DATA;
				break;
			}

			/*	Graphic control extension - store the frame delay.
			*/
			if (gif_data[1] == 0xf9) {
				flags = gif_data[3];
				if (flags & 0x01) transparency_index = gif_data[6];
				background_action = ((flags & 0x1c) >> 2);
				more_images = ((gif_data[4] | (gif_data[5] << 8)) == 0);
			}
			/*	Move to the first sub-block
			*/
			gif_data += 2;

			/*	Skip all the sub-blocks
			*/
			while (gif_data[0] != 0x00) {
				gif_data += gif_data[0] + 1;
				if ((gif_end - gif_data) < 10) {
					return_value = GIF_INSUFFICIENT_FRAME_DATA;
					break;
				}
			}
			gif_data++;
		}

		/*	Decode the header
		*/
		if (gif_data[0] != 0x2c) {
			return_value = GIF_DATA_ERROR;
			break;
		}
		offset_x = gif_data[1] | (gif_data[2] << 8);
		offset_y = gif_data[3] | (gif_data[4] << 8);
		width = gif_data[5] | (gif_data[6] << 8);
		height = gif_data[7] | (gif_data[8] << 8);

		/*	Boundary checking - shouldn't ever happen except unless the data has been
			modified since initialisation.
		*/
		if ((offset_x + width > gif->width) || (offset_y + height > gif->height)) {
			return_value = GIF_DATA_ERROR;
			break;
		}

		/*	Decode the flags
		*/
		flags = gif_data[9];
		colour_table_size = 2 << (flags & 0x07);
		interlace = flags & 0x40;

		/*	Move through our data
		*/
		gif_data += 10;
		gif_bytes = (int)(gif_end - gif_data);

		/*	Set up the colour table
		*/
		if (flags & 0x80) {
			if (gif_bytes < (int)(3 * colour_table_size)) {
				return_value = GIF_INSUFFICIENT_FRAME_DATA;
				break;
			}
			colour_table = gif->local_colour_table;
			if (!clear_image) {
				for (index = 0; index < colour_table_size; index++) {
					colour_table[index] = gif_data[0] | (gif_data[1] << 8) |
						(gif_data[2] << 16) | 0xff000000;
					gif_data += 3;
				}
			} else {
				gif_data += 3 * colour_table_size;
			}
			gif_bytes = (int)(gif_end - gif_data);
		} else {
			colour_table = gif->global_colour_table;
		}

		/*	If we are clearing the image we just clear, if not decode
		*/
		if (!clear_image) {
			/*	Set our dirty status
			*/
			if ((background_action == 2) || (background_action == 3)) {
				gif->dirty_frame = frame;
//				LOG(("Dirty frame %i", gif->dirty_frame));
			}

			/*	Initialise the LZW decoding
			*/
			set_code_size = gif_data[0];
			gif->buffer_position = (gif_data - gif->gif_data) + 1;

			/*	Set our code variables
			*/
			code_size = set_code_size + 1;
			clear_code = (1 << set_code_size);
			end_code = clear_code + 1;
			max_code_size = clear_code << 1;
			max_code = clear_code + 2;
			curbit = lastbit = 0;
			last_byte = 2;
			get_done = 0;
			return_clear = 1;
			stack_pointer = stack;

			/*	Decompress the data
			*/
			for (y = 0; y < height; y++) {
				if (interlace) {
					decode_y = gif_interlaced_line(height, y) + offset_y;
				} else {
					decode_y = y + offset_y;
				}
				frame_scanline = frame_data + offset_x + (decode_y * gif->width);

				/*	Rather than decoding pixel by pixel, we try to burst out streams
					of data to remove the need for end-of data checks every pixel.
				*/
				x = width;
				while (x-- > 0) {
					/*	Do the first pixel to get the stack of some sort
					*/
					if ((colour = gif_read_LZW(gif)) >= 0) {
						if (colour != transparency_index) {
							*frame_scanline = colour_table[colour];
						}
						frame_scanline++;
					} else {
						return_value = GIF_INSUFFICIENT_FRAME_DATA;
						goto gif_decode_frame_exit;
					}

					/*	Try to burst some bytes out
					*/
					burst_bytes = (stack_pointer - stack);
					if (burst_bytes > x) burst_bytes = x;
					x -= burst_bytes;
					while (burst_bytes-- > 0) {
						if ((colour = *--stack_pointer) != transparency_index) {
							  *frame_scanline = colour_table[colour];
						}
						frame_scanline++;
					}
				}
			}
		} else {
			/*	Clear our frame
			*/
			if ((background_action == 2) || (background_action == 3)) {
				for (y = 0; y < height; y++) {
					frame_scanline = frame_data + offset_x + ((offset_y + y) * gif->width);
					memset(frame_scanline, 0x00, width * 4);
				}
			}

			/*	Repeatedly skip blocks until we get a zero block or run out of data
			*/
			gif_bytes = gif->buffer_size - gif->buffer_position;
			gif_data = gif->gif_data + gif->buffer_size;
			block_size = 0;
			while (block_size != 1) {
				/*	Skip the block data
				*/
				block_size = gif_data[0] + 1;
				if ((gif_bytes -= block_size) < 0) {
					return_value = GIF_INSUFFICIENT_FRAME_DATA;
					goto gif_decode_frame_exit;
				}
				gif_data += block_size;
			}
		}
gif_decode_frame_exit:

		/*	Check for end of data
		*/
		gif_bytes = gif->buffer_size - gif->buffer_position;
		if ((gif_bytes < 1) || (gif_data[0] == 0x3b)) more_images = 0;
		gif->buffer_position++;
	}

	/*	Check if we should test for optimisation
	*/
	if (gif->frames[frame].virgin) {
		gif->frames[frame].opaque = bitmap_test_opaque(gif->frame_image);
		gif->frames[frame].virgin = false;
	}
	bitmap_set_opaque(gif->frame_image, gif->frames[frame].opaque);

	/*	Restore the buffer position
	*/
	gif->buffer_position = save_buffer_position;

	/*	Success!
	*/
	return return_value;

}

static unsigned int gif_interlaced_line(int height, int y) {
	if ((y << 3) < height) return (y << 3);
	y -= ((height + 7) >> 3);
	if ((y << 3) < (height - 4)) return (y << 3) + 4;
	y -= ((height + 3) >> 3);
	if ((y << 2) < (height - 2)) return (y << 2) + 2;
	y -= ((height + 1) >> 2);
	return (y << 1) + 1;
}

/*	Releases any workspace held by the animation
*/
void gif_finalise(struct gif_animation *gif) {
	/*	Release all our memory blocks
	*/
	free(gif->frame_image);
	gif->frame_image = NULL;
	free(gif->frames);
	gif->frames = NULL;
	free(gif->local_colour_table);
	gif->local_colour_table = NULL;
	free(gif->global_colour_table);
	gif->global_colour_table = NULL;
}


static int gif_next_LZW(struct gif_animation *gif) {
	static int firstcode, oldcode;
	int code, incode;
	unsigned int i, block_size;

	while ((code = gif_next_code(gif, code_size)) >= 0) {
		if (code == clear_code) {
			/*	Check we have a valid clear code
			*/
			if (clear_code >= (1 << GIF_MAX_LZW)) return -2;

			/*	Initialise our table
			*/
			memset(table, 0x00, (1 << GIF_MAX_LZW) * 8);
			for (i = 0; i < (unsigned int)clear_code; ++i) {
				table[1][i] = i;
			}

			/*	Update our LZW parameters
			*/
			code_size = set_code_size + 1;
			max_code_size = clear_code << 1;
			max_code = clear_code + 2;
			stack_pointer = stack;
			do {
				firstcode = oldcode = gif_next_code(gif, code_size);
			} while (firstcode == clear_code);
			return firstcode;
		}

		if (code == end_code) {
			/*	Skip to the end of our data so multi-image GIFs work
			*/
			if (zero_data_block) return -2;
			block_size = 0;
			while (block_size != 1) {
				block_size = gif->gif_data[gif->buffer_position] + 1;
				gif->buffer_position += block_size;
			}
			return -2;
		}

		/*	Fill the stack with some data
		*/
		incode = code;

		if (code >= max_code) {
			*stack_pointer++ = firstcode;
			code = oldcode;
		}

		while (code >= clear_code) {
			*stack_pointer++ = table[1][code];
			if (code == table[0][code]) return(code);
			if (((char *)stack_pointer - (char *)stack) >= (int)sizeof(stack)) return(code);
			code = table[0][code];
		}

		*stack_pointer++ = firstcode = table[1][code];

		if ((code = max_code) < (1 << GIF_MAX_LZW)) {
			table[0][code] = oldcode;
			table[1][code] = firstcode;
			++max_code;
			if ((max_code >= max_code_size) && (max_code_size < (1 << GIF_MAX_LZW))) {
				max_code_size = max_code_size << 1;
				++code_size;
			}
		}

		oldcode = incode;

		if (stack_pointer > stack) return *--stack_pointer;
	}
	return code;
}


static int gif_next_code(struct gif_animation *gif, int code_size) {
	int i, j, end, count;
	long ret;
	unsigned char *gif_data;

	if (return_clear) {
		return_clear = 0;
		return clear_code;
	}

	end = curbit + code_size;
	if (end >= lastbit) {
		if (get_done) return -1;
		buf[0] = buf[last_byte - 2];
		buf[1] = buf[last_byte - 1];

		/*	Get the next block
		*/
		gif_data = gif->gif_data + gif->buffer_position;
		zero_data_block = ((count = gif_data[0]) == 0);
		if ((gif->buffer_position + count) >= gif->buffer_size) return -1;
		if (count == 0) {
			get_done = 1;
		} else {
			memcpy(&buf[2], gif_data + 1, count);
		}
		gif->buffer_position += count + 1;

		/*	Update our variables
		*/
		last_byte = 2 + count;
		curbit = (curbit - lastbit) + 16;
		lastbit = (2 + count) << 3;
		end = curbit + code_size;
	}

	j = end >> 3;
	i = curbit >> 3;
	if (i == j) {
		ret = (long)buf[i];
	} else if (i + 1 == j) {
		ret = (long)buf[i] | ((long)buf[i+1] << 8);
	} else {
		ret = (long)buf[i] | ((long)buf[i+1] << 8) | ((long)buf[i+2] << 16);
	}

	ret = (ret >> (curbit % 8)) & maskTbl[code_size];
	curbit += code_size;
	return (int)ret;
}
