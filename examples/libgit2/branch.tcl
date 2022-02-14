# Based on libgit2/include/branch.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::alias define PBRANCH_ITERATOR pointer.git_branch_iterator

libgit2 functions {
    git_branch_create GIT_ERROR_CODE {
        ref         {PREFERENCE out}
        pRepo       PREPOSITORY
        branch_name STRING
        target      PCOMMIT
        force       int
    }
    git_branch_create_from_annotated GIT_ERROR_CODE {
        ref         {PREFERENCE out}
        pRepo       PREPOSITORY
        branch_name STRING
        target      PANNOTATED_COMMIT
        force       int
    }
    git_branch_delete GIT_ERROR_CODE {
        branch PREFERENCE
    }
    git_branch_iterator_new GIT_ERROR_CODE {
        iter       {PBRANCH_ITERATOR out}
        pRepo      PREPOSITORY
        list_flags GIT_BRANCH_T
    }
    git_branch_next {int {enum git_error_code}} {
        ref  {PREFERENCE   out}
        type {GIT_BRANCH_T out}
        iter PBRANCH_ITERATOR
    }
    git_branch_iterator_free void {
        iter {PBRANCH_ITERATOR nullok dispose}
    }
    git_branch_move GIT_ERROR_CODE {
        new_ref         {PREFERENCE out}
        branch          PREFERENCE
        new_branch_name STRING
        force           int
    }
    git_branch_lookup GIT_ERROR_CODE {
        ref         {PREFERENCE out}
        pRepo       PREPOSITORY
        branch_name STRING
        branch_type GIT_BRANCH_T
    }
    git_branch_name GIT_ERROR_CODE {
        name {STRING out}
        ref  PREFERENCE
    }
    git_branch_upstream GIT_ERROR_CODE {
        ref    {PREFERENCE out}
        branch PREFERENCE
    }
    git_branch_set_upstream GIT_ERROR_CODE {
        branch      PREFERENCE
        branch_name {STRING nullifempty}
    }
    git_branch_upstream_name GIT_ERROR_CODE {
        buf     PBUF
        pRepo   PREPOSITORY
        refname STRING
    }
    git_branch_is_head int {
        branch PREFERENCE
    }
    git_branch_is_checked_out int {
        branch PREFERENCE
    }
    git_branch_remote_name GIT_ERROR_CODE {
        buf     PBUF
        pRepo   PREPOSITORY
        refname STRING
    }
    git_branch_upstream_remote GIT_ERROR_CODE {
        buf     PBUF
        pRepo   PREPOSITORY
        refname STRING
    }
    git_branch_upstream_merge GIT_ERROR_CODE {
        buf     PBUF
        pRepo   PREPOSITORY
        refname STRING
    }
    git_branch_name_is_valid GIT_ERROR_CODE {
        valid {int out}
        name  STRING
    }
}
