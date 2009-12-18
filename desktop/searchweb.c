/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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
 * web search (core)
 */
#include "utils/config.h"

#include <ctype.h>
#include <string.h>
#include "content/content.h"
#include "content/fetchcache.h"
#include "content/fetch.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "desktop/searchweb.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"

static struct search_provider {
	char *name; /**< readable name such as 'google', 'yahoo', etc */
	char *hostname; /**< host address such as www.google.com */
	char *searchstring; /** < such as "www.google.com?search=%s" */
	char *ico; /** < location of domain's favicon */
} current_search_provider;

static struct content *search_ico = NULL;
char *search_engines_file_location;
char *search_default_ico_location;

/** 
 * creates a new browser window according to the search term
 * \param searchterm such as "my search term"
 */

bool search_web_new_window(struct browser_window *bw, const char *searchterm)
{
	char *encsearchterm;
	char *url;
	if (url_escape(searchterm,0, true, NULL, &encsearchterm) != 
			URL_FUNC_OK)
		return false;
	url = search_web_get_url(encsearchterm);
	free(encsearchterm);
	browser_window_create(url, bw, NULL, false, true);
	free(url);
	return true;
}

/** simplistic way of checking whether an entry from the url bar is an
 * url / a search; could be improved to properly test terms
 */

bool search_is_url(const char *url)
{
	char *url2, *host;
	
	if (url_normalize(url, &url2) != URL_FUNC_OK)
		return false;
	
	if (url_host(url2, &host) != URL_FUNC_OK) {
		free(url2);
		return false;
	}
	free(url2);
	return true;
}

/**
 * caches the details of the current web search provider
 * \param reference the enum value of the provider
 * browser init code [as well as changing preferences code] should call
 * search_web_provider_details(option_search_provider)
 */

void search_web_provider_details(int reference)
{
	char buf[300];
	int ref = 0;
	if (search_engines_file_location == NULL)
		return;
	FILE *f = fopen(search_engines_file_location, "r");
	if (f == NULL)
		return;
	while (fgets(buf, sizeof(buf), f) != NULL) {
		if (buf[0] == '\0')
			continue;
		buf[strlen(buf)-1] = '\0';
		if (ref++ == (int)reference)
			break;
	}
	fclose(f);
	if (current_search_provider.name != NULL)
		free(current_search_provider.name);
	current_search_provider.name = strdup(strtok(buf, "|"));
	if (current_search_provider.hostname != NULL)
		free(current_search_provider.hostname);
	current_search_provider.hostname = strdup(strtok(NULL, "|"));
	if (current_search_provider.searchstring != NULL)
		free(current_search_provider.searchstring);
	current_search_provider.searchstring = strdup(strtok(NULL, "|"));
	if (current_search_provider.ico != NULL)
		free(current_search_provider.ico);
	current_search_provider.ico = strdup(strtok(NULL, "|"));
	return;
}

/**
 * escapes a search term then creates the appropriate url from it
 */

char *search_web_from_term(const char *searchterm)
{
	char *encsearchterm, *url;
	if (url_escape(searchterm, 0, true, NULL, &encsearchterm)
			!= URL_FUNC_OK)
		return strdup(searchterm);
	url = search_web_get_url(encsearchterm);
	free(encsearchterm);
	return url;
}

/** accessor for global search provider name */

char *search_web_provider_name(void)
{
	if (current_search_provider.name)
		return strdup(current_search_provider.name);
	return strdup("google");
}

/** accessor for global search provider hostname */

char *search_web_provider_host(void)
{
	if (current_search_provider.hostname)
		return strdup(current_search_provider.hostname);
	return strdup("www.google.com");
}

/** accessor for global search provider ico name */

char *search_web_ico_name(void)
{
	if (current_search_provider.ico)
		return strdup(current_search_provider.ico);
	return strdup("http://www.google.com/favicon.ico");
}

/**
 * creates a full url from an encoded search term
 */

char *search_web_get_url(const char *encsearchterm)
{
	char *pref, *ret;
	int len;
	if (current_search_provider.searchstring)
		pref = strdup(current_search_provider.searchstring);
	else
		pref = strdup("http://www.google.com/search?q=%s");
	if (pref == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}
	len = strlen(encsearchterm) + strlen(pref);
	ret = malloc(len -1); /* + '\0' - "%s" */
	if (ret == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(pref);
		return NULL;
	}
	snprintf(ret, len-1, pref, encsearchterm);
	free(pref);
	return ret;
}

/**
 * function to retrieve the search web ico, from cache / from local
 * filesystem / from the web
 * \param localdefault true when there is no appropriate favicon
 * update the search_ico cache else delay until fetcher callback
 */

void search_web_retrieve_ico(bool localdefault)
{
	char *url;
	if (localdefault) {
		if (search_default_ico_location == NULL)
			return;
		url = malloc(SLEN("file://") + strlen(
				search_default_ico_location) + 1);
		if (url == NULL) {
			warn_user(messages_get("NoMemory"), 0);
			return;
		}
		strcpy(url, "file://");
		strcat(url, search_default_ico_location);
	} else {
		url = search_web_ico_name();
	}

	struct content *icocontent = NULL;
	if (url == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return;
	}
	icocontent = fetchcache(url, search_web_ico_callback,
				0, 0, 20, 20, true, 0,
				0, false, false);
	free(url);
	if (icocontent == NULL)
		return;
	
	fetchcache_go(icocontent, 0, search_web_ico_callback,
			0, 0, 20, 20,
			0, 0, false, 0);

	if (icocontent == NULL) 
		LOG(("web search ico loading delayed"));
	else
		search_ico = icocontent;
}

/**
 * returns a reference to the static global search_ico [ / NULL]
 * caller may adjust ico's settings; clearing / free()ing is the core's
 * responsibility
 */

struct content *search_web_ico(void)
{
	return search_ico;
}

/**
 * callback function to cache ico then notify front when successful
 * else retry default from local file system
 */

void search_web_ico_callback(content_msg msg, struct content *ico,
		intptr_t p1, intptr_t p2, union content_msg_data data)
{

	switch (msg) {
	case CONTENT_MSG_LOADING:
	case CONTENT_MSG_READY:
		break;

	case CONTENT_MSG_DONE:
		LOG(("got favicon '%s'", ico->url));
		if (ico->type == CONTENT_ICO) {
			search_ico = ico; /* cache */
			gui_window_set_search_ico(search_ico);
		} else {
			search_web_retrieve_ico(true);
		}
		break;

	case CONTENT_MSG_LAUNCH:
	case CONTENT_MSG_ERROR:
		LOG(("favicon %s error: %s", ico->url, data.error));
		ico = 0;
		search_web_retrieve_ico(true);
		break;

	case CONTENT_MSG_STATUS:
	case CONTENT_MSG_NEWPTR:
	case CONTENT_MSG_AUTH:
	case CONTENT_MSG_SSL:
		break;

	default:
		assert(0);
	}
}
