# Tcl cffi package

This is the source code for the Tcl `cffi` package which permits calling C functions
in shared libraries from within Tcl scripts. It is based on the `dyncall` library
available from https://dyncall.org.

The repository for the package is at https://github.com/apnadkarni/tcl-cffi.

Documentation pages are at https://cffi.magicsplat.com.

Source distributions and binary packages for some platforms are at
https://sourceforge.net/projects/magicsplat/files/cffi

## Building

To build the package from the source, see the BUILD.md file in the repository
or source distribution.

## About the package

Distinguishing features of the package are

- Automatic conversion of numerics, strings, structures and arrays to and from
the corresponding Tcl script values
- Safety mechanisms to protect against pointer errors like double frees,
mismatched types
- Support for automatic encoding of string values passed and returned from
C functions
- Exception generation based on error annotations
- Proc-like argument processing with defaults, error messages etc.
- Utilities for managing memory and conversion to native formats
- Extensible type aliases for common system definitions
- Introspection

Limitations in the current version include

- No support for arrays of non-scalars
- No support of functions taking variable number of arguments
- No support for callbacks from C code
- No support for passing structs by value

