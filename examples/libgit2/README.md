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
accordingly so does the `lg2` wrapper. The `porcelain` directory contains
implementations of simpler versions of the high level `git` commands that can be
used as a starting point for more complete implemetations.

## Prerequisites

The `lg2` package has two prerequisites.

* The Tcl [`cffi`](https://cffi.magicsplat.com) extension must be present
somewhere in the Tcl package search path.

* The [`libgit2`](https://libgit2.org] shared library must be loadable. On most
Unix/Linux systems `libgit2` can be installed using the system's package
manager. However, note that the version of `libgit2` installed may not be
one supported by the `lg2` package.

## Usage

To use the `lg2` package, it must be loaded and then initialized with the
path to the `libgit2` shared library. For example,

```
package require lg2
lg2::init /lib/x86_64-linux-gnu/libgit2.so
```

## Command reference

There is no separate documentation for the `lg2` package commands as it (almost)
directly maps the `libgit2` API and the
[documentation](https://libgit2.org/libgit2/) for `libgit2` can be used as a
reference. A few differences are listed below. Also, the samples in the
`porcelain` directory may be useful as a tutorial for command usage.


# allocations

out parameters are modelled as retval

# git_signature

May be allocatd by libgit2 or through git_signature allocate. Use the appropriate
free function! `git_signature_free` or `git_signature free`.


# Encoded strings
message_encoding encoded_message pointer instead of STRING

# strarray
`util::strarray_new`
`util::strarray_free`

# buffers

# File paths

Must use forward slashes
Must be relative to top of the working tree
