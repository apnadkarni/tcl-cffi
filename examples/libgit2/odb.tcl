# Based on libgit2/include/odb.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_odb_expand_id {
    id     struct.git_oid
    length ushort
    type   GIT_OBJECT_T
}

::cffi::prototype function git_odb_foreach_cb int {
    oid {struct.git_oid byref}
    payload {pointer unsafe}
}

AddFunctions {
    git_odb_new {
        GIT_ERROR_CODE
        {
            ppOdb {PODB out}
        }
    }
    git_odb_open {
        GIT_ERROR_CODE
        {
            ppOdb {PODB out}
            objects_dir STRING
        }
    }
    git_odb_add_disk_alternate {
        GIT_ERROR_CODE
        {
            db PODB
            path STRING
        }
    }
    git_odb_free {
        void
        {
            db {PODB nullok dispose}
        }
    }
    git_odb_read {
        GIT_ERROR_CODE
        {
            pOdbObject {PODB_OBJECT out}
            db PODB
            id {struct.git_oid byref}
        }
    }
    git_odb_read_prefix {
        GIT_ERROR_CODE
        {
            pOdbObject {PODB_OBJECT out}
            db PODB
            short_id {struct.git_oid byref}
            len size_t
        }
    }
    git_odb_read_header {
        GIT_ERROR_CODE
        {
            len_out  {size_t out}
            type_out {GIT_OBJECT_T out}
            db       PODB
            id       {struct.git_oid byref}
        }
    }
    git_odb_exists {
        int
        {
            db PODB
        }
    }
    git_odb_exists_prefix {
        int
        {
            oid      {struct.git_oid out}
            db       PODB
            short_id {struct.git_oid byref}
            len      size_t
        }
    }
    git_odb_expand_ids {
        GIT_ERROR_CODE
        {
            db PODB
            ids {pointer.git_odb_expand_id}
            count size_t
        }
    }
    git_odb_refresh {
        GIT_ERROR_CODE
        {
            db PODB
        }
    }
    git_odb_foreach {
        GIT_ERROR_CODE
        {
            db      PODB
            cb      {pointer.git_odb_foreach_cb unsafe}
            payload {pointer unsafe}
        }
    }
    git_odb_write {
        GIT_ERROR_CODE
        {
            oid {struct.git_oid out}
            odb PODB
            data binary
            len size_t
            type GIT_OBJECT_T
        }
    }
    git_odb_open_wstream {
        GIT_ERROR_CODE
        {
            ppStream {PODB_STREAM out}
            db       PODB
            size     git_object_size_t
            type     GIT_OBJECT_T
        }
    }
    git_odb_stream_write {
        GIT_ERROR_CODE
        {
            stream PODB_STREAM
            buffer binary
            len    size_t
        }
    }
    git_odb_stream_finalize_write {
        GIT_ERROR_CODE
        {
            oid    {struct.git_oid out}
            stream PODB_STREAM
        }
    }
    git_odb_stream_read {
        GIT_ERROR_CODE
        {
            stream PODB_STREAM
            buffer bytes[len]
            len    size_t
        }
    }
    git_odb_stream_free {
        void
        {
            stream {PODB_STREAM dispose}
        }
    }
    git_odb_open_rstream {
        GIT_ERROR_CODE
        {
            ppStream {PODB_STREAM out}
            len      {size_t out}
            type     {GIT_OBJECT_T out}
            db       {PODB}
            oid      {struct.git_oid byref}
        }
    }
    git_odb_write_pack {
        GIT_ERROR_CODE
        {
            writepack {PODB_WRITEPACK out}
            db        PODB
            progress_cb pointer.git_indexer_progress_cb
            progress_payload {pointer unsafe}
        }
    }
    git_odb_write_multi_pack_index {
        GIT_ERROR_CODE
        {
            db PODB
        }
    }
    git_odb_hash {
        GIT_ERROR_CODE
        {
            oid  {struct.git_oid out}
            data binary
            len  size_t
            type GIT_OBJECT_T
        }
    }
    git_odb_hashfile {
        GIT_ERROR_CODE
        {
            oid {struct.git_oid out}
            path STRING
            type GIT_OBJECT_T
        }
    }
    git_odb_object_dup {
        GIT_ERROR_CODE
        {
            dest {PODB_OBJECT out}
            source PODB_OBJECT
        }
    }
    git_odb_object_free {
        void
        {
            object {PODB_OBJECT dispose}
        }
    }
    # {TBD - replace with struct.git_oid byref once byref is implemented}
    git_odb_object_id {
        {pointer.git_oid unsafe}
        {
            odb PODB
        }
    }
    git_odb_object_data {
        {pointer unsafe}
        {
            odb PODB
        }
    }
    git_odb_object_size {
        size_t
        {
            odb PODB
        }
    }
    git_odb_object_type {
        GIT_OBJECT_T
        {
            odb PODB
        }
    }
    git_odb_add_backend {
        GIT_ERROR_CODE
        {
            odb      PODB
            backend  PODB_BACKEND
            priority int
        }
    }
    git_odb_add_alternate {
        GIT_ERROR_CODE
        {
            odb      PODB
            backend  PODB_BACKEND
            priority int
        }
    }
    git_odb_num_backends {
        size_t
        {
            odb PODB
        }
    }
    git_odb_get_backend {
        GIT_ERROR_CODE
        {
            backend {PODB_BACKEND out}
            odb     PODB
            pos     size_t
        }
    }
    git_odb_set_commit_graph {
        GIT_ERROR_CODE
        {
            odb PODB
            cgraph PCOMMIT_GRAPH
        }
    }
}



