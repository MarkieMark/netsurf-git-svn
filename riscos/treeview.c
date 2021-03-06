/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
 * Generic tree handling (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <swis.h>
#include <time.h>
#include "oslib/colourtrans.h"
#include "oslib/dragasprite.h"
#include "oslib/osbyte.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"
#include "desktop/tree.h"
#include "riscos/bitmap.h"
#include "riscos/dialog.h"
#include "riscos/gui.h"
#include "riscos/image.h"
#include "riscos/menus.h"
#include "riscos/theme.h"
#include "riscos/tinct.h"
#include "riscos/textarea.h"
#include "riscos/treeview.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#ifndef wimp_KEY_END
#define wimp_KEY_END wimp_KEY_COPY
#endif

#define TREE_EXPAND 0
#define TREE_COLLAPSE 1


static bool ro_gui_tree_initialise_sprite(const char *name, int number);
static void ro_gui_tree_launch_selected_node(struct tree *tree, struct node *node, bool all);
static bool ro_gui_tree_launch_node(struct tree *tree, struct node *node);
static void tree_handle_node_changed_callback(void *p);


/* an array of sprite addresses for Tinct */
static char *ro_gui_tree_sprites[2];

/* origin adjustment */
static int ro_gui_tree_origin_x;
static int ro_gui_tree_origin_y;

/* element drawing */
static wimp_icon ro_gui_tree_icon;
static char ro_gui_tree_icon_validation[24];
static char ro_gui_tree_icon_null[] = "";

/* dragging information */
static struct tree *ro_gui_tree_current_drag_tree;
static wimp_mouse_state ro_gui_tree_current_drag_buttons;

/* editing information */
static wimp_icon_create ro_gui_tree_edit_icon;

/* dragging information */
static char ro_gui_tree_drag_name[12];

/* callback update */
struct node_update {
	struct tree *tree;
	struct node *node;
};


/**
 * Performs any initialisation for tree rendering
 */
bool ro_gui_tree_initialise(void)
{
	if (ro_gui_tree_initialise_sprite("expand", TREE_EXPAND) ||
			ro_gui_tree_initialise_sprite("collapse", TREE_COLLAPSE))
		return false;

	ro_gui_tree_edit_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
			wimp_ICON_VCENTRED | wimp_ICON_FILLED | wimp_ICON_BORDER |
			(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT) |
			(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			(wimp_BUTTON_WRITABLE << wimp_ICON_BUTTON_TYPE_SHIFT);
	ro_gui_tree_edit_icon.icon.data.indirected_text.validation =
			ro_gui_tree_icon_null;
	ro_gui_tree_edit_icon.icon.data.indirected_text.size = 256;

	return true;
}


/**
 * Initialise a sprite for use with Tinct
 *
 * \param  name	   the name of the sprite
 * \param  number  the sprite cache number
 * \return whether an error occurred during initialisation
 */
bool ro_gui_tree_initialise_sprite(const char *name, int number)
{
	char icon_name[12];
	const char *icon = icon_name;
	os_error *error;

	snprintf(icon_name, sizeof(icon_name), "tr_%s", name);

	error = xosspriteop_select_sprite(osspriteop_USER_AREA, gui_sprites,
			(osspriteop_id) icon,
			(osspriteop_header **) &ro_gui_tree_sprites[number]);
	if (error) {
		warn_user("MiscError", error->errmess);
		LOG(("Failed to find sprite 'tr_%s'", name));
		return true;
	}

	return false;
}


/**
 * Informs the current window manager that an area requires updating.
 *
 * \param tree	  the tree that is requesting a redraw
 * \param x	  the x co-ordinate of the redraw area
 * \param y	  the y co-ordinate of the redraw area
 * \param width	  the width of the redraw area
 * \param height  the height of the redraw area
 */
void tree_redraw_area(struct tree *tree, int x, int y, int width, int height)
{
	os_error *error;

	assert(tree);
	assert(tree->handle);

	if (tree->toolbar)
		y += ro_gui_theme_toolbar_height(tree->toolbar);
	error = xwimp_force_redraw((wimp_w)tree->handle, tree->offset_x + x - 2,
			-tree->offset_y - y - height, tree->offset_x + x + width + 4,
			-tree->offset_y - y);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Draws a line.
 *
 * \param tree	 the tree to draw a line for
 * \param x	 the x co-ordinate
 * \param x	 the y co-ordinate
 * \param x	 the width of the line
 * \param x	 the height of the line
 */
void tree_draw_line(int x, int y, int width, int height)
{
	os_error *error;
	int y0, y1;

	/* stop the 16-bit co-ordinate system from causing redraw errors */
	y1 = ro_gui_tree_origin_y - y;
	if (y1 < 0)
		return;
	y0 = y1 - height;
	if (y0 > 16384)
		return;
	if (y0 < 0)
		y0 = 0;
	if (y1 > 16384)
		y1 = 16384;

	error = xcolourtrans_set_gcol((os_colour)0x88888800, 0, os_ACTION_OVERWRITE,
			0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		return;
	}
	error = xos_plot(os_MOVE_TO, ro_gui_tree_origin_x + x, y0);
	if (!error)
		xos_plot(os_PLOT_TO, ro_gui_tree_origin_x + x + width, y1);
	if (error) {
		LOG(("xos_plot: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		return;
	}
}


/**
 * Draws an element, including any expansion icons
 *
 * \param tree	   the tree to draw an element for
 * \param element  the element to draw
 */
void tree_draw_node_element(struct tree *tree, struct node_element *element)
{
	os_error *error;
	int toolbar_height = 0;
	struct node_element *url_element;
	const struct bitmap *bitmap = NULL;
	struct node_update *update;
	const uint8_t *frame;
	rufl_code code;
	int x0, y0, x1, y1;
	bool selected = false;
	colour bg, c;

	assert(tree);
	assert(element);
	assert(element->parent);

	if (tree->toolbar)
		toolbar_height = ro_gui_theme_toolbar_height(tree->toolbar);

	x0 = ro_gui_tree_origin_x + element->box.x;
	x1 = x0 + element->box.width;
	y1 = ro_gui_tree_origin_y - element->box.y;
	y0 = y1 - element->box.height;
	if (&element->parent->data == element)
		if (element->parent->selected)
			selected = true;


	switch (element->type) {
	case NODE_ELEMENT_TEXT_PLUS_SPRITE:
		assert(element->sprite);

		ro_gui_tree_icon.flags = wimp_ICON_INDIRECTED | wimp_ICON_VCENTRED;
		if (selected)
			ro_gui_tree_icon.flags |= wimp_ICON_SELECTED;
		ro_gui_tree_icon.extent.x0 = tree->offset_x + element->box.x;
		ro_gui_tree_icon.extent.y1 = -tree->offset_y - element->box.y -
				toolbar_height;
		ro_gui_tree_icon.extent.x1 = ro_gui_tree_icon.extent.x0 +
				NODE_INSTEP;
		ro_gui_tree_icon.extent.y0 = -tree->offset_y - element->box.y -
				element->box.height - toolbar_height;
		ro_gui_tree_icon.flags |= wimp_ICON_TEXT | wimp_ICON_SPRITE;
		ro_gui_tree_icon.data.indirected_text_and_sprite.text =
				ro_gui_tree_icon_null;
		ro_gui_tree_icon.data.indirected_text_and_sprite.validation =
				ro_gui_tree_icon_validation;
		ro_gui_tree_icon.data.indirected_text_and_sprite.size = 1;
		if (element->parent->expanded) {
			sprintf(ro_gui_tree_icon_validation, "S%s",
					element->sprite->expanded_name);
		} else {
			sprintf(ro_gui_tree_icon_validation, "S%s",
					element->sprite->name);
		}
		error = xwimp_plot_icon(&ro_gui_tree_icon);
		if (error) {
			LOG(("xwimp_plot_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		x0 += NODE_INSTEP;

		/* fall through */
	case NODE_ELEMENT_TEXT:
		assert(element->text);

		if (element == tree->editing)
			return;

		if (ro_gui_tree_icon.flags & wimp_ICON_SELECTED)
			ro_gui_tree_icon.flags |= wimp_ICON_FILLED;
		if (selected) {
			error = xcolourtrans_set_gcol((os_colour)0x00000000, 0,
					os_ACTION_OVERWRITE, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_gcol: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
				return;
			}
			error = xos_plot(os_MOVE_TO, x0, y0);
			if (!error)
				error = xos_plot(os_PLOT_RECTANGLE | os_PLOT_TO, x1-1, y1-1);
			if (error) {
				LOG(("xos_plot: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
				return;
			}
			bg = 0x0000000;
			c = 0x00eeeeee;
		} else {
			bg = 0x00ffffff;
			c = 0x00000000;
		}
		error = xcolourtrans_set_font_colours(font_CURRENT,
				bg << 8, c << 8, 14, 0, 0, 0);
		if (error) {
			LOG(("xcolourtrans_set_font_colours: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}
		code = rufl_paint(ro_gui_desktop_font_family, ro_gui_desktop_font_style,
				ro_gui_desktop_font_size,
				element->text, strlen(element->text),
				x0 + 8, y0 + 10,
				rufl_BLEND_FONT);
		if (code != rufl_OK) {
			if (code == rufl_FONT_MANAGER_ERROR)
				LOG(("rufl_paint: rufl_FONT_MANAGER_ERROR: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
			else
				LOG(("rufl_paint: 0x%x", code));
		}
		break;
	case NODE_ELEMENT_THUMBNAIL:
		url_element = tree_find_element(element->parent, TREE_ELEMENT_URL);
		if (url_element)
			bitmap = urldb_get_thumbnail(url_element->text);
		if (bitmap) {
			frame = bitmap_get_buffer((struct bitmap *) bitmap);
			if (!frame)
				urldb_set_thumbnail(url_element->text, NULL);
			if ((!frame) || (element->box.width == 0)) {
				update = calloc(sizeof(struct node_update), 1);
				if (!update)
					return;
				update->tree = tree;
				update->node = element->parent;
				schedule(0, tree_handle_node_changed_callback,
						update);
				return;
			}
			image_redraw(bitmap->sprite_area,
					ro_gui_tree_origin_x + element->box.x + 2,
					ro_gui_tree_origin_y - element->box.y,
					bitmap->width, bitmap->height,
					bitmap->width, bitmap->height,
					0xffffff,
					false, false, false,
					IMAGE_PLOT_TINCT_OPAQUE);
			if (!tree->no_furniture) {
				tree_draw_line(element->box.x,
						element->box.y,
						element->box.width - 1,
						0);
				tree_draw_line(element->box.x,
						element->box.y,
						0,
						element->box.height - 3);
				tree_draw_line(element->box.x,
						element->box.y + element->box.height - 3,
						element->box.width - 1,
						0);
				tree_draw_line(element->box.x + element->box.width - 1,
						element->box.y,
						0,
						element->box.height - 3);
			}
		}
		break;
	}
}


void tree_handle_node_changed_callback(void *p)
{
	struct node_update *update = p;

	tree_handle_node_changed(update->tree, update->node, true, false);
	free(update);
}


/**
 * Draws an elements expansion icon
 *
 * \param tree	   the tree to draw the expansion for
 * \param element  the element to draw the expansion for
 */
void tree_draw_node_expansion(struct tree *tree, struct node *node)
{
	unsigned int type;

	assert(tree);
	assert(node);

	if ((node->child) || (node->data.next)) {
		if (node->expanded) {
			type = TREE_COLLAPSE;
		} else {
			type = TREE_EXPAND;
		}
		_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
				ro_gui_tree_sprites[type],
				ro_gui_tree_origin_x + node->box.x -
					(NODE_INSTEP / 2) - 8,
				ro_gui_tree_origin_y - node->box.y -
					(TREE_TEXT_HEIGHT / 2) - 8,
				tinct_BILINEAR_FILTER);

	}
}


/**
 * Sets the origin variables to the correct values for a specified tree
 *
 * \param tree  the tree to set the origin for
 */
void tree_initialise_redraw(struct tree *tree)
{
	os_error *error;
	wimp_window_state state;

	assert(tree->handle);

	state.w = (wimp_w)tree->handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	ro_gui_tree_origin_x = state.visible.x0 - state.xscroll + tree->offset_x;
	ro_gui_tree_origin_y = state.visible.y1 - state.yscroll - tree->offset_y;
	if (tree->toolbar)
		ro_gui_tree_origin_y -= ro_gui_theme_toolbar_height(tree->toolbar);
}


/**
 * Recalculates the dimensions of a node element.
 *
 * \param element  the element to recalculate
 */
void tree_recalculate_node_element(struct node_element *element)
{
	const struct bitmap *bitmap = NULL;
	struct node_element *url_element;
	rufl_code code;

	assert(element);

	switch (element->type) {
		case NODE_ELEMENT_TEXT_PLUS_SPRITE:
			assert(element->sprite);
		case NODE_ELEMENT_TEXT:
			assert(element->text);
			code = rufl_width(ro_gui_desktop_font_family, ro_gui_desktop_font_style,
					ro_gui_desktop_font_size,
					element->text, strlen(element->text),
					&element->box.width);
			if (code != rufl_OK) {
				if (code == rufl_FONT_MANAGER_ERROR)
					LOG(("rufl_width: rufl_FONT_MANAGER_ERROR: 0x%x: %s",
						rufl_fm_error->errnum,
						rufl_fm_error->errmess));
				else
					LOG(("rufl_width: 0x%x", code));
			}
			element->box.width += 16;
			element->box.height = TREE_TEXT_HEIGHT;
			if (element->type == NODE_ELEMENT_TEXT_PLUS_SPRITE)
				element->box.width += NODE_INSTEP;
			break;
		case NODE_ELEMENT_THUMBNAIL:
			url_element = tree_find_element(element->parent, TREE_ELEMENT_URL);
			if (url_element)
				bitmap = urldb_get_thumbnail(url_element->text);
			if (bitmap) {
/*				if ((bitmap->width == 0) && (bitmap->height == 0))
					frame = bitmap_get_buffer(bitmap);
				element->box.width = bitmap->width * 2 + 2;
				element->box.height = bitmap->height * 2 + 4;
*/				element->box.width = THUMBNAIL_WIDTH * 2 + 2;
				element->box.height = THUMBNAIL_HEIGHT * 2 + 4;
			} else {
				element->box.width = 0;
				element->box.height = 0;
			}
	}
}


/**
 * Sets a node element as having a specific sprite.
 *
 * \param node	    the node to update
 * \param sprite    the sprite to use
 * \param selected  the expanded sprite name to use
 */
void tree_set_node_sprite(struct node *node, const char *sprite,
		const char *expanded)
{
	assert(node);
	assert(sprite);
	assert(expanded);

	node->data.sprite = calloc(sizeof(struct node_sprite), 1);
	if (!node->data.sprite) return;
	node->data.type = NODE_ELEMENT_TEXT_PLUS_SPRITE;
	node->data.sprite->area = (osspriteop_area *)1;
	sprintf(node->data.sprite->name, sprite);
	sprintf(node->data.sprite->expanded_name, expanded);
}


/**
 * Sets a node element as having a folder sprite
 *
 * \param node  the node to update
 */
void tree_set_node_sprite_folder(struct node *node)
{
	assert(node->folder);

	tree_set_node_sprite(node, "small_dir", "small_diro");
}


/**
 * Updates the node details for a URL node.
 * The internal node dimensions are not updated.
 *
 * \param node  the node to update
 * \param url	the URL
 * \param data  the data the node is linked to, or NULL for unlinked data
 */
void tree_update_URL_node(struct node *node,
		const char *url, const struct url_data *data)
{
	struct node_element *element;
	char buffer[256];

	assert(node);

	element = tree_find_element(node, TREE_ELEMENT_URL);
	if (!element)
		return;
	if (data) {
		/* node is linked, update */
		assert(!node->editable);
		if (!data->title)
			urldb_set_url_title(url, url);

		if (!data->title)
			return;

		node->data.text = data->title;
	} else {
		/* node is not linked, find data */
		assert(node->editable);
		data = urldb_get_url_data(element->text);
		if (!data)
			return;
	}

	if (element) {
		sprintf(buffer, "small_%.3x", ro_content_filetype_from_type(data->type));
		if (ro_gui_wimp_sprite_exists(buffer))
			tree_set_node_sprite(node, buffer, buffer);
		else
			tree_set_node_sprite(node, "small_xxx", "small_xxx");
	}

	element = tree_find_element(node, TREE_ELEMENT_LAST_VISIT);
	if (element) {
		snprintf(buffer, 256, messages_get("TreeLast"),
				(data->last_visit > 0) ?
					ctime((time_t *)&data->last_visit) :
					messages_get("TreeUnknown"));
		if (data->last_visit > 0)
			buffer[strlen(buffer) - 1] = '\0';
		free((void *)element->text);
		element->text = strdup(buffer);
	}

	element = tree_find_element(node, TREE_ELEMENT_VISITS);
	if (element) {
		snprintf(buffer, 256, messages_get("TreeVisits"),
				data->visits);
		free((void *)element->text);
		element->text = strdup(buffer);
	}
}


/**
 * Updates the tree owner following a tree resize
 *
 * \param tree  the tree to update the owner of
 */
void tree_resized(struct tree *tree)
{
	os_error *error;
	wimp_window_state state;

	assert(tree->handle);


	state.w = (wimp_w)tree->handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if (state.flags & wimp_WINDOW_OPEN)
		ro_gui_tree_open(PTR_WIMP_OPEN(&state));
}


/**
 * Redraws a tree window
 *
 * \param redraw  the area to redraw
 * \param tree	  the tree to redraw
 */
void ro_gui_tree_redraw(wimp_draw *redraw)
{
	struct tree *tree;
	osbool more;
	int clip_x0, clip_x1, clip_y0, clip_y1, origin_x, origin_y;

	tree = (struct tree *)ro_gui_wimp_event_get_user_data(redraw->w);

	assert(tree);

	more = wimp_redraw_window(redraw);
	while (more) {
		clip_x0 = redraw->clip.x0;
		clip_y0 = redraw->clip.y0;
		clip_x1 = redraw->clip.x1;
		clip_y1 = redraw->clip.y1;
		origin_x = redraw->box.x0 - redraw->xscroll;
		origin_y = redraw->box.y1 - redraw->yscroll;
		if (tree->toolbar)
			origin_y -= ro_gui_theme_toolbar_height(tree->toolbar);
		tree_draw(tree, clip_x0 - origin_x - tree->offset_x,
				origin_y - clip_y1 - tree->offset_y,
				clip_x1 - clip_x0, clip_y1 - clip_y0);
		more = wimp_get_rectangle(redraw);
	}
}


/**
 * Handles a mouse click for a tree
 *
 * \param pointer  the pointer state
 * \param tree	   the tree to handle a click for
 * \return whether the click was handled
 */
bool ro_gui_tree_click(wimp_pointer *pointer, struct tree *tree)
{
	bool furniture;
	struct node *node;
	struct node *last;
	struct node_element *element;
	int x, y;
	int alt_pressed = 0;
	wimp_window_state state;
	wimp_caret caret;
	wimp_drag drag;
	wimp_auto_scroll_info scroll;
	os_error *error;
	os_box box = { pointer->pos.x - 34, pointer->pos.y - 34,
			pointer->pos.x + 34, pointer->pos.y + 34 };

	assert(tree);
	assert(tree->root);

	/* gain the input focus when required */
	state.w = (wimp_w)tree->handle;
	error = xwimp_get_window_state(&state);
	if (error)
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
	error = xwimp_get_caret_position(&caret);
	if (error)
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
	if (((pointer->buttons == (wimp_CLICK_SELECT << 8)) ||
			(pointer->buttons == (wimp_CLICK_ADJUST << 8))) &&
			(caret.w != state.w)) {
		error = xwimp_set_caret_position((wimp_w)tree->handle, -1, -100,
						-100, 32, -1);
		if (error)
			LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
	}

	if (!tree->root->child)
		return true;

	tree_initialise_redraw(tree);
	x = pointer->pos.x - ro_gui_tree_origin_x;
	y = ro_gui_tree_origin_y - pointer->pos.y;
	element = tree_get_node_element_at(tree->root->child, x, y, &furniture);


	/* stop editing for anything but a drag */
	if ((tree->editing) && (pointer->i != tree->edit_handle) &&
			(pointer->buttons != (wimp_CLICK_SELECT << 4)))
		ro_gui_tree_stop_edit(tree);

	/* handle a menu click */
	if (pointer->buttons == wimp_CLICK_MENU) {
		if ((!element) || (!tree->root->child) ||
				(tree_has_selection(tree->root->child)))
			return true;

		node = element->parent;
		tree->temp_selection = node;
		node->selected = true;
		tree_handle_node_element_changed(tree, &node->data);
		return true;

	}

	/* no item either means cancel selection on (select) click or a drag */
	if (!element) {
	  	if (tree->single_selection) {
			tree_set_node_selected(tree, tree->root->child, false);
	  		return true;
	  	}
		if ((pointer->buttons == (wimp_CLICK_SELECT << 4)) ||
				(pointer->buttons == (wimp_CLICK_SELECT << 8)))
			tree_set_node_selected(tree, tree->root->child, false);
		if ((pointer->buttons == (wimp_CLICK_SELECT << 4)) ||
				(pointer->buttons == (wimp_CLICK_ADJUST << 4))) {

			scroll.w = (wimp_w)tree->handle;
			scroll.pause_zone_sizes.y0 = 80;
			scroll.pause_zone_sizes.y1 = 80;
			if (tree->toolbar)
				scroll.pause_zone_sizes.y1 +=
						ro_gui_theme_toolbar_height(tree->toolbar);
			scroll.pause_duration = 0;
			scroll.state_change = (void *)0;
			error = xwimp_auto_scroll(wimp_AUTO_SCROLL_ENABLE_VERTICAL,
					&scroll, 0);
			if (error)
				LOG(("xwimp_auto_scroll: 0x%x: %s",
						error->errnum, error->errmess));

			gui_current_drag_type = GUI_DRAG_TREE_SELECT;
			ro_gui_tree_current_drag_tree = tree;
			ro_gui_tree_current_drag_buttons = pointer->buttons;

			drag.w = (wimp_w)tree->handle;
			drag.type = wimp_DRAG_USER_RUBBER;
			drag.initial.x0 = pointer->pos.x;
			drag.initial.x1 = pointer->pos.x;
			drag.initial.y0 = pointer->pos.y;
			drag.initial.y1 = pointer->pos.y;
			drag.bbox.x0 = state.visible.x0;
			drag.bbox.x1 = state.visible.x1;
			drag.bbox.y0 = -16384;//state.visible.y0;
			drag.bbox.y1 = 16384;//state.visible.y1 - tree->offset_y;
			error = xwimp_drag_box_with_flags(&drag,
					wimp_DRAG_BOX_KEEP_IN_LINE |
					wimp_DRAG_BOX_CLIP);
			if (error)
				LOG(("xwimp_drag_box_with_flags: 0x%x: %s",
						error->errnum, error->errmess));

		}
		return true;
	}

	node = element->parent;

	/* click on furniture or double click on folder toggles node expansion */
	if (((furniture) && ((pointer->buttons == wimp_CLICK_SELECT << 8) ||
			(pointer->buttons == wimp_CLICK_ADJUST << 8) ||
			(pointer->buttons == wimp_CLICK_SELECT) ||
			(pointer->buttons == wimp_CLICK_ADJUST))) ||
			((!furniture) && (node->child) &&
			((pointer->buttons == wimp_CLICK_SELECT) ||
			(pointer->buttons == wimp_CLICK_ADJUST)))) {
		node->expanded = !node->expanded;
		if (!furniture)
			node->selected = false;
		tree_handle_node_changed(tree, node, false, true);

		/* find the last child node if expanded */
		last = node;
		if ((last->child) && (last->expanded)) {
			last = last->child;
			while ((last->next) || ((last->child) && (last->expanded))) {
				if (last->next)
					last = last->next;
				else
					last = last->child;
			}
		}
		/* scroll to the bottom element then back to the top */
		element = &last->data;
		if (last->expanded)
			for (; element->next; element = element->next);
		ro_gui_tree_scroll_visible(tree, element);
		ro_gui_tree_scroll_visible(tree, &node->data);
		return true;
	}

	/* no use for any other furniture click */
	if (furniture)
		return true;

	/* single/double alt+click starts editing */
	if ((node->editable) && (!tree->editing) &&
			((element->data == TREE_ELEMENT_URL) ||
				(element->data == TREE_ELEMENT_TITLE)) &&
			((pointer->buttons == wimp_CLICK_SELECT) ||
			(pointer->buttons == (wimp_CLICK_SELECT << 8)))) {
		xosbyte1(osbyte_SCAN_KEYBOARD, 2 ^ 0x80, 0, &alt_pressed);
		if (alt_pressed == 0xff) {
			ro_gui_tree_start_edit(tree, element, pointer);
			return true;
		}
	}

	/* double click starts launches the leaf */
	if ((pointer->buttons == wimp_CLICK_SELECT) ||
			(pointer->buttons == wimp_CLICK_ADJUST)) {
		if (!ro_gui_tree_launch_node(tree, node))
			return false;
		if (pointer->buttons == wimp_CLICK_ADJUST)
			ro_gui_dialog_close((wimp_w)tree->handle);
		return true;
	}

	/* single click (select) cancels current selection and selects item */
	if ((pointer->buttons == (wimp_CLICK_SELECT << 8)) ||
			((pointer->buttons == (wimp_CLICK_ADJUST << 8)) &&
				(tree->single_selection))) {
		if (!node->selected) {
			tree_set_node_selected(tree, tree->root->child, false);
			node->selected = true;
			tree_handle_node_element_changed(tree, &node->data);
		}
		return true;
	}

	/* single click (adjust) toggles item selection */
	if (pointer->buttons == (wimp_CLICK_ADJUST << 8)) {
		node->selected = !node->selected;
		tree_handle_node_element_changed(tree, &node->data);
		return true;
	}

	/* drag starts a drag operation */
	if ((!tree->editing) && ((pointer->buttons == (wimp_CLICK_SELECT << 4)) ||
			(pointer->buttons == (wimp_CLICK_ADJUST << 4)))) {
		if (tree->no_drag)
			return true;

		if (!node->selected) {
			node->selected = true;
			tree_handle_node_element_changed(tree, &node->data);
		}

		scroll.w = (wimp_w)tree->handle;
		scroll.pause_zone_sizes.y0 = 80;
		scroll.pause_zone_sizes.y1 = 80;
		if (tree->toolbar)
			scroll.pause_zone_sizes.y1 +=
					ro_gui_theme_toolbar_height(tree->toolbar);
		scroll.pause_duration = -1;
		scroll.state_change = (void *)0;
		error = xwimp_auto_scroll(wimp_AUTO_SCROLL_ENABLE_VERTICAL,
				&scroll, 0);
		if (error)
			LOG(("xwimp_auto_scroll: 0x%x: %s",
					error->errnum, error->errmess));

		gui_current_drag_type = GUI_DRAG_TREE_MOVE;
		ro_gui_tree_current_drag_tree = tree;
		ro_gui_tree_current_drag_buttons = pointer->buttons;

		node = tree_get_selected_node(tree->root);
		if (node) {
			if (node->folder) {
				if ((node->expanded) &&
						(ro_gui_wimp_sprite_exists("directoryo")))
					sprintf(ro_gui_tree_drag_name, "directoryo");
				else
					sprintf(ro_gui_tree_drag_name, "directory");
			} else {
				/* small_xxx -> file_xxx */
				sprintf(ro_gui_tree_drag_name, "file_%s",
						node->data.sprite->name + 6);
				if (!ro_gui_wimp_sprite_exists(ro_gui_tree_drag_name))
					sprintf(ro_gui_tree_drag_name, "file_xxx");
			}
		} else {
			sprintf(ro_gui_tree_drag_name, "package");
		}

		error = xdragasprite_start(dragasprite_HPOS_CENTRE |
				dragasprite_VPOS_CENTRE |
				dragasprite_BOUND_POINTER |
				dragasprite_DROP_SHADOW,
				(osspriteop_area *) 1,
				ro_gui_tree_drag_name, &box, 0);
		if (error)
			LOG(("xdragasprite_start: 0x%x: %s",
					error->errnum, error->errmess));
		return true;
	}


	return false;
}


/**
 * Handles a menu closed event
 *
 * \param tree  the tree to handle the event for
 */
void ro_gui_tree_menu_closed(struct tree *tree)
{
	assert(tree);

	if (tree->temp_selection) {
		tree->temp_selection->selected = false;
		tree_handle_node_element_changed(tree, &tree->temp_selection->data);
		tree->temp_selection = NULL;
		ro_gui_menu_prepare_action((wimp_w)tree->handle, TREE_SELECTION, false);
		ro_gui_menu_prepare_action((wimp_w)tree->handle, TREE_EXPAND_ALL, false);
	}
}


/**
 * Respond to a mouse click for a tree (hotlist or history) toolbar
 *
 * \param pointer  the pointer state
 */
bool ro_gui_tree_toolbar_click(wimp_pointer* pointer)
{
	struct node *node;

	struct toolbar *toolbar =
			(struct toolbar *)ro_gui_wimp_event_get_user_data(pointer->w);
	assert(toolbar);
	struct tree *tree =
		(struct tree *)ro_gui_wimp_event_get_user_data(toolbar->parent_handle);
	assert(tree);

	ro_gui_tree_stop_edit(tree);

	if (pointer->buttons == wimp_CLICK_MENU) {
		ro_gui_menu_create(tree_toolbar_menu, pointer->pos.x,
				pointer->pos.y, (wimp_w)tree->handle);
		return true;
	}

	if (tree->toolbar->editor) {
		ro_gui_theme_toolbar_editor_click(tree->toolbar, pointer);
		return true;
	}

	switch (pointer->i) {
		case ICON_TOOLBAR_CREATE:
			node = tree_create_folder_node(tree->root,
					messages_get("TreeNewFolder"));
			tree_redraw_area(tree, node->box.x - NODE_INSTEP,
					0, NODE_INSTEP, 16384);
			tree_handle_node_changed(tree, node, false, true);
			ro_gui_tree_start_edit(tree, &node->data, NULL);
			break;
		case ICON_TOOLBAR_OPEN:
			tree_handle_expansion(tree, tree->root,
					(pointer->buttons == wimp_CLICK_SELECT),
					true, false);
			break;
		case ICON_TOOLBAR_EXPAND:
			tree_handle_expansion(tree, tree->root,
					(pointer->buttons == wimp_CLICK_SELECT),
					false, true);
			break;
		case ICON_TOOLBAR_DELETE:
			ro_gui_menu_handle_action((wimp_w)tree->handle,
					TREE_SELECTION_DELETE, false);
			break;
		case ICON_TOOLBAR_LAUNCH:
			ro_gui_menu_handle_action((wimp_w)tree->handle,
					TREE_SELECTION_LAUNCH, false);
			break;
	}
	return true;
}


/**
 * Starts an editing session
 *
 * \param tree	   the tree to start editing for
 * \param element  the element to edit
 * \param pointer  the pointer data to use for caret positioning (or NULL)
 */
void ro_gui_tree_start_edit(struct tree *tree, struct node_element *element,
		wimp_pointer *pointer)
{
	os_error *error;
	struct node *parent;
	int toolbar_height = 0;

	assert(tree);
	assert(element);

	if (tree->editing)
		ro_gui_tree_stop_edit(tree);
	if (tree->toolbar)
		toolbar_height = ro_gui_theme_toolbar_height(tree->toolbar);

	parent = element->parent;
	if (&parent->data == element)
		parent = parent->parent;
	for (; parent; parent = parent->parent) {
		if (!parent->expanded) {
			parent->expanded = true;
			tree_handle_node_changed(tree, parent, false, true);
		}
	}

	tree->editing = element;
	ro_gui_tree_edit_icon.w = (wimp_w)tree->handle;
	ro_gui_tree_edit_icon.icon.extent.x0 = tree->offset_x + element->box.x - 2;
	ro_gui_tree_edit_icon.icon.extent.x1 = tree->offset_x +
			element->box.x + element->box.width + 2;
	ro_gui_tree_edit_icon.icon.extent.y1 = -tree->offset_y - toolbar_height -
			element->box.y;
	ro_gui_tree_edit_icon.icon.extent.y0 = -tree->offset_y - toolbar_height -
			element->box.y - element->box.height;
	if (element->type == NODE_ELEMENT_TEXT_PLUS_SPRITE)
		ro_gui_tree_edit_icon.icon.extent.x0 += NODE_INSTEP;
	ro_gui_tree_edit_icon.icon.data.indirected_text.text = 
			(char *) element->text;
	error = xwimp_create_icon(&ro_gui_tree_edit_icon,
			(wimp_i *)&tree->edit_handle);
	if (error)
		LOG(("xwimp_create_icon: 0x%x: %s",
				error->errnum, error->errmess));

	tree->textarea_handle = ro_textarea_create((wimp_w)tree->handle,
			(wimp_i)tree->edit_handle, 0, ro_gui_desktop_font_family,
			ro_gui_desktop_font_size,
			ro_gui_desktop_font_style);
	if (!tree->textarea_handle) {
		ro_gui_tree_stop_edit(tree);
		return;
	}
	ro_textarea_set_text(tree->textarea_handle, element->text);
	if (pointer)
		ro_textarea_set_caret_xy(tree->textarea_handle,
				pointer->pos.x, pointer->pos.y);
	else
		ro_textarea_set_caret(tree->textarea_handle,
				strlen(element->text));

	tree_handle_node_element_changed(tree, element);
	ro_gui_tree_scroll_visible(tree, element);
}


/**
 * Stops any current editing session
 *
 * \param tree  the tree to stop editing for
 */
void ro_gui_tree_stop_edit(struct tree *tree)
{
	os_error *error;

	assert(tree);

	if (!tree->editing) return;

	if (tree->textarea_handle) {
	  	ro_textarea_destroy(tree->textarea_handle);
	  	tree->textarea_handle = 0;
	}
	error = xwimp_delete_icon((wimp_w)tree->handle, (wimp_i)tree->edit_handle);
	if (error)
		LOG(("xwimp_delete_icon: 0x%x: %s",
				error->errnum, error->errmess));
	tree_handle_node_element_changed(tree, tree->editing);
	tree->editing = NULL;

	error = xwimp_set_caret_position((wimp_w)tree->handle, -1, -100,
			-100, 32, -1);
	if (error)
		LOG(("xwimp_set_caret_position: 0x%x: %s",
			error->errnum, error->errmess));
	tree_recalculate_size(tree);
}


/**
 * Scrolls the tree to make an element visible
 *
 * \param tree	   the tree to scroll
 * \param element  the element to display
 */
void ro_gui_tree_scroll_visible(struct tree *tree, struct node_element *element)
{
	wimp_window_state state;
	int x0, x1, y0, y1;
	os_error *error;
	int toolbar_height = 0;

	assert(element);

	if (tree->toolbar)
		toolbar_height = ro_gui_theme_toolbar_height(tree->toolbar);

	state.w = (wimp_w)tree->handle;
	error = xwimp_get_window_state(&state);
	if (error)
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
	if (!(state.flags & wimp_WINDOW_OPEN))
		return;
	x0 = state.xscroll;
	y0 = -state.yscroll;
	x1 = x0 + state.visible.x1 - state.visible.x0 - tree->offset_x;
	y1 = y0 - state.visible.y0 + state.visible.y1 - tree->offset_y - toolbar_height;

	state.yscroll = state.visible.y1 - state.visible.y0 - tree->offset_y -
			toolbar_height - y1;
	if ((element->box.y >= y0) && (element->box.y + element->box.height <= y1))
		return;
	if (element->box.y < y0)
		state.yscroll = -element->box.y;
	if (element->box.y + element->box.height > y1)
		state.yscroll = state.visible.y1 - state.visible.y0 -
				tree->offset_y - toolbar_height -
				(element->box.y + element->box.height);
	ro_gui_tree_open(PTR_WIMP_OPEN(&state));
}


/**
 * Shows the a tree window.
 */
void ro_gui_tree_show(struct tree *tree)
{
	struct toolbar *toolbar;

	/* we may have failed to initialise */
	if (!tree) return;
	toolbar = tree->toolbar;

	/* handle first time opening */
	if (!ro_gui_dialog_open_top((wimp_w)tree->handle, toolbar, 600, 800)) {
		ro_gui_tree_stop_edit(tree);
		if (tree->root->child) {
			tree_set_node_selected(tree, tree->root, false);
			tree_handle_node_changed(tree, tree->root,
					false, true);
		}
	}

	/* set the caret position */
	xwimp_set_caret_position((wimp_w)tree->handle, -1, -100, -100, 32, -1);
}


/**
 * Handles a window open request
 *
 * \param open  the window state
 */
void ro_gui_tree_open(wimp_open *open)
{
	struct tree *tree;
	os_error *error;
	int width;
	int height;
	int toolbar_height = 0;
	bool vscroll;

	tree = (struct tree *)ro_gui_wimp_event_get_user_data(open->w);

	if (!tree)
		return;
	if (tree->toolbar)
		toolbar_height = ro_gui_theme_toolbar_height(tree->toolbar);

	width = open->visible.x1 - open->visible.x0;
	if (width < (tree->offset_x + tree->width))
		width = tree->offset_x + tree->width;
	height = open->visible.y1 - open->visible.y0;
	if (height < (tree->offset_y + toolbar_height + tree->height))
		height = tree->offset_y + toolbar_height + tree->height;

	if ((height != tree->window_height) || (width != tree->window_width)) {
		os_box extent = { 0, -height, width, 0};
		error = xwimp_set_extent((wimp_w)tree->handle, &extent);
		if (error) {
			LOG(("xwimp_set_extent: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}

		/* hide the scroll bar? */
		if ((tree->no_vscroll) && (height != tree->window_height)) {
		  	vscroll = (tree->height > height);
			if (ro_gui_wimp_check_window_furniture(open->w,
					wimp_WINDOW_VSCROLL) != vscroll) {
				ro_gui_wimp_update_window_furniture(open->w,
					0, wimp_WINDOW_VSCROLL);
				if (vscroll)
					open->visible.x1 -= ro_get_vscroll_width(open->w);
				else
					open->visible.x1 += ro_get_vscroll_width(open->w);
			}
		}

		tree->window_width = width;
		tree->window_height = height;
	}

	error = xwimp_open_window(open);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	if (tree->toolbar)
		ro_gui_theme_process_toolbar(tree->toolbar, -1);
	ro_gui_menu_prepare_action((wimp_w)tree->handle, TREE_SELECTION, false);
	ro_gui_menu_prepare_action((wimp_w)tree->handle, TREE_EXPAND_ALL, false);
}


/**
 * Handles a keypress for a tree
 *
 * \param key	the key pressed
 * \param tree  the tree to handle a keypress for
 * \return whether the key was processed
 */
bool ro_gui_tree_keypress(wimp_key *key)
{
	wimp_window_state state;
	int y;
	char *new_string;
	struct tree *tree;
	int strlen;
	os_error *error;

	tree = (struct tree *)ro_gui_wimp_event_get_user_data(key->w);
	if (!tree)
		return false;

	/*	Handle basic keys
	*/
	switch (key->c) {
		case 1:		/* CTRL+A */
			ro_gui_menu_handle_action((wimp_w)tree->handle,
					TREE_SELECT_ALL, false);
			return true;
		case 24:	/* CTRL+X */
			ro_gui_menu_handle_action((wimp_w)tree->handle,
					TREE_SELECTION_DELETE, false);
			return true;
		case 26:	/* CTRL+Z */
			ro_gui_menu_handle_action((wimp_w)tree->handle,
					TREE_CLEAR_SELECTION, false);
			return true;
		case wimp_KEY_RETURN:
			if ((tree->editing) && (tree->textarea_handle)) {
			  	strlen = ro_textarea_get_text(
						tree->textarea_handle,
			  			NULL, 0);
			  	if (strlen == -1) {
					ro_gui_tree_stop_edit(tree);
			  		return true;
			  	}
			  	new_string = malloc(strlen);
			  	if (!new_string) {
					ro_gui_tree_stop_edit(tree);
			  	  	LOG(("No memory for malloc()"));
			  	  	warn_user("NoMemory", 0);
			  	  	return true;
			  	}
			  	ro_textarea_get_text(tree->textarea_handle,
			  			new_string, strlen);
			  	free((void *)tree->editing->text);
			  	tree->editing->text = new_string;
				ro_gui_tree_stop_edit(tree);
				tree_recalculate_size(tree);
			} else {
				ro_gui_tree_launch_selected(tree);
			}
			return true;
		case wimp_KEY_ESCAPE:
			if (tree->editing) {
				ro_gui_tree_stop_edit(tree);
			} else {
				/* \todo cancel drags etc. */
			}
			return true;

		case IS_WIMP_KEY | wimp_KEY_UP:
		case IS_WIMP_KEY | wimp_KEY_DOWN:
		case IS_WIMP_KEY | wimp_KEY_PAGE_UP:
		case IS_WIMP_KEY | wimp_KEY_PAGE_DOWN:
		case IS_WIMP_KEY | wimp_KEY_HOME:
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_UP:
		case IS_WIMP_KEY | wimp_KEY_END:
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_DOWN:
			break;
		default:
			return false;
	}

	/* keyboard scrolling */
	state.w = (wimp_w)tree->handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		return true;
	}

	y = state.visible.y1 - state.visible.y0 - TREE_TEXT_HEIGHT;
	if (tree->toolbar)
		y -= ro_gui_theme_toolbar_full_height(tree->toolbar);

	switch (key->c) {
		case IS_WIMP_KEY | wimp_KEY_UP:
/*			if ((state.yscroll % TREE_TEXT_HEIGHT) != 0)
				state.yscroll -= (state.yscroll % TREE_TEXT_HEIGHT);
			else
*/				state.yscroll += TREE_TEXT_HEIGHT;
			break;
		case IS_WIMP_KEY | wimp_KEY_DOWN:
/*			if (((state.yscroll + y) % TREE_TEXT_HEIGHT) != 0)
				state.yscroll -= ((state.yscroll + y) % TREE_TEXT_HEIGHT);
			else
*/				state.yscroll -= TREE_TEXT_HEIGHT;
			break;
		case IS_WIMP_KEY | wimp_KEY_PAGE_UP:
			state.yscroll += y;
			break;
		case IS_WIMP_KEY | wimp_KEY_PAGE_DOWN:
			state.yscroll -= y;
			break;
		case IS_WIMP_KEY | wimp_KEY_HOME:
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_UP:
			state.yscroll = 0x10000000;
			break;
		case IS_WIMP_KEY | wimp_KEY_END:
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_DOWN:
			state.yscroll = -0x10000000;
			break;
	}

	error = xwimp_open_window(PTR_WIMP_OPEN(&state));
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
	}

	return true;
}


/**
 * Handles the completion of a selection drag (GUI_DRAG_TREE_SELECT)
 *
 * \param drag  the drag box information
 */
void ro_gui_tree_selection_drag_end(wimp_dragged *drag)
{
	wimp_window_state state;
	wimp_auto_scroll_info scroll;
	os_error *error;
	int x0, y0, x1, y1;
	int toolbar_height = 0;

	if (ro_gui_tree_current_drag_tree->toolbar)
		toolbar_height = ro_gui_theme_toolbar_height(
				ro_gui_tree_current_drag_tree->toolbar);

	scroll.w = (wimp_w)ro_gui_tree_current_drag_tree->handle;
	error = xwimp_auto_scroll(0, &scroll, 0);
	if (error)
		LOG(("xwimp_auto_scroll: 0x%x: %s", error->errnum, error->errmess));

	state.w = (wimp_w)ro_gui_tree_current_drag_tree->handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	x0 = drag->final.x0 - state.visible.x0 - state.xscroll +
			ro_gui_tree_current_drag_tree->offset_x;
	y0 = state.visible.y1 - state.yscroll - drag->final.y0 -
			ro_gui_tree_current_drag_tree->offset_y - toolbar_height;
	x1 = drag->final.x1 - state.visible.x0 - state.xscroll +
			ro_gui_tree_current_drag_tree->offset_x;
	y1 = state.visible.y1 - state.yscroll - drag->final.y1 -
			ro_gui_tree_current_drag_tree->offset_y - toolbar_height;
	tree_handle_selection_area(ro_gui_tree_current_drag_tree, x0, y0,
		x1 - x0, y1 - y0,
		(ro_gui_tree_current_drag_buttons == (wimp_CLICK_ADJUST << 4)));
	ro_gui_menu_prepare_action((wimp_w)ro_gui_tree_current_drag_tree->handle,
			TREE_SELECTION, false);
	ro_gui_menu_prepare_action((wimp_w)ro_gui_tree_current_drag_tree->handle,
			TREE_EXPAND_ALL, false);
}


/**
 * Converts screen co-ordinates to tree ones
 *
 * \param tree	  the tree to calculate for
 * \param x	  the screen x co-ordinate
 * \param x	  the screen y co-ordinate
 * \param tree_x  updated to the tree x co-ordinate
 * \param tree_y  updated to the tree y co-ordinate
 */
void ro_gui_tree_get_tree_coordinates(struct tree *tree, int x, int y,
		int *tree_x, int *tree_y) {
	wimp_window_state state;
	os_error *error;

	state.w = (wimp_w)tree->handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	*tree_x = x - state.visible.x0 - state.xscroll + tree->offset_x;
	*tree_y = state.visible.y1 - state.yscroll - y - tree->offset_y;
	if (tree->toolbar)
		*tree_y -= ro_gui_theme_toolbar_height(tree->toolbar);
}


/**
 * Handles the completion of a move drag (GUI_DRAG_TREE_MOVE)
 *
 * \param drag  the drag box information
 */
void ro_gui_tree_move_drag_end(wimp_dragged *drag)
{
	struct gui_window *g;
	wimp_pointer pointer;
	wimp_auto_scroll_info scroll;
	os_error *error;
	struct node *node;
	struct node *single;
	struct node_element *element;
	bool before;
	int x, y;

	scroll.w = (wimp_w)ro_gui_tree_current_drag_tree->handle;
	error = xwimp_auto_scroll(0, &scroll, 0);
	if (error)
		LOG(("xwimp_auto_scroll: 0x%x: %s", error->errnum, error->errmess));

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s", error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (pointer.w != (wimp_w)ro_gui_tree_current_drag_tree->handle) {
		/* try to drop into a browser window */
		single = tree_get_selected_node(ro_gui_tree_current_drag_tree->root->child);
		element = tree_find_element(single, TREE_ELEMENT_URL);
		if (!element)
			return;
		if (single) {
			/* \todo:send datasave for element */
			g = ro_gui_window_lookup(pointer.w);
			if (g)
				browser_window_go(g->bw, element->text, 0, true);
			return;
		} else {
			/* \todo:update save.c to handle multiple concurrent saves */
		}
		return;
	}

	/* internal drag */
	if (!ro_gui_tree_current_drag_tree->movable)
		return;
	ro_gui_tree_get_tree_coordinates(ro_gui_tree_current_drag_tree,
			drag->final.x0 + 34, drag->final.y0 + 34, &x, &y);
	node = tree_get_link_details(ro_gui_tree_current_drag_tree, x, y, &before);
	tree_move_selected_nodes(ro_gui_tree_current_drag_tree, node, before);
}


/**
 * Launches all selected nodes.
 *
 * \param tree  the tree to launch all selected nodes for
 */
void ro_gui_tree_launch_selected(struct tree *tree)
{
	assert(tree);

	if (tree->root->child)
		ro_gui_tree_launch_selected_node(tree, tree->root->child, false);
}


/**
 * Launches all selected nodes.
 *
 * \param node  the node to launch all selected nodes for
 */
void ro_gui_tree_launch_selected_node(struct tree *tree, struct node *node,
		bool all)
{
	for (; node; node = node->next) {
		if (((node->selected) || (all)) && (!node->folder))
			ro_gui_tree_launch_node(tree, node);
		if ((node->child) && ((node->expanded) || (node->selected) | (all)))
			ro_gui_tree_launch_selected_node(tree, node->child,
				(node->selected) | (all));
	}
}


/**
 * Launches a node using all known methods.
 *
 * \param node  the node to launch
 * \return whether the node could be launched
 */
bool ro_gui_tree_launch_node(struct tree *tree, struct node *node)
{
	struct node_element *element;

	assert(node);

	element = tree_find_element(node, TREE_ELEMENT_URL);
	if (element) {
		browser_window_create(element->text, NULL, 0, true, false);
		return true;
	}

	element = tree_find_element(node, TREE_ELEMENT_SSL);
	if (element) {
		ro_gui_cert_open(tree, node);
		return true;
	}

	return false;
}

int ro_gui_tree_help(int x, int y)
{
	return -1;
}


void ro_gui_tree_update_theme(struct tree *tree)
{
	if ((tree) && (tree->toolbar)) {
		if (tree->toolbar->editor)
			if (!ro_gui_theme_update_toolbar(NULL, tree->toolbar->editor))
				tree->toolbar->editor = NULL;
		if (!ro_gui_theme_update_toolbar(NULL, tree->toolbar)) {
			ro_gui_theme_destroy_toolbar(tree->toolbar);
			tree->toolbar = NULL;
		}
		ro_gui_theme_toolbar_editor_sync(tree->toolbar);
		ro_gui_theme_attach_toolbar(tree->toolbar, (wimp_w)tree->handle);
		tree_resized(tree);
		xwimp_force_redraw((wimp_w)tree->handle, 0, -16384, 16384, 16384);
	}
}
