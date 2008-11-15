/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

/* This file generates an AmigaOS compliant version string in version.c */

address command 'svn info >t:ns_svn'

if open('tmp','t:ns_svn','R') then do
	do until word(var,1) = "REVISION:"
		var = upper(readln('tmp'))
	end
	dummy = close('tmp')
	address command 'delete t:ns_svn'
end

svnrev = word(var,2)

if open('tmp','desktop/version.c','R') then do
	do until word(var,3) = "NETSURF_VERSION_MAJOR"
		var = upper(readln('tmp'))
	end
	dummy = close('tmp')
end

majorver = compress(word(var,5),";")

/* ARexx only returns two digits for year, but AmigaOS version string dates are
 * supposed to have four digits for the year, so the below specifies the prefix
 * (century-1 really).  This will need to be increased in 2100 and every hundred
 * years thereafter, if this script is still in use :-)  */
century = 20
date = translate(left(date('E'),6) || century || right(date('E'),2),'.','/')

say '/* This file was automatically generated by version.rexx */'
say 'static const __attribute__((used)) char *verstag = "\0$VER: NetSurf' majorver || '.' || svnrev '(' || date || ')\0";'
say 'const char * const versvn = "SVN' svnrev || '";'
say 'const char * const verdate = "' || date || '";'
