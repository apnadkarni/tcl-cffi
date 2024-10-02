# Tcl cffi package

The Tcl `cffi` package permits calling C functions in shared libraries from
within Tcl scripts via either the `libffi` or `dyncall` open source libraries.
The package supports Tcl 8.6 and 9.0+.

The source repository is at https://github.com/apnadkarni/tcl-cffi.

Documentation is at https://cffi.magicsplat.com. Some additional
tutorial material is available at https://www.magicsplat.com/blog/tags/cffi/
and the samples in https://github.com/apnadkarni/tcl-cffi/tree/main/examples.

Source distributions and binary packages for some platforms can be
downloaded from https://sourceforge.net/projects/magicsplat/files/cffi.
The binary packages for Windows require the VC++ runtime and UCRT be
present on the system. These will already be installed on Windows 10 and
later. For Windows 7 and 8.x, download if necessary from
https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist
and
https://support.microsoft.com/en-gb/topic/update-for-universal-c-runtime-in-windows-c0514201-7fe6-95a3-b0a5-287930f3560c.

## Building

To build the package from the source, see `BUILD.md` in the repository
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
