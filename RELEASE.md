# Release process

A manual process that should be automated. Some day ...

Assumes release as in working directory ...

## Preliminaries

Update Docs

- Update README.md
- Generate documentation - `cd doc && tclsh makedocs.tcl`
- Tag release
- Commit
- Push to github
- Push tag to github
- Verify CI builds pass

## Windows binaries

Run release.cmd to generate Windows binary distribution under `dist/cffi-VERSION`.
This will build the binaries for Tcl 8.6 and 9 for both x86 and x64.

Unzip into distribution into each of the 8.6 and 9.0 directories for
x86 and x64. Test that the package loads successfully.

## Source code archives

One of the following, both from WSL.

### Method 1

From WSL,

```
CFFIVER=$(egrep AC_INIT configure.ac | egrep -o --color=never '[0-9]\.[0-9][ab.][0-9]')
/mnt/c/bin/git-archive-all.sh --tree-ish v$CFFIVER --verbose --format tar /tmp/cffi$CFFIVER-src.tar 
cd /tmp && tar xvf /tmp/cffi$CFFIVER-src.tar 
gzip cffi$CFFIVER-src
zip -r cffi$CFFIVER-src.zip cffi$CFFIVER-src
```

NOTE: only use tar format above. Directly specifying zip or tgz creates
separate submodule archives.

### Method 2

```
cd /tmp
git clone --recurse-submodules git@github.com:apnadkarni/tcl-cffi.git cffi-u
cd cffi-u
CFFIVER=$(egrep AC_INIT configure.ac | egrep -o --color=never '[0-9]\.[0-9][ab.][0-9]')
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
