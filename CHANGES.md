# Revision history

## Changes in v2.0

### Platform and backends

- Support for Tcl 9

- The `dyncall` backend now requires dyncall 1.4 and adds support for
  callbacks, variable arguments and passing of structs by value.

### Type declarations and annotations

- New class `Interface` for modeling COM objects.

- New class `Union` to support C unions.

- New base type `uuid`.

- Array type declarations can now use aliases as the base type.

- New `type` subcommands `tobinary` and `frombinary` to construct or
  deconstruct a binary string for any type declaration.

- The `nullifempty` annotation may also be applied for types `bytes`,
  `chars` and `unichars`.

- The `nullifempty` annotation may be used for optional `out` parameters
  to indicate they are optional.

- Added `saveerrors` annotation to save errno and Win32 error values and
  the `savederrors` command to retrieve them.

- New `discard` annotation to discard the result of a function call.

- New annotation `multisz` to support MULTI_SZ strings on Windows.

### Memory management

- New `memory` subcommands `towinstring`, `fromwinstring`, `tounistring`
  and `fromunistring`.

- New command `memory arena` for stack-like memory allocation at script
  level.

### Structs

- New option `-pack` for `Struct` to control alignment and padding.

- Added method `size` for structs and unions.

- Allow last field in a struct to be a variable size array.

### Enums

- Integer values for enum types that are bitmasks are no longer
  automatically mapped to list of enum members. Note enum members are
  still implicitly converted to integer values. *Incompatible change*

- New `enum` subcommand `alias` to couple an enum definition as an alias.

- New `enum` subcommand `names` to return names of enum members.

### Pointers

- `pointer castable` can now take multiple arguments.

- Pointer definitions within a struct or union have an implicit
`unsafe` annotation.

### Function definitions

- New syntax for comments within `functions` and `stdcalls` definitions.

- Output struct parameters can be passed as undefined variables.

- Annotation `retval` can be applied to parameters to `void` functions.

- Added option `-ignoremissing` to `Wrapper` methods `functions` and
  `stdcalls` to not raise error in case of missing shared library
  functions.

- Lengths of variable size array parameters can be passed through
  `byref` or `inout` parameters.

- On error exceptions, the `errorCode` variable now includes the numeric
  error code when one of the error handling annotations is present.


### Miscellaneous

- Enhanced `help` command.

- Built-in aliases are now loaded in the `::cffi::c` namespace and
  implicitly included in the search path.

- Various [bug fixes](https://github.com/apnadkarni/tcl-cffi/milestone/13?closed=1).

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

