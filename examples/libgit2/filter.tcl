# Based on libgit2/include/filter.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum define git_filter_mode_t {
    GIT_FILTER_TO_WORKTREE 0
    GIT_FILTER_SMUDGE      0
    GIT_FILTER_TO_ODB      1
    GIT_FILTER_CLEAN       1
}
::cffi::alias define GIT_FILTER_MODE_T {int {enum git_filter_mode_t}}

::cffi::enum define git_filter_flag_t {
    GIT_FILTER_DEFAULT                0
    GIT_FILTER_ALLOW_UNSAFE           1
    GIT_FILTER_NO_SYSTEM_ATTRIBUTES   2
    GIT_FILTER_ATTRIBUTES_FROM_HEAD   4
    GIT_FILTER_ATTRIBUTES_FROM_COMMIT 8
}
::cffi::alias define GIT_FILTER_FLAG_T {int {enum git_filter_flag_t} bitmask}

::cffi::Struct create git_filter_options {
    version {uint {default 1}}
    flags GIT_FILTER_FLAG_T
    reserved {pointer unsafe}
    attr_commit_id struct.git_oid
} -clear

::cffi::alias define PFILTER pointer.git_filter
::cffi::alias define PFILTER_LIST pointer.git_filter_list

libgit2 functions {
    git_filter_list_load {PFILTER_LIST nullok} {
        pRepo PREPOSITORY
        blob  {PBLOB nullok}
        path  STRING
        mode  GIT_FILTER_MODE_T
        flags GIT_FILTER_FLAG_T
    }
    git_filter_list_load_ext {PFILTER_LIST nullok} {
        pRepo PREPOSITORY
        blob  {PBLOB nullok}
        path  STRING
        mode  GIT_FILTER_MODE_T
        opts  {struct.git_filter_options byref}
    }
    git_filter_list_contains int {
        filters {PFILTER_LIST nullok}
        name    STRING
    }
    git_filter_list_apply_to_buffer GIT_ERROR_CODE {
        buf       PBUF
        filters   {PFILTER_LIST nullok}
        input     binary
        input_len size_t
    }
    git_filter_list_apply_to_file GIT_ERROR_CODE {
        buf     PBUF
        filters {PFILTER_LIST nullok}
        pRepo   PREPOSITORY
        path    STRING
    }
    git_filter_list_apply_to_blob GIT_ERROR_CODE {
        buf     PBUF
        filters {PFILTER_LIST nullok}
        blob    PBLOB
    }
    git_filter_list_stream_buffer GIT_ERROR_CODE {
        filters {PFILTER_LIST nullok}
        buffer  binary
        len     size_t
        target  PWRITESTREAM
    }
    git_filter_list_stream_file GIT_ERROR_CODE {
        filters {PFILTER_LIST nullok}
        pRepo   PREPOSITORY
        path    STRING
        target  PWRITESTREAM
    }
    git_filter_list_stream_buffer GIT_ERROR_CODE {
        filters {PFILTER_LIST nullok}
        blob    PBLOB
        target  PWRITESTREAM
    }
    git_filter_list_free void {
        filters {PFILTER_LIST nullok dispose}
    }
}
