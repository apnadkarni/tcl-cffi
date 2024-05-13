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

Edit release-mingw.cmd to update Tcl paths and CFFIVER.
Run release-mingw.cmd from the top level. Distribution will be
in dist/mingw-CFFIVER directory.

Ensure the ldd output does not show dependency on libffi.dll

Test the distribution with Tcl 8.6 built with VC++ 6. May need
to copy cffitest.dll for the appropriate architecture.

```
set TCLLIBPATH=d:/tcl/lib
cd tests
d:\tcl\archive\868-vc6\x64\bin\tclsh86t.exe all.tcl
d:\tcl\archive\868-vc6\x86\bin\tclsh86t.exe all.tcl
```

Copy README.md and LICENSE into distribution directory.

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
