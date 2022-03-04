# Demo of cffi libgit extension. Poor man's git diff emulation from libgit2
# Translated to Tcl from libgit2/examples/diff.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_diff_options {arguments} {
    variable DiffOptions
    variable DiffFindOptions
    variable DiffStatsFormat

    parse_options opt arg $arguments {
        -p - -u - --patch {
            # Generate patch (default)
            option_set Format GIT_DIFF_FORMAT_PATCH
            option_lappend Output Diff
        }
        --cached {
            # Show changes staged relative to a commit
            option_set_once Inputs $opt
        }
        --no-index {
            # Compare two paths on the file system.
            option_set_once Inputs $opt
        }
        --name-only {
            # Only show names of changed files
            option_set Format GIT_DIFF_FORMAT_NAME_ONLY
        }
        --name-status {
            # Only show name and status of changed files
            option_set Format GIT_DIFF_FORMAT_NAME_STATUS
        }
        --raw {
            # Show the diff in raw format
            option_set Format GIT_DIFF_FORMAT_RAW
        }
        --!color {
            # Enable color output
            option_set Colorize $arg
        }
        -R {
            # Swap the input order
            dict lappend DiffOptions flags GIT_DIFF_REVERSE
        }
        -a - --text {
            # Treat all files as text
            dict lappend DiffOptions flags GIT_DIFF_FORCE_TEXT
        }
        --ignore-space-at-eol {
            # Ignore end-of-line changes in whitespace
            dict lappend DiffOptions flags GIT_DIFF_IGNORE_WHITESPACE_EOL
        }
        -b - --ignore-space-change {
            # Ignore changes in whitespace
            dict lappend DiffOptions flags GIT_DIFF_IGNORE_WHITESPACE_CHANGE
        }
        -w - --ignore-all-space {
            # Completely ignore whitespace
            dict lappend DiffOptions flags GIT_DIFF_IGNORE_WHITESPACE
        }
        --ignored {
            # Include ignored files in the diff
            dict lappend DiffOptions flags GIT_DIFF_INCLUDE_IGNORED
        }
        --untracked {
            # Include untracked files in the diff
            dict lappend DiffOptions flags GIT_DIFF_INCLUDE_UNTRACKED
        }
        --patience {
            # Use patience differencing algorithm
            dict lappend DiffOptions flags GIT_DIFF_PATIENCE
        }
        --minimal {
            # Try producing the smallest possible diff
            dict lappend DiffOptions flags GIT_DIFF_MINIMAL
        }
        --stat {
            # Generate a diffstat.
            lappend DiffStatsFormat GIT_DIFF_STATS_FULL
        }
        --numstat {
            # Generate a diffstat in machine-friendly format
            lappend DiffStatsFormat GIT_DIFF_STATS_NUMBER
        }
        --shortstat {
            # Output only last line of --stat format
            lappend DiffStatsFormat GIT_DIFF_STATS_SHORT
        }
        --summary {
            # Output condensed summary of extended header
            lappend DiffStatsFormat GIT_DIFF_STATS_INCLUDE_SUMMARY
        }
        -M {
            # Detect renames with a default threshold of 50
            dict set DiffFindOptions rename_threshold 50
        }
        --find-renames:THRESHOLD {
            # Detect renames with specified threshold
            dict set DiffFindOptions rename_threshold [incr arg 0]
        }
        -C {
            # Detect copies/renames with a threshold of 50
            dict set DiffFindOptions copy_threshold 50
        }
        --find-copies:THRESHOLD {
            # Detect copies/renames with specified threshold
            dict set DiffFindOptions copy_threshold [incr arg 0]
        }
        --find-copies-harder {
            # Also check unmodified files as candidates for
            # the source in a copy
            dict lappend DiffFindOptions flags GIT_DIFF_FIND_COPIES_FROM_UNMODIFIED
i           dict with [${GIT_NS}::git_buf fromnative $pBuf] {}; # ptr, asize, size
        }
        -B {
            # Break complete rewrite changes into delete/create
            # pairs. Corresponds to --break-rewrites
            # with threshold defaulting to 50/60.
            dict set DiffFindOptions rename_from_rewrite_threshold 50
            dict set DiffFindOptions break_rewrite_threshold 60
        }
        --break-rewrites:THRESHOLD {
            # Break complete rewrite changes into delete and
            # create pairs. THRESHOLD must be of form [n][/m].
            # See documentation of git diff.
            lassign [split $arg /] n m
            if {$n eq ""} {
                set n 50
            }
            if {$m eq ""} {
                set m 60
            }
            dict set DiffFindOptions rename_from_rewrite_threshold [incr n 0]
            dict set DiffFindOptions break_rewrite_threshold [incr m 0]
        }
        -U - --unified:NLINES {
            # Generate patch with NLINES context.
            dict set DiffOptions context_lines [incr arg 0]
        }
        --inter-hunk-context:NLINES {
            # Show context between diff hunks up to NLINES
            dict set DiffOptions interhunk_lines [incr arg 0]
        }
        --abbrev:NCHARS {
            # Show only NCHARS of hex object name
            dict set DiffOptions id_abbrev [incr arg 0]
        }
        --src-prefix:PREFIX {
            # Use PREFIX instead of a/ as source prefix
            dict set DiffOptions old_prefix $arg
        }
        --dst-prefix:PREFIX {
            # Use PREFIX instead of b/ as destination prefix
            dict set DiffOptions old_prefix $arg
        }
        arglist {
            # [ARGS..]
            set rest $arg
        }
    }

    return $rest
}

proc update_diffopts_pathspec {pathspecs} {
    if {[llength $pathspecs] == 0} {
        return NULL
    }

    variable DiffOptions

    # Setting the path specs in the options is a bit tricky as they are embedded
    # as a git_strarray inside the options struct, and not as a git_strarray*
    # so first create an allocated strarray...
    set pPathSpec [lg2_strarray_new $pathspecs]
    # ..then do a value copy into a dictionary. The pointers in the dictionary
    # value will stay valid as long as pPathSpecs is not freed
    dict set opts pathspec [lg2_strarray fromnative $pPathSpec]

    # Return the path spec as it will need to be freed
    return $pPathSpec
}

proc diff_files {apath bpath} {
    variable DiffOptions
    # git_diff_from_buffer needs paths with forward slashes and at least one /
    set apath [file normalize $apath]
    set bpath [file normalize $bpath]
    set acontent [read_file $apath]
    set bcontent [read_file $bpath]
    set pBuf NULL
    set pPatch [git_patch_from_buffers \
                    $acontent [string length $acontent] $apath \
                    $bcontent [string length $acontent] $bpath \
                    $DiffOptions]
    try {
        # Allocate a buffer descriptor
        set pBuf [${::GIT_NS}::git_buf new]
        git_patch_to_buf $pBuf $pPatch
        lassign [${::GIT_NS}::git_buf fields $pBuf {ptr size}] ptr size
        puts [::cffi::memory tostring! $ptr]
        ::cffi::pointer safe $ptr; # Mark as safe for git_diff_from_buf benefit
        return [git_diff_from_buffer $ptr $size]
    } finally {
        # OK if pointers are NULL
        if {[info exists ptr]} {
            # Removes ptr from registered pointer list
            ::cffi::pointer dispose $ptr
        }
        git_buf_dispose $pBuf ; # Clear out libgit2 internal pointers
        ${::GIT_NS}::git_buf free $pBuf ;# Free our allocattion

        git_patch_free $pPatch
    }
}

proc print_stats {pDiff} {
    variable DiffStatsFormat

    if {![info exists DiffStatsFormat] || [llength $DiffStatsFormat] == 0} {
        return
    }

    set pBuf NULL
    set pStats [git_diff_get_stats $pDiff]
    try {
        set pBuf [${::GIT_NS}::git_buf new]
        git_diff_stats_to_buf $pBuf $pStats $DiffStatsFormat 80
        lassign [${::GIT_NS}::git_buf fields $pBuf {ptr size}] ptr size
        if {![::cffi::pointer isnull $ptr]} {
            # ptr is null terminated so we really do not need to use size
            puts [::cffi::memory tostring! $ptr]
        }
    } finally {
        # OK if pointers are NULL
        git_buf_dispose $pBuf ; # Clear out libgit2 internal pointers
        ${::GIT_NS}::git_buf free $pBuf ;# Free our allocattion
    }
}

proc git-diff {arguments} {
    variable DiffOptions
    variable DiffFindOptions

    set DiffOptions [git_diff_options_init]
    set DiffFindOptions [git_diff_find_options_init]

    set pPathSpec NULL
    set pDiff NULL
    set pRepo NULL

    set inputs [parse_diff_options $arguments]

    if {[option Inputs ""] eq "--no-index"} {
        # File to file comparison.
        # git diff --no-index path path
        # Treated separately because there is no commit, repository etc. involved
        if {[llength $inputs] != 2} {
            error "The --no-index option requires two file path arguments."
        }
        set pDiff [diff_files {*}$inputs]
        try {
            print_diffs $pDiff [option Format GIT_DIFF_FORMAT_PATCH]
        } finally {
            git_diff_free $pDiff
        }
        return
    }

    set pRepo [open_repository]
    try {
        # Try and figure out if passed arguments are possibly git tree references
        if {[llength $inputs] > 0} {
            if {![catch {set pTreeA [resolve_treeish $pRepo [lindex $inputs 0]]}]} {
                if {[file exists [lindex $inputs 0]]} {
                    error "Ambiguous argument \"[lindex $inputs 0]\". May be file or ref."
                }
                # First arg was a tree. See if second one is also a tree
                # Don't check whether it can be file just yet. Some options
                # will always interpret as file
                if {[llength $inputs] > 1} {
                    catch {set pTreeB [resolve_treeish $pRepo [lindex $inputs 1]]}
                }
            }
        }
        # Could be
        # (1) git diff [commit] [commit] [path ...]
        # (2) git diff [commit] [path ...]
        # (3) git diff [path ...]
        # (4) git diff --cached [commit] [path ...]
        if {[option Inputs ""] eq "--cached"} {
            # Case (4) Compare cache (index) against a commit
            if {[info exists pTreeA]} {
                # git diff --cached commit [path ...]
                set inputs [lrange $inputs 1 end]
            } else {
                # git diff --cached [path ...]
                set pTreeA [resolve_treeish $pRepo HEAD]
            }
            set pPathSpec [update_diffopts_pathspec $inputs]
            set pDiff [git_diff_tree_to_index $pRepo $pTreeA NULL $DiffOptions]
        } elseif {[info exists pTreeB]} {
            # Case (1) Note - this implies pTreeA also exists
            if {[file exists [lindex $inputs 1]]} {
                error "Ambiguous argument \"[lindex $inputs 1]\". May be file or ref."
            }
            set pPathSpec [update_diffopts_pathspec [lrange $inputs 2 end]]
            set pDiff [git_diff_tree_to_tree $pRepo $pTreeA $pTreeB $DiffOptions]
        } elseif {[info exists pTreeA]} {
            # Case (2)
            set pPathSpec [update_diffopts_pathspec [lrange $inputs 1 end]]
            # NOTE: NOT git_diff_tree_to_workdir - see libgit2 docs
            set pDiff [git_diff_tree_to_workdir_with_index $pRepo $pTreeA $DiffOptions]
        } else {
            # Case (3)
            set pPathSpec [update_diffopts_pathspec $inputs]
            set pDiff [git_diff_index_to_workdir $pRepo NULL $DiffOptions]
        }
        if {[::cffi::enum mask ${::GIT_NS}::git_diff_find_t [dict get $DiffFindOptions flags]] != 0} {
            git_diff_find_similar $pDiff $DiffFindOptions
        }

        print_stats $pDiff
        print_diffs $pDiff [option Format GIT_DIFF_FORMAT_PATCH]

    } finally {
        # Note - ok for pointers to be NULL
        lg2_strarray_free $pPathSpec
        git_diff_free $pDiff
        if {[info exists pTreeA]} {
            git_tree_free $pTreeA
        }
        if {[info exists pTreeB]} {
            git_tree_free $pTreeB
        }
        git_repository_free $pRepo
    }


}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-diff $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    puts stderr [dict get $edict -errorinfo]
    exit 1
}
