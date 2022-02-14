# Based on libgit2/include/worktree.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_worktree_add_options {
    version {uint {default 1}}
    lock    int
    ref     PREFERENCE
}

::cffi::enum flags git_worktree_prune_t {
    GIT_WORKTREE_PRUNE_VALID
    GIT_WORKTREE_PRUNE_LOCKED
    GIT_WORKTREE_PRUNE_WORKING_TREE
}
::cffi::alias define GIT_WORKTREE_PRUNE_T {int {enum git_worktree_prune_t} bitmask}

::cffi::Struct create git_worktree_prune_options {
    version {uint {default 1}}
    flags GIT_WORKTREE_PRUNE_T
}

libgit2 functions {
    git_worktree_list GIT_ERROR_CODE {
        pStrArray PSTRARRAY
        pRepo     PREPOSITORY
    }
    git_worktree_lookup GIT_ERROR_CODE {
        pwt   {PWORKTREE out}
        pRepo PREPOSITORY
        name  STRING
    }
    git_worktree_open_from_repository GIT_ERROR_CODE {
        pwt   {PWORKTREE out}
        pRepo PREPOSITORY
    }
    git_worktree_free void {
        wt {PWORKTREE nullok dispose}
    }
    git_worktree_add_options_init GIT_ERROR_CODE {
        vopts   {struct.git_worktree_add_options out}
        version {uint {default 1}}
    }
    git_worktree_add GIT_ERROR_CODE {
        vwt   {PWORKTREE out}
        pRepo PREPOSITORY
        name  STRING
        path  STRING
        opts  {struct.git_worktree_add_options byref}
    }
    git_worktree_lock int {
        wt     PWORKTREE
        reason STRING
    }
    git_worktree_is_locked int {
        pbuf {PBUF nullok}
        wt   PWORKTREE
    }
    git_worktree_name STRING {
        wt PWORKTREE
    }
    git_worktree_path STRING {
        wt PWORKTREE
    }
    git_worktree_prune_options_init GIT_ERROR_CODE {
        vopts   {struct.git_worktree_prune_options out}
        version {uint {default 1}}
    }
    git_worktree_is_prunable int {
        wt PWORKTREE
        opts {struct.git_worktree_prune_options byref}
    }
    git_worktree_is_prunable GIT_ERROR_CODE {
        wt PWORKTREE
        opts {struct.git_worktree_prune_options byref}
    }
}
