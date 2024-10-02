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
