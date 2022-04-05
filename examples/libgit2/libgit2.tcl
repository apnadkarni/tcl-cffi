# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Binding to libgit2 using Tcl CFFI. See README.md

namespace eval ::lg2 {
    variable packageDirectory [file dirname [file normalize [info script]]]
    variable libgit2Path
    variable libgit2SupportedVersions {1.3 1.4}

    # The order of loading these files is important!!!
    # Later files depend on earlier ones, just like the C headers
    variable initScriptFiles {
        common.tcl
        global.tcl
    }

    # The order of loading these files is important!!!
    # Later files depend on earlier ones, just like the C headers
    variable scriptFiles {
        strarray.tcl
        errors.tcl
        types.tcl
        buffer.tcl
        oid.tcl
        oidarray.tcl
        repository.tcl
        indexer.tcl
        odb.tcl
        odb_backend.tcl
        object.tcl
        tree.tcl
        net.tcl
        refspec.tcl
        diff.tcl
        checkout.tcl
        blob.tcl
        tag.tcl
        cert.tcl
        credential.tcl
        transport.tcl
        pack.tcl
        proxy.tcl
        remote.tcl
        apply.tcl
        annotated_commit.tcl
        attr.tcl
        blame.tcl
        branch.tcl
        index.tcl
        merge.tcl
        cherrypick.tcl
        commit.tcl
        config.tcl
        credential_helpers.tcl
        describe.tcl
        email.tcl
        filter.tcl
        graph.tcl
        ignore.tcl
        mailmap.tcl
        message.tcl
        notes.tcl
        pathspec.tcl
        patch.tcl
        rebase.tcl
        refdb.tcl
        reflog.tcl
        refs.tcl
        reset.tcl
        revert.tcl
        revparse.tcl
        revwalk.tcl
        signature.tcl
        stash.tcl
        status.tcl
        submodule.tcl
        trace.tcl
        transaction.tcl
        worktree.tcl
        clone.tcl
        lg2_util.tcl
    }

    proc lg2_locate_libgit2 {} {
        variable packageDirectory

        # Try find a libgit2 shared library to locate
        if {$::tcl_platform(platform) eq "windows"} {
            # Names depend on compiler (mingw/vc)
            set filenames [list libgit2.dll git2.dll]
        } else {
            set filenames [list libgit2[info sharedlibextension]]
        }
        package require platform
        foreach platform [platform::patterns [platform::identify]] {
            if {$platform eq "tcl"} continue
            foreach filename $filenames {
                set path [file join $packageDirectory $platform $filename]
                if {[file exists $path]} {
                    return $path
                }
            }
        }
        # Not found. Just return the shared library name
        return [lindex $filenames 0]
    }

    # This function must be called to initialize the package with the path
    # to the shared library to be wrapped. By default it will try
    # looking in a platform-specific dir on Windows only, otherwise leave
    # it up to the system loader to find one.
    proc lg2_init {{path ""}} {
        variable packageDirectory
        variable libgit2Path
        variable libgit2SupportedVersions
        variable initScriptFiles
        variable scriptFiles

        if {$path eq ""} {
            set path [lg2_locate_libgit2]
        }
        set libgit2Path $path

        uplevel #0 {package require cffi}
        ::cffi::alias load C

        ::cffi::Wrapper create libgit2 $libgit2Path

        # String encoding expected by libgit2
        cffi::alias define STRING string.utf-8
        # Standard error handler
        cffi::alias define GIT_ERROR_CODE \
            [list int nonnegative [list onerror [namespace current]::ErrorCodeHandler]]
        cffi::alias define PSTRARRAY pointer.git_strarray
        cffi::alias define PSTRARRAYIN pointer.lg2_strarray

        cffi::alias define CB_PAYLOAD {pointer unsafe nullok {default NULL}}
        cffi::alias define git_object_size_t uint64_t

        # Scripts needed for initialization
        # Note these are sourced into current namespace
        foreach file $initScriptFiles {
            source [file join $packageDirectory $file]
        }

        set ret [git_libgit2_init]

        git_libgit2_version major minor rev
        proc lg2_abi_version {} "return $major.$minor.$rev"
        proc lg2_abi_vsatisfies {args} {package vsatisfies [lg2_abi_version] {*}$args}
        if {![lg2_abi_vsatisfies {*}$libgit2SupportedVersions]} {
            error "libgit2 version $major.$minor.$rev is not supported. This package requires one of [join $libgit2SupportedVersions {, }]. Note libgit2 does not guarantee ABI compatibility between minor releases."
        }

        # Remaining scripts
        foreach file $scriptFiles {
            source [file join $packageDirectory $file]
        }

        # Redefine to no-op
        proc lg2_init args "return $ret"
        return $ret
    }
}
package provide lg2 0.1
