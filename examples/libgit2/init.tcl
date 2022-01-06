# This file should be sourced into whatever namespace commands should
# be created in.


AddFunctions {
    git_libgit2_init {
        GIT_ERROR_CODE
        {}
    }
    git_libgit2_shutdown {
        GIT_ERROR_CODE
        {major {int out} minor {int out} rev {int out}}
    }

}
