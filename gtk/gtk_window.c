/*
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
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

#include "gtk/gtk_window.h"
#include "desktop/browser.h"
#include "desktop/options.h"
#include "desktop/textinput.h"
#include "gtk/gtk_gui.h"
#include "gtk/gtk_scaffolding.h"
#include "gtk/gtk_plotters.h"
#include "gtk/gtk_schedule.h"
#undef NDEBUG
#include "utils/log.h"
#include "utils/utils.h"
#include <gdk/gdkkeysyms.h>
#include <assert.h>

struct gui_window {
        /* All gui_window objects have an ultimate scaffold */
        nsgtk_scaffolding	*scaffold;
        /* A gui_window is the rendering of a browser_window */
        struct browser_window	*bw;

        /* These are the storage for the rendering */
	int			caretx, carety, careth;
	gui_pointer_shape	current_pointer;
	int			last_x, last_y;

        /* Within GTK, a gui_window is a scrolled window
         * with a viewport inside
         * with a gtkfixed in that
         * with a drawing area in that
         * The scrolled window is optional and only chosen
         * for frames which need it. Otherwise we just use
         * a viewport.
         */
        GtkScrolledWindow	*scrolledwindow;
	GtkViewport		*viewport;
        GtkFixed                *fixed;
	GtkDrawingArea		*drawing_area;

        /* Keep gui_windows in a list for cleanup later */
        struct gui_window	*next, *prev;
};

static struct gui_window *window_list = 0;	/**< first entry in win list*/

static wchar_t gdkkey_to_nskey(GdkEventKey *);
static void nsgtk_gui_window_attach_child(struct gui_window *parent,
                                          struct gui_window *child);
/* Methods which apply only to a gui_window */
static gboolean nsgtk_window_expose_event(GtkWidget *, GdkEventExpose *,
                                          gpointer);
static gboolean nsgtk_window_motion_notify_event(GtkWidget *, GdkEventMotion *,
						gpointer);
static gboolean nsgtk_window_button_press_event(GtkWidget *, GdkEventButton *,
						gpointer);
static gboolean nsgtk_window_keypress_event(GtkWidget *, GdkEventKey *,
						gpointer);
static gboolean nsgtk_window_size_allocate_event(GtkWidget *, GtkAllocation *,
						gpointer);
/* Other useful bits */
static void nsgtk_redraw_caret(struct gui_window *g);


nsgtk_scaffolding *nsgtk_get_scaffold(struct gui_window *g)
{
        return g->scaffold;
}

struct browser_window *nsgtk_get_browser_for_gui(struct gui_window *g)
{
        return g->bw;
}

float nsgtk_get_scale_for_gui(struct gui_window *g)
{
        return g->bw->scale;
}

/* Create a gui_window */
struct gui_window *gui_create_browser_window(struct browser_window *bw,
                                             struct browser_window *clone)
{
	struct gui_window *g;		/**< what we're creating to return */
        GtkPolicyType scrollpolicy;

	g = malloc(sizeof(*g));
       	if (!g) {
		warn_user("NoMemory", 0);
		return 0;
	}

        LOG(("Creating gui window %p for browser window %p", g, bw));

	g->bw = bw;
	g->current_pointer = GUI_POINTER_DEFAULT;
	if (clone != NULL)
		bw->scale = clone->scale;
	else
		bw->scale = (float) option_scale / 100;

	g->careth = 0;

        /* Attach ourselves to the list (push_top) */
        if (window_list)
                window_list->prev = g;
        g->next = window_list;
        g->prev = NULL;
        window_list = g;

        if (bw->parent != NULL) {
                /* Find our parent's scaffolding */
                g->scaffold = bw->parent->window->scaffold;
        } else {
                /* Now construct and attach a scaffold */
                g->scaffold = nsgtk_new_scaffolding(g);
        }

        /* Construct our primary elements */
        g->fixed = GTK_FIXED(gtk_fixed_new());
        g->drawing_area = GTK_DRAWING_AREA(gtk_drawing_area_new());
        gtk_fixed_put(g->fixed, GTK_WIDGET(g->drawing_area), 0, 0);
        gtk_container_set_border_width(GTK_CONTAINER(g->fixed), 0);

        if (bw->parent != NULL ) {
                g->scrolledwindow = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
                gtk_scrolled_window_add_with_viewport(g->scrolledwindow,
                                                      GTK_WIDGET(g->fixed));
                gtk_scrolled_window_set_shadow_type(g->scrolledwindow,
                                                    GTK_SHADOW_NONE);
                g->viewport = GTK_VIEWPORT(gtk_bin_get_child(GTK_BIN(g->scrolledwindow)));
                /* Attach ourselves into our parent at the right point */
                nsgtk_gui_window_attach_child(bw->parent->window, g);
        } else {
                g->scrolledwindow = 0;
                g->viewport = GTK_VIEWPORT(gtk_viewport_new(NULL, NULL)); /* Need to attach adjustments */
                gtk_container_add(GTK_CONTAINER(g->viewport), GTK_WIDGET(g->fixed));

                /* Attach our viewport into the scaffold */
                nsgtk_attach_toplevel_viewport(g->scaffold, g->viewport);
        }

        gtk_container_set_border_width(GTK_CONTAINER(g->viewport), 0);
        gtk_viewport_set_shadow_type(g->viewport, GTK_SHADOW_NONE);
        if (g->scrolledwindow)
                gtk_widget_show(GTK_WIDGET(g->scrolledwindow));
        /* And enable visibility from our viewport down */
        gtk_widget_show(GTK_WIDGET(g->viewport));
        gtk_widget_show(GTK_WIDGET(g->fixed));
        gtk_widget_show(GTK_WIDGET(g->drawing_area));

        switch(bw->scrolling) {
        case SCROLLING_NO:
                scrollpolicy = GTK_POLICY_NEVER;
                break;
        case SCROLLING_YES:
                scrollpolicy = GTK_POLICY_ALWAYS;
                break;
        case SCROLLING_AUTO:
	default:
                scrollpolicy = GTK_POLICY_AUTOMATIC;
                break;
        };

        switch(bw->browser_window_type) {
        case BROWSER_WINDOW_FRAMESET:
                if (g->scrolledwindow)
                        gtk_scrolled_window_set_policy(g->scrolledwindow,
                                                       GTK_POLICY_NEVER,
                                                       GTK_POLICY_NEVER);
                break;
        case BROWSER_WINDOW_FRAME:
                if (g->scrolledwindow)
                        gtk_scrolled_window_set_policy(g->scrolledwindow,
                                                       scrollpolicy,
                                                       scrollpolicy);
                break;
        case BROWSER_WINDOW_NORMAL:
                if (g->scrolledwindow)
                        gtk_scrolled_window_set_policy(g->scrolledwindow,
                                                       scrollpolicy,
                                                       scrollpolicy);
                break;
        case BROWSER_WINDOW_IFRAME:
                if (g->scrolledwindow)
                        gtk_scrolled_window_set_policy(g->scrolledwindow,
                                                       scrollpolicy,
                                                       scrollpolicy);
                break;
        }

	/* set the events we're interested in receiving from the browser's
	 * drawing area.
	 */
	gtk_widget_add_events(GTK_WIDGET(g->drawing_area),
				GDK_EXPOSURE_MASK |
				GDK_LEAVE_NOTIFY_MASK |
				GDK_BUTTON_PRESS_MASK |
				GDK_POINTER_MOTION_MASK |
				GDK_KEY_PRESS_MASK |
				GDK_KEY_RELEASE_MASK);
	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(g->drawing_area), GTK_CAN_FOCUS);

	/* set the default background colour of the drawing area to white. */
	gtk_widget_modify_bg(GTK_WIDGET(g->drawing_area), GTK_STATE_NORMAL,
				&((GdkColor) { 0, 0xffff, 0xffff, 0xffff } ));

#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))
	CONNECT(g->drawing_area, "expose_event", nsgtk_window_expose_event, g);
	CONNECT(g->drawing_area, "motion_notify_event",
		nsgtk_window_motion_notify_event, g);
	CONNECT(g->drawing_area, "button_press_event",
	    	nsgtk_window_button_press_event, g);
	CONNECT(g->drawing_area, "key_press_event",
		nsgtk_window_keypress_event, g);
	CONNECT(g->viewport, "size_allocate",
		nsgtk_window_size_allocate_event, g);

        return g;
}

static void nsgtk_gui_window_attach_child(struct gui_window *parent,
                                          struct gui_window *child)
{
        /* Attach the child gui_window (frame) into the parent.
         * It will be resized later on.
         */
        GtkFixed *parent_fixed = parent->fixed;
        GtkWidget *child_widget = GTK_WIDGET(child->scrolledwindow);
        gtk_fixed_put(parent_fixed, child_widget, 0, 0);
}

void gui_window_position_frame(struct gui_window *g, int x0, int y0, int x1, int y1)
{
        /* g is a child frame, we need to place it relative to its parent */
        GtkWidget *w = GTK_WIDGET(g->scrolledwindow);
        GtkFixed *f = g->bw->parent->window->fixed;
        assert(w);
        assert(f);
        LOG(("%s: %d,%d  %dx%d", g->bw->name, x0, y0, x1-x0+2, y1-y0+2));

	/* if the window has not changed position or size, do not bother
	 * moving/resising it.
	 */

	LOG(("  current: %d,%d  %dx%d",
	      	w->allocation.x, w->allocation.y,
		w->allocation.width, w->allocation.height));

	if (w->allocation.x != x0 || w->allocation.y != y0 ||
		w->allocation.width != x1 - x0 + 2 ||
		w->allocation.height != y1 - y0 + 2) {
	  	LOG(("  frame has moved/resized."));
		gtk_fixed_move(f, w, x0, y0);
	        gtk_widget_set_size_request(w, x1 - x0 + 2, y1 - y0 + 2);
	}
}

gboolean nsgtk_window_expose_event(GtkWidget *widget,
                                   GdkEventExpose *event, gpointer data)
{
	struct gui_window *g = data;
	struct content *c;
	float scale = g->bw->scale;

	assert(g);
	assert(g->bw);

	struct gui_window *z;
	for (z = window_list; z && z != g; z = z->next)
		continue;
	assert(z);
	assert(GTK_WIDGET(g->drawing_area) == widget);

	c = g->bw->current_content;
	if (c == NULL)
		return FALSE;

	/* HTML rendering handles scale itself */
	if (c->type == CONTENT_HTML)
		scale = 1;

        current_widget = widget;
	current_drawable = widget->window;
	current_gc = gdk_gc_new(current_drawable);
#ifdef CAIRO_VERSION
	current_cr = gdk_cairo_create(current_drawable);
#endif

	plot = nsgtk_plotters;
	nsgtk_plot_set_scale(g->bw->scale);
	content_redraw(c, 0, 0,
			widget->allocation.width * scale,
			widget->allocation.height * scale,
			event->area.x,
			event->area.y,
			event->area.x + event->area.width,
			event->area.y + event->area.height,
			g->bw->scale, 0xFFFFFF);

	if (g->careth != 0)
		nsgtk_plot_caret(g->caretx, g->carety, g->careth);

	g_object_unref(current_gc);
#ifdef CAIRO_VERSION
	cairo_destroy(current_cr);
#endif

	return FALSE;
}

gboolean nsgtk_window_motion_notify_event(GtkWidget *widget,
                                          GdkEventMotion *event, gpointer data)
{
	struct gui_window *g = data;

	browser_window_mouse_track(g->bw, 0, event->x / g->bw->scale,
                                   event->y / g->bw->scale);

	g->last_x = event->x;
	g->last_y = event->y;

	return TRUE;
}

gboolean nsgtk_window_button_press_event(GtkWidget *widget,
                                         GdkEventButton *event, gpointer data)
{
	struct gui_window *g = data;
	int button = BROWSER_MOUSE_CLICK_1;

	if (event->button == 2) /* 2 == middle button on X */
		button = BROWSER_MOUSE_CLICK_2;

	if (event->button == 3) {
		/* 3 == right button on X */
		nsgtk_scaffolding_popup_menu(g->scaffold, event->button);
		return TRUE;
	}

	browser_window_mouse_click(g->bw, button,
                                   event->x / g->bw->scale,
				   event->y / g->bw->scale);

        gtk_widget_grab_focus(widget);

	return TRUE;
}

wchar_t gdkkey_to_nskey(GdkEventKey *key)
{
        /* this function will need to become much more complex to support
         * everything that the RISC OS version does.  But this will do for
         * now.  I hope.
         */

        switch (key->keyval)
        {
                case GDK_BackSpace:             return KEY_DELETE_LEFT;
                case GDK_Delete:                return KEY_DELETE_RIGHT;
                case GDK_Linefeed:              return 13;
                case GDK_Return:                return 10;
                case GDK_Left:                  return KEY_LEFT;
                case GDK_Right:                 return KEY_RIGHT;
                case GDK_Up:                    return KEY_UP;
                case GDK_Down:                  return KEY_DOWN;

                /* Modifiers - do nothing for now */
                case GDK_Shift_L:
                case GDK_Shift_R:
                case GDK_Control_L:
                case GDK_Control_R:
                case GDK_Caps_Lock:
                case GDK_Shift_Lock:
                case GDK_Meta_L:
                case GDK_Meta_R:
                case GDK_Alt_L:
                case GDK_Alt_R:
                case GDK_Super_L:
                case GDK_Super_R:
                case GDK_Hyper_L:
                case GDK_Hyper_R:               return 0;

                default:                        return key->keyval;
        }
}

gboolean nsgtk_window_keypress_event(GtkWidget *widget, GdkEventKey *event,
					gpointer data)
{
	struct gui_window *g = data;
	wchar_t nskey = gdkkey_to_nskey(event);

	if (browser_window_key_press(g->bw, nskey))
		return TRUE;

	if (event->state == 0) {
		double value;
		GtkAdjustment *vscroll = gtk_viewport_get_vadjustment(g->viewport);

		GtkAdjustment *hscroll = gtk_viewport_get_hadjustment(g->viewport);

		GtkAdjustment *scroll;

		const GtkAllocation *const alloc =
			&GTK_WIDGET(g->viewport)->allocation;

		switch (event->keyval) {
		default:
			return TRUE;

		case GDK_Home:
		case GDK_KP_Home:
			scroll = vscroll;
			value = scroll->lower;
			break;

		case GDK_End:
		case GDK_KP_End:
			scroll = vscroll;
			value = scroll->upper - alloc->height;
			if (value < scroll->lower)
				value = scroll->lower;
			break;

		case GDK_Left:
		case GDK_KP_Left:
			scroll = hscroll;
			value = gtk_adjustment_get_value(scroll) -
						scroll->step_increment;
			if (value < scroll->lower)
				value = scroll->lower;
			break;

		case GDK_Up:
		case GDK_KP_Up:
			scroll = vscroll;
			value = gtk_adjustment_get_value(scroll) -
						scroll->step_increment;
			if (value < scroll->lower)
				value = scroll->lower;
			break;

		case GDK_Right:
		case GDK_KP_Right:
			scroll = hscroll;
			value = gtk_adjustment_get_value(scroll) +
						scroll->step_increment;
			if (value > scroll->upper - alloc->width)
				value = scroll->upper - alloc->width;
			break;

		case GDK_Down:
		case GDK_KP_Down:
			scroll = vscroll;
			value = gtk_adjustment_get_value(scroll) +
						scroll->step_increment;
			if (value > scroll->upper - alloc->height)
				value = scroll->upper - alloc->height;
			break;

		case GDK_Page_Up:
		case GDK_KP_Page_Up:
			scroll = vscroll;
			value = gtk_adjustment_get_value(scroll) -
						scroll->page_increment;
			if (value < scroll->lower)
				value = scroll->lower;
			break;

		case GDK_Page_Down:
		case GDK_KP_Page_Down:
			scroll = vscroll;
			value = gtk_adjustment_get_value(scroll) +
						scroll->page_increment;
			if (value > scroll->upper - alloc->height)
				value = scroll->upper - alloc->height;
			break;
		}

		gtk_adjustment_set_value(scroll, value);
	}

	return TRUE;
}

gboolean nsgtk_window_size_allocate_event(GtkWidget *widget,
                                          GtkAllocation *allocation, gpointer data)
{
	struct gui_window *g = data;

	g->bw->reformat_pending = true;
	browser_reformat_pending = true;

	return TRUE;
}


void nsgtk_reflow_all_windows(void)
{
	for (struct gui_window *g = window_list; g; g = g->next)
		g->bw->reformat_pending = true;

	browser_reformat_pending = true;
}


/**
 * Process pending reformats
 */

void nsgtk_window_process_reformats(void)
{
	struct gui_window *g;

	browser_reformat_pending = false;
	for (g = window_list; g; g = g->next) {
		GtkWidget *widget = GTK_WIDGET(g->viewport);
		if (!g->bw->reformat_pending)
			continue;
		g->bw->reformat_pending = false;
		browser_window_reformat(g->bw,
				widget->allocation.width - 2,
				widget->allocation.height);
	}
}


void nsgtk_window_destroy_browser(struct gui_window *g)
{
	browser_window_destroy(g->bw);
}

void gui_window_destroy(struct gui_window *g)
{
	if (g->prev)
		g->prev->next = g->next;
	else
		window_list = g->next;

	if (g->next)
		g->next->prev = g->prev;


	LOG(("Destroying gui_window %p", g));
	assert(g != NULL);
	assert(g->bw != NULL);
	LOG(("     Scaffolding: %p", g->scaffold));
        LOG(("     Window name: %s", g->bw->name));

        /* If we're a top-level gui_window, destroy our scaffold */
        if (g->scrolledwindow == NULL) {
	  	gtk_widget_destroy(GTK_WIDGET(g->viewport));
                nsgtk_scaffolding_destroy(g->scaffold);
	} else {
	  	gtk_widget_destroy(GTK_WIDGET(g->scrolledwindow));
	}

	free(g);

}

void nsgtk_redraw_caret(struct gui_window *g)
{
	if (g->careth == 0)
		return;

	gui_window_redraw(g, g->caretx, g->carety,
				g->caretx, g->carety + g->careth);
}

void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	gtk_widget_queue_draw_area(GTK_WIDGET(g->drawing_area),
                                   x0, y0, x1-x0+1, y1-y0+1);
}

void gui_window_redraw_window(struct gui_window *g)
{
	gtk_widget_queue_draw(GTK_WIDGET(g->drawing_area));
}

void gui_window_update_box(struct gui_window *g,
                           const union content_msg_data *data)
{
	struct content *c = g->bw->current_content;

	if (c == NULL)
		return;

	gtk_widget_queue_draw_area(GTK_WIDGET(g->drawing_area),
                                   data->redraw.x, data->redraw.y,
                                   data->redraw.width, data->redraw.height);
}

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
        GtkAdjustment *vadj = gtk_viewport_get_vadjustment(g->viewport);
        GtkAdjustment *hadj = gtk_viewport_get_hadjustment(g->viewport);

        assert(vadj);
        assert(hadj);

        *sy = (int)(gtk_adjustment_get_value(vadj));
        *sx = (int)(gtk_adjustment_get_value(hadj));

        return true;
}

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
        GtkAdjustment *vadj = gtk_viewport_get_vadjustment(g->viewport);
        GtkAdjustment *hadj = gtk_viewport_get_hadjustment(g->viewport);
        gdouble vlower, vpage, vupper, hlower, hpage, hupper, x = (double)sx, y = (double)sy;
        
        assert(vadj);
        assert(hadj);
        
        g_object_get(vadj, "page-size", &vpage, "lower", &vlower, "upper", &vupper, NULL);
        g_object_get(hadj, "page-size", &hpage, "lower", &hlower, "upper", &hupper, NULL);
        
        if (x < hlower) x = hlower;
        if (x > (hupper - hpage)) x = hupper - hpage;
        if (y < vlower) y = vlower;
        if (y > (vupper - vpage)) y = vupper - vpage;
        
        gtk_adjustment_set_value(vadj, y);
        gtk_adjustment_set_value(hadj, x);
}


/**
 * Set the scale setting of a window
 *
 * \param  g	  gui window
 * \param  scale  scale value (1.0 == normal scale)
 */

void gui_window_set_scale(struct gui_window *g, float scale)
{
}


void gui_window_update_extent(struct gui_window *g)
{
	if (!g->bw->current_content)
		return;

	gtk_widget_set_size_request(GTK_WIDGET(g->drawing_area),
			g->bw->current_content->width * g->bw->scale,
			g->bw->current_content->height * g->bw->scale);

	gtk_widget_set_size_request(GTK_WIDGET(g->viewport), 0, 0);

}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
        GdkCursor *cursor = NULL;
        GdkCursorType cursortype;
        bool nullcursor = false;

	if (g->current_pointer == shape)
		return;

	g->current_pointer = shape;

	switch (shape) {
        case GUI_POINTER_POINT:
                cursortype = GDK_HAND1;
                break;
        case GUI_POINTER_CARET:
                cursortype = GDK_XTERM;
                break;
        case GUI_POINTER_UP:
                cursortype = GDK_TOP_SIDE;
                break;
        case GUI_POINTER_DOWN:
                cursortype = GDK_BOTTOM_SIDE;
                break;
        case GUI_POINTER_LEFT:
                cursortype = GDK_LEFT_SIDE;
                break;
        case GUI_POINTER_RIGHT:
                cursortype = GDK_RIGHT_SIDE;
                break;
        case GUI_POINTER_LD:
                cursortype = GDK_BOTTOM_LEFT_CORNER;
                break;
        case GUI_POINTER_RD:
                cursortype = GDK_BOTTOM_RIGHT_CORNER;
                break;
        case GUI_POINTER_LU:
                cursortype = GDK_TOP_LEFT_CORNER;
                break;
        case GUI_POINTER_RU:
                cursortype = GDK_TOP_RIGHT_CORNER;
                break;
        case GUI_POINTER_CROSS:
                cursortype = GDK_CROSS;
                break;
        case GUI_POINTER_MOVE:
                cursortype = GDK_FLEUR;
                break;
        case GUI_POINTER_WAIT:
                cursortype = GDK_WATCH;
                break;
        case GUI_POINTER_HELP:
                cursortype = GDK_QUESTION_ARROW;
                break;
        case GUI_POINTER_MENU:
                cursortype = GDK_RIGHTBUTTON;
                break;
        case GUI_POINTER_PROGRESS:
                /* In reality, this needs to be the funky left_ptr_watch
		 * which we can't do easily yet.
		 */
                cursortype = GDK_WATCH;
                break;
        /* The following we're not sure about */
        case GUI_POINTER_NO_DROP:
        case GUI_POINTER_NOT_ALLOWED:
        case GUI_POINTER_DEFAULT:
        default:
              nullcursor = true;
        }

        if (!nullcursor)
                cursor = gdk_cursor_new_for_display(
				gtk_widget_get_display(
					GTK_WIDGET(g->drawing_area)),
					cursortype);
        gdk_window_set_cursor(GTK_WIDGET(g->drawing_area)->window, cursor);

	if (!nullcursor)
                gdk_cursor_unref(cursor);
}

void gui_window_hide_pointer(struct gui_window *g)
{

}

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
	nsgtk_redraw_caret(g);

	g->caretx = x;
	g->carety = y + 1;
	g->careth = height;

	nsgtk_redraw_caret(g);

	gtk_widget_grab_focus(GTK_WIDGET(g->drawing_area));
}

void gui_window_remove_caret(struct gui_window *g)
{
	int oh = g->careth;

	if (oh == 0)
		return;

	g->careth = 0;

	gui_window_redraw(g, g->caretx, g->carety,
                          g->caretx, g->carety + oh);
}

void gui_window_new_content(struct gui_window *g)
{

}

bool gui_window_scroll_start(struct gui_window *g)
{
	return true;
}

bool gui_window_box_scroll_start(struct gui_window *g,
                    int x0, int y0, int x1, int y1)
{
	return true;
}

void gui_drag_save_object(gui_save_type type, struct content *c,
                          struct gui_window *g)
{

}

void gui_drag_save_selection(struct selection *s, struct gui_window *g)
{

}

void gui_start_selection(struct gui_window *g)
{

}

void gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{

}

bool gui_empty_clipboard(void)
{
	return true;
}

bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
  	return true;
}

bool gui_commit_clipboard(void)
{
	return true;
}


bool gui_copy_to_clipboard(struct selection *s)
{
	return true;
}


void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
                               bool scaled)
{
	*width = GTK_WIDGET(g->viewport)->allocation.width;
	*height = GTK_WIDGET(g->viewport)->allocation.height;

	if (scaled) {
		*width /= g->bw->scale;
		*height /= g->bw->scale;
	}
}

bool gui_window_frame_resize_start(struct gui_window *g)
{
	return true;
}
