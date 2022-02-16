# Based on libgit2/include/types.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::alias define {
    git_off_t int64_t
    git_time_t int64_t
    git_object_size_t uint64_t
}

::cffi::enum define git_object_t {
    GIT_OBJECT_ANY       -2
    GIT_OBJECT_INVALID   -1
    GIT_OBJECT_COMMIT     1
    GIT_OBJECT_TREE       2
    GIT_OBJECT_BLOB       3
    GIT_OBJECT_TAG        4
    GIT_OBJECT_OFS_DELTA  6
    GIT_OBJECT_REF_DELTA  7
}
::cffi::alias define GIT_OBJECT_T {int {enum git_object_t}}

::cffi::alias define {
    PODB                      {pointer.git_odb counted}
    PODB_BACKEND              pointer.git_odb_backend
    PODB_OBJECT               {pointer.git_odb_object counted}
    PODB_STREAM               pointer.git_odb_stream
    PODB_WRITEPACK            pointer.git_odb_writepack
    PMIDX_WRITER              pointer.git_midx_writer
    PREFDB                    {pointer.git_refdb counted}
    PREFDB_BACKEND            pointer.git_refdb_backend
    PCOMMIT_GRAPH             pointer.git_commit_graph
    PCOMMIT_GRAPH_WRITER      pointer.git_commit_graph_writer

    PREPOSITORY               pointer.git_repository
    PWORKTREE                 pointer.git_worktree
    POBJECT                   {pointer.git_object counted}
    PREVWALK                  pointer.git_revwalk
    PTAG                      {pointer.git_tag counted}
    PBLOB                     {pointer.git_blob counted}
    PCOMMIT                   {pointer.git_commit counted}
    PTREE_ENTRY               pointer.git_tree_entry
    PTREE                     {pointer.git_tree counted}
    PTREEBUILDER              pointer.git_treebuilder
    PINDEX                    {pointer.git_index counted}
    PINDEX_ITERATOR           pointer.git_index_iterator
    PINDEX_CONFLICT_ITERATOR  pointer.git_index_conflict_iterator
    PCONFIG                   {pointer.git_config counted}
    PCONFIG_BACKEND           pointer.git_config_backend
    PREFLOG_ENTRY             pointer.git_reflog_entry
    PREFLOG                   pointer.git_reflog
    PNOTE                     pointer.git_note
    PPACKBUILDER              pointer.git_packbuilder


}

cffi::Struct create git_time {
    time   git_time_t
    offset int
    sign   schar
}

cffi::Struct create git_signature {
    name  STRING
    email STRING
    when  struct.git_time
}
::cffi::alias define PSIGNATURE pointer.git_signature

::cffi::alias define {
    PREFERENCE           pointer.git_reference
    PREFERENCE_ITERATOR  pointer.git_reference_iterator
    PTRANSACTION         pointer.git_transaction
    PANNOTATED_COMMIT    pointer.git_annotated_commit
    PSTATUS_LIST         pointer.git_status_list
    PREBASE              pointer.git_rebase
}

::cffi::enum define git_reference_t {
    GIT_REFERENCE_INVALID   0
    GIT_REFERENCE_DIRECT    1
    GIT_REFERENCE_SYMBOLIC  2
    GIT_REFERENCE_ALL       3
}
::cffi::alias define GIT_REFERENCE_T {int {enum git_reference_t}}

::cffi::enum define git_branch_t {
    GIT_BRANCH_LOCAL  1
    GIT_BRANCH_REMOTE  2
    GIT_BRANCH_ALL 3
}
::cffi::alias define GIT_BRANCH_T {int {enum git_branch_t}}

::cffi::enum define git_filemode_t {
    GIT_FILEMODE_UNREADABLE        0000000
    GIT_FILEMODE_TREE              0040000
    GIT_FILEMODE_BLOB              0100644
    GIT_FILEMODE_BLOB_EXECUTABLE   0100755
    GIT_FILEMODE_LINK              0120000
    GIT_FILEMODE_COMMIT            0160000
}
::cffi::alias define GIT_FILEMODE_T {int {enum git_filemode_t}}

::cffi::alias define {
    PREFSPEC           pointer.git_refspec
    PREMOTE            pointer.git_remote
    PTRANSPORT         pointer.git_transport
    PPUSH              pointer.git_push
    PREMOTE_HEAD       pointer.git_remote_head
    PREMOTE_CALLBACKS  pointer.git_remote_callbacks
    PCERT              pointer.git_cert
    PSUBMODULE         {pointer.git_submodule counted}
}

::cffi::enum define git_submodule_update_t {
    GIT_SUBMODULE_UPDATE_CHECKOUT  1
    GIT_SUBMODULE_UPDATE_REBASE    2
    GIT_SUBMODULE_UPDATE_MERGE     3
    GIT_SUBMODULE_UPDATE_NONE      4
    GIT_SUBMODULE_UPDATE_DEFAULT   0
}
::cffi::alias define GIT_SUBMODULE_UPDATE_T {int {enum git_submodule_update_t}}

::cffi::enum define git_submodule_ignore_t {
    GIT_SUBMODULE_IGNORE_UNSPECIFIED   -1
    GIT_SUBMODULE_IGNORE_NONE       1
    GIT_SUBMODULE_IGNORE_UNTRACKED  2
    GIT_SUBMODULE_IGNORE_DIRTY      3
    GIT_SUBMODULE_IGNORE_ALL        4
}
::cffi::alias define GIT_SUBMODULE_IGNORE_T {int {enum git_submodule_ignore_t}}

::cffi::enum define git_submodule_recurse_t {
    GIT_SUBMODULE_RECURSE_NO        0
    GIT_SUBMODULE_RECURSE_YES       1
    GIT_SUBMODULE_RECURSE_ONDEMAND  2
}
::cffi::alias define GIT_SUBMODULE_RECURSE_T {int {enum git_submodule_recurse_t}}

::cffi::alias define PWRITESTREAM pointer.git_writestream

# TBD
# /** A type to write in a streaming fashion, for example, for filters. */
# struct git_writestream {
#     int GIT_CALLBACK(write)(git_writestream *stream, const char *buffer, size_t len);
#     int GIT_CALLBACK(close)(git_writestream *stream);
#     void GIT_CALLBACK(free)(git_writestream *stream);
# };


::cffi::alias define PMAILMAP pointer.git_mailmap
