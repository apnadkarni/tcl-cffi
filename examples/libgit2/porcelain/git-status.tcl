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
            option_remove StatusOpts \
                GIT_STATUS_OPT_INCLUDE_UNTRACKED \
                GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS
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
        --list-submodules {
            option_set ListSubmodules 1
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

proc print_short {pStatusList} {
    set nentries [git_status_list_entrycount $pStatusList]

    # First print all existing files
    for {set i 0} {$i < $nentries} {incr i} {
        set entry [git_status_byindex $pStatusList $i]
        dict with entry {}; # Explode entry -> status, head_to_index, index_to_workdir
        if {$status == 0} continue; # Nothing changed, nothing to report

        # Map to index and worktree status indicators
        lassign [map_status_key_letters $status] istatus wtstatus
        # New files printed later
        if {$istatus eq "?" && $wtstatus eq "?"} continue

        lassign "" old new submod extra
        if {![::cffi::pointer isnull $index_to_workdir]} {
            set itow_delta [git_diff_delta fromnative! $index_to_workdir]
            if {"GIT_FILEMODE_COMMIT" eq [dict get $itow_delta new_file mode]} {
                if {[catch {
                    git_submodule_status smstatus $pRepo [dict get $itow_delta new_file path] GIT_SUBMODULE_IGNORE_UNSPECIFIED
                } result]} {
                    inform $result 1
                } else {
                    if {"GIT_SUBMODULE_STATUS_WD_MODIFIED" in $smstatus} {
                        set extra " (new commits)"
                    } elseif {"GIT_SUBMODULE_STATUS_WD_INDEX_MODIFIED" in $smstatus} {
                        set extra " (modified content)"
                    } elseif {"GIT_SUBMODULE_STATUS_WD_WD_MODIFIED" in $smstatus} {
                        set extra " (modified content)"
                    } elseif {"GIT_SUBMODULE_STATUS_WD_UNTRACKED" in $smstatus} {
                        set extra " (untracked content)"
                    }
                }
            }
        }
        if {![::cffi::pointer isnull $head_to_index]} {
            set htoi_delta [git_diff_delta fromnative! $head_to_index]
            set old [dict get $htoi_delta old_file path]
            set new [dict get $htoi_delta new_file path]
        }
        if {![::cffi::pointer isnull $index_to_workdir]} {
            if {$old eq ""} {
                set old [dict get $itow_delta old_file path]
            }
            if {$new eq ""} {
                # Note "old_file path", NOT "new_file path" here
                set new [dict get $itow_delta old_file path]
            }
            set submod [dict get $itow_delta new_file path]
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

    # Now print all new files
    for {set i 0} {$i < $nentries} {incr i} {
        set entry [git_status_byindex $pStatusList $i]
        dict with entry {}; # Explode entry -> status, head_to_index, index_to_workdir
        if {"GIT_STATUS_WT_NEW" in $status} {
            set itow_delta [git_diff_delta fromnative! $index_to_workdir]
            puts "?? [dict get $itow_delta old_file path]"
        }
    }
}


proc print_long {pStatusList} {
    # This could be written to be considerably more efficient, but for pedagogic
    # purposes and lack of clarity of some semantics, writing exactly like the
    # libgit2 C original

    set rm_in_workdir ""
    set print_header 1

    set nentries [git_status_list_entrycount $pStatusList]

    # First print index changes
    for {set i 0} {$i < $nentries} {incr i} {
        set entry [git_status_byindex $pStatusList $i]
        dict with entry {}; # Explode entry -> status, head_to_index, index_to_workdir
        if {$status == 0} continue; # Nothing changed, nothing to report

        # Not sure of semantics, copied from libgit2/examples/status.c
        if {"GIT_STATUS_WT_DELETE" in $status} {
            set rm_in_workdir "/rm"; # Used in printed header
        }
        if {"GIT_STATUS_INDEX_TYPECHANGE" in $status} {
            set istatus "typechange: "
        } elseif {"GIT_STATUS_INDEX_RENAMED" in $status} {
            set istatus "renamed:    "
        } elseif {"GIT_STATUS_INDEX_DELETED" in $status} {
            set istatus "deleted:    "
        } elseif {"GIT_STATUS_INDEX_MODIFIED" in $status} {
            set istatus "modified:   "
        } elseif {"GIT_STATUS_INDEX_NEW" in $status} {
            set istatus "new file:   "
        } else {
            continue; # Not an index change, skip in this go around
        }

        if {$print_header} {
            puts "# Changes to be committed:"
            puts "#   (use \"git reset HEAD <file>...\" to unstage)"
            puts "#"
            set print_header 0
        }

        set htoi_delta [git_diff_delta fromnative! $head_to_index]
        set old [dict get $htoi_delta old_file path]
        set new [dict get $htoi_delta new_file path]
        if {$old ne "" && $new ne "" && $old ne $new} {
            puts "#\t$istatus  $old -> $new"
        } else {
            if {$old ne ""} {
                puts "#\t$istatus  $old"
            } else {
                puts "#\t$istatus  $new"
            }
        }
    }

    if {$print_header} {
        set changes_in_index 0
    } else {
        set changes_in_index 1
        puts "#"
        set print_header 1
    }

    # Now print workdir changes to tracked files
    for {set i 0} {$i < $nentries} {incr i} {
        set entry [git_status_byindex $pStatusList $i]
        dict with entry {}; # Explode entry -> status, head_to_index, index_to_workdir
        if {$status == 0 || [::cffi::pointer isnull $index_to_workdir]} {
            continue
        }
        if {"GIT_STATUS_WT_TYPECHANGE" in $status} {
            set wstatus "typechange: "
        } elseif {"GIT_STATUS_WT_RENAMED" in $status} {
            set wstatus "renamed:    "
        } elseif {"GIT_STATUS_WT_DELETED" in $status} {
            set wstatus "deleted:    "
        } elseif {"GIT_STATUS_WT_MODIFIED" in $status} {
            set wstatus "modified:   "
        } else {
            # Could be GIT_STATUS_WT_NEW - handled in previous loop - or
            # GIT_STATUS_WT_UNREADABLE - which is not printed a la git status
            continue
        }
        if {$print_header} {
            puts "# Changes not staged for commit:";
            puts "#   (use \"git add$rm_in_workdir <file>...\" to update what will be committed)"
            puts "#   (use \"git checkout -- <file>...\" to discard changes in working directory)";
            puts "#"
            set print_header 0
        }
        set itow_delta [git_diff_delta fromnative! $index_to_workdir]
        set old [dict get $itow_delta old_file path]
        set new [dict get $itow_delta new_file path]
        if {$old ne "" && $new ne "" && $old ne $new} {
            puts "#\t$wstatus  $old -> $new"
        } else {
            if {$old ne ""} {
                puts "#\t$wstatus  $old"
            } else {
                puts "#\t$wstatus  $new"
            }
        }
    }

    if {$print_header} {
        set changes_in_workdir 0
    } else {
        set changes_in_workdir 1
        puts "#"
        set print_header 1
    }

    # Print untracked files
    for {set i 0} {$i < $nentries} {incr i} {
        set entry [git_status_byindex $pStatusList $i]
        dict with entry {}; # Explode entry -> status, head_to_index, index_to_workdir
        if {"GIT_STATUS_WT_NEW" in $status} {
            if {$print_header} {
                puts "# Untracked files:"
                puts "#   (use \"git add <file>...\" to include in what will be committed)"
                puts "#"
                set print_header 0
            }
            set itow_delta [git_diff_delta fromnative! $index_to_workdir]
            puts "#\t[dict get $itow_delta old_file path]"
        }
    }

    set print_header 1
    for {set i 0} {$i < $nentries} {incr i} {
        set entry [git_status_byindex $pStatusList $i]
        dict with entry {}; # Explode entry -> status, head_to_index, index_to_workdir
        if {"GIT_STATUS_IGNORED" in $status} {
            if {$print_header} {
                puts "# Ignored files:"
                puts "#   (use \"git add -f <file>...\" to include in what will be committed)"
                puts "#"
                set print_header 1
            }
            set itow_delta [git_diff_delta fromnative! $index_to_workdir]
            puts "#\t[dict get $itow_delta old_file path]"
        }
    }

    if {(!$changes_in_index) && (!$changes_in_workdir)} {
        puts "no changes added to commit (use \"git add\" and/or \"git commit -a\")"
    }
}

proc show_branch {pRepo format} {
    try {
        git_repository_head head $pRepo
        set branch [git_reference_shorthand $head]
        git_reference_free $head
    } trap {GIT -9} {} - trap {GIT -3} {} {
        # EUNBORNBRANCH -9
        # ENOTFOUND -3
        if {$format eq "long"} {
            set branch "Not currently on any branch."
        } else {
            set branch "HEAD (no branch)"
        }
    }
    if {$format eq "long"} {
        puts "# On branch $branch"
    } else {
        puts "## $branch"
    }
}

proc print_submod {pSubmod name payload} {
    # git_submodule_name etc. expect safe pointers.
    # We do not know in a callback if what is passed in is already marked
    # as safe or not so we need to mark it as such. On the other hand, we
    # should not dispose of it unless we are the ones that marked it.
    set was_safe [cffi::pointer isvalid $pSubmod]
    if {! $was_safe} {
        ::cffi::pointer safe $pSubmod
    }
    try {
        # Keep track in caller's context of number of submodules
        upvar 1 submodule_count count
        if {$count == 0} {
            puts "# Submodules"
        }
        puts "# - submodule '[git_submodule_name $pSubmod]' at [git_submodule_path $pSubmod]"
    } finally {
        if {! $was_safe} {
            # If we marked it as safe, dispose of it.
            ::cffi::pointer dispose $pSubmod
        }
    }
    return 0
}

proc main {} {
    # Defaults before parsing options
    # Status should include untracked and renamed files by default
    option_set StatusOpts {
        GIT_STATUS_OPT_INCLUDE_UNTRACKED
        GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX
    }
    set path_specs [parse_options $::argv]

    git_repository_open pRepo [option GitDir .]
    try {
        # Set up options for retrieving status
        git_status_options_init opts
        dict set opts flags [option! StatusOpts]
        dict set opts show GIT_STATUS_SHOW_INDEX_AND_WORKDIR

        # Setting the path specs in the options is a bit tricky as they are embedded
        # as a git_strarray inside the options struct, and not as a git_strarray*
        # so first create an allocated strarray...
        set pPathSpecs [cffi_strarray_new $path_specs]
        # ..then do a value copy into a dictionary. The pointers in the dictionary
        # value will stay valid as long as pPathSpecs is not freed
        dict set opts pathspec [cffi_strarray fromnative $pPathSpecs]

        set format [option Format long]
        if {[option ShowBranch [string equal $format long]]} {
            show_branch $pRepo $format
        }

        if {[option ListSubmodules 0]} {
            try {
                set submodule_count 0; # Used by callback
                set submodule_cb [::cffi::callback ${::GIT_NS}::git_submodule_cb print_submod -1]
                git_submodule_foreach $pRepo $submodule_cb NULL
            } finally {
                if {[info exists submodule_cb]} {
                    ::cffi::callback_free $submodule_cb
                }
            }
        }

        # Retrieve the whole list and print status. An alternative would be to
        # use callbacks with either git_status_foreach or git_status_foreach_ext.
        # For production code, you may try all and choose the most performant.
        git_status_list_new pStatusList $pRepo $opts
        switch -exact -- $format {
            long      { print_long $pStatusList }
            default   { print_short $pStatusList }
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
catch {main} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
