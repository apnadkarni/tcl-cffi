# Demo of cffi libgit extension. Poor man's git blame emulation from libgit2
# Translated to Tcl from libgit2/examples/blame.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_blame_options {arguments} {
    parse_options opt arg $arguments {
        -L: {
            # Only process lines within RANGE (m,n)
            option_set Range $arg
        }
        -C {
            # Find line moves within and across files
            option_lappend Flags GIT_BLAME_TRACK_COPIES_SAME_COMMIT_COPIES
        }
        -M {
            # Find line moves within and across files
            option_lappend Flags GIT_BLAME_TRACK_COPIES_SAME_COMMIT_MOVES
        }
        -F {
            # Only follow first parent commits
            option_lappend Flags GIT_BLAME_FIRST_PARENT
        }
        arglist {
            # [COMMITRANGE] PATH
            set rest $arg
        }
    }

    if {[llength $rest] == 0} {
        getopt::usage "No file specified."
    } elseif {[llength $rest] > 2} {
        getopt::usage "Too many arguments."
    } elseif {[llength $rest] == 1} {
        return $rest
    } else {
        return [list [lindex $rest 1] [lindex $rest 0]]
    }

    return $rest
}

proc git-blame {arguments} {
    lassign [parse_blame_options $arguments] path commit_range

    set opts [git_blame_options_init]
    dict set opts flags [option Flags GIT_BLAME_NORMAL]

    set pRepo [open_repository]
    try {
        set path [make_relative_path $path [git_repository_workdir $pRepo]]
        if {$commit_range ne ""} {
            set revspec [git_revparse $pRepo $commit_range]
            set flags [dict get $revspec flags]
            if {"GIT_REVSPEC_SINGLE" in $flags} {
                set from [dict get $revspec from]
                dict set opts newest_commit [git_object_id $from]
                git_object_free $from
            } elseif {"GIT_REVSPEC_RANGE" in $flags} {
                set from [dict get $revspec from]
                set to   [dict get $revspec to]
                dict set opts oldest_commit [git_object_id $from]
                dict set opts newest_commit [git_object_id $to]
                # Note - git_object is reference counted so we can call
                # git_object_free twice even if to == from
                git_object_free $to
                git_object_free $from
            } else {
                error "Unexpected or unsupported revspec flag(s) [dict get $revspec flags]"
            }
        }
        set pBlame [git_blame_file $pRepo $path $opts]
        set newest_commit_oid [dict get $opts newest_commit]
        if {[git_oid_is_zero $newest_commit_oid]} {
            set spec HEAD
        } else {
            set spec [git_oid_tostr_s $newest_commit_oid]
        }
        append spec : $path
        set pObject [git_revparse_single $pRepo $spec]
        set pBlob [git_blob_lookup $pRepo [git_object_id $pObject]]

        set data [git_blob_rawcontent $pBlob]
        set size [git_blob_rawsize $pBlob]

        # TBD - assumes utf-8 encoded
        # Not directly using tostring! because no guarantee it is null terminated
        set content [encoding convertfrom utf-8 [::cffi::memory tobinary! $data $size]]
        set line 1
        set i 0
        set len [string length $content]
        set break_on_null_hunk 0
        while {$i < $len} {
            set eol [string first \n $content $i]
            if {$eol == -1} {
                set eol $len
            }

            # NOTE: pHunk is an internal pointer. NOT to be freed
            set pHunk [git_blame_get_hunk_byline $pBlame $line]
            set isnull [::cffi::pointer isnull $pHunk]
            if {$break_on_null_hunk && $isnull} {
                break
            }
            if {! $isnull} {
                set break_on_null_hunk 1
                set hunk [git_blame_hunk fromnative! $pHunk]
                git_oid_tostr oid 9 [dict get $hunk final_commit_id]
                # Note pSig internal pointer, NOT to be freed
                set pSig [dict get $hunk final_signature]
                set sig [git_signature fromnative! $pSig]
                # TBD - Not sure about \r\n on Windows. Does the blob store the \r?
                set text [string trimright [string range $content $i $eol-1] \r]
                puts [format "$oid (%-30s %3d) $text" "[dict get $sig name] <[dict get $sig email]>" $line ]
            }

            incr line
            set i [expr {$eol+1}]
        }
    } finally {
        if {[info exists pBlob]} {
            git_blob_free $pBlob
        }
        if {[info exists pObject]} {
            git_object_free $pObject
        }
        if {[info exists pBlame]} {
            git_blame_free $pBlame
        }
        git_repository_free $pRepo
    }


}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-blame $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    pdict $edict
    exit 1
}
