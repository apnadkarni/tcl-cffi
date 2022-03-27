# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/submodule.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum flags git_submodule_status_t {
    GIT_SUBMODULE_STATUS_IN_HEAD
    GIT_SUBMODULE_STATUS_IN_INDEX
    GIT_SUBMODULE_STATUS_IN_CONFIG
    GIT_SUBMODULE_STATUS_IN_WD
    GIT_SUBMODULE_STATUS_INDEX_ADDED
    GIT_SUBMODULE_STATUS_INDEX_DELETED
    GIT_SUBMODULE_STATUS_INDEX_MODIFIED
    GIT_SUBMODULE_STATUS_WD_UNINITIALIZED
    GIT_SUBMODULE_STATUS_WD_ADDED
    GIT_SUBMODULE_STATUS_WD_DELETED
    GIT_SUBMODULE_STATUS_WD_MODIFIED
    GIT_SUBMODULE_STATUS_WD_INDEX_MODIFIED
    GIT_SUBMODULE_STATUS_WD_WD_MODIFIED
    GIT_SUBMODULE_STATUS_WD_UNTRACKED
}
::cffi::alias define GIT_SUBMODULE_STATUS_T {int {enum git_submodule_status_t} bitmask}

::cffi::prototype function git_submodule_cb int {
    sm      {pointer.git_submodule unsafe}
    name    STRING
    payload CB_PAYLOAD
}

::cffi::Struct create git_submodule_update_options {
    version {uint {default 1}}
    checkout_opts struct.git_checkout_options
    fetch_opts struct.git_fetch_options
    allow_fetch int
}

libgit2 functions {
    git_submodule_update_options_init GIT_ERROR_CODE {
        optsvar {struct.git_submodule_update_options retval}
        version {uint {default 1}}
    }
    git_submodule_update GIT_ERROR_CODE {
        sm   PSUBMODULE
        init int
        opts {struct.git_submodule_update_options byref}
    }
    git_submodule_lookup GIT_ERROR_CODE {
        submodulevar {PSUBMODULE retval}
        pRepo        PREPOSITORY
        name         STRING
    }
    git_submodule_dup GIT_ERROR_CODE {
        submodulevar {PSUBMODULE retval}
        source PSUBMODULE
    }
    git_submodule_free void {
        sm {PSUBMODULE nullok dispose}
    }
    git_submodule_foreach GIT_ERROR_CODE {
        pRepo    PREPOSITORY
        callback pointer.git_submodule_cb
        payload  CB_PAYLOAD
    }
    git_submodule_add_setup GIT_ERROR_CODE {
        submodulevar {PSUBMODULE retval}
        pRepo        PREPOSITORY
        url          STRING
        path         STRING
        use_gitlink  int
    }
    git_submodule_clone GIT_ERROR_CODE {
        pRepo     PREPOSITORY
        submodule PSUBMODULE
        opts      {struct.git_submodule_update_options byref}
    }
    git_submodule_add_finalize GIT_ERROR_CODE {
        submodule PSUBMODULE
    }
    git_submodule_add_to_index GIT_ERROR_CODE {
        submodule PSUBMODULE
        write_index int
    }
    git_submodule_owner PREPOSITORY {
        submodule PSUBMODULE
    }
    git_submodule_name STRING {
        submodule PSUBMODULE
    }
    git_submodule_path STRING {
        submodule PSUBMODULE
    }
    git_submodule_url STRING {
        submodule PSUBMODULE
    }
    git_submodule_resolve_url GIT_ERROR_CODE {
        pbuf  PBUF
        pRepo PREPOSITORY
        url   STRING
    }
    git_submodule_branch STRING {
        submodule PSUBMODULE
    }
    git_submodule_set_branch GIT_ERROR_CODE {
        pRepo  PREPOSITORY
        name   STRING
        branch STRING
    }
    git_submodule_set_url GIT_ERROR_CODE {
        pRepo PREPOSITORY
        name  STRING
        url   STRING
    }
    git_submodule_index_id {struct.git_oid byref} {
        submodule PSUBMODULE
    }
    git_submodule_head_id {struct.git_oid byref} {
        submodule PSUBMODULE
    }
    git_submodule_wd_id {struct.git_oid byref} {
        submodule PSUBMODULE
    }
    git_submodule_ignore GIT_SUBMODULE_IGNORE_T {
        submodule PSUBMODULE
    }
    git_submodule_set_ignore GIT_ERROR_CODE {
        pRepo  PREPOSITORY
        name   STRING
        ignore GIT_SUBMODULE_IGNORE_T
    }
    git_submodule_update_strategy GIT_SUBMODULE_UPDATE_T {
        submodule PSUBMODULE
    }
    git_submodule_set_update GIT_ERROR_CODE {
        pRepo  PREPOSITORY
        name   STRING
        update GIT_SUBMODULE_UPDATE_T
    }
    git_submodule_fetch_recurse_submodules GIT_ERROR_CODE {
        submodule PSUBMODULE
    }
    git_submodule_set_fetch_recurse_submodules GIT_ERROR_CODE {
        pRepo  PREPOSITORY
        name   STRING
        fetch_recurse_submodules GIT_SUBMODULE_RECURSE_T
    }
    git_submodule_init GIT_ERROR_CODE {
        submodule PSUBMODULE
        overwrite int
    }
    git_submodule_repo_init GIT_ERROR_CODE {
        vpRepo    {PREPOSITORY retval}
        submodule PSUBMODULE
    }
    git_submodule_sync GIT_ERROR_CODE {
        submodule PSUBMODULE
    }
    git_submodule_open GIT_ERROR_CODE {
        vpRepo    {PREPOSITORY retval}
        submodule PSUBMODULE
    }
    git_submodule_reload GIT_ERROR_CODE {
        submodule PSUBMODULE
        force     int
    }
    git_submodule_status GIT_ERROR_CODE {
        vstatus {GIT_SUBMODULE_STATUS_T retval}
        pRepo   PREPOSITORY
        name    STRING
        ignore  GIT_SUBMODULE_IGNORE_T
    }
    git_submodule_location GIT_ERROR_CODE {
        vlocation_status {GIT_SUBMODULE_STATUS_T retval}
        submodule        PSUBMODULE
    }
}
