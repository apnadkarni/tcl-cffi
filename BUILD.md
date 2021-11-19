# Build instructions for the Tcl CFFI package

The `cffi` package is built using the standard methods for Tcl extensions
but requires the `dyncall` library to be downloaded and built first.
The `dyncall` sources can be downloaded from https://dyncall.org/download.
The `cffi` package requires at least V1.2 of `dyncall`. The following sections
summarize steps for building `dyncall`. Detailed instructions are available in the
[dyncall documentation](https://dyncall.org/docs/manual/manualse3.html#x4-140003.4)
and further detailed in the README files for each platform in the `doc` subdirectory
of the `dyncall` distribution.

## Building on Unix-like systems

Again, the first step is to build `dyncall` using one of the prescribed methods
in the `dyncall` documentation. For example, using the `configure` script, from
 the top level directory of the distribution, run the following commands in a shell.

```
$ mkdir build-ubuntu-x64
$ cd build-ubuntu-x64
$ ../configure --prefix=/mnt/d/src/tcl-cffi/external-libs/ubuntu/x64
$ make
$ make install
```

Note if the `--prefix` option is not supplied, the build will install in the
system library directories.

The `cffi` package can then be built like any standard TEA based Tcl extension.

```
$ mkdir build-ubuntu-x64
$ cd build-ubuntu-x64
$ ../configure LDFLAGS=-L/mnt/d/src/tcl-cffi/external-libs/ubuntu/x64/lib CPPFLAGS=-I/mnt/d/src/tcl-cffi/external-libs/ubuntu/x64/include 
$ make
$ make install
```

Note the `LDFLAGS` and `CPPFLAGS` environment variables have to be specified if
`dyncall` libraries and headers are not in a standard system directory.

The TEA build system using gcc does not strip symbols so you may want to
optionally strip the built library if desired. You can either do this using
`strip` after building or include the `-s` flag in `LDFLAGS` on the `configure`
commands above.


## Building on Windows

The `dyncall` documentation describes multiple ways to build on Windows. Just
two of these are illustrated here. Note that pre-built binaries are also
available from https://sourceforge.net/projects/magicsplat/files/cffi.

### Building with Visual C++

The first example illustrated uses `cmake` , `nmake` and Visual C++ 2017. There
is some use of C99 headers and runtime so earlier version may need some editing
of the code. The commmands also assume CMake 3 versions prior to 3.13 and may need to
be modified for later versions. If you do not have cmake, see the `dyncall`
documentation for alternative methods.

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

This will build and install the `dyncall` headers and libraries under the specified
path `d:\src\tcl-cffi\external-libs\win\x64`. You can of course choose a different
path as long as the same is specified for the `cffi` build below

The `cffi` package can now be built using the standard `nmake` based build system
for Tcl extensions. From within the `win` subdirectory in the `cffi` source
distribution, run the following commands to build and install the package.

```
nmake /f makefile.vc INSTALLDIR=d:\tcl\debug\x64 DYNCALLDIR=d:\src\tcl-cffi\external-libs\win\x64
nmake /f makefile.vc INSTALLDIR=d:\tcl\debug\x64 DYNCALLDIR=d:\src\tcl-cffi\external-libs\win\x64 install
```

Note the path to the `dyncall` *installation* directory has to be specified in
the command as the `DYNCALLDIR` variable.

### Building with MinGW-w64/gcc

As an alternative to Visual C++, the package may also be build with the MinGW-w64
gcc compiler suite. This follows almost the same exact method as the Unix builds
described above as the steps below illustrate.

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
$ ../configure --with-tcl=/d/tcl/mingw-8610/x64/lib LDFLAGS=-L/d/src/tcl-cffi/external-libs/mingw/x64/lib CPPFLAGS=-I/d/src/tcl-cffi/external-libs/mingw/x64/include 
$ make
$ make install
```

Note that the `--with-tcl` option has to be specified pointing to a Tcl
installation that was also built with MinGW-w64. Without this, the build system
will use the `tclsh` that comes with the `msys2` installation that comes with
MinGW-w64. That is **not** what you want.

The TEA build system using gcc does not strip symbols so you may want to optionally
strip the built library if desired. On Windows, this will shrink the binary by
more than a factor of three. You can either do this using `strip` after building
or include the `-s` flag in `LDFLAGS` on the `configure` commands above.
