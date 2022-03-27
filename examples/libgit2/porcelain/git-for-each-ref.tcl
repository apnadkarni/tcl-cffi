# Demo of cffi libgit extension. Poor man's git for-each-ref emulation from libgit2
# Translated to Tcl from libgit2/examples/for-each-ref.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

proc parse_for-each-ref_options {arguments} {
    parse_options opt arg $arguments {
        --count:COUNT {
            # Stop after showing COUNT refs
            option_set MaxCount [incr arg 0]
        }
        arglist {
            # [PATTERN]
            set rest $arg
        }
    }
    if {[llength $rest] > 1} {
        getopt::usage "Too many arguments"
    }
    return [lindex $rest 0]
}

proc show_ref {pRepo pRef} {
    if {[git_reference_type $pRef] eq "GIT_REFERENCE_SYMBOLIC"} {
        set pResolved [git_reference_resolve $pRef]
        try {
            set oid [git_reference_target $pResolved]
        } finally {
            git_reference_free $pResolved
        }
    } else {
        set oid [git_reference_target $pRef]
    }
    set pObj NULL
    try {
        set oidstr [git_oid_tostr_s $oid]
        set pObj [git_object_lookup $pRepo $oid GIT_OBJECT_ANY]
        set type [git_object_type2string [git_object_type $pObj]]
        puts [format "%s %-6s\t%s" $oidstr $type [git_reference_name $pRef]]
    } finally {
        git_object_free $pObj
    }
}

proc git-for-each-ref {arguments} {
    set pattern [parse_for-each-ref_options $arguments]
    set pIter NULL
    set pRepo [open_repository]
    try {
        if {$pattern eq ""} {
            set pIter [git_reference_iterator_new $pRepo]
        } else {
            set pIter [git_reference_iterator_glob_new $pRepo $pattern]
        }
        set max [option MaxCount 100000000]
        set count 0
        while {[incr count] <= $max &&
               [git_reference_next pRef $pIter] eq "GIT_OK"} {
            # Note pRef is internal pointer, not to be freed.
            # But many APIs require a safe pointer so mark it as such
            ::cffi::pointer safe $pRef
            try {
                show_ref $pRepo $pRef
            } finally {
                # Note this only unregisters pRef, does NOT free it
                ::cffi::pointer dispose $pRef
            }
        }
    } finally {
        git_reference_iterator_free $pIter
        git_repository_free $pRepo
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-for-each-ref $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
