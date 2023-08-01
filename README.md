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

## Changes in 1.3.0

- Support for Tcl 9.0

## Changes in 1.2.0

- New `Struct` methods `tonative!`, `setnative!`, `getnative!`,
`getnativefields` to access structures via unsafe pointers.
- Type alias for C99 `bool`
- Miscellaneous bug fixes. See https://github.com/apnadkarni/tcl-cffi/milestone/12?closed=1

## Changes in 1.1.0

- Added support for varargs functions
- Added offset argument to `memory fill`, `memory tobinary`
and `memory tostring`
- Added `memory fill!` command
- Added support of explicit values in `enum sequence`
- Miscellaneous bug fixes
See https://github.com/apnadkarni/tcl-cffi/milestone/11?closed=1

## Changes in 1.0.7

First official release.

- Load from platform specific directory.
- Install manpages on Unix.

## Changes in 1.0b5

- Added a *Cookbook* section to documentation to help in mapping C declarations
to CFFI.

- Added the `retval` annotation to return a output parameter as the
command result.

- Added `getnativefields` method to retrieve multiple field values from a
native struct.

- Added `new` method for struct instances to allocate and initialize a native
struct in memory.

- Added implicit and explicit casting for pointers.

- The `enum value` and `enum name` commands now accept defaults.

- **Incompatibility** The `get` and `set` methods for struct instances
renamed to `getnative` and `setnative`.

- **Incompatibility** The `callback` and `callback_free` commands are now
part of a `callback` command ensemble.

- **Incompatibility** The last element of a bitmask enumeration list returns original
integer value.

- Miscellaneous bug fixes. 


## Changes in 1.0b4

- Added `callback` and `callback_free` commands for supporting callbacks
from C functions.

- Program element names are now scoped based on namespace at definition time.

- New methods `set` and `get` for `Struct` instances to store and retrieve
fields in native structs in memory.

- New method `fromnative!` method for `Struct` instances.

- New method `fieldpointer` to retrieve the pointer to a field in a
native struct.

- Support `string` and `unistring` types for struct fields.

- Support default values and `-clear` option for `Struct` fields.

- Support `enum` and `bitmask` annotations for struct field types as for
function parameters.

- Allow enum literals in type declarations in addition to defined enums.

- New commands `enum clear` and `enum flags`.

- New command `alias clear`.

- The `memory allocate` command now accepts a type declaration for specifying
allocation size.

- New commands `memory set`, `memory set!`, `memory get` and `memory get!` 
to store and retrieve typed values in native form.

- Zero size dynamic arrays are passed as NULL pointers.

- The `byref` annotation is now permitted on return types to implicitly
dereference pointers returned by functions.

- New command `limits` to retrieve range of integer types.

- Support arrays of `string` and `unistring` types.

- New command `pointer make`.

- **Incompatibility** `memory set` command renamed to `memory fill`.

- **Incompatibility** The format and content of type information returned by
`type info` has changed.

- **Incompatibility** Commands that accept patterns only treat last component
in a program element name as a pattern.

## Changes in 1.0b3

- **Incompatibility** `cffi::dyncall::Library` class renamed to 
`cffi::Wrapper` as it is no longer specific to `dyncall`.

- **Incompatibility** The `cffi::dyncall::Symbols` class has been removed.

- Added support for the `libffi` library back end as an alternative to `dyncall`.
The selection is made at build time. See `BUILD.md` in the source distribution.

- C structs can be returned from functions and passed by value if the `libffi`
back end is being used. This is still not supported with `dyncall` library.


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

