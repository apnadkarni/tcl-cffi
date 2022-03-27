# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/revwalk.tcl
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum define git_sort_t {
    GIT_SORT_NONE        0
    GIT_SORT_TOPOLOGICAL 1
    GIT_SORT_TIME        2
    GIT_SORT_REVERSE     4
}
::cffi::alias define GIT_SORT_T {int {enum git_sort_t} bitmask}

::cffi::prototype function git_revwalk_hide_cb int {
    id      {struct.git_oid byref}
    payload CB_PAYLOAD
}

libgit2 functions {
    git_revwalk_new GIT_ERROR_CODE {
        vwalk {PREVWALK retval}
        pRepo PREPOSITORY
    }
    git_revwalk_reset GIT_ERROR_CODE {
        walk PREVWALK
    }
    git_revwalk_push GIT_ERROR_CODE {
        walk PREVWALK
        id   {struct.git_oid byref}
    }
    git_revwalk_push_glob GIT_ERROR_CODE {
        walk PREVWALK
        glob STRING
    }
    git_revwalk_push_head GIT_ERROR_CODE {
        walk PREVWALK
    }
    git_revwalk_hide GIT_ERROR_CODE {
        walk      PREVWALK
        commit_id {struct.git_oid byref}
    }
    git_revwalk_hide_glob GIT_ERROR_CODE {
        walk PREVWALK
        glob STRING
    }
    git_revwalk_hide_head GIT_ERROR_CODE {
        walk PREVWALK
    }
    git_revwalk_push_ref GIT_ERROR_CODE {
        walk    PREVWALK
        refname STRING
    }
    git_revwalk_hide_ref GIT_ERROR_CODE {
        walk    PREVWALK
        refname STRING
    }
    git_revwalk_next GIT_ITER_ERROR_CODE {
        id   {struct.git_oid out}
        walk PREVWALK
    }
    git_revwalk_sorting GIT_ERROR_CODE {
        walk      PREVWALK
        sort_mode GIT_SORT_T
    }
    git_revwalk_push_range GIT_ERROR_CODE {
        walk  PREVWALK
        range STRING
    }
    git_revwalk_simplify_first_parent GIT_ERROR_CODE {
        walk PREVWALK
    }
    git_revwalk_free void {
        walk {PREVWALK nullok dispose}
    }
    git_revwalk_repository PREPOSITORY {
        walk PREVWALK
    }
    git_revwalk_add_hide_cb GIT_ERROR_CODE {
        walk    PREVWALK
        hide_cb pointer.git_revwalk_hide_cb
        payload CB_PAYLOAD
    }
}



