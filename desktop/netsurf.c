/**
 * $Id: netsurf.c,v 1.10 2003/06/05 13:17:55 philpem Exp $
 */

#include "netsurf/desktop/options.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/content/cache.h"
#include "netsurf/content/fetch.h"
#include "netsurf/utils/log.h"
#include <stdlib.h>

int netsurf_quit = 0;
gui_window* netsurf_gui_windows = NULL;

static void netsurf_init(int argc, char** argv);
static void netsurf_exit(void);


void netsurf_poll(void)
{
  gui_poll();
  fetch_poll();
}


void netsurf_init(int argc, char** argv)
{
  stdout = stderr;
  options_init(&OPTIONS);
  options_read(&OPTIONS, NULL);
  gui_init(argc, argv);
  fetch_init();
  cache_init();
  nspng_init();
  nsgif_init();
}


void netsurf_exit(void)
{
  cache_quit();
  fetch_quit();
}


int main(int argc, char** argv)
{
  netsurf_init(argc, argv);

  while (netsurf_quit == 0)
    netsurf_poll();

  LOG(("Netsurf quit!"));
  netsurf_exit();

  return 0;
}


