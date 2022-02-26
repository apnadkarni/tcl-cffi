# Based on libgit2/include/odb.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_indexer_progress {
    total_objects    uint
    indexed_objects  uint
    received_objects uint
    local_objects    uint
    total_deltas     uint
    indexed_deltas   uint
    received_bytes   size_t
}

::cffi::prototype function git_indexer_progress_cb int {stats {struct.git_indexer_progress byref} payload CB_PAYLOAD}

::cffi::Struct create git_indexer_options {
    version             int
    progress_cb         {pointer nullok unsafe}
    progress_cb_payload {pointer nullok unsafe}
    verify              uchar
}
::cffi::alias define PINDEXER_OPTIONS pointer.git_indexer_options
::cffi::alias define PINDEXER pointer.git_indexer

libgit2 functions {
    git_indexer_options_init GIT_ERROR_CODE {
            pOpts   PINDEXER_OPTIONS
            version uint
    }
    git_indexer_new GIT_ERROR_CODE {
            ppIndexer {PINDEXER retval}
            path      STRING
            mode      uint
            pOdb      {PODB nullok}
    }
    git_indexer_append GIT_ERROR_CODE {
            pIndexer  PINDEXER
            data      binary
            size      size_t
            stats     {struct.git_indexer_progress retval}
    }
    git_indexer_commit GIT_ERROR_CODE {
            pIndexer  PINDEXER
            stats     {struct.git_indexer_progress retval}
    }
    git_indexer_free void {
            pIndexer {PINDEXER nullok dispose}
    }
}

libgit2 function git_indexer_hash {struct.git_oid byref} {pIndexer PINDEXER}
