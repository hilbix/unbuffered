# Copy stdin to stdout and stderr, unbuffered

A quick tool to debug a pipe.  And more.

> Please note:  Version 1.x differs from 0.x that `-q` no more defaults to `LF` if no other line modifying option is present.
>
> This makes it easier to use in case you want to use `-c` without other options.
>
> If `-v`, `-l` or `-u` is present, the default is `LF` again.


## Usage

```bash
git clone https://github.com/hilbix/unbuffered.git
cd unbuffered
git submodule update --init
make
```

### Examples

- Record a pipe to a file.  Uses option -q to remove LFs from incomplete lines:
```bash
producer | unbuffered 2>>file | consumer
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


## Notes

To see the difference between no buffering, `-b` and `-bb` use following example (this are 3 lines!):

```bash
show() { echo "unbuffered $*:"; { echo -n "partial line :"; sleep 1; echo -n ": complete
partial :"; sleep 1; echo -n ": multiline :"; sleep 1; echo ": complete"; } |
unbuffered -vuc "$@" -- cat; }; show; show -b; show -bb; show -bbb
```

The unmodified stream currently is buffered as well with `-b`.
This can be considered a bug.  In future this might change.

The idea is:

- With `-b` the typical "broken lines" which come from buffer flushes are understood.  Note that there must be at least one complete line seen on each read.
- With `-bb` a real buffering is done (up to 100000 byte or just a bit more), such that fragmented lines are joined together.
- With `-bbb` there is no memory limit for buffering of lines.  So this can eat up all of your memory, if there never is an `LF` seen.

(In the example above you do not see any difference between `-bb` and `-bbb`, as the line is too short.)


### Known bugs

Hexdump is not yet UTF-8 aware.  As it dumps all chars with bit 7 set, this might produce some buggy output on UTF-8 terminals, including swallowed `LF`.  Hint: Pipe through `less`.


# License

This Works is placed under the terms of the Copyright Less License,
see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

This means, it is free as in free speech, free beer and free baby.
(Babys usually have no Copyright on them, right?)

