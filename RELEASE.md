# Release process

A manual process that should be automated. Some day ...

Assumes release tagged as in working directory ...

Assumes version set in environment variable CFFIVER (e.g. 1.0.7)

## Preliminaries

Update Docs

- README.md
- Generate documentation
- Tag release
- Commit
- Push to github
- Push tag to github
- Verify CI builds pass

## Windows binaries

From MINGW64 shell (note --enable-64bit required for correct installation into 
architecture-specific subdirectory). This builds the *libffi* version.

```
cd build-mingw-x64
export CFFIVERNODOT=${CFFIVER//./}
rm *
../configure --with-tcl=/d/tcl/mingw-8610/x64/lib --with-tclinclude=/d/tcl/mingw-8610/x64/include --prefix=/d/tcl --exec-prefix=/d/tcl --enable-64bit
make
make test
make install
strip -s /d/tcl/lib/cffi$CFFIVER/win32-x86_64/tclcffi$CFFIVERNODOT.dll
ldd tclcffi$CFFIVERNODOT.dll
cp cffitest.dll /d/tcl/lib/cffi$CFFIVER/win32-x86_64
```

Ensure the ldd output does not show dependency on libffi.dll

From MINGW32 shell

```
cd build-mingw-x86
export CFFIVERNODOT=${CFFIVER//./}
rm *
../configure --with-tcl=/d/tcl/mingw-8610/x86/lib --with-tclinclude=/d/tcl/mingw-8610/x86/include LDFLAGS=-L/d/src/tcl-cffi/external-libs/mingw/x86/lib CPPFLAGS=-I/d/src/tcl-cffi/external-libs/mingw/x86/include  --prefix=/d/tcl --exec-prefix=/d/tcl
make
make test
make install
strip -s /d/tcl/lib/cffi$CFFIVER/win32-ix86/tclcffi$CFFIVERNODOT.dll
ldd tclcffi$CFFIVERNODOT.dll
cp cffitest.dll /d/tcl/lib/cffi$CFFIVER/win32-ix86
```

Ensure the ldd output does not show dependency on libffi.dll

Test the distribution with Tcl 8.6 built with VC++ 6

```
set TCLLIBPATH=d:/tcl/lib
cd tests
d:\tcl\archive\868-vc6\x64\bin\tclsh86t.exe all.tcl
d:\tcl\archive\868-vc6\x86\bin\tclsh86t.exe all.tcl
```

Copy README.md and LICENSE into distribution directory.

Delete the cffitest.dll from both architectures.

Zip up the distribution from d:\tcl\lib\cffi$CFFIVER

## Windows sources

From the *top* of the source repository

```
git archive --prefix cffi%CFFIVER%-src/ -o d:\src\cffi%CFFIVER%-src.zip v%CFFIVER%
```

Extract the archive somewhere and ensure it builds.

## Unix sources

When generating a distribution from Windows, line endings are CR-LF. Rather than
fixing them after the fact, clone the repository in WSL.

```
git archive --prefix cffi$CFFIVER-src/ -o ../cffi$CFFIVER-src.tgz v$CFFIVER
```

Extract the archive somewhere and ensure it builds.

## Web site documentation

Upload documentation.
