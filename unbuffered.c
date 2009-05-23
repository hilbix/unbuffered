/* $Header$
 *
 * Copy stdin to stdout and stderr, unbuffered.
 *
 * Copyright (C)2006-2009 by Valentin Hilbig
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Log$
 * Revision 1.6  2009-05-23 10:22:39  tino
 * Options -l and -u
 *
 * Revision 1.5  2009-03-17 10:41:08  tino
 * Unready next version
 *
 * Revision 1.4  2008-10-17 19:17:54  tino
 * Usage corrected
 *
 * Revision 1.3  2008-07-08 20:53:11  tino
 * next dist
 *
 * Revision 1.2  2008-07-08 20:10:03  tino
 * Bugfix (hexdump bytecount) and Option -c
 *
 * Revision 1.1  2006-12-12 13:26:27  tino
 * commit for first dist
 */

#include "tino/main_getopt.h"
#include "tino/buf_printf.h"
#include "tino/xd.h"

#include <time.h>

#include "unbuffered_version.h"

static int	flag_linecount, flag_hexdump, flag_escape;
static char	line_terminator;
static const char *line_prefix, *line_suffix, *line_cont_prefix, *line_cont_suffix, *field_order;
static int	in_line, line_nr, flag_cat, flag_utc, flag_localtime;
static unsigned long long	byte_pos;
static TINO_DATA		out;
static TINO_BUF			prefix;

/* This is plain crap, really, however there is no good support for
 * this all in my lib yet (there is no support for such things in
 * nearly no language, either.  Python is best in this regard).
 */
/* This is wrong (on hexdump continuation lines), but I cannot help it now
 */
static void
add_prefix(const char *what, ...)
{
  tino_va_list	list;

  if (!tino_buf_get_lenO(&prefix))
    {
      const char *p= in_line ? line_cont_prefix : line_prefix;
      if (p)
	tino_buf_add_sO(&prefix, p);
    }
  tino_va_start(list, what);
  tino_buf_add_vsprintfO(&prefix, &list);
  tino_va_end(list);
}

static void
add_time(struct tm *fn(const time_t *timep))
{
  time_t	tim;
  struct tm	*tm;

  time(&tim);	/* Double calc on l and u, cannot help	*/
  tm	= fn(&tim);
  add_prefix("[%04d%02d%02d-%02d%02d%02d]", 1900+tm->tm_year, 1+tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/* This is plain crap, really, however there is no good support for
 * this all in my lib yet (there is no support for such things in
 * nearly no language, either.  Python is best in this regard).
 */
static void
dump_line(const char *ptr, size_t n, int lineend)
{
  const char		*p;

  tino_buf_resetO(&prefix);
  if (flag_localtime)
    add_time(localtime);
  if (flag_utc)
    add_time(gmtime);
  if (flag_linecount)
    {
      if (!in_line)
	line_nr++;
      add_prefix("%5d ", line_nr);
    }

  /* PHP parses A ? B : C ? E : F as (A ? B : C) ? E : F which is
   * plain crap.  Luckily we are in C which parses this correctly like
   * if-else, which is A ? B : ( C ?  D : E )
   */
  p	= tino_buf_get_lenO(&prefix) ? tino_buf_get_sN(&prefix) : in_line ? line_cont_prefix : line_prefix;
  if (!p)
    p	= "";
  if (flag_hexdump)
    tino_xd(&out, p, 8, byte_pos, (const unsigned char *)ptr, n+lineend);
  else
    {
      if (p && *p)
	tino_data_putsA(&out, p);
#if 0
      if (flag_escape)
	tino_data_write_xmlA(&out, ptr, n, flag_escape-1);
      else
#endif
        tino_data_writeA(&out, ptr, n);
    }

  byte_pos	+= n+lineend;

  in_line	= !lineend;
  p		= in_line ? line_cont_suffix : line_suffix;
  if (!p && !flag_hexdump)
    p		= "\n";
  if (p && *p)
    tino_data_putsA(&out, p);
}

static void
unbuffered(void)
{
  TINO_BUF	buf;

  tino_data_fileA(&out, (flag_cat ? 1 : 2));
  tino_buf_initO(&buf);
  while (tino_buf_readE(&buf, 0, -1))
    {
      size_t		n;

      while ((n=tino_buf_get_lenO(&buf))>0)
	{
	  const char	*ptr;
	  size_t	k, p;

	  ptr	= tino_buf_getN(&buf);
	  p	= 0;
	  for (k=0; k<n; k++)
	    if (ptr[k]==line_terminator)
	      {
		dump_line(ptr+p, k-p, 1);
		p	= k+1;
	      }
	  /* We shall add nonblocking line info if it is available.
	   * Leave this to future
	   */
	  TINO_XXX;

	  if (p<n)
	    dump_line(ptr+p, n-p, 0);
	  if (flag_cat)
	    tino_buf_advanceO(&buf, k);
	  else if (tino_buf_write_away_allE(&buf, 1, k))
	    exit(1);	/* silently drop out	*/
	}
    }
  tino_data_freeA(&out);	/* close(2)	*/
}

int
main(int argc, char **argv)
{
  return tino_main_g0(unbuffered,
		      NULL,
		      argc, argv,
		      TINO_GETOPT_VERSION(UNBUFFERED_VERSION)
		      "\n"
                      "\tproducer | unbuffered 2>>file | consumer\n"
                      "\tproducer | unbuffered -c | consumer"
		      ,

                      TINO_GETOPT_USAGE
                      "h	this help"
                      ,

		      TINO_GETOPT_FLAG
		      "c	change (or cat) mode, do the dumping to stdout, faster than:\n"
		      "		producer | unbuffered 2>&1 >/dev/null | consumer\n"
		      "		Without this option input is copied to output unchanged"
		      , &flag_cat,
#if 0
		      TINO_GETOPT_FLAG
		      TINO_GETOPT_MAX
		      "e	escape mode, XML compatible. Give twice for CDATA.\n"
		      "		Use with -p and -s to add XML tags\n"
		      "		Does not work with option -x (latter takes precedence)\n"
		      , &flag_escape,
		      2,
#endif
		      TINO_GETOPT_FLAG
		      "l	LOCAL timestamp each line"
		      , &flag_localtime,

		      TINO_GETOPT_FLAG
		      "n	print line numbers"
		      , &flag_linecount,
#if 0
		      TINO_GETOPT_ENV
		      TINO_GETOPT_STRING
		      TINO_GETOPT_DEFAULT
		      "o str	comma separated order of the line fields (letter=option).\n"
		      "		Use ,, to get a comma.  c stands for line contents."
		      , "UNBUFFERD_ORDER",
		      , &field_order,
		      , "p,[l],[u],n5 ,cs"
#endif
		      TINO_GETOPT_STRING
		      "p str	line prefix"
		      , &line_prefix,

		      TINO_GETOPT_STRING
		      "q str	line suffix on continuation lines"
		      , &line_cont_suffix,

		      TINO_GETOPT_STRING
		      "r str	line prefix on continuation lines"
		      , &line_cont_prefix,

		      TINO_GETOPT_STRING
		      "s str	line suffix"
		      , &line_suffix,

		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_CHAR
		      "t char	line termination character (not 0!)"
		      , &line_terminator,
		      '\n',

		      TINO_GETOPT_FLAG
		      "u	UTC timestamp each line"
		      , &flag_utc,

		      TINO_GETOPT_FLAG
		      "x	hexdump output"
		      , &flag_hexdump,

		      NULL
		      );
}
