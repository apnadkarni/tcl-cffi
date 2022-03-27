# Demo of cffi libgit extension. Poor man's git push emulation from libgit2
# Translated to Tcl from libgit2/examples/push.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

proc parse_push_options {arguments} {
    parse_options opt arg $arguments {
        arglist {
            # [REMOTE [REFSPEC ...]]
            set rest $arg
        }
    }

    return $rest
}

proc git-push {arguments} {
    set arguments [parse_push_options $arguments]
    if {[llength $arguments] > 0} {
        set arguments [lassign $arguments remote]
        if {[llength $arguments] > 0} {
            set refspecs $arguments
        }
        set remote "origin"
    }

    if {![info exists remote]} {
        set remote "origin"
        puts "No remote specified. Defaulting to $remote."
    }
    if {![info exists refspecs]} {
        set refspecs [list "refs/heads/main"]
        puts "No refspecs specified. Defaulting to $refspecs."
    }
    puts "Pushing [join $refspecs {, }] to $remote."

    set push_opts [git_push_options_init]
    set pRefSpecs NULL
    set pRemote NULL
    set pRepo [open_repository]
    try {
        set credentials_cb \
            [::cffi::callback new ${::GIT_NS}::git_credential_acquire_cb \
                 cred_acquire_cb -1]
        dict set push_opts callbacks credentials $credentials_cb

        set pRemote [git_remote_lookup $pRepo $remote]
        set pRefSpecs [lg2_strarray_new $refspecs]
        git_remote_push $pRemote $pRefSpecs $push_opts
        puts "pushed to $remote"
    } finally {
        lg2_strarray_free $pRefSpecs
        git_repository_free $pRepo
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-push $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
