# Based on libgit2/include/global.h

# This file should be sourced into whatever namespace commands should
# be created in.


libgit2 functions {
    git_libgit2_init GIT_ERROR_CODE {}
    git_libgit2_version GIT_ERROR_CODE {
        major {int out} minor {int out} rev {int out}
    }
    git_libgit2_shutdown int {}
}