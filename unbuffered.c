/* Copy stdin to stdout and stderr, unbuffered.
 *
 * This is release early code.  Use at own risk.
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#include "tino/main_getext.h"
#include "tino/proc.h"
#include "tino/buf_printf.h"
#include "tino/xd.h"

#include "tino/signals.h"

#include <sys/time.h>

#include "unbuffered_version.h"

#undef HAVE_ESCAPE_XML		/* not written yet	*/
#undef HAVE_ESCAPE_JSON		/* not written yet	*/
#undef HAVE_FIELD_FORMAT	/* not yet implemented	*/

static int	flag_linecount, flag_hexdump, flag_both, flag_verbose;
static int	flag_killme;
#ifdef HAVE_ESCAPE_XML
static int	flag_xml;
#endif
#ifdef HAVE_ESCAPE_JSON
static int	flag_json;
#endif
static char	line_terminator;
static const char *line_prefix, *line_suffix, *line_cont_prefix, *line_cont_suffix;
#ifdef HAVE_FIELD_FORMAT
static const char *field_format;
#endif
static const char *append_file;
static int	in_line, line_nr, flag_cat, flag_utc, flag_localtime, flag_micro, flag_buffer;
static int	fd_in, fd_out;
static unsigned long long	byte_pos;
static TINO_DATA		out;
static int			out_is_open;
static TINO_BUF			prefix;
static pid_t			producer;

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
      if (p && *p)
	tino_buf_add_sO(&prefix, p);
    }
  tino_va_start(list, what);
  tino_buf_add_vsprintfO(&prefix, &list);
  tino_va_end(list);
}

static struct timeval stamp;

static void
add_time(struct tm *fn(const time_t *timep))
{
  struct tm	*tm;

  tm	= fn(&stamp.tv_sec);
  if (flag_micro)
    add_prefix("[%04d%02d%02d-%02d%02d%02d-%06ld]", 1900+tm->tm_year, 1+tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, stamp.tv_usec);
  else
    add_prefix("[%04d%02d%02d-%02d%02d%02d]", 1900+tm->tm_year, 1+tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static void
start_line(int verbose, int lineend)
{
  char	flg;

  if (flag_localtime || flag_utc)
    gettimeofday(&stamp, NULL);

  tino_buf_resetO(&prefix);

  add_prefix("");
  if (flag_localtime)
    add_time(localtime);
  if (flag_utc)
    add_time(gmtime);

  flg	= verbose ? '-' : lineend ? ' ' : '+';
  if (flag_verbose && producer)
    add_prefix("[%ld]", (long)producer);

  if (flag_linecount)
    {
      if (!in_line)
	line_nr++;
      add_prefix((flag_linecount==1 ? "%5d%c" : flag_linecount==2 ? "%05d%c" : "%d%c"), line_nr, flg);
    }
  else if (flag_verbose)
    add_prefix("%c", flg);
}

/* This is plain crap, really, however there is no good support for
 * this all in my lib yet (there is no support for such things in
 * nearly no language, either.  Python is best in this regard).
 */
static void
dump_line(const char *ptr, size_t n, int lineend)
{
  const char		*p;

  start_line(0, lineend);
  p = tino_buf_get_sN(&prefix);

  if (flag_hexdump)
    tino_xd(&out, p, 8, byte_pos, (const unsigned char *)ptr, n+lineend);
  else
    {
      if (p && *p)
	tino_data_putsA(&out, p);
#ifdef HAVE_ESCAPE_XML
      if (flag_xml)
	tino_data_write_xmlA(&out, ptr, n, flag_xml-1);
      else
#endif
#ifdef HAVE_ESCAPE_JSON
      if (flag_json)
	tino_data_write_jsonA(&out, ptr, n);
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

/* Actually, this is an awful hack for dying consumers.
 *
 * The right thing to do is to start a session on the child
 * and wait for the session to terminate.
 *
 * This would catch consumers, which go away before all data is read.
 * However this variant also might have it's use on deamons.
 * So "session" mode definitively needs to be some (independent) option.
 */
static pid_t consumer;

static void
terminate()
{
  int	status;
  pid_t	pid;

  /* Be sure not to re-enter waitpid()!	*/
  while ((pid=tino_wait_child_poll(&status, NULL))!=0)
    if (pid==consumer)
      exit(status);	/* Premature termination of consumer	*/
}

static void
out_open(void)
{
  if (out_is_open)
    return;
  if (!append_file)
    {
      tino_data_fileA(&out, fd_out>=0 ? fd_out : flag_cat ? 1 : 2);
      out_is_open = -1;
    }
  else
    {
      tino_data_fileA(&out, tino_file_open_createE(append_file, O_APPEND|O_WRONLY, 0666));
      out_is_open	= 1;
    }
}


static void
out_close(void)
{
  if (!out_is_open)
    return;
  tino_data_freeA(&out);
  out_is_open	= 0;
}

static void
out_flush(void)
{
  if (out_is_open>0)
    out_close();
}

static void
unbuffered(const char *arg0, int argc, char **argv)
{
  TINO_BUF	buf;
  int		fd_target;

  if (!line_cont_suffix && !flag_hexdump)
    line_cont_suffix = (flag_localtime || flag_linecount || flag_utc || flag_verbose) ? "\n" : "";
  producer	= 0;
  fd_target	= 1;
  if (argc)
    {
      int	fds[2], redir[TINO_OPEN_MAX], fdmax, i;

      if (tino_file_pipeE(fds))
	tino_exit("cannot create pipe");

      fdmax = fd_in<3 ? 3 : fd_in+1;

      for (i=fdmax; --i>=0; )
        redir[i] = -1;				/* preserve all FDs	*/

      if (fd_in<0)
        fd_in = 1;

      redir[2] = flag_both ? fds[1] : 2;	/* catch stderr on -d, too	*/
      if (fd_in==2)
        redir[1] = redir[2];			/* swap stdin/stderr on -i2	*/
      redir[fd_in] = fds[!!fd_in];		/* catch 0:input else:output of producer	*/

      tino_file_close_on_exec_setE(fds[!fd_in]);	/* we do not want to keep the other one	*/

      /* catch the child's 0:input else:output for preprocessing
       */
      producer = tino_fork_execO(redir, fdmax, argv, NULL, 0, NULL);
      tino_file_closeE(fds[!!fd_in]);		/* close the used side of pipe	*/
      if (fd_in)
        fd_in = fds[0];
      else
        fd_target = fds[1];			/* write input to producer	*/

      if (flag_killme)
        {
          /* Following is a mess.  It is only needed for a consumer, though.
           * With a producer we see EOF on the pipe.
           * Shall be handled implicitely by a library somehow:
           */
          consumer	= producer;		/* it's a static, so 0 if not set here */
          tino_sigset(SIGCHLD, terminate);
          terminate();	/* catch early terminated childs	*/
        }
    }

  tino_buf_initO(&buf);
  if (producer && flag_verbose)
    {
      start_line(1, 1);
      add_prefix("start");
      for (; *argv; argv++)
	add_prefix(" '%s'", *argv);	/* XXX TODO sadly, escape is implemented in tino_io, not in tino_buf	*/
      add_prefix("\n");
      out_open();
      tino_data_putsA(&out, tino_buf_get_sN(&prefix));
    }

  if (fd_in<0)
    fd_in = 0;
  tino_file_blockE(fd_in);
  while (tino_buf_readE(&buf, fd_in, -1))
    {
      size_t		n;

      out_open();
      /* XXX TODO MISSING:
       * for flag_buffer==1 or flag_buffer==2
       * immediately write STDOUT(1),
       * but buffer STDERR(dump_line)
       */
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
	  /* k=n	*/
	  if (flag_buffer && p)
	    n = p;	/* do not output incomplete line	*/
	  else if ((flag_buffer>1 && n<=99999) || flag_buffer>2)
	    break;		/* buffer fragments	*/

	  /* We shall, nonblockingly, read additional input data here,
	   * if available.  Leave this to future.
	   */
	  TINO_XXX;

	  if (p<n)
	    dump_line(ptr+p, n-p, 0);
	  if (flag_cat)
	    tino_buf_advanceO(&buf, n);
	  else if (tino_buf_write_away_allE(&buf, fd_target, n))
	    {
	      /* silently drop out	*/
	      *tino_main_errflag	= 1;
	      break;
	    }
	  if (flag_buffer)
	    break;
	}
      out_flush();
    }
  {
      size_t		n;

      /* in case of flag_buffer: send the rest	*/
      if ((n=tino_buf_get_lenO(&buf))>0)
        {
          const char	*ptr;

          out_open();
          ptr	= tino_buf_getN(&buf);
          dump_line(ptr, n, 0);
          if (!flag_cat && tino_buf_write_away_allE(&buf, fd_target, n))
            *tino_main_errflag	= 1;
        }
  }
  if (producer)
    {
      char *cause;

      if (consumer)
        tino_sigdummy(SIGCHLD);	/* prevent reentrance of waitpid()	*/

      /* wait for child to finish after the pipe was closed,
       * so give the child the chance to terminate.
       */
      tino_file_closeE(fd_in);
      if (fd_out != fd_target)
        tino_file_closeE(fd_target);
      *tino_main_errflag	= tino_wait_child_exact(producer, &cause);
      if (flag_verbose)
	{
	  start_line(1, 1);
	  add_prefix("end %s\n", cause);
	  out_open();
	  tino_data_putsA(&out, tino_buf_get_sN(&prefix));
	}
    }
  out_close();			/* close(2)	*/
}

int
main(int argc, char **argv)
{
  return tino_main_g1(unbuffered,
		      NULL,
		      argc, argv,
		      0, -1,
		      TINO_GETOPT_VERSION(UNBUFFERED_VERSION)
		      " [command args...]\n"
                      "\t# producer | unbuffered 2>>file | consumer\n"
		      "\t\tfile gets copy of stdin appended\n"
                      "\t# producer | unbuffered -a file | consumer\n"
		      "\t\tAs before, but file is not kept open for easy rotation\n"
                      "\t# producer | unbuffered -cq$'\\n' | consumer\n"
		      "\t\tAdd LF on read boundaries (consumer sees partial lines as lines)\n"
                      "\t# producer | unbuffered -xuca file\n"
		      "\t\tHexdump producer's output with timestamp to file, allow rotation\n"
		      "\t# var=\"`unbuffered producer [producerargs]`\"; echo $?\n"
		      "\t\tDisplay producer's output while catching it and it's return code\n"
		      "\t# ./unbuffered -ci2 -bbp 'OUT ' ./unbuffered -ci2 -bbp 'ERR ' ./testfile.sh; echo $?\n"
		      "\t\tPrefix producer's STDIN with 'OUT ' and STDERR with 'ERR ' and get result\n"
		      "\t\t(Hint: The 1st unbuffered gets STDOUT of testfile on STDERR from 2nd)\n"
		      "\t\tNote that producer should not produce output on both channels too fast."
		      ,

                      TINO_GETOPT_USAGE
                      "h	this help"
                      ,

		      TINO_GETOPT_STRING
		      "a file	Append STDERR to file.  It is closed and reopened\n"
		      "		from time to time to allow easy logfile rotation.\n"
		      "		With option -c this becomes a data sink."
		      , &append_file,

		      TINO_GETOPT_FLAG
		      TINO_GETOPT_MAX
		      "b	Buffer partial lines\n"
		      "		Buffer incomplete lines which remain after reading STDIN.\n"
		      "		Give it twice to buffer all incomplete lines up to 100kB.\n"
		      "		Triple it to force buffering even on longer lines"
		      , &flag_buffer,
		      3,

		      TINO_GETOPT_FLAG
		      "c	Change (or cat) mode, do the dumping to stdout, faster than:\n"
		      "		producer | unbuffered 2>&1 >/dev/null | consumer\n"
		      "		Without this option input is copied to output unchanged"
		      , &flag_cat,

		      TINO_GETOPT_FLAG
		      "d	Debug forked producer by redirecting it's STDERR to STDOUT.\n"
		      "		This option only has an effect on producers."
		      , &flag_both,
#ifdef HAVE_ESCAPE_XML
		      TINO_GETOPT_FLAG
		      TINO_GETOPT_MAX
		      "e	Escape mode, XML compatible. Give twice for CDATA.\n"
		      "		Use with -p and -s to add XML tags\n"
		      "		Does not work with option -x (latter takes precedence)\n"
		      , &flag_xml,
		      2,
#endif
#if 0
		      TINO_GETOPT_ENV
		      TINO_GETOPT_STRING
		      TINO_GETOPT_DEFAULT
		      "f str	comma separated order of the line Fields (letter=option).\n"
		      "		Use ,, to get a comma.  c stands for line contents."
		      , "UNBUFFERD_FORMAT",
		      , &field_format,
		      , "p,[l],[u],n5 ,cs"
#endif
#if 0
                      TINO_GETOPT_FLAG
                      "g	start a (global) session (setsid) for the child\n"
                      "		-k then waits for the session and not the immediate child"
                      , &flag_session,
#endif

		      TINO_GETOPT_INT
		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_MIN
		      TINO_GETOPT_MAX
		      "i fd	Input file descriptor instead of the default, usually STDIN (0).\n"
		      "		With producers this is STDOUT (1).  See also -d.\n"
		      "		With -i2 on producers STDIN and STDERR swap positions"
		      , &fd_in,
		      -1,
		      0,
		      TINO_OPEN_MAX-1,

#ifdef HAVE_ESCAPE_JSON
		      TINO_GETOPT_FLAG
		      "j	Escape mode, JSON compatible\n"
		      "		Use with -p and -s to add JSON framing\n"
		      "		Does not work with option -x (latter takes precedence)\n"
		      , &flag_json,
#endif

                      TINO_GETOPT_FLAG
                      "k	detect death of command and terminate with it's return code early\n"
                      "		Unless used with -i0 this has a BUG: Unbuffered terminates too early."
                      , &flag_killme,

		      TINO_GETOPT_FLAG
		      "l	Local timestamp each line"
		      , &flag_localtime,

		      TINO_GETOPT_FLAG
		      "m	Microseconds accuracy for timestamps."
		      , &flag_micro,

		      TINO_GETOPT_FLAG
		      TINO_GETOPT_MAX
		      "n	print line Numbers, twice with 0, triple unindented."
		      , &flag_linecount,
		      3,

		      TINO_GETOPT_INT
		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_MIN
		      TINO_GETOPT_MAX
		      "o fd	Output file descriptor instead of STDERR\n"
		      "		This is used instead of STDOUT in cat mode (option -c)"
		      , &fd_out,
		      -1,
		      0,
		      TINO_OPEN_MAX-1,

		      TINO_GETOPT_STRING
		      "p str	line Prefix"
		      , &line_prefix,

		      TINO_GETOPT_STRING
		      "q str	line suffix on incomplete lines\n"
		      "		(Unless -x this defaults to LF for options: l n u v)"
		      , &line_cont_suffix,

		      TINO_GETOPT_STRING
		      "r str	line prefix on continuation lines"
		      , &line_cont_prefix,

		      TINO_GETOPT_STRING
		      "s str	line Suffix (unless -x this defaults to LF)"
		      , &line_suffix,

		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_CHAR
		      "t char	line Termination character on input"
		      , &line_terminator,
		      '\n',

		      TINO_GETOPT_FLAG
		      "u	UTC timestamp each line"
		      , &flag_utc,

		      TINO_GETOPT_FLAG
		      "v	Verbosely report return code of forked producer.\n"
		      "		Also adds a marker after the prefixes:\n"
		      "		'-' for informational, ' ' for complete, '+' for incomplete lines."
		      , &flag_verbose,

		      /* w */
		      TINO_GETOPT_FLAG
		      "x	heXdump output"
		      , &flag_hexdump,

		      /* yz */
		      NULL
		      );
}
