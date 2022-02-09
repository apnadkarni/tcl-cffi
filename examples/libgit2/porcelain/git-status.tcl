# Demo of cffi libgit extension. Poor man's git status emulation from libgit2
# Translated to Tcl from libgit2/examples/status.c
# tclsh git-status.tcl --help

proc parse_options {arguments} {
    getopt::getopt opt arg $arguments {
        -s - --short {
            # Show output in short format.
            option_set Format short
        }
        --long {
            # Show output in long format.
            option_set Format long
            option_set ShowBranch 1;# Implicit in long format
        }
        --porcelain {
            # Show output in easy to parse porcelain format.
            # Currently same as --short.
            option_set Format short
        }
        -b - --branch {
            # Show branch and tracking info
            option_set ShowBranch 1
        }
        --ignored {
            # Show ignored files as well
            option_lappend StatusOpts GIT_STATUS_OPT_INCLUDE_IGNORED
        }
        --untracked-files:MODE {
            switch -exact -- $arg {
                normal {
                    option_append StatusOpts GIT_STATUS_OPT_INCLUDE_UNTRACKED
                }
                all {
                    option_append StatusOpts \
                        GIT_STATUS_OPT_INCLUDE_UNTRACKED \
                        GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS
                }
                none {}
                default {
                    error "Invalid mode \"$arg\". for --untracked-files."
                }
            }
        }
        --ignore-submodules:MODE {
            if {$arg ne "all"} {
                error "Invalid mode \"$arg\". Currently only mode \"all\" is supported for the --ignore-submodules option."
            }
            option_append StatusOpts GIT_STATUS_OPT_EXCLUDE_SUBMODULES
        }
        --git-dir:GITDIR {
            # Specify the path to the repository
            option_set GitDir $arg
        }
        arglist {
            # ?PATHSPEC? ...
            set path_specs $arg
        }
    }
    return $path_specs
}

proc map_status_key_letters {status} {
    # Maps status flags for index into key letters

    # Following as from original libgit2 example. Keeping as is because I'm not
    # sure of exact semantics. Can more than one bit be set and does the order
    # set the (reverse) priotity? Otherwise a single dictionary lookup would be
    # cleaner and the order could also be rearranged for earlier exits for
    # some cases. Since I'm unsure of semantics, keep exact order as original

    set istatus  " "; # Index status key
    set wtstatus " "; # Worktree status key

    # Map index status
    if {"GIT_STATUS_INDEX_NEW" in $status} {set istatus A}
    if {"GIT_STATUS_INDEX_MODIFIED" in $status} {set istatus M}
    if {"GIT_STATUS_INDEX_DELETED" in $status} {set istatus D}
    if {"GIT_STATUS_INDEX_RENAMED" in $status} {set istatus R}
    if {"GIT_STATUS_INDEX_TYPECHANGE" in $status} {set istatus T}

    # Map worktree status
    if {"GIT_STATUS_WT_NEW" in $status} {
        if {$istatus eq " "} {
            set istatus ?
        }
        set wtstatus ?
    }
    if {"GIT_STATUS_WT_MODIFIED" in $status} {set wtstatus M}
    if {"GIT_STATUS_WT_DELETED" in $status} {set wtstatus D}
    if {"GIT_STATUS_WT_RENAMED" in $status} {set wtstatus R}
    if {"GIT_STATUS_WT_TYPECHANGE" in $status} {set wtstatus T}

    if {"GIT_STATUS_IGNORED" in $status} {
        set istatus !
        set wtstatus !
    }
    return [list $istatus $wtstatus]
}

proc print_short_status {pStatusList} {
    set n [git_status_list_entrycount $pStatusList]

    # First print all new files
    for {set i 0} {$i < $n} {incr i} {
        set entry [git_status_byindex $pStatusList $i]
        dict with entry {}; # Explode entry -> status, head_to_index, index_to_workdir
        if {$status == 0} continue; # Nothing changed, nothing to report

        lassign "" old new submod extra

        # Map to index and worktree status indicators
        lassign [map_status_key_letters $status] istatus wtstatus
        if {![::cffi::pointer isnull $index_to_workdir]} {
            set itow_delta [git_diff_delta fromnative! $index_to_workdir]
            # TBD - need submodule for this
        }
        if {![::cffi::pointer isnull $head_to_index]} {
            set htoi_delta [git_diff_delta fromnative! $head_to_index]
            set old [dict get $htoi_delta old_file path]
            set new [dict get $htoi_delta new_file path]
        }
        if {![::cffi::pointer isnull $index_to_workdir]} {
            if {$a eq ""} {
                set old [dict get $itow_delta old_file path]
            }
            if {$b eq ""} {
                # Note "old_file path", NOT "new_file path" here
                set new [dict get $itow_delta old_file path]
            }
            set submod [dict $itow_delta new_file path]
        }
        if {$istatus eq "R"} {
            if {$wtstatus eq "R"} {
                puts "$istatus$wtstatus $old $new $submod $extra"
            } else {
                puts "$istatus$wtstatus $old $new $extra"
            }
        } else {
            if {$wtstatus eq "R"} {
                puts "$istatus$wtstatus $old $submod $extra"
            } else {
                puts "$istatus$wtstatus $old $extra"
            }
        }
    }
}

proc main {} {
    set path_specs [parse_options $::argv]
    git_repository_open pRepo [option GitDir .]

    try {
        # Set up options for retrieving status
        git_status_options_init opts
        # Status should include untracked and renamed files
        dict set opts flags {GIT_STATUS_OPT_INCLUDE_UNTRACKED GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX}

        # Setting the path specs in the options is a bit tricky as they are embedded
        # as a git_strarray inside the options struct, and not as a git_strarray*
        # so first create an allocated strarray...
        set pPathSpecs [cffi_strarray_new $path_specs]
        # ..then do a value copy into a dictionary. The pointers in the dictionary
        # value will stay valid as long as pPathSpecs is not freed
        dict set opts pathspec [cffi_strarray fromnative $pPathSpecs]

        # Retrieve the whole list and print status. An alternative would be to
        # use callbacks with either git_status_foreach or git_status_foreach_ext.
        # For production code, you may try all and choose the most performant.
        git_status_list_new pStatusList $pRepo $opts
        switch -exact -- [option Format short] {
            long      { print_long_status $pStatusList }
            porcelain { print_porcelain_status $pStatusList }
            default   { print_short_status $pStatusList }
        }
    } finally {
        if {[info exists pStatusList]} {
            git_status_list_free $pStatusList
        }
        if {[info exists pPathSpecs]} {
            cffi_strarray_free $pPathSpecs
        }
        if {[info exists pRepo]} {
            git_repository_free $pRepo
        }
        git_libgit2_shutdown
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
if {1} {
    catch {main} result edict
    git_libgit2_shutdown
    if {[dict get $edict -code]} {
        puts stderr $result
        exit 1
    }
} else {
#    source ../../examples/libgit2/libgit2.tcl
    git::init {D:\temp\git2.dll}
    namespace path git
    cd /temp/g
    git_repository_open pRepo .
    set pPathSpecs [cffi_strarray_new {}]
    set dPathSpecs [cffi_strarray fromnative $pPathSpecs]
    ::git::git_status_options_init opts
    dict set opts flags {GIT_STATUS_OPT_INCLUDE_UNTRACKED GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX}
    dict set opts pathspec $dPathSpecs
    git_status_list_new pStatusList $pRepo $opts
    set n [git_status_list_entrycount $pStatusList]
    puts $n
    set pEntry [git_status_byindex $pStatusList 0] 
    puts $pEntry
}
