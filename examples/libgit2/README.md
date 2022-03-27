#  `lg2` - Tcl/CFFI binding to libgit2

The `lg2` package is a binding to the [libgit2](https://libgit2.org) library. It
is only intended as a demonstration of using CFFI to wrap a fairly substantial
shared library (more than 800 functions) and thus lacks a test suite. Use in
production will require client applications to write their own.

## Porcelain and plumbing

The commands in official git implementation can be divided into two categories:

* The *plumbing* commands like `hash-object`, `write-tree` etc.
which implement the low level operations.

* The *porcelain* commands, like `git-clone`, `git-commit` etc. that are invoked
by the user in daily usage and are built on top of the plumbing commands.

The `libgit2` API implements the equivalent of the plumbing commands and
accordingly so does the `lg2` wrapper. Be warned that using the package requires
understanding the `libgit2` API which in turn requires understanding git and its
internal structures. The `porcelain` directory contains implementations of
simpler versions of the high level `git` commands that illustrate the use of the
plumbing command and which can be used as a starting point for more complete
implementations.

## Prerequisites

The `lg2` package has two prerequisites.

* The Tcl [`cffi`](https://cffi.magicsplat.com) extension, version 1.0b5 or
later, must be present somewhere in the Tcl package search path.

* The [`libgit2`](https://libgit2.org] shared library must be loadable. On most
Unix/Linux systems `libgit2` can be installed using the system's package
manager. Currently the `lg2` package supports `libgit2` versions 1.3 and 1.4.

**Important:** Only use supported **release* versions of `libgit2` as it does
not guarantee ABI compatibility even between minor releases. Moreover, binaries built
from repository sources may not work even if version numbers are the same since
structures may change between releases. 

## Usage

To use the `lg2` package, it must be loaded and then initialized with the
path to the `libgit2` shared library. For example,

```
package require lg2
lg2::lg2_init /lib/x86_64-linux-gnu/libgit2.so
```

If no path is supplied to `lg2_init` it will try to locate `git2.dll` on
Windows or `libgit2.so` on other platforms under architecture-specific 
directories under the package directory. If not found there, it will just attempt
to load using the unqualified shared library name.

The scripts in the `porcelain` directory are standalone scripts that
mimic the porcelain git commands. All support the `--help` option to 
display some basic help on syntax and options. For example,

```
> tclsh git-init.tcl --help
Usage: tclsh.exe git-init.tcl [OPTION]... DIRECTORY

Demo of cffi libgit extension. Poor man's git init emulation from libgit2
Translated to Tcl from libgit2/examples/init.c

Mandatory arguments to long options are also mandatory for short options.
  -q, --quiet               Only print error and warning messages;
                            all other output will be suppressed.
      --bare                Create a bare repository.
      --initial-commit      Create an empty initial commit
      --shared=PERMS        Set the sharing permissions. PERMS should be
                            "umask" (default), "group", "all" or an
                            integer umask value.
      --template=DIRECTORY  Uses the templates from DIRECTORY.
      --help                display this help and exit
```

## Command reference

There is no separate documentation for the `lg2` package commands as it (almost)
directly maps the `libgit2` API and the
[documentation](https://libgit2.org/libgit2/) for `libgit2` can be used as a
reference. A few differences in usage are listed below. Also, the samples in the
`porcelain` directory may be useful as a tutorial for command usage.

* All commands are placed in the `lg2` namespace.

* `libgit2` prefixes all its functions with `git_`. The `lg2` package adds a few
utility commands. These are prefixed with `lg2_`. It is thus safe to put the
`lg2` namespace in the application namespace path as they are unlikely to clash
with commands from other packages.

* Most `libgit2` functions return an error code on failure. The corresponding
wrapped Tcl commands raise a Tcl exception instead with the error message
retrieved from `libgit2`.

* Since `libgit2` uses function return values to indicate success and failure,
it returns the actual function result through an output parameter. Because the
wrapped commands use Tcl's exception mechanism, the command result is not needed
to indicate success or failure. The commands thus return the output parameter
value as the command result.

* Handling of `git_strarray` structs can be slightly tricky because the internal
buffers may be allocated by `libgit2` or the application. Thus a "shadow" struct
`lg2_strarray` is defined to distinguish the two use cases. See the comments
in `strarray.tcl` more information and some utility commands to deal with these.

* Along similar lines, the `git_signature` structure may be returned by a
`libgit2` function. The `lg2` package defines the script level struct of
the same name. Now a pointer to a `git_signature` may come either from
`libgit2` function or by from the CFFI allocating commands
`git_signature allocate` or `git_signature new`. It is **crucial** that
the former is freed by a call to the `libgit2` (wrapped) function
`git_signature_free` while the latter must be freed through the
`git_signature free` (note one is a wrapped function, other is a call to
the `free` method for the CFFI `git_signature` struct command instance.

* `libgit2` uses utf-8 string encoding by default. Correspondingly, `lg2`
defines the `STRING` CFFI alias that is used by most declarations. Some commands
allow for strings in arbitrary encodings. These have to be passed as
encoded binary strings with the encoding name in a separate parameter.
The encoding names are from IANA, not those used by Tcl's `encoding` command.
The package therefore provides some utility commands `lg2_encoding convertto` and
`lg2_encoding convertfrom` to help with such conversions. They work like
Tcl's `encoding` equivalents except they accept the encoding names used
by `libgit2` instead of Tcl encoding names.

* One note to keep in mind with `libgit2` (this is independent of the `lg2` package)
is that many functions that take file paths as arguments expect `/` to be used as
the path separator and will not work correctly with `\`. Moreover, some expect
paths to be relative to top of the working directory.

All of the above are illustrated by the samples in the `porcelain` directory.
