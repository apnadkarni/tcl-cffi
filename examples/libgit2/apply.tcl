# Based on libgit2/include/apply.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::prototype function git_apply_delta_cb int {
    delta   {struct.git_diff_delta byref}
    payload CB_PAYLOAD
}

::cffi::prototype function git_apply_hunk_cb int {
    hunk    {struct.git_diff_hunk byref}
    payload CB_PAYLOAD
}

::cffi::enum flags git_apply_flags_t {
    GIT_APPLY_CHECK
}
::cffi::alias define GIT_APPLY_FLAGS_T {int {enum git_apply_flags_t} bitmask}

::cffi::enum sequence git_apply_location_t {
    GIT_APPLY_LOCATION_WORKDIR
    GIT_APPLY_LOCATION_INDEX
    GIT_APPLY_LOCATION_BOTH
}
::cffi::alias define GIT_APPLY_LOCATION_T {int {enum git_apply_location_t}}


::cffi::Struct create git_apply_options {
    version  uint
    delta_cb pointer.git_apply_delta_cb
    hunk_cb  pointer.git_apply_hunk_cb
    payload  CB_PAYLOAD
    flags    uint
}

libgit2 functions {
    git_apply_options_init GIT_ERROR_CODE {
        opts    {struct.git_apply_options retval}
        version uint
    }
    git_apply_to_tree GIT_ERROR_CODE {
        pIndex    {PINDEX  retval}
        pRepo     PREPOSITORY
        pPreimage PTREE
        diff      PDIFF
        opts      {struct.git_apply_options byref}
    }
    git_apply GIT_ERROR_CODE {
        pRepo    PREPOSITORY
        diff     PDIFF
        location GIT_APPLY_LOCATION_T
        opts     {struct.git_apply_options byref}
    }
}
