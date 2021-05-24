# Build instructions for the Tcl CFFI package

The `cffi` package is built using the standard methods for Tcl extensions
but requires the `dyncall` library to be downloaded and built first.
The `dyncall` sources can be downloaded from https://dyncall.org/download.
The `cffi` package requires at least V1.2 of `dyncall`. The following sections
summarize steps for building `dyncall`. Detailed instructions are available in the
[dyncall documentation](https://dyncall.org/docs/manual/manualse3.html#x4-140003.4)
and further detailed in the README files for each platform in the `doc` subdirectory
of the `dyncall` distribution.

## Building on Windows

The `dyncall` documentation describes multiple ways to build on Windows. The one
illustrated below uses `cmake` , `nmake` and Visual C++. (The commmands assume
CMake 3 versions prior to 3.13 and may need to be modified for later versions.)
If you do not have cmake, see the `dyncall` documentation for alternative methods.

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
the command.

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
$ LDFLAGS=-L/mnt/d/src/tcl-cffi/external-libs/ubuntu/x64 ../configure --enable-64bit
$ make
$ make install
```

Note the `LDFLAGS` environment variable that has to be specified if `dyncall` is not in a standard system directory.
