# Based on libgit2/include/stash.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum define git_stash_flags {
    GIT_STASH_DEFAULT           0
    GIT_STASH_KEEP_INDEX        1
    GIT_STASH_INCLUDE_UNTRACKED 2
    GIT_STASH_INCLUDE_IGNORED   4
}
::cffi::alias define GIT_STASH_FLAGS_T {int {enum git_stash_flags}}

::cffi::enum define git_stash_apply_flags {
    GIT_STASH_APPLY_DEFAULT         0
    GIT_STASH_APPLY_REINSTATE_INDEX 1
}
::cffi::alias define GIT_STASH_APPLY_FLAGS_T {int {enum git_stash_apply_flags}}


::cffi::enum sequence git_stash_apply_progress_t {
    GIT_STASH_APPLY_PROGRESS_NONE
    GIT_STASH_APPLY_PROGRESS_LOADING_STASH
    GIT_STASH_APPLY_PROGRESS_ANALYZE_INDEX
    GIT_STASH_APPLY_PROGRESS_ANALYZE_MODIFIED
    GIT_STASH_APPLY_PROGRESS_ANALYZE_UNTRACKED
    GIT_STASH_APPLY_PROGRESS_CHECKOUT_UNTRACKED
    GIT_STASH_APPLY_PROGRESS_CHECKOUT_MODIFIED
    GIT_STASH_APPLY_PROGRESS_DONE
}
::cffi::alias define GIT_STASH_APPLY_PROGRESS_T {int {enum git_stash_apply_progress_t}}

::cffi::prototype function git_stash_apply_progress_cb int {
    progress GIT_STASH_APPLY_PROGRESS_T
    payload  CB_PAYLOAD
}

::cffi::prototype function git_stash_cb int {
    index    size_t
    message  STRING
    stash_id {struct.git_oid byref}
    payload  CB_PAYLOAD
}

::cffi::Struct create git_stash_apply_options {
    version          {uint {default 1}}
    flags            GIT_STASH_APPLY_FLAGS_T
    checkout_options struct.git_checkout_options
    progress_cb      {pointer.git_stash_apply_progress_cb nullok}
    progress_payload CB_PAYLOAD
}

libgit2 functions {
    git_stash_save GIT_ERROR_CODE {
        id      {struct.git_oid retval}
        pRepo   PREPOSITORY
        stasher {struct.git_signature byref}
        message {STRING nullifempty}
        flags   GIT_STASH_FLAGS_T
    }
    git_stash_apply_options_init GIT_ERROR_CODE {
        opts    {struct.git_stash_apply_options retval}
        version {uint   {default 1}}
    }
    git_stash_apply GIT_ERROR_CODE {
        pRepo PREPOSITORY
        index size_t
    }
    git_stash_foreach {int {enum git_error_code}} {
        pRepo PREPOSITORY
        callback pointer.git_stash_cb
        payload CB_PAYLOAD
    }
    git_stash_drop GIT_ERROR_CODE {
        pRepo PREPOSITORY
        index size_t
    }
    git_stash_pop GIT_ERROR_CODE {
        pRepo   PREPOSITORY
        index   size_t
        options {struct.git_stash_apply_options byref}
    }
}
