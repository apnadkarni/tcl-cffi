# Demo of cffi libgit2 extension. Poor man's git add emulation.
# Checks out a specific git reference (branch, tag or commit)
# Translated to Tcl from libgit2/examples/add.c
# tclsh git-add.tcl --help

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_options {arguments} {
    getopt::getopt opt arg $arguments {
        -f - --force {
            # Force checkout even in presence of modifications in
            # the working tree or cache
            option_set Force $arg
        }
        --guess {
            # Try mapping REF to a remote branch if no local match found.
            # Does not check if the mapping is unique.
            option_set Guess $arg
        }
        --progress {
            # Show progress of checkout
            option_set ShowProgress $arg
        }
        --git-dir:GITDIR {
            # Specify the path to the repository
            option_set GitDir $arg
        }
        arglist {
            # REF
            set refs $arg
        }
    }

    if {[llength $refs] == 1} {
        return [lindex $refs 0]
    }

    if {[llength $refs] == 0} {
        getopt::usage "No refs specified."
    } else {
        getopt::usage "Exactly one ref must be specified."
    }
}

proc guess_refish {pRepo ref} {
    # Try to map a ref to a remote branch
    set pRemotes [git_strarray allocate]
    try {
        git_remote_list $pRemotes $pRepo
        foreach remote [lg2_strarray_strings $pRemotes] {
            if {![catch {
                set pRef [git_reference_lookup $pRepo "refs/remotes/$remote/$ref"]
            } result edict]} {
                try {
                    set pAnnotatedCommit [git_annotated_commit_from_ref $pRepo $pRef]
                    return $pAnnotatedCommit
                } finally {
                    git_reference_free $pRef
                }
            }
        }
        # Look failed
        return -options $edict $result
    } finally {
        lg2_strarray_free $pRemotes
    }
}

proc print_checkout_progress {path completed_steps total_steps payload} {
    if {$path eq ""} {
        inform "checkout started: $total_steps steps"
    } else {
        inform "checkout: $path $completed_steps/$total_steps steps"
    }
    # void return value
}

proc perform_checkout_ref {pRepo pAnnotatedTarget target_ref} {

    # Set up checkout options
    set opts [git_checkout_options_init]
    if {[option Force 0]} {
        dict lappend opts Force GIT_CHECKOUT_FORCE
    } else {
        dict lappend opts Force GIT_CHECKOUT_SAFE
    }
    if {[option ShowProgress 1]} {
        set progress_cb [::cffi::callback ${::GIT_NS}::git_checkout_progress_cb print_checkout_progress]
        dict set opts progress_cb $progress_cb
    }

    # Retrieve the target commit
    set pCommitTarget [git_commit_lookup $pRepo [git_annotated_commit_id $pAnnotatedTarget]]
    try {
        # Check out to the work directory
        git_checkout_tree $pRepo $pCommitTarget $opts

        # Now update the HEAD
        set ref [git_annotated_commit_ref $pAnnotatedTarget]
        if {$ref eq ""} {
            git_repository_set_head_detached_from_annotated $pRepo $pAnnotatedTarget
        } else {
            set pRef [git_reference_lookup $pRepo $ref]
            if {[git_reference_is_remote $pRef]} {
                set pBranch [git_branch_create_from_annotated $pRepo $target_ref $pAnnotatedTarget 0]
                set target_head [git_reference_name $pBranch]
            } else {
                set target_head [git_annotated_commit_ref $pAnnotatedTarget]
            }
            git_repository_set_head $pRepo $target_head
        }
    } finally {
        if {[info exists pBranch]} {
            git_reference_free $pBranch
        }
        if {[info exists pRef]} {
            git_reference_free $pRef
        }
        git_commit_free $pCommitTarget
    }
}

proc git-checkout {} {
    set ref [parse_options $::argv]
    set pRepo [git_repository_open_ext [option GitDir .]]
    try {

        set state [git_repository_state $pRepo]
        if {$state ne "GIT_REPOSITORY_STATE_NONE"} {
            error "Repository is in unexpected state $state."
        }
        if {[catch {
            set pAnnotatedCommit [resolve_refish  $pRepo $ref]
        } result edict]} {
            if {![option Guess 0]} {
                return -options $edict $result
            }
            set pAnnotatedCommit [guess_refish $pRepo $ref]
        }
        perform_checkout_ref $pRepo $pAnnotatedCommit $ref

    } finally {
        if {[info exists pAnnotatedCommit]} {
            git_annotated_commit_free $pAnnotatedCommit
        }
        git_repository_free $pRepo
    }

}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-checkout} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    puts $edict
    exit 1
}
