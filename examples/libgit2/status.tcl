# Based on libgit2/include/status.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum define git_status_t {
    GIT_STATUS_CURRENT          0

    GIT_STATUS_INDEX_NEW        1
    GIT_STATUS_INDEX_MODIFIED   2
    GIT_STATUS_INDEX_DELETED    4
    GIT_STATUS_INDEX_RENAMED    8
    GIT_STATUS_INDEX_TYPECHANGE 16

    GIT_STATUS_WT_NEW           128
    GIT_STATUS_WT_MODIFIED      256
    GIT_STATUS_WT_DELETED       512
    GIT_STATUS_WT_TYPECHANGE    1024
    GIT_STATUS_WT_RENAMED       2048
    GIT_STATUS_WT_UNREADABLE    4096

    GIT_STATUS_IGNORED          16384
    GIT_STATUS_CONFLICTED       32768
}
::cffi::alias define GIT_STATUS_T {int {enum git_status_t} bitmask}

::cffi::enum sequence git_status_show_t {
    GIT_STATUS_SHOW_INDEX_AND_WORKDIR
    GIT_STATUS_SHOW_INDEX_ONLY
    GIT_STATUS_SHOW_WORKDIR_ONLY
}
::cffi::alias define GIT_STATUS_SHOW_T {int {enum git_status_show_t}}

::cffi::enum flags git_status_opts_t {
    GIT_STATUS_OPT_INCLUDE_UNTRACKED
    GIT_STATUS_OPT_INCLUDE_IGNORED
    GIT_STATUS_OPT_INCLUDE_UNMODIFIED
    GIT_STATUS_OPT_EXCLUDE_SUBMODULES
    GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS
    GIT_STATUS_OPT_DISABLE_PATHSPEC_MATCH
    GIT_STATUS_OPT_RECURSE_IGNORED_DIRS
    GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX
    GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR
    GIT_STATUS_OPT_SORT_CASE_SENSITIVELY
    GIT_STATUS_OPT_SORT_CASE_INSENSITIVELY
    GIT_STATUS_OPT_RENAMES_FROM_REWRITES
    GIT_STATUS_OPT_NO_REFRESH
    GIT_STATUS_OPT_UPDATE_INDEX
    GIT_STATUS_OPT_INCLUDE_UNREADABLE
    GIT_STATUS_OPT_INCLUDE_UNREADABLE_AS_UNTRACKED
}
::cffi::alias define GIT_STATUS_OPTS_T {uint {enum git_status_opts_t} bitmask}

::cffi::Struct create git_status_entry {
    status           GIT_STATUS_T
    head_to_index    pointer.git_diff_delta
    index_to_workdir pointer.git_diff_delta
}

::cffi::Struct create git_status_options {
    version  {uint {default 1}}
    show     GIT_STATUS_SHOW_T
    flags    GIT_STATUS_OPTS_T
    pathspec PSTRARRAYIN
    baseline PTREE
}

::cffi::prototype function git_status_cb int {
    path         STRING
    status_flags GIT_STATUS_T
    payload      CB_PAYLOAD
}

libgit2 functions {
    git_status_options_init GIT_ERROR_CODE {
        opts    {struct.git_status_options out}
        version {uint                      {default 1}}
    }
    git_status_foreach GIT_ERROR_CODE {
        pRepo    PREPOSITORY
        callback pointer.git_status_cb
        payload  CB_PAYLOAD
    }
    git_status_foreach_ext GIT_ERROR_CODE {
        pRepo    PREPOSITORY
        opts     {struct.git_status_options byref}
        callback pointer.git_status_cb
        payload  CB_PAYLOAD
    }
    git_status_file GIT_ERROR_CODE {
        vstatus_flags {GIT_STATUS_T out}
        pRepo         PREPOSITORY
        path          STRING
    }
    git_status_list_new GIT_ERROR_CODE {
        vstatuslist {PSTATUS_LIST out}
        pRepo       PREPOSITORY
        opts        {struct.git_status_options byref}
    }
    git_status_list_entrycount size_t {
        statuslist PSTATUS_LIST
    }
    git_status_byindex {pointer.git_status_entry unsafe} {
        statuslist PSTATUS_LIST
        idx        size_t
    }
    git_status_list_free void {
        statuslist {PSTATUS_LIST dispose}
    }
    git_status_should_ignore GIT_ERROR_CODE {
        ignored {int out}
        pRepo   PREPOSITORY
        path    STRING
    }
}
