# Release process

A manual process that should be automated. Some day ...

Assumes release as in working directory ...

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

Run release.cmd to generate Windows binary distribution under `dist/latest`.
This will build the binaries for Tcl 8.6 and 9 for both x86 and x64.

Copy README.md and LICENSE into distribution directory.

Rename `latest` to `cffiVERSION` and zip it.

Unzip into distribution into each of the 8.6 and 9.0 directories for
x86 and x64. Test that the package loads successfully.

## Source code archives

Clone the repository in WSL.

```
cd /tmp
git clone --recurse-submodules git@github.com:apnadkarni/tcl-cffi.git cffi-u
cd cffi-u
git archive --prefix cffi$CFFIVER-src/ v$CFFIVER | (cd /tmp ; tar xf -)
cd ./tclh
git archive --prefix cffi$CFFIVER-src/tclh/ HEAD | (cd /tmp ; tar xf -)
cd /tmp
tar -czf cffi$CFFIVER-src.tgz cffi$CFFIVER-src
zip -r cffi$CFFIVER-src.zip cffi$CFFIVER-src
```

Extract the archives somewhere and ensure it builds.

## Web site documentation

Upload documentation.
