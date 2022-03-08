# Based on libgit2/include/tag.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::prototype function git_tag_foreach_cb int {
    name STRING
    oid  {struct.git_oid byref}
    payload {pointer unsafe}
}

libgit2 functions {
    git_tag_lookup GIT_ERROR_CODE {
        pTag  {PTAG retval}
        pRepo PREPOSITORY
        id    {struct.git_oid byref}
    }
    git_tag_lookup_prefix GIT_ERROR_CODE {
        pTag  {PTAG retval}
        pRepo PREPOSITORY
        id    {struct.git_oid byref}
        len   size_t
    }
    git_tag_free void {
        pTag {PTAG nullok dispose}
    }
    git_tag_id {struct.git_oid byref} {
        pTag PTAG
    }
    git_tag_owner PREPOSITORY {
        pTag PTAG
    }
    git_tag_target GIT_ERROR_CODE {
        target_out {POBJECT retval}
        pTag PTAG
    }
    git_tag_target_id {struct.git_oid byref} {
        pTag PTAG
    }
    git_tag_target_type GIT_OBJECT_T {
        pTag PTAG
    }
    git_tag_name STRING {
        pTag PTAG
    }
    git_tag_tagger {struct.git_signature byref nullok} {
        pTag PTAG
    }
    git_tag_message STRING {
        pTag PTAG
    }
    git_tag_create GIT_ERROR_CODE {
        oid      {struct.git_oid retval}
        pRepo    PREPOSITORY
        tag_name STRING
        target   POBJECT
        tagger   {struct.git_signature byref}
        message  STRING
        force    int
    }
    git_tag_annotation_create GIT_ERROR_CODE {
        oid      {struct.git_oid retval}
        pRepo    PREPOSITORY
        tag_name STRING
        target   POBJECT
        tagger   {struct.git_signature byref}
        message  STRING
    }
    git_tag_create_from_buffer GIT_ERROR_CODE {
        oid    {struct.git_oid retval}
        pRepo  PREPOSITORY
        buffer pointer
        force  int
    }
    git_tag_create_lightweight GIT_ERROR_CODE {
        oid      {struct.git_oid retval}
        pRepo    PREPOSITORY
        tag_name STRING
        target   POBJECT
        force    int
    }
    git_tag_delete GIT_ERROR_CODE {
        pRepo    PREPOSITORY
        tag_name STRING
    }
    git_tag_list GIT_ERROR_CODE {
        tag_names {pointer.git_strarray unsafe}
        pRepo     PREPOSITORY
    }
    git_tag_list_match GIT_ERROR_CODE {
        tag_names {pointer.git_strarray unsafe}
        pattern   STRING
        pRepo     PREPOSITORY
    }
    git_tag_foreach GIT_ERROR_CODE {
        pRepo    PREPOSITORY
        callback pointer.git_tag_foreach_cb
        payload  {pointer unsafe}
    }
    git_tag_peel GIT_ERROR_CODE {
        tag_target {POBJECT retval}
        tag PTAG
    }
    git_tag_dup GIT_ERROR_CODE {
        tag    {PTAG retval}
        source PTAG
    }
    git_tag_name_is_valid GIT_ERROR_CODE {
        valid {int retval}
        name  STRING
    }
}
