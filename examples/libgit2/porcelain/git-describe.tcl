# Demo of cffi libgit extension. Poor man's git describe emulation from libgit2
# Translated to Tcl from libgit2/examples/describe.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

proc parse_describe_options {arguments} {
    variable DescribeOptions
    variable DescribeFormatOptions

    parse_options opt arg $arguments {
        --all {
            # Use any ref, not just annotated tags
            dict set DescribeOptions describe_strategy GIT_DESCRIBE_ALL
        }
        --tags {
            # Use any tag, not just annotated tags
            dict set DescribeOptions describe_strategy GIT_DESCRIBE_TAGS
        }
        --exact_match {
            # Only output tags directly matching commit
            if {$arg} {
                dict set DescribeOptions max_candidates_tags 0
            } else {
                # Revert to default
                dict set DescribeOptions max_candidates_tags 10
            }
        }
        --always {
            # Show commit object as fallback
            dict set DescribeOptions show_commit_oid_as_fallback $arg
        }
        --candidates:N {
            # Try matching against N recent tags instead of default 10
            dict set DescribeOptions max_candidates_tags [incr arg 0]
        }
        --first-parent {
            # Only follow first parent in a merge commit
            dict set DescribeOptions only_follow_first_pattern $arg
        }
        --match:GLOB {
            # Only consider tags matching the GLOB pattern
            dict set DescribeOptions pattern $arg
        }
        --long {
            # Always output long format even when tag is matched
            dict set DescribeFormatOptions always_use_long_format $arg
        }
        --dirty {
            # Append "-dirty" as a suffix when working tree is dirty
            dict set DescribeFormatOptions dirty_suffix "-dirty"
        }
        --abbrev:N {
            # Abbreviate to N digits of commit id instead of default 7
            dict set DescribeFormatOptions abbreviated_size [incr arg 0]
        }
        arglist {
            # [COMMIT-ISH ...]
            set rest $arg
        }
    }

    if {[dict get $DescribeFormatOptions dirty_suffix] eq ""} {
        if {[llength $rest] == 0} {
            lappend rest HEAD
        }
    } else {
        if {[llength $rest] > 0} {
            error "--dirty is incompatible with commit-ishes"
        }
    }
    return $rest
}

proc print_describe_result {pDescribeResult} {
    # Prints AND frees describe result
    variable DescribeFormatOptions
    set pBuf [git_buf new]
    try {
        git_describe_format $pBuf $pDescribeResult $DescribeFormatOptions
        puts [lg2_buf_tostr $pBuf]
    } finally {
        git_buf_dispose $pBuf; # Clears pBuf, NOT frees it!
        git_buf free $pBuf; # Frees pBuf itself
        git_describe_result_free $pDescribeResult
    }
}

proc describe_commit {pRepo commitish} {
    variable DescribeOptions
    set pCommit [git_revparse_single $pRepo $commitish]
    try {
        set pCommit [::cffi::pointer cast $pCommit ${::GIT_NS}::git_commit]
        print_describe_result [git_describe_commit $pCommit $DescribeOptions]
    } finally {
        git_object_free $pCommit
    }
}

proc git-describe {arguments} {
    variable DescribeOptions
    variable DescribeFormatOptions
    set DescribeOptions [git_describe_options_init]
    set DescribeFormatOptions [git_describe_format_options_init]

    set commitishes [parse_describe_options $arguments]

    set pRepo [open_repository]
    try {
        if {[llength $commitishes] == 0} {
            print_describe_result [git_describe_workdir $pRepo $DescribeOptions]
        } else {
            foreach commitish $commitishes {
                describe_commit $pRepo $commitish
            }
        }
    } finally {
        git_repository_free $pRepo
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-describe $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
