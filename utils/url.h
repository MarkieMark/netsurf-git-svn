/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * URL parsing and joining (interface).
 */

#ifndef _NETSURF_UTILS_URL_H_
#define _NETSURF_UTILS_URL_H_

typedef enum {
	URL_FUNC_OK,     /**< No error */
	URL_FUNC_NOMEM,  /**< Insufficient memory */
	URL_FUNC_FAILED  /**< Non fatal error (eg failed to match regex) */
} url_func_result;

void url_init(void);
url_func_result url_normalize(const char *url, char **result);
url_func_result url_join(const char *rel, const char *base, char **result);
url_func_result url_host(const char *url, char **result);
url_func_result url_scheme(const char *url, char **result);
url_func_result url_nice(const char *url, char **result,
		bool remove_extensions);
url_func_result url_escape(const char *unescaped, char **result);
url_func_result url_canonical_root(const char *url, char **result);
url_func_result url_strip_lqf(const char *url, char **result);
url_func_result url_plq(const char *url, char **result);
url_func_result url_path(const char *url, char **result);
url_func_result url_compare(const char *url1, const char *url2,
		bool *result);

char *path_to_url(const char *path);
char *url_to_path(const char *url);

#endif
