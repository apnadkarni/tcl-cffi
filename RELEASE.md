# Release process

A manual proceess that should be automated. Some day ...

Assumes release tagged as v1.0a5 in working directory ...

## Preliminaries

Update Docs

- README.md
- Generate manpages
- Commit

Tag release

## Windows binaries

From MINGW64 shell

```
cd build-mingw-x64
rm *
../configure --enable-64bit --with-tcl=/d/tcl/mingw-8610/x64/lib LDFLAGS=-L/d/src/tcl-cffi/external-libs/mingw/x64/lib CPPFLAGS=-I/d/src/tcl-cffi/external-libs/mingw/x64/include  --prefix=/d/tcl --exec-prefix=/d/tcl
make
make test
make install
strip -s /d/tcl/lib/cffi1.0a5/AMD64/cffi10a5.dll
```

From MINGW32 shell

```
cd build-mingw-x86
rm *
../configure --with-tcl=/d/tcl/mingw-8610/x86/lib LDFLAGS=-L/d/src/tcl-cffi/external-libs/mingw/x86/lib CPPFLAGS=-I/d/src/tcl-cffi/external-libs/mingw/x86/include  --prefix=/d/tcl --exec-prefix=/d/tcl
make
make test
make install
strip -s /d/tcl/lib/cffi1.0a5/x86/cffi10a5.dll
```

Test the distribution with Tcl 8.6 built with VC++ 6

```
set TCLLIBPATH=d:/tcl/lib
cd tests
d:\tcl\archive\868-vc6\x64\bin\tclsh86t.exe all.tcl
d:\tcl\archive\868-vc6\x86\bin\tclsh86t.exe all.tcl
```

Zip up the distribution from d:\tcl\lib\cffi1.0a5

## Windows sources

From the *top* of the source repository

```
git archive --prefix cffi1.0a5-src/ -o d:\src\cffi1.0a5-src.zip v1.0a5
```

Extract the archive somewhere and ensure it builds.

## Unix sources

When generating a distribution from Windows, line endings are CR-LF. Rather than
fixing them after the fact, clone the repository in WSL.

```
git archive --prefix cffi1.0a5-src/ -o ../cffi1.0a5-src.tgz v1.0a5
```

Extract the archive somewhere and ensure it builds.

## Web site documentation

Upload documentation.
