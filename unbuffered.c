/* $Header$
 *
 * Copy stdin to stdout and stderr, unbuffered.
 *
 * Copyright (C)2006-2008 by Valentin Hilbig
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

#include "unbuffered_version.h"

static int	flag_linecount, flag_hexdump;
static char	line_terminator, *line_prefix, *line_suffix, *line_cont_prefix, *line_cont_suffix;
static int	in_line, line_nr, flag_cat;
static unsigned long long	byte_pos;
static TINO_DATA		out;

static void
dump_line(const char *ptr, size_t n, int lineend)
{
  const char	*p;

  p		= in_line ? line_cont_prefix : line_prefix;
  if (flag_linecount)
    {
      static TINO_BUF	prefix;
      
      if (!in_line)
	line_nr++;

      tino_buf_resetO(&prefix);
      if (p)
	tino_buf_add_sO(&prefix, p);
      tino_buf_add_sprintfO(&prefix, "%5d ", line_nr);
      p	= tino_buf_get_sN(&prefix);
    }

  if (flag_hexdump)
    tino_xd(&out, (p ? p : ""), 8, byte_pos, (const unsigned char *)ptr, n+lineend);
  else
    {
      if (p && *p)
	tino_data_putsA(&out, p);
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

  tino_data_file(&out, (flag_cat ? 1 : 2));
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
		      "c	change input instead dumping to stderr, same as:\n"
		      "		producer | unbuffered 2>&1 >/dev/null | consumer"
		      , &flag_cat,

		      TINO_GETOPT_FLAG
		      "n	print line numbers"
		      , &flag_linecount,

		      TINO_GETOPT_STRING
		      "p	line prefix"
		      , &line_prefix,

		      TINO_GETOPT_STRING
		      "q	line suffix on continuation lines"
		      , &line_cont_suffix,

		      TINO_GETOPT_FLAG
		      TINO_GETOPT_STRING
		      "r	line prefix on continuation lines"
		      , &line_cont_prefix,

		      TINO_GETOPT_STRING
		      "s	line suffix"
		      , &line_suffix,

		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_CHAR
		      "t	line termination character (not 0!)"
		      , &line_terminator,
		      '\n',

		      TINO_GETOPT_FLAG
		      "x	hexdump output"
		      , &flag_hexdump,

		      NULL
		      );
}
