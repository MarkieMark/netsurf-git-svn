/*
 * Copyright 2008 John Tytgat <John.Tytgat@aaug.net>
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
 * Export a content as a PDF file (implementation).
 */

#include <stdbool.h>
#include "oslib/osfile.h"
#include "content/content.h"
#include "desktop/print.h"
#include "pdf/pdf_plotters.h"
#include "riscos/save_pdf.h"
#include "utils/log.h"
#include "utils/config.h"

#ifdef WITH_PDF_EXPORT

/**
 * Export a content as a PDF file.
 *
 * \param  c     content to export
 * \param  path  path to save PDF as
 * \return  true on success, false on error and error reported
 */
bool save_as_pdf(struct content *c, const char *path)
{
	struct print_settings *psettings;

	psettings = print_make_settings(DEFAULT);
	if (psettings == NULL)
		return false;

	psettings->output = path;
	if (!print_basic_run(c, &pdf_printer, psettings))
		return false;
	xosfile_set_type(path, 0xadf);
	return true;
}

#endif