/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Fetching of data from a URL (interface).
 */

#ifndef _NETSURF_DESKTOP_FETCH_H_
#define _NETSURF_DESKTOP_FETCH_H_

#include <stdbool.h>
#include <sys/select.h>
#include "utils/config.h"

typedef enum {
              FETCH_TYPE,
              FETCH_PROGRESS,
              FETCH_DATA,
              FETCH_FINISHED,
              FETCH_ERROR,
              FETCH_REDIRECT,
              FETCH_NOTMODIFIED,
#ifdef WITH_AUTH
              FETCH_AUTH,
#endif
#ifdef WITH_SSL
              FETCH_CERT_ERR,
#endif
} fetch_msg;

struct content;
struct fetch;
struct form_successful_control;

struct cache_data {
	time_t req_time;	/**< Time of request */
	time_t res_time;	/**< Time of response */
	time_t date;		/**< Date: response header */
	time_t expires;		/**< Expires: response header */
#define INVALID_AGE -1
	int age;		/**< Age: response header */
	int max_age;		/**< Max-age Cache-control parameter */
	bool no_cache;		/**< no-cache Cache-control parameter */
	char *etag;		/**< Etag: response header */
	time_t last_modified;	/**< Last-Modified: response header */
};

#ifdef WITH_SSL
struct ssl_cert_info {
	long version;		/**< Certificate version */
	char not_before[32];	/**< Valid from date */
	char not_after[32];	/**< Valid to date */
	int sig_type;		/**< Signature type */
	long serial;		/**< Serial number */
	char issuer[256];	/**< Issuer details */
	char subject[256];	/**< Subject details */
	int cert_type;		/**< Certificate type */
};
#endif

extern bool fetch_active;

typedef void (*fetch_callback)(fetch_msg msg, void *p, const void *data,
                               unsigned long size);


void fetch_init(void);
struct fetch * fetch_start(const char *url, const char *referer,
                           fetch_callback callback,
                           void *p, bool only_2xx, const char *post_urlenc,
                           struct form_successful_control *post_multipart,
                           bool verifiable, const char *parent_url, char *headers[]);
void fetch_abort(struct fetch *f);
void fetch_poll(void);
void fetch_quit(void);
const char *fetch_filetype(const char *unix_path);
char *fetch_mimetype(const char *ro_path);
bool fetch_can_fetch(const char *url);
void fetch_change_callback(struct fetch *fetch,
                           fetch_callback callback,
                           void *p);
long fetch_http_code(struct fetch *fetch);
const char *fetch_get_referer(struct fetch *fetch);

/* API for fetchers themselves */

typedef bool (*fetcher_initialise)(const char *);
typedef void* (*fetcher_setup_fetch)(struct fetch *, const char *,
                                     bool, const char *, 
                                     struct form_successful_control *, bool,
                                     const char *, const char **);
typedef bool (*fetcher_start_fetch)(void *);
typedef void (*fetcher_abort_fetch)(void *);
typedef void (*fetcher_free_fetch)(void *);
typedef void (*fetcher_poll_fetcher)(const char *);
typedef void (*fetcher_finalise)(const char *);

bool fetch_add_fetcher(const char *scheme,
                       fetcher_initialise initialiser,
                       fetcher_setup_fetch setup_fetch,
                       fetcher_start_fetch start_fetch,
                       fetcher_abort_fetch abort_fetch,
                       fetcher_free_fetch free_fetch,
                       fetcher_poll_fetcher poll_fetcher,
                       fetcher_finalise finaliser);

void fetch_send_callback(fetch_msg msg, struct fetch *fetch, void *data, unsigned long size);
void fetch_can_be_freed(struct fetch *fetch);
void fetch_set_http_code(struct fetch *fetch, long http_code);
const char *fetch_get_referer_to_send(struct fetch *fetch);
#endif
