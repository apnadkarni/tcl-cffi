# Based on libgit2/include/pack.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum sequence git_packbuilder_stage_t {
    GIT_PACKBUILDER_ADDING_OBJECTS
    GIT_PACKBUILDER_DELTAFICATION
}
::cffi::alias define GIT_PACKBUILDER_STAGE_T {int {enum git_packbuilder_stage_t}}

::cffi::prototype function git_packbuilder_foreach_cb int {
    buf {pointer unsafe}
    size size_t
    payload CB_PAYLOAD
}
::cffi::prototype function git_packbuilder_progress int {
    stage int
    current uint32_t
    total uint32_t
    payload CB_PAYLOAD
}

libgit2 functions {
    git_packbuilder_new GIT_ERROR_CODE {
        pb {PPACKBUILDER retval}
    }
    git_packbuilder_set_threads GIT_ERROR_CODE {
        pb PPACKBUILDER
        n  int
    }
    git_packbuilder_insert GIT_ERROR_CODE {
        pb   PPACKBUILDER
        id   {struct.git_oid byref}
        name {STRING nullok nullifempty}
    }
    git_packbuilder_insert_tree GIT_ERROR_CODE {
        pb PPACKBUILDER
        id {struct.git_oid byref}
    }
    git_packbuilder_insert_commit GIT_ERROR_CODE {
        pb PPACKBUILDER
        id {struct.git_oid byref}
    }
    git_packbuilder_insert_walk GIT_ERROR_CODE {
        pb PPACKBUILDER
        walk PREVWALK
    }
    git_packbuilder_insert_recur GIT_ERROR_CODE {
        pb PPACKBUILDER
        id   {struct.git_oid byref}
        name {STRING nullok nullifempty}
    }
    git_packbuilder_write_buf GIT_ERROR_CODE {
        buf PBUF
        pb PPACKBUILDER

    }
    git_packbuilder_write GIT_ERROR_CODE {
        pb          PPACKBUILDER
        path        {STRING nullok nullifempty}
        mode        uint
        progress_cb {pointer.git_indexer_progress_cb}
        progress_cb_payload CB_PAYLOAD
    }
    git_packbuilder_hash {struct.git_oid byref} {
        pb PPACKBUILDER
    }
    git_packbuilder_foreach GIT_ERROR_CODE {
        pb PPACKBUILDER
        cb pointer.git_packbuilder_foreach_cb
        payload CB_PAYLOAD
    }
    git_packbuilder_object_count size_t {
        pb PPACKBUILDER
    }
    git_packbuilder_written size_t {
        pb PPACKBUILDER
    }
    git_packbuilder_set_callbacks GIT_ERROR_CODE {
        pb PPACKBUILDER
        progress_cb pointer.git_packbuilder_progress
        progress_cb_payload CB_PAYLOAD
    }
    git_packbuilder_free void {
        pb {PPACKBUILDER nullok dispose}
    }
}
