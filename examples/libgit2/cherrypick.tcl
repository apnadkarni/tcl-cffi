# Based on libgit2/include/cherrypick.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_cherrypick_options {
    version       {uint {default 1}}
    mainline      uint
    merge_opts    struct.git_merge_options
    checkout_opts struct.git_checkout_options
}

libgit2 functions {
    git_cherrypick_options_init GIT_ERROR_CODE {
        opts {struct.git_cherrypick_options out}
        version {uint {default 1}}
    }
    git_cherrypick_commit GIT_ERROR_CODE {
        index {PINDEX out}
        pRepo PREPOSITORY
        cherrypick_commit PCOMMIT
        our_commit PCOMMIT
        mainline uint
        merge_options {struct.git_merge_options byref}
    }
    git_cherrypick GIT_ERROR_CODE {
        pRepo PREPOSITORY
        commit PCOMMIT
        cherrypick_options {struct.git_cherrypick_options byref}
    }
}
