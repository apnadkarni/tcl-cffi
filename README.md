# Tcl cffi package

The Tcl `cffi` package permits calling C functions in shared libraries from
within Tcl scripts via either the `libffi` or `dyncall` open source libraries.

The package source repository is at https://github.com/apnadkarni/tcl-cffi.

Documentation is at https://cffi.magicsplat.com. Some additional
tutorial material is available at https://www.magicsplat.com/blog/tags/cffi/
and the samples in https://github.com/apnadkarni/tcl-cffi/tree/main/examples.

Source distributions and binary packages for some platforms can be
downloaded from https://sourceforge.net/projects/magicsplat/files/cffi.

## Building

To build the package from the source, see the `BUILD.md` file in the repository
or source distribution.

## About the package

Major features of the package are

- Implicit conversions of numerics, strings, structs and arrays
- Safety mechanisms for pointers
- Encoding of string values passed and returned from C functions
- Exception generation based on C function return values
- Proc-like argument processing with defaults, error messages etc.
- Utilities for managing memory and conversion to native formats
- Extensible type aliases and enums
- Introspection

Limitations in the current version include

- No support for *asynchronous* callbacks

## Version history

See the [Change log](CHANGES.md).
