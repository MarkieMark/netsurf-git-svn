/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net> 
 */

/** \file
 * Free text search (implementation)
 */

#include <ctype.h>
#include <string.h>

#include "oslib/hourglass.h"
#include "oslib/wimp.h"

#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/selection.h"
#include "netsurf/render/box.h"
#include "netsurf/render/html.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_SEARCH

#ifndef NOF_ELEMENTS
#define NOF_ELEMENTS(array) (sizeof(array)/sizeof(*(array)))
#endif

struct list_entry {
	/* start position of match */
	struct box *start_box;
	unsigned start_idx;
	/* end of match */
	struct box *end_box;
	unsigned end_idx;

	struct list_entry *prev;
	struct list_entry *next;
};

struct gui_window *search_current_window = 0;
static struct selection *search_selection = 0;

static char *search_string = 0;
static struct list_entry search_head = { 0, 0, 0, 0, 0, 0 };
static struct list_entry *search_found = &search_head;
static struct list_entry *search_current = 0;
static struct content *search_content = 0;
static bool search_prev_case_sens = false;

#define RECENT_SEARCHES 8
bool search_insert;
static char *recent_search[RECENT_SEARCHES];
static wimp_MENU(RECENT_SEARCHES) menu_recent;
wimp_menu *recent_search_menu = (wimp_menu *)&menu_recent;
#define DEFAULT_FLAGS (wimp_ICON_TEXT | wimp_ICON_FILLED | \
		(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | \
		(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT))


static void start_search(bool forwards);
static void do_search(char *string, int string_len, bool case_sens, bool forwards);
static const char *find_pattern(const char *string, int s_len,
		const char *pattern, int p_len, bool case_sens, int *m_len);
static bool find_occurrences(const char *pattern, int p_len, struct box *cur,
		bool case_sens);
static void show_status(bool found);

static bool ro_gui_search_next(wimp_w w);
static bool ro_gui_search_click(wimp_pointer *pointer);
static bool ro_gui_search_keypress(wimp_key *key);
static void ro_gui_search_add_recent(char *search);

void ro_gui_search_init(void) {
	dialog_search = ro_gui_dialog_create("search");
	ro_gui_wimp_event_register_keypress(dialog_search,
			ro_gui_search_keypress);
	ro_gui_wimp_event_register_close_window(dialog_search,
			ro_gui_search_end);
	ro_gui_wimp_event_register_menu_gright(dialog_search, ICON_SEARCH_TEXT,
			ICON_SEARCH_MENU, recent_search_menu);
	ro_gui_wimp_event_register_text_field(dialog_search, ICON_SEARCH_STATUS);
	ro_gui_wimp_event_register_checkbox(dialog_search, ICON_SEARCH_CASE_SENSITIVE);
	ro_gui_wimp_event_register_mouse_click(dialog_search,
			ro_gui_search_click);
	ro_gui_wimp_event_register_ok(dialog_search, ICON_SEARCH_FIND_NEXT,
			ro_gui_search_next);
	ro_gui_wimp_event_register_cancel(dialog_search, ICON_SEARCH_CANCEL);
	ro_gui_wimp_event_set_help_prefix(dialog_search, "HelpSearch");

	recent_search_menu->title_data.indirected_text.text =
			(char*)messages_get("Search");
	ro_gui_menu_init_structure(recent_search_menu, RECENT_SEARCHES);
}

/**
 * Wrapper for the pressing of an OK button for wimp_event.
 *
 * \return false, to indicate the window should not be closed
 */
bool ro_gui_search_next(wimp_w w) {
	search_insert = true;
	start_search(true);
	return false;
}

bool ro_gui_search_click(wimp_pointer *pointer) {
	switch (pointer->i) {
	  	case ICON_SEARCH_FIND_PREV:
			search_insert = true;
			start_search(false);
			return true;
		case ICON_SEARCH_CASE_SENSITIVE:
			start_search(true);
			return true;
	}
	return false;
}

void ro_gui_search_add_recent(char *search) {
  	char *tmp;
  	int i;

	if ((search == NULL) || (search[0] == '\0'))
		return;

	if (!search_insert) {
	  	free(recent_search[0]);
	  	recent_search[0] = strdup(search);
		ro_gui_search_prepare_menu();
		return;
	}

	if ((recent_search[0] != NULL) &&
			(!strcmp(recent_search[0], search)))
		return;	  

	tmp = strdup(search);
	if (!tmp) {
	  	warn_user("NoMemory", 0);
	  	return;
	}
	free(recent_search[RECENT_SEARCHES - 1]);
	for (i = RECENT_SEARCHES - 1; i > 0; i--)
		recent_search[i] = recent_search[i - 1];
	recent_search[0] = tmp;
	search_insert = false;
	
	ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_MENU, false);
	ro_gui_search_prepare_menu();
}

bool ro_gui_search_prepare_menu(void) {
	os_error *error;
	int i;
	int suggestions = 0;
	
	for (i = 0; i < RECENT_SEARCHES; i++)
		if (recent_search[i] != NULL)
			suggestions++;

	if (suggestions == 0)
		return false;

	for (i = 0; i < suggestions; i++) {
		recent_search_menu->entries[i].menu_flags &= ~wimp_MENU_LAST;
		recent_search_menu->entries[i].data.indirected_text.text =
				recent_search[i];
		recent_search_menu->entries[i].data.indirected_text.size =
				strlen(recent_search[i]) + 1;
	}
	recent_search_menu->entries[suggestions - 1].menu_flags |= wimp_MENU_LAST;

	if ((current_menu_open) && (current_menu == recent_search_menu)) {
		error = xwimp_create_menu(current_menu, 0, 0);
		if (error) {
			LOG(("xwimp_create_menu: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MenuError", error->errmess);
		}
	}
	return true;
}

/**
 * Open the search dialog
 *
 * \param g the gui window to search
 */
void ro_gui_search_prepare(struct gui_window *g)
{
	struct content *c;

	assert(g != NULL);

	/* if the search dialogue is reopened over a new window, we still
	   need to cancel the previous search */
	ro_gui_search_end(dialog_search);

	search_current_window = g;

	ro_gui_set_icon_string(dialog_search, ICON_SEARCH_TEXT, "");
	ro_gui_set_icon_selected_state(dialog_search,
				ICON_SEARCH_CASE_SENSITIVE, false);

	c = search_current_window->bw->current_content;

	/* only handle html contents */
	if ((!c) || (c->type != CONTENT_HTML))
		return;

	show_status(true);
	ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_FIND_PREV, true);
	ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_FIND_NEXT, true);

/* \todo build me properly please! */
	search_selection = selection_create(g->bw);
	if (!search_selection)
		warn_user("NoMemory", 0);

	selection_init(search_selection, c->data.html.layout);
	ro_gui_wimp_event_memorise(dialog_search);
	search_insert = true;
}

/**
 * Handle keypresses in the search dialog
 *
 * \param key wimp_key block
 * \return true if keypress handled, false otherwise
 */
bool ro_gui_search_keypress(wimp_key *key)
{
	bool state;

	switch (key->c) {
		case 9: /* ctrl i */
			state = ro_gui_get_icon_selected_state(dialog_search, ICON_SEARCH_CASE_SENSITIVE);
			ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_CASE_SENSITIVE, !state);
			start_search(true);
			return true;
		case wimp_KEY_UP:
			search_insert = true;
			start_search(false);
			return true;
		case wimp_KEY_DOWN:
			search_insert = true;
			start_search(true);
			return true;

		default:
			if (key->c == 21) /* ctrl+u means the user's starting a new search */
				search_insert = true;
			if (key->c == 8  || /* backspace */
			    key->c == 21 || /* ctrl u */
			    (key->c >= 0x20 && key->c <= 0x7f)) {
				start_search(true);
				return true;
			}
			break;
	}

	return false;
}

/**
 * Begins/continues the search process
 * Note that this may be called many times for a single search.
 *
 * \param  forwards  search forwards from start/current position
 */

void start_search(bool forwards)
{
	int string_len;
	char *string;

	string = ro_gui_get_icon_string(dialog_search, ICON_SEARCH_TEXT);
	assert(string);

	ro_gui_search_add_recent(string);

	string_len = strlen(string);
	if (string_len <= 0) {
		show_status(true);
		ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_FIND_PREV, true);
		ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_FIND_NEXT, true);
		gui_window_set_scroll(search_current_window, 0, 0);
		return;
	}

	do_search(string, string_len,
		ro_gui_get_icon_selected_state(dialog_search,
						ICON_SEARCH_CASE_SENSITIVE),
		forwards);
}

/**
 * Ends the search process, invalidating all global state and
 * freeing the list of found boxes
 *
 * \param w	the search window handle (not used)
 */
void ro_gui_search_end(wimp_w w)
{
	struct list_entry *a, *b;


	if (search_selection) {
		selection_clear(search_selection, true);
		selection_destroy(search_selection);
	}
	search_selection = 0;

	search_current_window = 0;

	if (search_string) {
		ro_gui_search_add_recent(search_string);
		free(search_string);
	}
	search_string = 0;

	for (a = search_found->next; a; a = b) {
		b = a->next;
		free(a);
	}
	search_found->prev = 0;
	search_found->next = 0;

	search_current = 0;

	search_content = 0;

	search_prev_case_sens = false;
}

/**
 * Search for a string in the box tree
 *
 * \param string the string to search for
 * \param string_len length of search string
 * \param case_sens whether to perform a case sensitive search
 * \param forwards direction to search in
 */
void do_search(char *string, int string_len, bool case_sens, bool forwards)
{
	struct content *c;
	struct box *box;
	struct list_entry *a, *b;
	int x0,y0,x1,y1;
	bool new = false;

	if (!search_current_window)
		return;

	c = search_current_window->bw->current_content;

	/* only handle html contents */
	if ((!c) || (c->type != CONTENT_HTML))
		return;

	box = c->data.html.layout;

	if (!box)
		return;

//	LOG(("'%s' - '%s' (%p, %p) %p (%d, %d) %d", search_string, string, search_content, c, search_found->next, search_prev_case_sens, case_sens, forwards));

	selection_clear(search_selection, true);

	/* check if we need to start a new search or continue an old one */
	if (!search_string || c != search_content || !search_found->next ||
	    search_prev_case_sens != case_sens ||
	    (case_sens && strcmp(string, search_string) != 0) ||
	    (!case_sens && strcasecmp(string, search_string) != 0)) {

		if (search_string)
			free(search_string);
		search_current = 0;
		for (a = search_found->next; a; a = b) {
			b = a->next;
			free(a);
		}
		search_found->prev = 0;
		search_found->next = 0;

		search_string = strdup(string);

		xhourglass_on();
		if (!find_occurrences(string, string_len, box, case_sens)) {
			for (a = search_found->next; a; a = b) {
				b = a->next;
				free(a);
			}
			search_found->prev = 0;
			search_found->next = 0;

			xhourglass_off();
			return;
		}
		xhourglass_off();

		new = true;
		search_content = c;
		search_prev_case_sens = case_sens;
	}

//	LOG(("%d %p %p (%p, %p)", new, search_found->next, search_current, search_current->prev, search_current->next));

	if (new) {
		/* new search, beginning at the top of the page */
		search_current = search_found->next;
	}
	else if (search_current) {
		/* continued search in the direction specified */
		if (forwards) {
			if (search_current->next)
				search_current = search_current->next;
		}
		else {
			if (search_current->prev)
				search_current = search_current->prev;
		}
	}

	show_status(search_current != NULL);

	ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_FIND_PREV,
		!search_current || !search_current->prev);
	ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_FIND_NEXT,
		!search_current || !search_current->next);

	if (!search_current)
		return;

	selection_set_start(search_selection, search_current->start_box,
		search_current->start_idx);
	selection_set_end(search_selection, search_current->end_box,
		search_current->end_idx);

	/* get box position and jump to it */
	box_coords(search_current->start_box, &x0, &y0);
	x0 += 0;	/* \todo: move x0 in by correct idx */
	box_coords(search_current->end_box, &x1, &y1);
	x1 += search_current->end_box->width;	/* \todo: move x1 in by correct idx */
	y1 += search_current->end_box->height;
	
	gui_window_scroll_visible(search_current_window, x0, y0, x1, y1);

}


/**
 * Find the first occurrence of 'match' in 'string' and return its index
 *
 * /param  string     the string to be searched (unterminated)
 * /param  s_len      length of the string to be searched
 * /param  pattern    the pattern for which we are searching (unterminated)
 * /param  p_len      length of pattern
 * /param  case_sens  true iff case sensitive match required
 * /param  m_len      accepts length of match in bytes
 * /return pointer to first match, NULL if none
 */

const char *find_pattern(const char *string, int s_len, const char *pattern,
		int p_len, bool case_sens, int *m_len)
{
	struct { const char *ss, *s, *p; bool first; } context[16];
	const char *ep = pattern + p_len;
	const char *es = string  + s_len;
	const char *p = pattern - 1;  /* a virtual '*' before the pattern */
	const char *ss = string;
	const char *s = string;
	bool first = true;
	int top = 0;

	while (p < ep) {
		bool matches;
		if (p < pattern || *p == '*') {
			char ch;

			/* skip any further asterisks; one is the same as many */
			do p++; while (p < ep && *p == '*');

			/* if we're at the end of the pattern, yes, it matches */
			if (p >= ep) break;

			/* anything matches a # so continue matching from
			   here, and stack a context that will try to match
			   the wildcard against the next character */
			
			ch = *p;
			if (ch != '#') {
				/* scan forwards until we find a match for this char */
				if (!case_sens) ch = toupper(ch);
				while (s < es) {
					if (case_sens) {
						if (*s == ch) break;
					} else if (toupper(*s) == ch)
						break;
					s++;
				}
			}

			if (s < es) {
				/* remember where we are in case the match fails;
					we can then resume */
				if (top < (int)NOF_ELEMENTS(context)) {
					context[top].ss = ss;
					context[top].s  = s + 1;
					context[top].p  = p - 1;    /* ptr to last asterisk */
					context[top].first = first;
					top++;
				}

				if (first) {
					ss = s;  /* remember first non-'*' char */
					first = false;
				}

				matches = true;
			}
			else
				matches = false;
		}
		else if (s < es) {
			char ch = *p;
			if (ch == '#')
				matches = true;
			else {
				if (case_sens)
					matches = (*s == ch);
				else
					matches = (toupper(*s) == toupper(ch));
			}
			if (matches && first) {
				ss = s;  /* remember first non-'*' char */
				first = false;
			}
		}
		else
			matches = false;

		if (matches) {
			p++; s++;
		}
		else {
			/* doesn't match, resume with stacked context if we have one */
			if (--top < 0) return NULL;  /* no match, give up */
			
			ss = context[top].ss;
			s  = context[top].s;
			p  = context[top].p;
			first = context[top].first;
		}
	}

	/* end of pattern reached */
	*m_len = s - ss;
	return ss;
}


/**
 * Finds all occurrences of a given string in the box tree
 *
 * \param pattern   the string pattern to search for
 * \param p_len     pattern length
 * \param cur       pointer to the current box
 * \param case_sens whether to perform a case sensitive search
 * \return true on success, false on memory allocation failure
 */
bool find_occurrences(const char *pattern, int p_len, struct box *cur,
		bool case_sens)
{
	struct box *a;

	/* ignore this box, if there's no visible text */
	if (!cur->object && cur->text) {
		const char *text = cur->text;
		unsigned length = cur->length;

		while (length > 0) {
			unsigned match_length;
			unsigned match_offset;
			const char *new_text;
			struct list_entry *entry;
			const char *pos = find_pattern(text, length,
					pattern, p_len, case_sens, &match_length);
			if (!pos) break;

			/* found string in box => add to list */
			entry = calloc(1, sizeof(*entry));
			if (!entry) {
				warn_user("NoMemory", 0);
				return false;
			}
	
			match_offset = pos - cur->text;
	
			entry->start_box = cur;
			entry->start_idx = match_offset;
			entry->end_box = cur;
			entry->end_idx = match_offset + match_length;
			entry->next = 0;
			entry->prev = search_found->prev;
			if (!search_found->prev)
				search_found->next = entry;
			else
				search_found->prev->next = entry;
			search_found->prev = entry;

			new_text = pos + match_length;
			length -= (new_text - text);
			text = new_text;
		}
	}

	/* and recurse */
	for (a = cur->children; a; a = a->next) {
		if (!find_occurrences(pattern, p_len, a, case_sens))
			return false;
	}

	return true;
}



/**
 * Determines whether any portion of the given text box should be
 * selected because it matches the current search string.
 *
 * \param  g          gui window
 * \param  box        box being tested
 * \param  start_idx  byte offset within text box of highlight start
 * \param  end_idx    byte offset of highlight end
 * \return true iff part of the box should be highlighted
 */

bool gui_search_term_highlighted(struct gui_window *g, struct box *box,
		unsigned *start_idx, unsigned *end_idx) 
{
	if (g == search_current_window && search_selection) {
		if (selection_defined(search_selection))
			return selection_highlighted(search_selection, box,
					start_idx, end_idx);
	}
	return false;
}


/**
 * Change the displayed search status.
 *
 * \param found  search pattern matched in text
 */

void show_status(bool found)
{
	ro_gui_set_icon_string(dialog_search, ICON_SEARCH_STATUS,
			found ? "" : messages_get("Notfound"));
}

#endif
