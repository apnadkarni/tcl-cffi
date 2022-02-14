# Based on libgit2/include/reflog.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

libgit2 functions {
    git_reflog_read GIT_ERROR_CODE {
        reflog {PREFLOG out}
        pRepo  PREPOSITORY
        name   STRING
    }
    git_reflog_write GIT_ERROR_CODE {
        reflog PREFLOG
    }
    git_reflog_append GIT_ERROR_CODE {
        reflog    PREFLOG
        id        {struct.git_oid       byref}
        committer {struct.git_signature byref}
        msg       STRING
    }
    git_reflog_rename GIT_ERROR_CODE {
        pRepo    PREPOSITORY
        old_name STRING
        new_name STRING
    }
    git_reflog_delete GIT_ERROR_CODE {
        pRepo PREPOSITORY
        name STRING
    }
    git_reflog_entrycount size_t {
        reflog PREFLOG
    }
    git_reflog_entry_byindex {PREFLOG_ENTRY unsafe} {
        reflog PREFLOG
        idx    size_t
    }
    git_reflog_drop GIT_ERROR_CODE {
        reflog                 PREFLOG
        idx                    size_t
        rewrite_previous_entry int
    }
    git_reflog_entry_id_old {struct.git_oid byref} {
        entry {PREFLOG_ENTRY unsafe}
    }
    git_reflog_entry_id_new {struct.git_oid byref} {
        entry {PREFLOG_ENTRY unsafe}
    }
    git_reflog_entry_committer {struct.git_signature byref} {
        entry {PREFLOG_ENTRY unsafe}
    }
    git_reflog_entry_message {STRING nullok} {
        entry {PREFLOG_ENTRY unsafe}
    }
    git_reflog_free void {
        reflog {PREFLOG nullok dispose}
    }
}
