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

::cffi::prototype function git_indexer_progress_cb int {stats {struct.git_indexer_progress out} payload {pointer unsafe}}

::cffi::Struct create git_indexer_options {
    version             int
    progress_cb         {pointer nullok unsafe}
    progress_cb_payload {pointer nullok unsafe}
    verify              uchar
}
::cffi::alias define PINDEXER_OPTIONS pointer.git_indexer_options
::cffi::alias define PINDEXER pointer.git_indexer

AddFunctions {
    git_indexer_options_init {
        GIT_ERROR_CODE
        {
            pOpts   PINDEXER_OPTIONS
            version uint
        }
    }
    git_indexer_new {
        GIT_ERROR_CODE
        {
            ppIndexer {PINDEXER out}
            path      STRING
            mode      uint
            pOdb      {PODB nullok}
        }
    }
    git_indexer_append {
        GIT_ERROR_CODE
        {
            pIndexer  PINDEXER
            data      binary
            size      size_t
            stats     {struct.git_indexer_progress out}
        }
    }
    git_indexer_commit {
        GIT_ERROR_CODE
        {
            pIndexer  PINDEXER
            stats     {struct.git_indexer_progress out}
        }
    }
    git_indexer_free {
        void
        {
            pIndexer {PINDEXER dispose}
        }
    }
}

# TODO - should git_indexer_hash return a {struct.git_oid byref} once byref is
# implemented for return values. For now do a proc wrapper
AddFunction {git_indexer_hash git_indexer_hash_internal} {POID unsafe} {pIndexer PINDEXER}
proc git_indexer_hash {pIndexer} {
    set pOid [git_indexer_hash_internal $pIndexer]
    # pOid points INSIDE pIndexer, no need to free or dispose etc.
    return [git_oid fromnative! $pOid]
}
