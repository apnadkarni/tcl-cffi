# Based on libgit2/include/odb_backend.tcl
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum define git_odb_stream_t {
    GIT_STREAM_RDONLY 2
    GIT_STREAM_WRONLY 4
    GIT_STREAM_RW     6
}
::cffi::alias define GIT_ODB_STREAM_T {int {enum git_odb_stream_t}}

::cffi::prototype function git_odb_backend_read_cb int {
    pstream {PODB_STREAM unsafe}
    pbuffer {pointer unsafe}
    len     size_t
}
::cffi::prototype function git_odb_backend_write_cb int {
    pstream {PODB_STREAM unsafe}
    pbuffer {pointer unsafe}
    len     size_t
}
::cffi::prototype function git_odb_backend_finalize_write_cb int {
    pstream {PODB_STREAM unsafe}
    oid {struct.git_oid byref}
}
::cffi::prototype function git_odb_backend_free_cb void {
    pstream {PODB_STREAM unsafe}
}
::cffi::prototype function git_odb_writepack_append_cb int {
    pwritepack {pointer.git_odb_writeback unsafe}
    data       {pointer unsafe}
    size       size_t
    pstats     pointer.git_indexer_progress
}
::cffi::prototype function git_odb_writepack_commit_cb int {
    pwritepack {pointer.git_odb_writeback unsafe}
    pstats     pointer.git_indexer_progress
}
::cffi::prototype function git_odb_writepack_free_cb int {
    pwritepack {pointer.git_odb_writeback unsafe}
}

::cffi::Struct create git_odb_stream {
    podb_backend      PODB_BACKEND
    mode              uint
    hash_ctx          pointer
    declared_size     git_object_size_t
    received_bytes    git_object_size_t
    read_cb           pointer.git_odb_backend_read_cb
    write_cb          pointer.git_odb_backend_write_cb
    finalize_write_cb pointer.git_odb_backend_finalize_write_cb
    free_cb           pointer.git_odb_backend_free_cb
}

::cffi::Struct create git_odb_writepack {
    pbackend PODB_BACKEND
    append_cb pointer.git_odb_writeback_append_cb
    commit_cb pointer.git_odb_writeback_commit_cb
    free_cb pointer.git_odb_writeback_free_cb
}

libgit2 functions {
    git_odb_backend_pack GIT_ERROR_CODE {
        vbackend    {PODB_BACKEND retval}
        objects_dir STRING
    }
    git_odb_backend_loose GIT_ERROR_CODE {
        vbackend          {PODB_BACKEND retval}
        objects_dir       STRING
        compression_level int
        do_fsync          int
        dir_mode          uint
        file_mode         uint
    }
    git_odb_backend_one_pack GIT_ERROR_CODE {
        vbackend   {PODB_BACKEND retval}
        index_file STRING
    }

}
