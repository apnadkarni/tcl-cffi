# Demo of cffi libgit extension. Poor man's git log emulation from libgit2
# Translated to Tcl from libgit2/examples/log.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

proc parse_log_options {arguments} {
    parse_options opt arg $arguments {
        --date-order {
            # Show in commit timestamp order
            option_lupdate SortOrder GIT_SORT_TOPOLOGICAL $arg
        }
        --topo-order {
            # Do not intermix commits on different history lines
            option_lupdate SortOrder GIT_SORT_TIME $arg
        }
        --reverse {
            # Show commits in reverse order
            option_lupdate SortOrder GIT_SORT_REVERSE $arg
        }
        --committer:REGEXP {
            # Limit output to those with committer matching REGEXP (case-insensitive)
            if {[catch {regexp -about $arg}]} {
                error "Invalid regular expression \"$arg\"."
            }
            option_set CommitterRE $arg
        }
        --author:REGEXP {
            # Limit output to those with author matching REGEXP (case-insensitive)
            if {[catch {regexp -about $arg}]} {
                error "Invalid regular expression \"$arg\"."
            }
            option_set AuthorRE $arg
        }
        --grep:REGEXP {
            # Only show lines matching REGEXP (case-insensitive)
            if {[catch {regexp -about $arg}]} {
                error "Invalid regular expression \"$arg\"."
            }
            option_set MessageRE $arg
        }
        --skip:COUNT {
            # Skip COUNT commits before showing output
            option_set SkipCount [incr arg 0]
        }
        -n - --max-count:COUNT {
            # Show at most COUNT commits
            option_set MaxCount [incr arg 0]
        }
        --merges {
            # Only show merge commits
            if {$arg} {
                option_set MinParents 2; # --merges
            } else {
                option_set MaxParents 1; # --no-merges
            }
        }
        --min-parents:NUMPARENTS {
            # Only show commits with at least NUMPARENTS parents
            option_set MinParents [incr arg 0]
        }
        --max-parents:NUMPARENTS {
            # Only show commits with at most NUMPARENTS parents
            option_set MaxParents [incr arg 0]
        }
        -p -
        -u -
        --patch {
            # Generate a patch
            option_set ShowPatch $arg
        }
        --log-size {
            # Include a "log size" line indicating size of log message
            option_set ShowLogSize $arg
        }
        arglist {
            # [REVISIONS] [PATH ...]
            set rest $arg
        }
    }

    return $rest
}

proc changed_from_parent {pRepo pCommit parent_index diffopts} {
    try {
        set pParent [git_commit_parent $pCommit $parent_index]
        set pParentTree [git_commit_tree $pParent]
        set pTree [git_commit_tree $pCommit]
        set pDiff [git_diff_tree_to_tree $pRepo $pParentTree $pTree $diffopts]
        return [expr {[git_diff_num_deltas $pDiff] > 0}]
    } finally {
        if {[info exists pDiff]} {
            git_diff_free $pDiff
        }
        if {[info exists pParentTree]} {
            git_tree_free $pParentTree
        }
        if {[info exists pTree]} {
            git_tree_free $pTree
        }
        if {[info exists pParent]} {
            git_commit_free $pParent
        }
    }
}

proc changed {pRepo pCommit pPathSpec nparents diffopts} {
    if {$nparents == 0} {
        # Root commit
        set pTree [git_commit_tree $pCommit]
        try {
            if {[git_pathspec_match_tree_raw NULL $pTree GIT_PATHSPEC_NO_MATCH_ERROR $pPathSpec] == 0} {
                return 1
            } else {
                return 0
            }
        } finally {
            git_tree_free $pTree
        }
    }

    if {$nparents == 1} {
        return [changed_from_parent $pRepo $pCommit 0 $diffopts]
    }

    # Multiple parents as in merge
    for {set i 0} {$i < $nparents} {incr i} {
        if {![changed_from_parent $pRepo $pCommit $i $diffopts]} {
            return 0
        }
    }
    # Changed from all parents
    return 1 fs
}

proc parse_revspec {pRepo pWalk revspec} {
    if {[catch {
        set revs [git_revparse $pRepo $revspec]
    }]} {
        return 0; # Not a rev spec.
    }

    try {
        dict with revs {}
        if {"GIT_REVSPEC_SINGLE" in $flags} {
            git_revwalk_push $pWalk [git_object_id $from]
        } elseif {"GIT_REVSPEC_RANGE" in $flags} {
            git_revwalk_push $pWalk [git_object_id $to]
            git_revwalk_hide $pWalk [git_object_id $from]
        } else {
            # We do not support the A...B syntax
            error "Unsupported revision syntax \"$revspec\"."
        }
    } finally {
        if {![cffi::pointer isnull $from]} {
            git_object_free $from
        }
        if {![cffi::pointer isnull $to]} {
            git_object_free $to
        }
    }
    return 1
}

proc matches_signature {re sig} {
    if {[regexp -nocase -- $re [dict get $sig name]] ||
        [regexp -nocase -- $re [dict get $sig email]]} {
        return 1
    }
    return 0
}

proc print_commit {pCommit} {
    puts "commit [git_oid_tostr_s [git_commit_id $pCommit]]"
    set message [lg2_commit_message $pCommit]
    if {[option ShowLogSize 0]} {
        puts "log size [string length $message]"
    }

    # Parents only shown if more than one
    set nparents [git_commit_parentcount $pCommit]
    if {$nparents > 1} {
        for {set i 0} {$i < $nparents} {incr i} {
            git_oid_tostr oid 8 [git_commit_parent_id $pCommit $i]
            lappend parents $oid
        }
        puts "Merge: [join $parents { }]"
    }
    set sig [git_commit_author $pCommit]
    print_signature Author: $sig
    print_git_time Date: [dict get $sig when]

    puts "\n    $message"
}

proc print_patch {pRepo pCommit diffopts} {
    set nparents [git_commit_parentcount $pCommit]
    if {$nparents > 1} {
        return
    }

    set aTree NULL
    set bTree [git_commit_tree $pCommit]
    set pDiff NULL
    try {
        if {$nparents == 1} {
            set pParent [git_commit_parent $pCommit 0]
            try {
                set aTree [git_commit_tree $pParent]
            } finally {
                git_commit_free $pParent
            }
        }
        set pDiff [git_diff_tree_to_tree $pRepo $aTree $bTree $diffopts]
        print_diffs $pDiff GIT_DIFF_FORMAT_PATCH
    } finally {
        # Note - ok to call *free for NULLs
        git_diff_free $pDiff
        git_tree_free $aTree
        git_tree_free $bTree
    }
}

proc git-log {arguments} {
    set arguments [parse_log_options $arguments]

    set pRepo [open_repository]
    try {
        # Create the revision walker and set sorting mode.
        set pWalk [git_revwalk_new $pRepo]
        git_revwalk_sorting $pWalk [option SortOrder GIT_SORT_NONE]

        # Check if a revision spec was supplied.
        # This is a teeny bit buggy since the revision spec might look like
        # a revision spec but is intended to be a file path. That's a limitation
        # of our option parsing package that doesn't let us handle this before
        # a --.
        if {[llength $arguments] > 0 &&
            [parse_revspec $pRepo $pWalk [lindex $arguments 0]]} {
                set arguments [lrange $arguments 1 end]
        } else {
            git_revwalk_push_head $pWalk
        }

        set diffopts [git_diff_options_init]

        # Check if we have to match against path specifications
        set npathspecs [llength $arguments]
        if {$npathspecs > 0} {
            # Path specs need to be made relative to work dir and use forward /
            # Also, setting the path specs in the options is a bit tricky as they are
            # embedded as a git_strarray inside the options struct, and not as a
            # git_strarray* so first create an allocated strarray...
            set pStrArray [lg2_strarray_new [make_relative_to_workdir $pRepo $arguments]]
            # ..then do a value copy into a dictionary. The pointers in the dictionary
            # value will stay valid as long as pPathSpecs is not freed
            dict set diffopts pathspec [lg2_strarray fromnative $pStrArray]
            set pPathSpec [git_pathspec_new $pStrArray]
        }
        set min_parents [option MinParents 0]
        set max_parents [option MaxParents -1]
        if {$max_parents < 0} {
            set max_parents 1000000000; # Effectively remove limit
        }

        set print_count [option MaxCount -1]
        set skip_count [option SkipCount 0]
        set nskipped 0
        set nprinted 0

        while {[git_revwalk_next oid $pWalk] == "GIT_OK"} {
            set pCommit [git_commit_lookup $pRepo $oid]
            try {
                # Skip if parent count does not meet conditions
                set nparents [git_commit_parentcount $pCommit]
                if {$nparents < $min_parents} continue
                if {$nparents > $max_parents} continue

                # Skip if no match against path specs
                if {$npathspecs > 0 &&
                    ![changed $pRepo $pCommit $pPathSpec $nparents $diffopts]} {
                    continue
                }
                # Skip if author specified does not match
                if {[option? AuthorRE re] &&
                    ![matches_signature $re [git_commit_author $pCommit]]} {
                        continue
                }
                # Skip if committer specified does not match
                if {[option? CommitterRE re] &&
                    ![matches_signature $re [git_commit_committer $pCommit]]} {
                    continue
                }
                # Skip if log message does not match
                if {[option? MessageRE re] &&
                    ![regexp -nocase $re [lg2_commit_message $pCommit]]} {
                    continue
                }

                if {[incr nskipped] <= $skip_count} continue

                # Print the main commit content
                print_commit $pCommit

                # Print the diff if requested, but only for single parents
                if {[option ShowPatch 0]} {
                    print_patch $pRepo $pCommit $diffopts
                }

                if {$print_count >= 0 && [incr nprinted] == $print_count} {
                    break
                }
            } finally {
                git_commit_free $pCommit
            }
        }
    } finally {
        if {[info exists pStrArray]} {
            lg2_strarray_free $pStrArray
        }
        if {[info exists pWalk]} {
            git_revwalk_free $pWalk
        }
        git_repository_free $pRepo
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]

# The standard CFFI definition of git_pathspec_match_tree actually returns the
# matched path specs. For our purposes, this is inefficient since we only want
# to know if there is a match or not. This requires passing a NULL pointer which
# the standard definitions does not allow for. So we just define our own CFFI for the
# same function with different annotations that uses raw pointer values.
namespace eval $::GIT_NS {
    libgit2 function {git_pathspec_match_tree git_pathspec_match_tree_raw} int {
        ppMatches {pointer.PPATHSPEC_MATCH_LIST nullok unsafe}
        pTree PTREE
        flags     GIT_PATHSPEC_FLAG_T
        ps        PPATHSPEC
    }
}



catch {git-log $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
