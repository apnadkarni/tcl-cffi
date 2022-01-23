# Demo of cffi libgit extension. Poor man's git init emulation.
# tclsh git-init.tcl --help

source [file join [file dirname [info script]] porcelain-utils.tcl]

proc parse_options {arguments} {

    # NOTE: getopt uses comments below to generate help. Careful about changing them.
    getopt::getopt opt arg $arguments {
        -q - --quiet {
            # Only print error and warning messages;
            # all other output will be suppressed.
            option_set Quiet 1
        }
        --bare {
            # Create a bare repository.
            option_set Bare 1
        }
        --initial-commit {
            # Create an empty initial commit
            option_set InitialCommit 1
        }
        --separate-git-dir:DIRECTORY {
            # Create text file containing path to actual repository.
            if {![file isdirectory $arg]} {
                error "$arg is not a directory."
            }
            option_set SeparateGitDir $arg
        }
        --shared:PERMS {
            # Set the sharing permissions. PERMS should be "umask" (default),
            # "group", "all" or an integer umask value.
            if {[string is integer -strict $arg]} {
                option_set ShareMode $arg
            } else {
                switch -exact -- $arg {
                    false -
                    umask { option_set ShareMode GIT_REPOSITORY_INIT_SHARED_UMASK}
                    true -
                    group { option_set ShareMode GIT_REPOSITORY_INIT_SHARED_GROUP}
                    world -
                    everybody -
                    all { option_set ShareMode GIT_REPOSITORY_INIT_SHARED_ALL}
                    default { error "Unknown share mode \"$arg\". Should be false, umask, true, group, world, everybody, all or an integer umask value."}
                }
            }
        }
        --template:DIRECTORY {
            # Uses the templates from DIRECTORY.
            if {![file isdirectory $arg]} {
                error "$arg is not a directory."
            }
            option_set Template $arg
        }
        arglist {
            # DIRECTORY
            set dir $arg
        }
    }
    if {[llength $dir] == 0} {
        return [pwd]
    } elseif {[llength $dir] == 1} {
        return [lindex $dir 0]
    } else {
        getopt::usage {[options] [directory]}
    }
}

proc initialize_repository {dir} {
    # Initialize an libgit2 options structure to defaults
    git_repository_init_options_init init_opts

    # Have libgit create directories as necessary
    dict set init_opts flags GIT_REPOSITORY_INIT_MKPATH

    # Modify default options based on command line options
    dict set init_opts mode [option ShareMode GIT_REPOSITORY_INIT_SHARED_UMASK]
    if {[option Bare 0]} {
        dict lappend init_opts flags GIT_REPOSITORY_INIT_BARE
    }
    if {[option? Template template]} {
        dict lappend init_opts flags GIT_REPOSITORY_INIT_EXTERNAL_TEMPLATE
        dict set init_opts template_path $template
    }
    if {[option? SeparateGitDir gitdir]} {
        # The workdir is not the git dir. Adjust accordingly.
        dict set init_opts workdir_path $dir
        set dir $gitdir
    }

    # Make the actual libgit2 call to initialize the repository
    git_repository_init_ext pRepo $dir $init_opts
    return $pRepo
}

proc create_initial_commit {pRepo} {
    # Use the default signature for commit
    # Because of the way the API is structured, the default signature is returned
    # as a pointer to an internal allocation. It is easier to pass around as a
    # dictionary value corresponding to the git_signature struct so we do that
    # conversion.
    git_signature_default pSig $pRepo
    set sig [git_signature fromnative $pSig]
    git_signature_free $pSig

    # Create a tree for this commit
    git_repository_index pIndex $pRepo
    try {
        git_index_write_tree tree_id $pIndex
    } finally {
        git_index_free $pIndex
    }

    # Now create the commit for the created tree
    # First get the tree from its id
    git_tree_lookup pTree $pRepo $tree_id
    # Message has to be passed in as a pointer
    set pMessage [::cffi::memory fromstring "Initial commit" utf-8]
    try {
        # This is an intial commit so there are no parents. Thus the
        # last two parameters - parent_count and parent list are 0 and empty
        git_commit_create commit_id $pRepo "HEAD" $sig $sig "" $pMessage $pTree 0 {}
        inform "Initial commit: [hexify_id $commit_id]"
    } finally {
        ::cffi::memory free $pMessage
        git_tree_free $pTree
    }
}

proc main {} {
    try {
        set dir [parse_options $::argv]
        set pRepo [initialize_repository $dir]

        if {! [option Quiet 0]} {
            if {[option Bare 0] || [option? SeparateGitDir _]} {
                variable Options
                set path [git_repository_path $pRepo]
            } else {
                set path [git_repository_workdir $pRepo]
            }
            inform "Initialized git repository at $path."
        }
        if {[option InitialCommit 0]} {
            create_initial_commit $pRepo
        }
    } on error {message edict} {
        puts stderr $message
        exit 1
    } finally {
        if {[info exists pRepo]} {
            git_repository_free $pRepo
        }
        git_libgit2_shutdown
    }
}
main
