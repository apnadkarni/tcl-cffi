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

    proc AddFunction {name type params} {
        variable functionDefinitions
        set functionDefinitions($name) [list $type $params]
    }
    proc AddFunctions {functions} {
        variable functionDefinitions
        array set functionDefinitions $functions
    }

    # Lazy initialization
    #
    # The wrapped functions are lazy-initialized by default, i.e. they are
    # actually wrapped the first time they are called. This has two benefits:
    #  - faster initialization since symbols do not need to be looked up in
    #    the symbol table if they are never used
    #  - more important, symbols that are not defined in the shared library
    #    (if it is an older version for example) will not prevent the package
    #    from loading and other functions that are defined can still be used.

    proc LoadLibgit2 {} {
        variable packageDirectory
        variable libgit2Path

        ::cffi::Wrapper create libgit2 $libgit2Path

        # Redefine as no-op for further calls
        proc LoadPackage args {}
    }

    proc InitFunctions {{lazy 1}} {
        variable functionDefinitions

        unset -nocomplain functionDefinitions(#); # Disregard comments
        if {$lazy} {
            foreach {fn_name prototype} [array get functionDefinitions] {
                if {[llength $fn_name] > 1} {
                    set cmd_name [lindex $fn_name 1]
                } else {
                    set cmd_name $fn_name
                }

                proc $cmd_name args "LoadLibgit2; libgit2 function $fn_name [list {*}$prototype]; tailcall $cmd_name {*}\$args"
            }
        } else {
            LoadLibgit2
            foreach {fn_name prototype} [array get functionDefinitions] {
                # Wrap in catch because else function name causing error does not
                # show in error stack making finding fault tedious.
                if {[catch {
                    libgit2 function $fn_name {*}$prototype
                } result]} {
                    throw {CFFI LIBGIT2 LOAD} "Error loading $fn_name: $result"
                }
            }
        }
    }

    # This function must be called to initialize the package with the path
    # to the shared library to be wrapped. By default it will try
    # looking in a platform-specific dir on Windows only, otherwise leave
    # it up to the system loader to find one.
    proc init {{path ""} {lazy 1}} {
        variable packageDirectory
        variable functionDefinitions
        variable libgit2Path

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

        # String encoding expected by libgit2
        cffi::alias define STRING string.utf-8
        # Standard error handler
        cffi::alias define GIT_ERROR_CODE \
            [list int nonnegative [list onerror [namespace current]::ErrorCodeHandler]]
        cffi::alias define git_object_size_t uint64_t

        source [file join $packageDirectory common.tcl]
        source [file join $packageDirectory strarray.tcl]
        source [file join $packageDirectory global.tcl]
        source [file join $packageDirectory errors.tcl]
        source [file join $packageDirectory types.tcl]
        source [file join $packageDirectory buffer.tcl]
        source [file join $packageDirectory oid.tcl]
        source [file join $packageDirectory oidarray.tcl]
        source [file join $packageDirectory repository.tcl]
        source [file join $packageDirectory indexer.tcl]
        source [file join $packageDirectory odb.tcl]
        source [file join $packageDirectory object.tcl]
        source [file join $packageDirectory tree.tcl]
        source [file join $packageDirectory net.tcl]
        source [file join $packageDirectory refspec.tcl]
        source [file join $packageDirectory diff.tcl]

        InitFunctions $lazy

        # Redefine to no-op
        proc init args {}
    }


}
