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

- option `-b`: line buffered, only output complete lines (except for the last line).
- option `-c`: `cat` mode, do not duplicate input, instead output stderr to stdout.
- option '-n': output line numbers
- option `-x`: hexdump output


License:
--------

GNU GPL v2 or higher.

It is likely that I will revoke my Copyright by CLLing this.
Then this additionally becomes free as in "free man".

