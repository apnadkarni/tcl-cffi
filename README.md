# Tcl cffi package

This is the source code for the Tcl `cffi` package which permits calling C
functions in shared libraries from within Tcl scripts. It is based on the
`dyncall` library available from https://dyncall.org.

The package source repository is at https://github.com/apnadkarni/tcl-cffi.

Documentation is at https://cffi.magicsplat.com. Some additional
tutorial material is available at https://www.magicsplat.com/blog/tags/cffi/
and the samples in https://github.com/apnadkarni/tcl-cffi/tree/main/examples.

Source distributions and binary packages for some platforms can be
downloaded from https://sourceforge.net/projects/magicsplat/files/cffi.

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

- No support of functions taking variable number of arguments
- No support for callbacks from C code

## Changes in 1.0b3

- Added support for the `libffi` library back end as an alternative to `dyncall`.
The selection is made at build time. See `Build.md` in the source distribution.

- C structs can be returned from functions and passed by value if the `libffi`
back end is being used. This is still not supported with `dyncall` library.

- **Incompatibility** `cffi::dyncall::Library` class renamed to 
`cffi::Wrapper` as it is no longer specific to `dyncall`.

- **Incompatibility** The `cffi::dyncall::Symbols` class has been removed.


## Changes in 1.0b1

- **Incompatibility** Null pointers will now raise a Tcl exception when passed
as arguments or returned from functions unless the `nullok` annotation is present.
Related to this, the `nonzero` annotation can no longer be applied to pointers.

- **Incompatibility** The format of arguments passed to `onerror` handler
has changed.

- The `onerror`, `lasterror` and `errno` annotations can be included on
parameter definitions and are ignored. This permits common type aliases for 
return values and paramters.

- The `alias define` command now allows multiple alias definitions to be passed.

## Changes in 1.0b0

- Added `help` command for wrapped function list and syntax
- Added `enum value` and `enum name` to map enumeration values and names
- Map enum values for output parameters and return values
- Support arrays for pointers and structs

## Changes in 1.0a7

- Support `string`, `unistring` and `binary` as return types and output parameters
- Support `nonzero` annotation for strings and unistrings
- Added `enum` command to define constants and enumerations
- Added `bitmask` annotation for integer parameters
- Added `onerror` annotation for custom error handlers
- Added `disposeonsuccess` annotation to only invalidate pointers on success
- Sample `libzip` wrapper in examples directory
- Miscellaneous bug fixes. See https://github.com/apnadkarni/tcl-cffi/milestone/1?closed=1 for a full list.

