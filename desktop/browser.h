/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Browser window creation and manipulation (interface).
 */

#ifndef _NETSURF_DESKTOP_BROWSER_H_
#define _NETSURF_DESKTOP_BROWSER_H_

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

struct box;
struct content;
struct form;
struct form_control;
struct form_successful_control;
struct gui_window;
struct history;
struct selection;
struct browser_window;


typedef void (*browser_caret_callback)(struct browser_window *bw,
	wchar_t key, void *p);
typedef bool (*browser_paste_callback)(struct browser_window *bw,
	const char *utf8, unsigned utf8_len, bool last, void *p);

/** Browser window data. */
struct browser_window {
	/** Page currently displayed, or 0. Must have status READY or DONE. */
	struct content *current_content;
	/** Page being loaded, or 0. */
	struct content *loading_content;

	/** Window history structure. */
	struct history *history;

	/** Selection state */
	struct selection *sel;

	/** Handler for keyboard input, or 0. */
	browser_caret_callback caret_callback;
	/** Handler for pasting text, or 0. */
	browser_paste_callback paste_callback;

	/** User parameter for caret_callback and paste_callback */
	void *caret_p;

	/** Platform specific window data. */
	struct gui_window *window;

	/** Busy indicator is active. */
	bool throbbing;
	/** Add loading_content to the window history when it loads. */
	bool history_add;
	/** Start time of fetching loading_content. */
	clock_t time0;

	/** Fragment identifier for current_content. */
	char *frag_id;

	/** Current drag status. */
	enum {
		DRAGGING_NONE,
		DRAGGING_VSCROLL,
		DRAGGING_HSCROLL,
		DRAGGING_SELECTION,
		DRAGGING_PAGE_SCROLL,
		DRAGGING_2DSCROLL
	} drag_type;

	/** Box currently being scrolled, or 0. */
	struct box *scrolling_box;
	/** Mouse position at start of current scroll drag. */
	int scrolling_start_x;
	int scrolling_start_y;
	/** Scroll offsets at start of current scroll draw. */
	int scrolling_start_scroll_x;
	int scrolling_start_scroll_y;
	/** Well dimensions for current scroll drag. */
	int scrolling_well_width;
	int scrolling_well_height;

	/** Referer for current fetch, or 0. */
	char *referer;

	/** Current fetch is download */
	bool download;
};


typedef enum {
	BROWSER_MOUSE_CLICK_1  = 1,  /* primary mouse button down (eg. Select) */
	BROWSER_MOUSE_CLICK_2  = 2,

	BROWSER_MOUSE_DRAG_1   = 8,  /* start of drag operation */
	BROWSER_MOUSE_DRAG_2   = 16,

	BROWSER_MOUSE_HOLDING_1 = 64,   /* whilst drag is in progress */
	BROWSER_MOUSE_HOLDING_2 = 128,

	BROWSER_MOUSE_MOD_1    = 512,  /* primary modifier key pressed (eg. Shift) */
	BROWSER_MOUSE_MOD_2    = 1024
} browser_mouse_state;


extern struct browser_window *current_redraw_browser;

void browser_window_create(const char *url, struct browser_window *clone,
		char *referer);
void browser_window_go(struct browser_window *bw, const char *url,
		char *referer);
void browser_window_go_post(struct browser_window *bw, const char *url,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool history_add, char *referer, bool download);
void browser_window_stop(struct browser_window *bw);
void browser_window_reload(struct browser_window *bw, bool all);
void browser_window_destroy(struct browser_window *bw);
void browser_window_update(struct browser_window *bw, bool scroll_to_top);

void browser_window_mouse_click(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y);
void browser_window_mouse_track(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y);
void browser_window_mouse_drag_end(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y);

bool browser_window_key_press(struct browser_window *bw, wchar_t key);
bool browser_window_paste_text(struct browser_window *bw, const char *utf8,
		unsigned utf8_len, bool last);
void browser_window_form_select(struct browser_window *bw,
		struct form_control *control, int item);
void browser_redraw_box(struct content *c, struct box *box);
void browser_form_submit(struct browser_window *bw, struct form *form,
		struct form_control *submit_button);

void browser_window_redraw_rect(struct browser_window *bw, int x, int y,
		int width, int height);

/* In platform specific hotlist.c. */
void hotlist_visited(struct content *content);

/* In platform specific history.c. */
struct history *history_create(void);
void history_add(struct history *history, struct content *content,
		char *frag_id);
void history_update(struct history *history, struct content *content);
void history_destroy(struct history *history);
void history_back(struct browser_window *bw, struct history *history);
void history_forward(struct browser_window *bw, struct history *history);
bool history_back_available(struct history *history);
bool history_forward_available(struct history *history);

/* In platform specific global_history.c. */
void global_history_add(struct gui_window *g);
void global_history_add_recent(const char *url);
char **global_history_get_recent(int *count);


/* In platform specific schedule.c. */
void schedule(int t, void (*callback)(void *p), void *p);
void schedule_remove(void (*callback)(void *p), void *p);
void schedule_run(void);

/* In platform specific theme_install.c. */
#ifdef WITH_THEME_INSTALL
void theme_install_start(struct content *c);
#endif

#endif
