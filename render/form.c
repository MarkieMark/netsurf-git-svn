/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

/** \file
 * Form handling functions (implementation).
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "curl/curl.h"
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/utils/utils.h"


static char *form_textarea_value(struct form_control *textarea);


/**
 * Create a struct form_control.
 *
 * \param  type  control type
 * \return  a new structure, or 0 on memory exhaustion
 */

struct form_control *form_new_control(form_control_type type)
{
	struct form_control *control;

	if ((control = malloc(sizeof *control)) == NULL)
		return NULL;
	control->type = type;
	control->name = NULL;
	control->value = NULL;
	control->initial_value = NULL;
	control->disabled = false;
	control->form = NULL;
	control->box = NULL;
	control->caret_inline_container = NULL;
	control->caret_text_box = NULL;
	control->caret_box_offset = control->caret_form_offset = 0;
	control->length = control->maxlength = 0;
	control->selected = false;
	control->prev = NULL;
	control->next = NULL;
	return control;
}


/**
 * Add a control to the list of controls in a form.
 */

void form_add_control(struct form *form, struct form_control *control)
{
	control->form = form;
	if (form->controls != NULL) {
		assert(form->last_control);
		form->last_control->next = control;
		control->prev = form->last_control;
		control->next = NULL;
		form->last_control = control;
	} else {
		form->controls = form->last_control = control;
	}
}


/**
 * Free a struct form_control.
 *
 * \param  control  structure to free
 */

void form_free_control(struct form_control *control)
{
	free(control->name);
	free(control->value);
	free(control->initial_value);
	if (control->type == GADGET_SELECT) {
		struct form_option *option, *next;
		for (option = control->data.select.items; option;
				option = next) {
			next = option->next;
			free(option->text);
			free(option->value);
			free(option);
		}
	}
	free(control);
}


/**
 * Identify 'successful' controls.
 *
 * See HTML 4.01 section 17.13.2.
 */

struct form_successful_control *form_successful_controls(struct form *form,
		struct form_control *submit_button)
{
	struct form_control *control;
	struct form_option *option;
	struct form_successful_control sentinel, *last_success;

	last_success = &sentinel;
	sentinel.next = 0;

	for (control = form->controls; control; control = control->next) {
		struct form_successful_control *success_new;
		bool add_val;

		/* ignore disabled controls */
		if (control->disabled)
			continue;

		/* ignore controls with no name */
		if (!control->name)
			continue;

		switch (control->type) {
			case GADGET_HIDDEN:
			case GADGET_TEXTBOX:
			case GADGET_PASSWORD:
				add_val = true;
				break;

			case GADGET_RADIO:
			case GADGET_CHECKBOX:
				/* ignore checkboxes and radio buttons which
				 * aren't selected
				 */
				add_val = control->selected;
				break;

			case GADGET_SELECT:
				/* select */
				for (option = control->data.select.items;
						option != NULL;
						option = option->next) {
					if (option->selected) {
						success_new = xcalloc(1, sizeof(*success_new));
						success_new->file = false;
						success_new->name = cnv_str_local_enc(control->name);
						success_new->value = cnv_str_local_enc(option->value);
						success_new->next = NULL;
						last_success->next = success_new;
						last_success = success_new;
					}
				}

				add_val = false;
				break;

			case GADGET_TEXTAREA:
				/* textarea */
				success_new = xcalloc(1, sizeof(*success_new));
				success_new->file = false;
				success_new->name = cnv_str_local_enc(control->name);
				success_new->value = form_textarea_value(control);
				success_new->next = 0;
				last_success->next = success_new;
				last_success = success_new;

				add_val = false;
				break;

			case GADGET_IMAGE: {
				/* image */
				const size_t len = strlen(control->name) + 3;

				/* x */
				success_new = xcalloc(1, sizeof(*success_new));
				success_new->file = false;
				success_new->name = xcalloc(1, len);
				sprintf(success_new->name, "%s.x", control->name);
				success_new->value = xcalloc(1, 20);
				sprintf(success_new->value, "%i", control->data.image.mx);
				success_new->next = 0;
				last_success->next = success_new;
				last_success = success_new;

				/* y */
				success_new = xcalloc(1, sizeof(*success_new));
				success_new->file = false;
				success_new->name = xcalloc(1, len);
				sprintf(success_new->name, "%s.y", control->name);
				success_new->value = xcalloc(1, 20);
				sprintf(success_new->value, "%i", control->data.image.my);
				success_new->next = 0;
				last_success->next = success_new;
				last_success = success_new;

				add_val = false;
				break;
			}

			case GADGET_SUBMIT:
				/* only the activated submit button is
				 * successful
				 */
				add_val = (control != submit_button) ? false : true;
				break;

			case GADGET_RESET:
				/* ignore reset */
				add_val = false;
				break;

			case GADGET_FILE:
				/* file */
				success_new = xcalloc(1, sizeof(*success_new));
				success_new->file = true;
				success_new->name = cnv_str_local_enc(control->name);
				success_new->value = cnv_str_local_enc(control->value);
				success_new->next = 0;
				last_success->next = success_new;
				last_success = success_new;

				add_val = false;
				break;

			default:
				assert(0);
				break;
		}

		/* all others added if they have a value */
		if (add_val && control->value != NULL) {
			success_new = xcalloc(1, sizeof(*success_new));
			success_new->file = false;
			success_new->name = cnv_str_local_enc(control->name);
			success_new->value = cnv_str_local_enc(control->value);
			success_new->next = NULL;
			last_success->next = success_new;
			last_success = success_new;
		}
	}

	return sentinel.next;
}


/**
 * Find the value for a textarea control.
 */

char *form_textarea_value(struct form_control *textarea)
{
	unsigned int len = 1;
	char *value, *s;
	struct box *inline_container, *text_box;

	/* find required length */
	for (inline_container = textarea->box->children;
			inline_container != NULL;
			inline_container = inline_container->next) {
		for (text_box = inline_container->children;
				text_box != NULL;
				text_box = text_box->next) {
			len += text_box->length + 1;
		}
		len += 2;
	}

	/* construct value */
	s = value = xcalloc(1, len);
	for (inline_container = textarea->box->children;
			inline_container != NULL;
			inline_container = inline_container->next) {
		for (text_box = inline_container->children;
				text_box != NULL;
				text_box = text_box->next) {
			strncpy(s, text_box->text, text_box->length);
			s += text_box->length;
			*s++ = ' ';
		}
		*s++ = '\r';
		*s++ = '\n';
	}
	*s = 0;

	return value;
}


/**
 * Encode controls using application/x-www-form-urlencoded.
 */

char *form_url_encode(struct form_successful_control *control)
{
	char *s = xcalloc(1, 0);
	unsigned int len = 0, len1;

	for (; control; control = control->next) {
		const char *name = curl_escape(control->name, 0);
		const char *value = curl_escape(control->value, 0);
		len1 = len + strlen(name) + strlen(value) + 2;
		s = xrealloc(s, len1 + 1);
		sprintf(s + len, "%s=%s&", name, value);
		len = len1;
		curl_free(name);
		curl_free(value);
	}
	if (len)
		s[len - 1] = 0;
	return s;
}


/**
 * Free a linked list of form_successful_control.
 */

void form_free_successful(struct form_successful_control *control)
{
	struct form_successful_control *next;
	for (; control; control = next) {
		next = control->next;
		free(control->name);
		free(control->value);
		free(control);
	}
}
