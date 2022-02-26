# Based on libgit2/include/blob.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum flags git_blob_filter_flag_t {
    GIT_BLOB_FILTER_CHECK_FOR_BINARY
    GIT_BLOB_FILTER_NO_SYSTEM_ATTRIBUTES
    GIT_BLOB_FILTER_ATTRIBUTES_FROM_HEAD
    GIT_BLOB_FILTER_ATTRIBUTES_FROM_COMMIT
}

::cffi::Struct create git_blob_filter_options {
    version int
    flags uint32_t
    commit_id {pointer.git_oid unsafe}
    attr_commit_id struct.git_oid
}

libgit2 functions {
    git_blob_filter_options_init GIT_ERROR_CODE {
        opts    {struct.git_blob_filter_options retval}
        version uint
    }
    git_blob_lookup GIT_ERROR_CODE {
        pBlob {PBLOB retval}
        pRepo PREPOSITORY
        id    {struct.git_oid byref}
    }
    git_blob_lookup_prefix GIT_ERROR_CODE {
        pBlob {PBLOB retval}
        pRepo PREPOSITORY
        id    {struct.git_oid byref}
        len   size_t
    }
    git_blob_free void {
        pBlob {PBLOB nullok dispose}
    }
    git_blob_id {struct.git_oid byref} {
        pBlob PBLOB
    }
    git_blob_owner PREPOSITORY {
        pBlob PBLOB
    }
    git_blob_rawcontent {pointer unsafe} {
        pBlob PBLOB
    }
    git_blob_rawsize git_object_size_t {
        pBlob PBLOB
    }
    git_blob_filter GIT_ERROR_CODE {
        pBuf  PBUF
        pBlob PBLOB
        as_path STRING
        opts {struct.git_blob_filter_options byref}
    }
    git_blob_create_from_workdir GIT_ERROR_CODE {
        id            {struct.git_oid byref}
        pRepo         PREPOSITORY
        relative_path STRING
    }
    git_blob_create_from_disk GIT_ERROR_CODE {
        id    {struct.git_oid byref}
        pRepo PREPOSITORY
        path  STRING
    }
    git_blob_create_from_stream GIT_ERROR_CODE {
        pStream  {PWRITESTREAM retval}
        pRepo    PREPOSITORY
        hintpath STRING
    }
    git_blob_create_from_stream_commit GIT_ERROR_CODE {
        id {struct.git_oid retval}
        stream {PWRITESTREAM nullok dispose}
    }
    git_blob_create_from_buffer GIT_ERROR_CODE {
        id     {struct.git_oid retval}
        pRepo  PREPOSITORY
        buffer {pointer unsafe}
        len    size_t
    }
    git_blob_is_binary int {
        blob PBLOB
    }
    git_blob_dup GIT_ERROR_CODE {
        out_blob {PBLOB retval}
        src_blob PBLOB
    }
}
