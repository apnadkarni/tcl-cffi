# Demo of cffi libgit extension. Poor man's git init emulation from libgit2
# Translated to Tcl from libgit2/examples/init.c

# NOTE COMMENTS ABOVE ARE AUTOMATICALLY DISPLAYED IN PROGRAM HELP

proc parse_init_options {arguments} {

    # Do not use parse_options because not all common options are relevant for init

    # NOTE: getopt uses comments below to generate help. Careful about changing them.
    getopt::getopt opt arg $arguments {
        -q - --quiet {
            # Only print error and warning messages;
            # all other output will be suppressed.
            option_set Quiet $arg
        }
        --bare {
            # Create a bare repository.
            option_set Bare $arg
        }
        --initial-commit {
            # Create an empty initial commit
            option_set InitialCommit $arg
        }
        --shared:PERMS {
            # Set the sharing permissions. PERMS should be
            # "umask" (default), "group", "all" or an
            # integer umask value.
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
    if {[llength $dir] > 1} {
        getopt::usage "At most one directory may be specified."
    } else {
        return [lindex $dir 0]
    }
}

proc initialize_repository {dir} {
    # Initialize an libgit2 options structure to defaults
    git_repository_init_options_init 1

    # Have libgit create directories as necessary
    dict set init_opts flags GIT_REPOSITORY_INIT_MKPATH

    # Modify default options based on command line options
    dict set init_opts mode [option ShareMode GIT_REPOSITORY_INIT_SHARED_UMASK]
    if {[option Bare 0]} {
        dict lappend init_opts flags GIT_REPOSITORY_INIT_BARE
    }
    if {[option? Template template]} {
        # Note this only creates directories that have files in them
        dict lappend init_opts flags GIT_REPOSITORY_INIT_EXTERNAL_TEMPLATE
        dict set init_opts template_path $template
    }

    # Make the actual libgit2 call to initialize the repository
    set pRepo [git_repository_init_ext $dir $init_opts]
    return $pRepo
}

proc create_initial_commit {pRepo} {
    # Use the default signature for commit
    # Because of the way the API is structured, the default signature is returned
    # as a pointer to an internal allocation. It is easier to pass around as a
    # dictionary value corresponding to the git_signature struct so we do that
    # conversion.
    set pSig [git_signature_default $pRepo]
    set sig  [git_signature fromnative $pSig]
    git_signature_free $pSig

    # Create a tree for this commit
    set pIndex [git_repository_index $pRepo]
    try {
        set tree_id [git_index_write_tree $pIndex]
    } finally {
        git_index_free $pIndex
    }

    # Now create the commit for the created tree
    # First get the tree from its id
    set pTree [git_tree_lookup $pRepo $tree_id]
    # Message has to be passed in as a pointer
    set pMessage [::cffi::memory fromstring "Initial commit" utf-8]
    try {
        # This is an intial commit so there are no parents. Thus the
        # last two parameters - parent_count and parent list are 0 and empty
        set commit_id [git_commit_create $pRepo "HEAD" $sig $sig "" $pMessage $pTree 0 {}]
        inform "Initial commit: [hexify_id $commit_id]"
    } finally {
        ::cffi::memory free $pMessage
        git_tree_free $pTree
    }
}

proc git-init {arguments} {
    set dir [parse_init_options $arguments]

    # The following tries to somewhat reflect git's command line behavior
    # except for environment variables.
    if {$dir eq ""} {
        set dir .
    }
    set pRepo [initialize_repository [file normalize $dir]]
    try {
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
    } finally {
        git_repository_free $pRepo
    }
}

source [file join [file dirname [info script]] porcelain-utils.tcl]
catch {git-init $::argv} result edict
git_libgit2_shutdown
if {[dict get $edict -code]} {
    puts stderr $result
    exit 1
}
