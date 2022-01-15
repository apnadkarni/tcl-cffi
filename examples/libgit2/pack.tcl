# Based on libgit2/include/pack.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum sequence git_packbuilder_stage_t {
    GIT_PACKBUILDER_ADDING_OBJECTS
    GIT_PACKBUILDER_DELTAFICATION
}
::cffi::alias define GIT_PACKBUILDER_STAGE_T {int {enum git_packbuilder_stage_t}}

libgit2 functions{
    git_packbuilder_new GIT_ERROR_CODE {
        pb {PPACKBUILDER out}
    }
    git_packbuilder_set_threads GIT_ERROR_CODE {
        
    }
}
