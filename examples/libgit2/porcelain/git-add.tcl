# Demo of cffi libgit extension. Poor man's git add emulation from libgit2
# Translated to Tcl from libgit2/examples/add.c. For options, type

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_add_options {arguments} {
    parse_options opt arg $arguments {
        -v - --verbose  {
            # Enable verbose output
            option_set Verbose $arg
        }
        -n - --dry-run {
            # Do not actually add file(s). Only show actions.
            option_set DryRun $arg
        }
        -u - --update {
            # Update the index only when it already has an entry for the path spec
            option_set Update $arg
        }
        -f - --force {
            # Force adding of ignored files
            option_set Force $arg
        }
        arglist {
            # ?PATHSPEC? ...
            set path_specs $arg
        }
    }
    if {[llength $path_specs] == 0} {
        getopt::usage "No paths specified."
    }
    return $path_specs
}

proc add_cb {pRepo path pathspec payload} {
    # We need to print what libgit would do so get the status of the file
    set status [git_status_file $pRepo $path]
    if {"GIT_STATUS_WT_MODIFIED" in $status || "GIT_STATUS_WT_NEW" in $status} {
        # Ask to skip if it is a dry run.
        set skip [option DryRun 0]
        inform "add '$path'" 1
    } else {
        set skip 1
    }
    return $skip
}

proc git-add {arguments} {
    set path_specs [parse_add_options $arguments]
    set pRepo [open_repository]

    try {
        set git_dir [git_repository_workdir $pRepo]
        # Paths must be relative to top and use forward slashes
        set path_specs [make_relative_to_workdir $pRepo $path_specs]

        set pIndex [git_repository_index $pRepo]

        # We can optionally set a callback to print actions taken as they happen
        # Only needed if we are either printing in verbose mode or doing a dry
        # run. Else no need for libgit to inform us.
        if {[option Verbose 0] || [option DryRun 0]} {
            # git_index_matched_path_cb is the prototype for the callback
            # The callback script is a command prefix where the first arg is the
            # repository object. The third argument -1 is to value to return from
            # callback in case of errors (-1 will abort the iteration)
            set callback [::cffi::callback new \
                              ::git::git_index_matched_path_cb \
                              [list [namespace current]::add_cb $pRepo] \
                              -1]
        } else {
            set callback NULL
        }

        set pPathSpecs [lg2_strarray_new $path_specs]
        if {[option Update 0]} {
            git_index_update_all $pIndex $pPathSpecs $callback NULL
        } else {
            set flags {}
            if {[option Force 0]} {
                # Flags from enum git_index_add_option_t
                lappend flags GIT_INDEX_ADD_FORCE
            }
            git_index_add_all $pIndex $pPathSpecs $flags $callback NULL
        }
        git_index_write $pIndex
    } finally {
        if {[info exists pIndex]} {
            git_index_free $pIndex
        }
        if {[info exists pPathSpecs]} {
            lg2_strarray_free $pPathSpecs
        }
        git_repository_free $pRepo
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-add $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
