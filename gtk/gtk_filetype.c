/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
 * Copyright 2007 Vincent Sanders <vince@debian.org>
 */
 
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "netsurf/gtk/gtk_filetype.h"
#include "netsurf/content/fetch.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/hashtable.h"

static struct hash_table *mime_hash = NULL;

void gtk_fetch_filetype_init(const char *mimefile)
{
	struct stat statbuf;
	FILE *fh = NULL;

	mime_hash = hash_create(117);
	
	/* first, check to see if /etc/mime.types in preference */
	
	if ((stat("/etc/mime.types", &statbuf) == 0) &&
			S_ISREG(statbuf.st_mode)) {
		mimefile = "/etc/mime.types";	
		
	}
	
	fh = fopen(mimefile, "r");
	
	if (fh == NULL) {
		LOG(("Unable to open a mime.types file, so building a minimal one for you."));
		hash_add(mime_hash, "css", "text/css");
		hash_add(mime_hash, "jpg", "image/jpeg");
		hash_add(mime_hash, "jpeg", "image/jpeg");
		hash_add(mime_hash, "gif", "image/gif");
		hash_add(mime_hash, "png", "image/png");
		hash_add(mime_hash, "jng", "image/jng");
		
		return;
	}
	
	while (!feof(fh)) {
		char line[256], *ptr, *type, *ext;
		fgets(line, 256, fh);
		if (!feof(fh) && line[0] != '#') {
			ptr = line;
			
			/* search for the first non-whitespace character */
			while (isspace(*ptr))
				ptr++;
				
			/* is this line empty other than leading whitespace? */
			if (*ptr == '\n' || *ptr == '\0')
				continue;
			
			type = ptr;
			
			/* search for the first non-whitespace char or NUL or
			 * NL */		
			while (*ptr && (!isspace(*ptr)) && *ptr != '\n')
				ptr++;
			
			if (*ptr == '\0' || *ptr == '\n') {
				/* this mimetype has no extensions - read next
				 * line.
				 */
				continue;
			}
			
			*ptr++ = '\0';
			
			/* search for the first non-whitespace character which
			 * will be the first filename extenion */
			while (isspace(*ptr))
				ptr++;
			
			while(true) {
				ext = ptr;
			
				/* search for the first whitespace char or
				 * NUL or NL which is the end of the ext.
				 */		
				while (*ptr && (!isspace(*ptr)) &&
					*ptr != '\n')
					ptr++;
			
				if (*ptr == '\0' || *ptr == '\n') {
					/* special case for last extension on
					 * the line
					 */
					*ptr = '\0';
					hash_add(mime_hash, ext, type);
					break;
				}
			
				*ptr++ = '\0';
				hash_add(mime_hash, ext, type);
				
				/* search for the first non-whitespace char or
				 * NUL or NL, to find start of next ext.
				 */		
				while (*ptr && (isspace(*ptr)) && *ptr != '\n')
					ptr++;				
			}
		}
	}
	
	fclose(fh);
}

void gtk_fetch_filetype_fin(void)
{
	hash_destroy(mime_hash);
}

const char *fetch_filetype(const char *unix_path)
{
	struct stat statbuf;
	char *ext;
	const char *ptr;
	char *lowerchar;
	const char *type;
	
	stat(unix_path, &statbuf);
	if (S_ISDIR(statbuf.st_mode))
		return "application/x-netsurf-directory";
	
	if (strchr(unix_path, '.') == NULL) {
		/* no extension anywhere! */
		return "text/plain";
	}
	
	ptr = unix_path + strlen(unix_path);
	while (*ptr != '.' && *ptr != '/')
		ptr--;
	
	if (*ptr != '.')
		return "text/plain";
		
	ext = strdup(ptr + 1);	/* skip the . */
	
	/* the hash table only contains lower-case versions - make sure this
	 * copy is lower case too.
	 */
	lowerchar = ext;
	while(*lowerchar) {
		*lowerchar = tolower(*lowerchar);
		lowerchar++;
	}
	
	type = hash_get(mime_hash, ext);
	free(ext);
	
	return type != NULL ? type : "text/plain";		
}

char *fetch_mimetype(const char *unix_path)
{
	return strdup(fetch_filetype(unix_path));
}

#ifdef TEST_RIG

int main(int argc, char *argv[])
{
	unsigned int c1, *c2;
	const char *key;
	
	gtk_fetch_filetype_init("./mime.types");
	
	c1 = 0; c2 = 0;
	
	while ( (key = hash_iterate(mime_hash, &c1, &c2)) != NULL) {
		printf("%s ", key);
	}
	
	printf("\n");
	
	if (argc > 1) {
		printf("%s maps to %s\n", argv[1], fetch_filetype(argv[1]));
	}
	
	gtk_fetch_filetype_fin();
}

#endif
