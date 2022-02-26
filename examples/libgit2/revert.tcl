# Based on libgit2/include/revert.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_revert_options {
    version       {uint {default 1}}
    mainline      uint
    merge_opts    struct.git_merge_options
    checkout_opts struct.git_checkout_options
}

libgit2 functions {
    git_revert_options_init GIT_ERROR_CODE {
        opts    {struct.git_revert_options retval}
        version {uint {default 1}}
    }
    git_revert_commit GIT_ERROR_CODE {
        index         {PINDEX retval}
        pRepo         PREPOSITORY
        revert_commit PCOMMIT
        our_commit    PCOMMIT
        mainline      uint
        merge_options {struct.git_merge_options byref}
    }
    git_revert GIT_ERROR_CODE {
        pRepo PREPOSITORY
        commit PCOMMIT
        given_opts {struct.git_revert_options byref}
    }
}
