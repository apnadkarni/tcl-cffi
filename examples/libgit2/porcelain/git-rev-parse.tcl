# Demo of cffi libgit extension. Poor man's git rev-parse emulation from libgit2
# Translated to Tcl from libgit2/examples/rev-parse.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

proc parse_rev-parse_options {arguments} {
    parse_options opt arg $arguments {
        arglist {
            # [REVSPEC ... 
            set rest $arg
        }
    }


    return $rest
}

proc git-rev-parse {arguments} {
    set revspecs [parse_rev-parse_options $arguments]

    set pRepo [open_repository]
    try {
        foreach revspec $revspecs {
            set rev [git_revparse $pRepo $revspec]
            try {
                dict with rev {
                    if {"GIT_REVSPEC_SINGLE" in $flags} {
                        puts [git_oid_tostr_s [git_object_id $from]]
                    } elseif {"GIT_REVSPEC_RANGE" in $flags} {
                        set to_oid [git_object_id $to]
                        set from_oid [git_object_id $from]
                        puts [git_oid_tostr_s $to_oid]
                        if {"GIT_REVSPEC_MERGE_BASE" in $flags} {
                            set base_oid [git_merge_base $pRepo $from_oid $to_oid]
                            puts [git_oid_tostr_s $base_oid]
                        }
                        puts "^[git_oid_tostr_s [git_object_id $from]]"
                    } else {
                        error "Invalid revspec flags '$flags' parsing '$revspec.'"
                    }

                }
            } finally {
                # OK if NULL
                git_object_free [dict get $rev to]
                git_object_free [dict get $rev from]
            }
        }
    } finally {
        git_repository_free $pRepo
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-rev-parse $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
