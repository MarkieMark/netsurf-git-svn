/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

#include "netsurf/desktop/options.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/riscos/configure.h"
#include "netsurf/riscos/configure/configure.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


#define SECURITY_REFERRER 2
#define SECURITY_DURATION_FIELD 6
#define SECURITY_DURATION_INC 7
#define SECURITY_DURATION_DEC 8
#define SECURITY_DEFAULT_BUTTON 10
#define SECURITY_CANCEL_BUTTON 11
#define SECURITY_OK_BUTTON 12

static void ro_gui_options_security_default(wimp_pointer *pointer);
static bool ro_gui_options_security_ok(wimp_w w);

bool ro_gui_options_security_initialise(wimp_w w) {

	/* set the current values */
	ro_gui_set_icon_selected_state(w, SECURITY_REFERRER, 
			option_send_referer);
	ro_gui_set_icon_integer(w, SECURITY_DURATION_FIELD,
			option_expire_url);

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_checkbox(w, SECURITY_REFERRER);
	ro_gui_wimp_event_register_numeric_field(w, SECURITY_DURATION_FIELD,
			SECURITY_DURATION_DEC, SECURITY_DURATION_INC,
			0, 365, 1, 0);
	ro_gui_wimp_event_register_button(w, SECURITY_DEFAULT_BUTTON,
			ro_gui_options_security_default);
	ro_gui_wimp_event_register_cancel(w, SECURITY_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, SECURITY_OK_BUTTON,
			ro_gui_options_security_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpSecurityConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

void ro_gui_options_security_default(wimp_pointer *pointer) {
	/* set the default values */
	ro_gui_set_icon_integer(pointer->w, SECURITY_DURATION_FIELD, 28);
	ro_gui_set_icon_selected_state(pointer->w, SECURITY_REFERRER, true);
}

bool ro_gui_options_security_ok(wimp_w w) {
  	option_send_referer = ro_gui_get_icon_selected_state(w,
  			SECURITY_REFERRER);
	option_expire_url = ro_gui_get_icon_decimal(w,
			SECURITY_DURATION_FIELD, 0);

	ro_gui_save_options();
  	return true;
}