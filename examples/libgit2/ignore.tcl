# Based on libgit2/include/ignore.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

libgit2 functions {
    git_ignore_add_rule GIT_ERROR_CODE {
        pRepo PREPOSITORY
        rules STRING
    }
    git_ignore_clear_internal_rules GIT_ERROR_CODE {
        pRepo PREPOSITORY
    }
    git_ignore_path_is_ignored GIT_ERROR_CODE {
        ignored {int retval}
        pRepo   PREPOSITORY
        path    STRING
    }
}
