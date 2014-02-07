Copy stdin to stdout and stderr, unbuffered
===========================================

A quick tool to debug a pipe.  And more.


Usage:
------

```bash
git clone https://github.com/hilbix/unbuffered.git
cd unbuffered
git submodule update --init
make
```

Examples:
---------

- Record a pipe to a file.  Uses option -q to remove LFs from incomplete lines:
```bash
producer | unbuffered -q '' 2>>file | consumer
```

- Record a pipe to a file with timestamping, incomplete lines are marked with `+`:
```bash
producer | unbuffered -uv 2>>file | consumer
```

- Debug some pipe step: Record stdin and stderr into `debug.log`:
```bash
input to producer | unbuffered -uvd producer 2>>debug.log | consumer
```

Option `-h` prints a complete list of options, like:

- option `-b`: buffer incomplete lines.  Give twice to buffer all fragments.
- option `-c`: `cat` mode, do not duplicate input, instead output stderr to stdout.
- option '-n': output line numbers
- option `-x`: hexdump output


Notes:
------

To see the difference between no buffering, `-b` and `-bb` see:

```bash
show() { { echo -n "partial line "; sleep 1; echo -n " complete
partial multiline "; sleep 1; echo " complete"; } |
unbuffered -vuc "$@" -- cat; }; show; show -b; show -bb
```

Note that the unmodified stream is buffered as well with `-b`.  This can be
considered a bug.  In future this might change.  To get the old behavior,
`-bbb` and `-bbbb` will be needed.  If you want to be sure to keep the
full buffering, use `-bbb` and `-bbbb` today.


License:
--------

GNU GPL v2 or higher.

It is likely that I will revoke my Copyright by CLLing this.
Then this additionally becomes free as in "free man".

