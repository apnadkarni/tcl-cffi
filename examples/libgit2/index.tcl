# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/index.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_index_time {
    seconds int32_t
    nanoseconds uint32_t
}

::cffi::Struct create git_index_entry {
    ctime          struct.git_index_time
    mtime          struct.git_index_time

    dev            uint32_t
    ino            uint32_t
    mode           uint32_t
    uid            uint32_t
    gid            uint32_t
    file_size      uint32_t

    id             struct.git_oid
    flags          uint16_t
    flags_extended uint16_t
    path           STRING
}

::cffi::enum define git_index_entry_flag_t {
    GIT_INDEX_ENTRY_NAMEMASK             0x0fff
    GIT_INDEX_ENTRY_STAGEMASK            0x3000
    GIT_INDEX_ENTRY_INDEX_ENTRY_EXTENDED 0x4000
    GIT_INDEX_ENTRY_VALID                0x8000
}
::cffi::alias define GIT_INDEX_ENTRY_FLAG_T {int {enum git_index_entry_flag_t}}
proc GIT_INDEX_ENTRY_STAGE {index_entry} {
    # (((E)->flags & GIT_INDEX_ENTRY_STAGEMASK) >> GIT_INDEX_ENTRY_STAGESHIFT)
    expr {([dict get $index_entry flags] & 0x3000) >> 12}
}
proc GIT_INDEX_ENTRY_STAGE_SET {index_entry_var stage} {
    uplevel 1 $index_entry_var index_entry
    dict set index_entry flags \
        [expr {([dict get $index_entry flags] & ~0x3000) | ((stage & 0x03) << 12)}]
}

::cffi::enum define git_index_entry_extended_flag_t {
    GIT_INDEX_ENTRY_INTENT_TO_ADD  0x2000
    GIT_INDEX_ENTRY_SKIP_WORKTREE  0x4000
    GIT_INDEX_ENTRY_EXTENDED_FLAGS 0x6000
    GIT_INDEX_ENTRY_UPTODATE       0x0004
}
::cffi::alias define GIT_INDEX_ENTRY_EXTENDED_FLAG_T {int {enum git_index_entry_extended_flag_t}}


::cffi::enum define git_index_capability_t {
    GIT_INDEX_CAPABILITY_IGNORE_CASE 1
    GIT_INDEX_CAPABILITY_NO_FILEMODE 2
    GIT_INDEX_CAPABILITY_NO_SYMLINKS 4
    GIT_INDEX_CAPABILITY_FROM_OWNER -1
}
::cffi::alias define GIT_INDEX_CAPABILITY_T {int {enum git_index_capability_t}}

::cffi::prototype function git_index_matched_path_cb int {
    path             STRING
    matched_pathspec STRING
    payload          CB_PAYLOAD
}

::cffi::enum define git_index_add_option_t {
    GIT_INDEX_ADD_DEFAULT                0
    GIT_INDEX_ADD_FORCE                  1
    GIT_INDEX_ADD_DISABLE_PATHSPEC_MATCH 2
    GIT_INDEX_ADD_CHECK_PATHSPEC         4
}
::cffi::alias define GIT_INDEX_ADD_OPTION_T {int {enum git_index_add_option_t}}

::cffi::enum define git_index_state_t {
    GIT_INDEX_STAGE_ANY      -1
    GIT_INDEX_STAGE_NORMAL   0
    GIT_INDEX_STAGE_ANCESTOR 1
    GIT_INDEX_STAGE_OURS     2
    GIT_INDEX_STAGE_THEIRS   3
}
::cffi::alias define GIT_INDEX_STATE_T {int {enum git_index_state_t}}

libgit2 functions {
    git_index_open GIT_ERROR_CODE {
        index      {PINDEX retval}
        index_path STRING
    }
    git_index_new GIT_ERROR_CODE {
        index {PINDEX retval}
    }
    git_index_free void {
        index {PINDEX nullok dispose}
    }
    git_index_owner PREPOSITORY {
        index PINDEX
    }
    git_index_caps {GIT_INDEX_CAPABILITY_T bitmask} {
        index PINDEX
    }
    git_index_set_caps GIT_ERROR_CODE {
        index PINDEX
        caps  int
    }
    git_index_version int {
        index PINDEX
    }
    git_index_set_version GIT_ERROR_CODE {
        index   PINDEX
        version uint
    }
    git_index_read GIT_ERROR_CODE {
        index PINDEX
        force int
    }
    git_index_write GIT_ERROR_CODE {
        index PINDEX
    }
    git_index_path STRING {
        index PINDEX
    }
    git_index_checksum {struct.git_oid byref} {
        index PINDEX
    }
    git_index_read_tree GIT_ERROR_CODE {
        index PINDEX
        tree  PTREE
    }
    git_index_write_tree GIT_ERROR_CODE {
        id {struct.git_oid retval}
        index PINDEX
    }
    git_index_write_tree_to GIT_ERROR_CODE {
        id {struct.git_oid retval}
        index PINDEX
        pRepo PREPOSITORY
    }
    git_index_entrycount size_t {
        index PINDEX
    }
    git_index_clear GIT_ERROR_CODE {
        index PINDEX
    }
    git_index_get_byindex {struct.git_index_entry byref} {
        index PINDEX
        n     size_t
    }
    git_index_get_bypath {struct.git_index_entry byref} {
        index PINDEX
        path  STRING
        stage int
    }
    git_index_remove GIT_ERROR_CODE {
        index PINDEX
        path  STRING
        stage int
    }
    git_index_remove_directory GIT_ERROR_CODE {
        index PINDEX
        dir   STRING
        stage int
    }
    git_index_add GIT_ERROR_CODE {
        index PINDEX
        entry {struct.git_index_entry byref}
    }
    git_index_entry_stage int {
        entry {struct.git_index_entry byref}
    }
    git_index_entry_is_conflict int {
        entry {struct.git_index_entry byref}
    }
    git_index_iterator_new GIT_ERROR_CODE {
        iterator {PINDEX_ITERATOR retval}
        index    PINDEX
    }
    git_index_iterator_next GIT_ITER_ERROR_CODE {
        index_entry {pointer.git_index_entry unsafe out}
        iterator PINDEX_ITERATOR
    }
    git_index_iterator_free void {
        iterator {PINDEX_ITERATOR nullok dispose}
    }
    git_index_add_bypath GIT_ERROR_CODE {
        index PINDEX
        path  STRING
    }
    git_index_add_from_buffer GIT_ERROR_CODE {
        index PINDEX
        entry {struct.git_index_entry byref}
        buffer binary
        len    size_t
    }
    git_index_remove_bypath GIT_ERROR_CODE {
        index PINDEX
        path  STRING
    }
    git_index_add_all GIT_ERROR_CODE {
        index    PINDEX
        pathspec PSTRARRAYIN
        flags    {GIT_INDEX_ADD_OPTION_T bitmask}
        callback {pointer.git_index_matched_path_cb nullok}
        payload  CB_PAYLOAD
    }
    git_index_remove_all GIT_ERROR_CODE {
        index    PINDEX
        pathspec PSTRARRAYIN
        callback {pointer.git_index_matched_path_cb nullok}
        payload  CB_PAYLOAD
    }
    git_index_update_all GIT_ERROR_CODE {
        index    PINDEX
        pathspec PSTRARRAYIN
        callback {pointer.git_index_matched_path_cb nullok}
        payload  CB_PAYLOAD
    }
    git_index_find GIT_ERROR_CODE {
        at_pos {size_t retval}
        index  PINDEX
        path   STRING
    }
    git_index_find_prefix GIT_ERROR_CODE {
        at_pos {size_t retval}
        index  PINDEX
        prefix STRING
    }
    git_index_conflict_add GIT_ERROR_CODE {
        index          PINDEX
        ancestor_entry {struct.git_index_entry byref nullifempty}
        our_entry      {struct.git_index_entry byref nullifempty}
        their_entry    {struct.git_index_entry byref nullifempty}
    }
    git_index_conflict_get GIT_ERROR_CODE {
        pAncestor {pointer.git_index_entry out unsafe}
        pOur      {pointer.git_index_entry out unsafe}
        pTheir    {pointer.git_index_entry out unsafe}
        index     PINDEX
        path      STRING
    }
    git_index_conflict_remove GIT_ERROR_CODE {
        index PINDEX
        path  STRING
    }
    git_index_conflict_cleanup GIT_ERROR_CODE {
        index PINDEX
    }
    git_index_has_conflicts int {
        index PINDEX
    }
    git_index_conflict_iterator_new GIT_ERROR_CODE {
        iterator {PINDEX_CONFLICT_ITERATOR retval}
        index PINDEX
    }
    git_index_conflict_next GIT_ITER_ERROR_CODE {
        pAncestor {pointer.git_index_entry out unsafe}
        pOur      {pointer.git_index_entry out unsafe}
        pTheir    {pointer.git_index_entry out unsafe}
        iterator  PINDEX_CONFLICT_ITERATOR
    }
    git_index_conflict_iterator_free void {
        iterator {PINDEX_CONFLICT_ITERATOR nullok dispose}
    }
}
