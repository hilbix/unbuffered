/* $Header$
 *
 * Copy stdin to stdout and stderr, unbuffered.
 *
 * Copyright (C)2006 by Valentin Hilbig
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
 * Revision 1.1  2006-12-12 13:26:27  tino
 * commit for first dist
 *
 */

#include "tino/main_getopt.h"
#include "tino/buf_printf.h"
#include "tino/xd.h"

#include "unbuffered_version.h"

static int	flag_linecount, flag_hexdump;
static char	line_terminator, *line_prefix, *line_suffix, *line_cont_prefix, *line_cont_suffix;
static int	in_line, line_nr;
static unsigned long long	byte_pos;

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

      tino_buf_reset(&prefix);
      if (p)
	tino_buf_add_s(&prefix, p);
      tino_buf_add_sprintf(&prefix, "%5d ", line_nr);
      p	= tino_buf_get_s(&prefix);
    }

  if (flag_hexdump)
    tino_xd(stderr, (p ? p : ""), 8, byte_pos, ptr, n+lineend);
  else
    {
      if (p && *p)
	fputs(p, stderr);
      fwrite(ptr, n, 1, stderr);
    }

  byte_pos	+= n;

  in_line	= !lineend;
  p		= in_line ? line_cont_suffix : line_suffix;
  if (!p && !flag_hexdump)
    p		= "\n";
  if (p && *p)
    fputs(p, stderr);
  fflush(stderr);
}

static void
unbuffered(void)
{
  TINO_BUF	buf;

  tino_buf_init(&buf);
  while (tino_buf_read(&buf, 0, -1))
    {
      size_t		n;

      while ((n=tino_buf_get_len(&buf))>0)
	{
	  const char	*ptr;
	  size_t	k, p;


	  ptr	= tino_buf_get(&buf);
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
	  tino_buf_write_away_all(&buf, 1, k);
	}
    }

}

int
main(int argc, char **argv)
{
  return tino_main_g0(unbuffered, argc, argv,
		      TINO_GETOPT_VERSION(UNBUFFERED_VERSION)
                      "",

                      TINO_GETOPT_USAGE
                      "h	this help"
                      ,

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
