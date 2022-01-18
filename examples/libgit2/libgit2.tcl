# Mingw build of libgit
# mkdir build
# cd build
# cmake .. -G "MinGW Makefiles"
# cmake --build .
# or maybe
# cmake --build . -DCMAKE_BUILD_TYPE=Release
# or
# cmake .. -G "MinGW Makefiles" -DBUILD_CLAR=OFF -DEMBED_SSH_PATH=/d/src/AAThirdparty/C,C++/libssh2-1.10.0 -DCMAKE_BUILD_TYPE=Release -DUSE_BUNDLED_ZLIB=ON
# Edit build2\src\CMakeFiles\git2.dir linklibs.rsp to add -lbcrypt
# For a verbose build, add "-- VERBOSE=1" before the cmake --build command




if {![info exists GIT_NS]} {
    set GIT_NS [namespace current]::git
}

namespace eval $GIT_NS {
    variable packageDirectory [file dirname [file normalize [info script]]]
}

namespace eval $GIT_NS {

    # This function must be called to initialize the package with the path
    # to the shared library to be wrapped. By default it will try
    # looking in a platform-specific dir on Windows only, otherwise leave
    # it up to the system loader to find one.
    proc init {{path ""}} {
        variable packageDirectory
        variable functionDefinitions
        variable libgit2Path

        # The order of loading these files is important!!!
        # Later files depend on earlier ones, just like the C headers
        variable scriptFiles {
            common.tcl
            strarray.tcl
            global.tcl
            errors.tcl
            types.tcl
            buffer.tcl
            oid.tcl
            oidarray.tcl
            repository.tcl
            indexer.tcl
            odb.tcl
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
        }

        if {$path eq ""} {
            set path "libgit2[info sharedlibextension]"
            if {$::tcl_platform(platform) eq "windows"} {
                # Prioritize loading from dir if possible over some system file
                if {$::tcl_platform(machine) eq "amd64"} {
                    set subdir AMD64
                } else {
                    set subdir X86
                }
                if {[file exists [file join $packageDirectory $subdir $path]]} {
                    set path [file join $packageDirectory $subdir $path]
                }
            }
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
        cffi::alias define CB_PAYLOAD {pointer unsafe}
        cffi::alias define git_object_size_t uint64_t

        # Note these are sourced into current namespace
        foreach file $scriptFiles {
            source [file join $packageDirectory $file]
        }

        set ret [git_libgit2_init]

        # Redefine to no-op
        proc init args "return $ret"
        return $ret
    }
}
