# Demo of cffi libgit extension. Poor man's git config emulation from libgit2
# Translated to Tcl from libgit2/examples/config.c.

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

proc parse_config_options {arguments} {
    parse_options opt arg $arguments {
        -l - --list {
            # Show all configuration variables and their values
            set list 1
        }
        arglist {
            # [KEY [VALUE]]
            set rest $arg
        }
    }

    if {[info exists list]} {
        if {[llength $rest]} {
            getopt::usage "No additional arguments allowed if -l or --list specified."
        }
        return ""
    }
    if {[llength $rest] == 0 || [llength $rest] > 2} {
        getopt::usage "Wrong number of arguments."
    }

    return $rest
}

proc config_list {pConfig} {
    set pIter [git_config_iterator_new $pConfig]
    try {
        while {[git_config_next pEntry $pIter] eq "GIT_OK"} {
            # NOTE: pEntry is internal to pIter and NOT to be freed
            set nameP [${::GIT_NS}::git_config_entry getnative $pEntry name]
            set valueP [${::GIT_NS}::git_config_entry getnative $pEntry value]
            set name [::cffi::memory tostring! $nameP utf-8]
            set value [::cffi::memory tostring! $valueP utf-8]
            puts "$name=$value"
        }
    } finally {
        git_config_iterator_free $pIter
    }
}

proc config_get {pConfig key} {
    set pEntry [git_config_get_entry $pConfig $key]
    try {
        set valueP [${::GIT_NS}::git_config_entry getnative $pEntry value]
        puts [::cffi::memory tostring! $valueP utf-8]
        # Note valueP is unsafe (internal), not to be freed
    } finally {
        git_config_entry_free $pEntry
    }
}

proc config_set {pConfig key value} {
    git_config_set_string $pConfig $key $value
}

proc git-config {arguments} {
    set arguments [parse_config_options $arguments]

    set pRepo [git_repository_open_ext [option GitDir .]]
    try {
        set pConfig [git_repository_config $pRepo]
        if {[llength $arguments] == 0} {
            config_list $pConfig
        } elseif {[llength $arguments] == 1} {
            config_get $pConfig [lindex $arguments 0]
        } else {
            config_set $pConfig {*}$arguments
        }
    } finally {
        if {[info exists pConfig]} {
            git_config_free $pConfig
        }
        git_repository_free $pRepo
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-config $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
