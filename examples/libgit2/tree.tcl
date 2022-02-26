# Based on libgit2/include/tree.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

# First, some callbacks used in the definitions

::cffi::prototype function git_treebuilder_filter_cb int {
    pEntry  {PTREE_ENTRY unsafe}
    payload {pointer unsafe}
}

::cffi::prototype function git_treewalk_cb int {
    root STRING
    pEntry {PTREE_ENTRY unsafe}
    payload {pointer unsafe}
}

# Enumerations
::cffi::enum sequence git_treewalk_mode {GIT_TREEWALK_PRE GIT_TREEWALK_POST}
::cffi::alias define GIT_TREEWALK_MODE {int {enum git_treewalk_mode}}

::cffi::enum sequence git_tree_update_t {
    GIT_TREE_UPDATE_UPSERT GIT_TREE_UPDATE_REMOVE
}
::cffi::alias define GIT_TREE_UPDATE_T {int {enum git_tree_update_t}}

::cffi::Struct create git_tree_update {
    action   GIT_TREE_UPDATE_T
    id       struct.git_oid
    filemode GIT_FILEMODE_T
    path     STRING
}

# git_tree_id
# {TBD - replace with struct byref once byref implemented for return}
# git_tree_owner
# {TBD - marked as unsafe return because repo would already have been returned by other calls}
libgit2 functions {
    git_tree_lookup GIT_ERROR_CODE {
        pTree  {PTREE retval}
        pRepo  PREPOSITORY
        id     {struct.git_oid byref}
    }
    git_tree_lookup_prefix GIT_ERROR_CODE {
        pTree {PTREE retval}
        pRepo PREPOSITORY
        id    {struct.git_oid byref}
        len   size_t
    }
    git_tree_free void {
        pTree {PTREE nullok dispose}
    }
    git_tree_id {struct.git_oid byref} {
        pTree PTREE
    }
    git_tree_owner PREPOSITORY {
        pTree PTREE
    }
    git_tree_entrycount size_t {
        pTree PTREE
    }
}

# NOTES on git_entry
# git_tree_entry_{byname,byindex,byid}
# these return a UNSAFE pointer and should NOT be freed
# git_tree_entry_{bypath,dup}
# - these return a SAFE pointer as it has to be freed
# git_tree_entry_id - TBD - replace with struct deref
libgit2 functions {
    git_tree_entry_byname {PTREE_ENTRY unsafe} {
        pTree    PTREE
        filename STRING
    }
    git_tree_entry_byindex {PTREE_ENTRY unsafe} {
        pTree PTREE
        idx   size_t
    }
    git_tree_entry_byid {PTREE_ENTRY unsafe} {
        pTree PTREE
        oid   {struct.git_oid byref}
    }
    git_tree_entry_bypath GIT_ERROR_CODE {
        pTreeEntry {PTREE_ENTRY retval}
        pTree      PTREE
        path       STRING
    }
    git_tree_entry_dup GIT_ERROR_CODE {
        pDest   {PTREE_ENTRY retval}
        pSource PTREE_ENTRY
    }
    git_tree_entry_free void {
        pTree {PTREE_ENTRY nullok dispose}
    }
    git_tree_entry_name STRING {
        pTreeEntry {PTREE_ENTRY unsafe}
    }
    git_tree_entry_id {struct.git_oid byref} {
        pTreeEntry {PTREE_ENTRY unsafe}
    }
    git_tree_entry_type GIT_OBJECT_T {
        pTreeEntry {PTREE_ENTRY unsafe}
    }
    git_tree_entry_filemode GIT_FILEMODE_T {
        pTreeEntry {PTREE_ENTRY unsafe}
    }
    git_tree_entry_filemode_raw int {
        pTreeEntry {PTREE_ENTRY unsafe}
    }
    git_tree_entry_cmp int {
        pEntry1 {PTREE_ENTRY unsafe}
        pEntry2 {PTREE_ENTRY unsafe}
    }
    git_tree_entry_to_object GIT_ERROR_CODE {
        pObject {POBJECT retval}
        pRepo   PREPOSITORY
        pEntry  {PTREE_ENTRY unsafe}
    }
}

libgit2 functions {
    git_treebuilder_new GIT_ERROR_CODE {
        pTreeBuilder {PTREEBUILDER retval}
        pRepo        PREPOSITORY
        pSource      {PTREE nullok {default NULL}}
    }
    git_treebuilder_clear GIT_ERROR_CODE {
        pTreeBuilder PTREEBUILDER
    }
    git_treebuilder_entrycount size_t {
        pTreeBuilder PTREEBUILDER
    }
    git_treebuilder_free void {
        pTreeBuilder {PTREEBUILDER nullok dispose}
    }
    git_treebuilder_get {PTREE_ENTRY unsafe} {
        pTreeBuilder PTREEBUILDER
        filename     STRING
    }
    git_treebuilder_insert GIT_ERROR_CODE {
        pTreeEntry   {PTREE_ENTRY retval unsafe}
        pTreeBuilder PTREEBUILDER
        filename     STRING
        id           {struct.git_oid byref}
        filemode     GIT_FILEMODE_T
    }
    git_treebuilder_remove GIT_ERROR_CODE {
        pTreeBuilder PTREEBUILDER
        filename     STRING
    }
    git_treebuilder_filter GIT_ERROR_CODE {
        pTreeBuilder PTREEBUILDER
        filterCb     pointer.git_treebuilder_filter_cb
        pPayload     {pointer unsafe}
    }
    git_treebuilder_write GIT_ERROR_CODE {
        oid {struct.git_oid retval}
        pTreeBuilder PTREEBUILDER
    }
    git_tree_walk GIT_ERROR_CODE {
        pTree PTREE
        mode  GIT_TREEWALK_MODE
        walkCb pointer.git_treewalk_cb
        payload {pointer unsafe}
    }
    git_tree_dup GIT_ERROR_CODE {
        pTree   {PTREE retval}
        pSource PTREE
    }
    git_tree_create_updated GIT_ERROR_CODE {
        oid       {struct.git_oid retval}
        pRepo     PREPOSITORY
        pBaseline PTREE
        nupdates  size_t
        updates   {struct.git_tree_update[nupdates] byref}
    }
}
