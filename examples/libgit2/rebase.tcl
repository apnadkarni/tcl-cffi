# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/rebase.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_rebase_options {
    version {uint {default 1}}
    quiet int
    inmemory int
    rewrite_notes_ref {STRING nullifempty}
    merge_options struct.git_merge_options
    checkout_options struct.git_checkout_options
    commit_create_cb pointer.git_commit_create_cb
    reserved {pointer unsafe}
    payload CB_PAYLOAD
}

::cffi::enum sequence git_rebase_operation_t {
    GIT_REBASE_OPERATION_PICk
    GIT_REBASE_OPERATION_REWORD
    GIT_REBASE_OPERATION_EDIT
    GIT_REBASE_OPERATION_SQUASH
    GIT_REBASE_OPERATION_FIXUP
    GIT_REBASE_OPERATION_EXEC
}
::cffi::alias define GIT_REBASE_OPERATION_T {int {enum git_rebase_operation_t}}

::cffi::Struct create git_rebase_operation {
    type GIT_REBASE_OPERATION_T
    id struct.git_oid
    exec STRING
}

libgit2 functions {
    git_rebase_options_init GIT_ERROR_CODE {
        opts {struct.git_rebase_options retval}
        version {uint {default 1}}
    }
    git_rebase_init GIT_ERROR_CODE {
        rebase   {PREBASE retval}
        pRepo    PREPOSITORY
        branch   PANNOTATED_COMMIT
        upstream PANNOTATED_COMMIT
        onto     PANNOTATED_COMMIT
        opts     {struct.git_rebase_options byref}
    }
    git_rebase_open GIT_ERROR_CODE {
        rebase   {PREBASE retval}
        pRepo    PREPOSITORY
        opts     {struct.git_rebase_options byref}
    }
    git_rebase_orig_head_name STRING {
        rebase PREBASE
    }
    git_rebase_orig_head_id {struct.git_oid byref} {
        rebase PREBASE
    }
    git_rebase_onto_name STRING {
        rebase PREBASE
    }
    git_rebase_onto_id {struct.git_oid byref} {
        rebase PREBASE
    }
    git_rebase_operation_entrycount size_t {
        rebase PREBASE
    }
    git_rebase_operation_current size_t {
        rebase PREBASE
    }
    git_rebase_operation_byindex {struct.git_rebase_operation byref} {
        rebase PREBASE
        idx    size_t
    }
    git_rebase_next GIT_ERROR_CODE {
        pNextOperation {pointer.git_rebase_operation retval}
        rebase         PREBASE
    }
    git_rebase_inmemory_index GIT_ERROR_CODE {
        index  {PINDEX retval}
        rebase PREBASE
    }
    git_rebase_commit GIT_ERROR_CODE {
        id               {struct.git_oid byref}
        rebase           PREBASE
        author           {struct.git_signature byref}
        committer        {struct.git_signature byref}
        message_encoding STRING
        encoded_message  binary
    }
    git_rebase_abort GIT_ERROR_CODE {
        rebase PREBASE
    }
    git_rebase_finish GIT_ERROR_CODE {
        rebase PREBASE
        signature {struct.git_signature byref}
    }
    git_rebase_free void {
        rebase {PREBASE nullok dispose}
    }
}
