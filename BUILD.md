# Build instructions for the Tcl CFFI package

The `cffi` package is built using the standard methods for Tcl extensions
but requires one of two back end libraries that implement FFI:

- `libffi` (v3.4.2 or later strongly recommended)
- `dyncall`

When choosing a back end, consider that the `dyncall` library 
at the time of writing does not support
returning or passing C structs to functions by value. However, most C API's pass
structs by reference (pointers) so this limitation may not matter. The `dyncall`
library is easier to build so that is another thing to consider if your platform
does not include `libffi** with its package manager.

## Building on Unix-like systems

**NOTE**

If not building against the system-installed Tcl, you may need to add
the option `--with-tclinclude=/path/to/private/tcl/install/include`.
This is particularly important if the private and system Tcl versions differ.

### Building on Unix using `libffi` back end

The first step is to install the `libffi` libraries. Most Unix systems will provide
this through the package manager as either `libffi` or `libffi-dev`. In addition,
depending on how your Tcl installation was built, you may also need to install
`libtommath`.  For example, on Ubuntu, these may be installed as

```
sudo apt install libffi-dev
sudo apt install libtommath-dev
```

If not available with the package manager, download sources from
http://sourceware.org/libffi/ and follow the instructions in the `README.md`
file in the downloaded sources.

**The libffi version must be at least 3.4.4 as earlier versions have a stack
corruption bug on x86 platforms.**

Once `libffi` is installed in standard system directories, the `cffi` package
itself can be built like any standard Tcl extension. From the top level
source directory,

```
mkdir build-ubuntu-x64
cd build-ubuntu-x64
../configure --with-libffi
make
make test
make install
```

The above assumes Tcl is also installed in standard system locations. The 
`--with-libffi` option need not be specified as `libffi` is the default back end.

The above commands will statically link `libffi`. To use the `libffi` shared
library, add the `--disable-staticffi` option to the `configure` command.

If `libffi` include files and libraries are not in a standard location in
the compiler or linker search paths, the `CFLAGS` and / or `LDFLAGS` environment
variables may be need to be specified.

For example, on MacOS when `libffi` is installed with `brew`, the required
`configure` will look something like

```
../configure --with-tcl=/usr/local/opt/tcl-tk/lib LDFLAGS="-L/usr/local/opt/libffi/lib" CFLAGS="-I/usr/local/Cellar/libffi/3.4.2/include" --disable-staticffi
```

The TEA build system using gcc does not strip symbols so you may want to
optionally strip the built library if desired. You can either do this using
`strip` after building or include the `-s` flag in `LDFLAGS` on the `configure`
commands above.

### Building on Unix using `dyncall` back end

Unlike `libffi`, `dyncall` is not included with most systems and must
be built from sources which can be downloaded from https://dyncall.org/download.
The `cffi` package requires at least V1.2 of `dyncall`.

The following summarizes basic steps for building dyncall.
Detailed instructions are available in the
[dyncall documentation](https://dyncall.org/docs/manual/manualse3.html#x4-140003.4)
and further detailed in the README files for each platform in the `doc` subdirectory
of the `dyncall` distribution.

After extracting the `dyncall` sources,

```
cd dyncall-1.2
mkdir build-ubuntu-x64
cd build-ubuntu-x64
../configure --prefix=/mnt/d/src/tcl-cffi/external-libs/ubuntu/x64
make
make install
```

Note if the `--prefix` option is not supplied, the build will install in the
system library directories.

As described for the `libffi` build, you may also need to install
the `libtommath-dev` package.

To build the `cffi` package, the `--with-dyncall` option must be specified
along with `CPPFLAGS` and `LDFLAGS` specifying the include and library
directories assuming they are not in standard locations.

From the top level `cffi` source directory,

```
mkdir build-ubuntu-x64
cd build-ubuntu-x64
../configure LDFLAGS=-L/mnt/d/src/tcl-cffi/external-libs/ubuntu/x64/lib CPPFLAGS=-I/mnt/d/src/tcl-cffi/external-libs/ubuntu/x64/include --with-dyncall
make
make test
make install
```

Again, you may want to strip symbols as described earlier.


## Building on Windows

Two back end libraries and two tool chains
(Visual C++/Nmake and MinGW-w64) give rise to four main possibilities
for building `cffi` on Windows.

### Building with Visual C++ using `libffi` back end

The `libffi` distribution has some complications with respect to building
with the Visual C++ tool chain so it is easiest to install it with
[`vcpkg`](https://github.com/microsoft/vcpkg/).
Assuming you have `vcpkg` installed as per the instructions in that link,
install both the 32-bit and 64-bit versions of the library with the
following at the command prompt.

```
vcpkg install libffi:x86-windows-static-md libffi:x64-windows-static-md
```

Note the above installs both the 32-bit and 64-bit libraries and include files
under the `installed` subdirectory under the `vcpkg** root directory.

**The libffi version must be at least 3.4.4 as earlier versions have a stack
corruption bug on x86 platforms.**

Once `libffi` is installed, to build the `cffi` extension, 
start a Visual Studio build command shell (32- or 64-bit depending on the
desired library) and cd to the top level of the `cffi` source directory.
Then run the commands

```
cd win
nmake /f makefile.vc INSTALLDIR=C:\tcl EXTDIR=c:\vcpkg\installed\x64-windows-static-md
nmake /f makefile.vc INSTALLDIR=C:\tcl EXTDIR=c:\vcpkg\installed\x64-windows-static-md install
```

The above assumes a 64-bit build with Tcl installed at `c:\tcl` and the `vcpkg`
root directory at `c:\vcpkg`. For 32-bit builds, change `EXTDIR` to
point to the 32-bit location (`x86` prefix instead of `x64`).

The Nmake based build always links the library statically. There is no option
to link against a DLL.

### Building with Visual C++ using `dyncall` back end

Download `dyncall` from https://dyncall.org/download and extract it.
The below illustration uses `cmake` , `nmake` and Visual C++ 2017. There is some
use of C99 headers and runtime so earlier versions of Visaul C++ may need some
editing of the code. The commmands also assume CMake 3 versions prior to 3.13
and may need to be modified for later versions. If you do not have cmake, see
the `dyncall` documentation for alternative methods.

Start a Visual Studio build command shell (32- or 64-bit depending on the
desired library) and cd to the top level of the `dyncall` source directory.
Then run commands as shown in the example below for a 64-bit build.

```
D:\src\dyncall>mkdir build-msvc-x64
D:\src\dyncall>cd build-msvc-x64
D:\src\dyncall\build-msvc-x64>cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=d:\src\tcl-cffi\external-libs\win\x64 -G"NMake Makefiles" ..
...output elided...
D:\src\dyncall\build-msvc-x64>cd ..
D:\src\dyncall>cmake --build build-msvc-x64 --config Release
...output elided...
D:\src\dyncall>cmake --build build-msvc-x64 --config Release --target install
```

This will build and install the `dyncall` headers and libraries under the
specified path `d:\src\tcl-cffi\external-libs\win\x64`. You can of course choose
a different path as long as the same is specified for the `cffi` build below.

The `cffi` package can now be built using the standard `nmake` based build system
for Tcl extensions. From within the `win` subdirectory in the `cffi` source
distribution, run the following commands to build and install the package.
The `OPTS=dyncall` option configures the build to use `dyncall` instead of
`libffi`.

```
nmake /f makefile.vc OPTS=dyncall INSTALLDIR=d:\tcl\debug\x64 EXTDIR=d:\src\tcl-cffi\external-libs\win\x64
nmake /f makefile.vc OPTS=dyncall INSTALLDIR=d:\tcl\debug\x64 EXTDIR=d:\src\tcl-cffi\external-libs\win\x64 install
```

Note the path to the `dyncall` *installation* directory has to be specified in
the command as the `EXTDIR` variable.

### Building with MinGW-w64/gcc using `libffi` back end

As an alternative to Visual C++, the package may also be build with the
MinGW-w64 gcc compiler suite. This follows almost the same exact method as the
Unix builds described above as the steps below illustrate. NOTE: one
difference - the --enable-64bit option for 64-bit builds must be specified else
`make install` installs into the wrong directory.

For MinGW-w64, libffi can be installed with the following commands.

```
pacboy sync libffi:i libffi:x
```

The `:i` and `:x` suffixes correspond to `gcc` 32- and 64-bit libraries
respectively. For `clang` these would be `z` and `c` instead.

**The libffi version must be at least 3.4.4 as earlier versions have a stack
corruption bug on x86 platforms.**

Then follow the standard procedure for Tcl extensions.
From a MinGW-w64 (not msys2) 64-bit shell, run the following commands at the top level

```
mkdir build-mingw-x64
../configure --with-tcl=/d/tcl/mingw-8610/x64/lib --with-tclinclude=/d/tcl/mingw-8610/x64/include --enable-64bit
make
make test
```

Note that the `--with-tcl` and `--with-tclinclude` options have to be specified
pointing to a Tcl installation that was also built with MinGW-w64. Without this,
the build system will use the `tclsh` that comes with the `msys2` installation
that comes with MinGW-w64. That is **not** what you want.

For 32-bit builds, use the 32-bit shell and leave off the `--enable-64bit` option.

The TEA build system using gcc does not strip symbols so you may want to optionally
strip the built library if desired. On Windows, this will shrink the binary by
more than a factor of three. You can either do this using `strip` after building
or include the `-s` flag in `LDFLAGS` on the `configure` commands above.

### Building with MinGW-w64/gcc using `dyncall` back end

From a MinGW-w64 (not msys2) 64-bit shell, run the following commands at the top level
`dyncall` directory. 

```
$ mkdir build-mingw-x64
$ cd build-mingw-x64
$ ../configure --prefix=/d/src/tcl-cffi/external-libs/mingw/x64
$ make
$ make install
```

Then build the `cffi` package. From the `cffi` top level directory,

```
$ mkdir build-mingw-x64
$ cd build-mingw-x64
$ ../configure --with-tcl=/d/tcl/mingw-8610/x64/lib --with-tclinclude=/d/tcl/mingw-8610/x64/include LDFLAGS=-L/d/src/tcl-cffi/external-libs/mingw/x64/lib CPPFLAGS=-I/d/src/tcl-cffi/external-libs/mingw/x64/include --enable-64bit --with-dyncall
$ make
$ make install
```

Note the `--with-dyncall` option in the `configure` command.

