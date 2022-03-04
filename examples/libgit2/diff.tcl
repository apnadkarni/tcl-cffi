# Based on libgit2/include/diff.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::alias define PDIFF {pointer.git_diff counted}
::cffi::alias define PDIFF_STATS pointer.git_diff_stats

::cffi::enum define git_diff_option_t {
    GIT_DIFF_NORMAL                          0x00000000
    GIT_DIFF_REVERSE                         0x00000001
    GIT_DIFF_INCLUDE_IGNORED                 0x00000002
    GIT_DIFF_RECURSE_IGNORED_DIRS            0x00000004
    GIT_DIFF_INCLUDE_UNTRACKED               0x00000008
    GIT_DIFF_RECURSE_UNTRACKED_DIRS          0x00000010
    GIT_DIFF_INCLUDE_UNMODIFIED              0x00000020
    GIT_DIFF_INCLUDE_TYPECHANGE              0x00000040
    GIT_DIFF_INCLUDE_TYPECHANGE_TREES        0x00000080
    GIT_DIFF_IGNORE_FILEMODE                 0x00000100
    GIT_DIFF_IGNORE_SUBMODULES               0x00000200
    GIT_DIFF_IGNORE_CASE                     0x00000400
    GIT_DIFF_INCLUDE_CASECHANGE              0x00000800
    GIT_DIFF_DISABLE_PATHSPEC_MATCH          0x00001000
    GIT_DIFF_SKIP_BINARY_CHECK               0x00002000
    GIT_DIFF_ENABLE_FAST_UNTRACKED_DIRS      0x00004000
    GIT_DIFF_UPDATE_INDEX                    0x00008000
    GIT_DIFF_INCLUDE_UNREADABLE              0x00010000
    GIT_DIFF_INCLUDE_UNREADABLE_AS_UNTRACKED 0x00020000
    GIT_DIFF_INDENT_HEURISTIC                0x00040000
    GIT_DIFF_IGNORE_BLANK_LINES              0x00080000
    GIT_DIFF_FORCE_TEXT                      0x00100000
    GIT_DIFF_FORCE_BINARY                    0x00200000
    GIT_DIFF_IGNORE_WHITESPACE               0x00400000
    GIT_DIFF_IGNORE_WHITESPACE_CHANGE        0x00800000
    GIT_DIFF_IGNORE_WHITESPACE_EOL           0x01000000
    GIT_DIFF_SHOW_UNTRACKED_CONTENT          0x02000000
    GIT_DIFF_SHOW_UNMODIFIED                 0x04000000
    GIT_DIFF_PATIENCE                        0x10000000
    GIT_DIFF_MINIMAL                         0x20000000
    GIT_DIFF_SHOW_BINARY                     0x40000000
}
::cffi::alias define GIT_DIFF_OPTION_T {int {enum git_diff_option_t} bitmask}


::cffi::enum flags git_diff_flag_t {
    GIT_DIFF_FLAG_BINARY
    GIT_DIFF_FLAG_NOT_BINARY
    GIT_DIFF_FLAG_VALID_ID
    GIT_DIFF_FLAG_EXISTS
}
::cffi::alias define GIT_DIFF_FLAG_T {int {enum git_diff_flag_t} bitmask}

::cffi::enum define git_delta_t {
    GIT_DELTA_UNMODIFIED  0
    GIT_DELTA_ADDED       1
    GIT_DELTA_DELETED     2
    GIT_DELTA_MODIFIED    3
    GIT_DELTA_RENAMED     4
    GIT_DELTA_COPIED      5
    GIT_DELTA_IGNORED     6
    GIT_DELTA_UNTRACKED   7
    GIT_DELTA_TYPECHANGE  8
    GIT_DELTA_UNREADABLE  9
    GIT_DELTA_CONFLICTED  10
}
::cffi::alias define GIT_DELTA_T {int {enum git_delta_t}}

::cffi::enum sequence git_diff_binary_t {
    GIT_DIFF_BINARY_NONE
    GIT_DIFF_BINARY_LITERAL
    GIT_DIFF_BINARY_DELTA
}
::cffi::alias define GIT_DIFF_BINARY_T {int {enum git_diff_binary_t}}

::cffi::enum define git_diff_stats_format_t {
    GIT_DIFF_STATS_NONE            0
    GIT_DIFF_STATS_FULL            1
    GIT_DIFF_STATS_SHORT           2
    GIT_DIFF_STATS_NUMBER          4
    GIT_DIFF_STATS_INCLUDE_SUMMARY 8
}
::cffi::alias define GIT_DIFF_STATS_FORMAT_T {int {enum git_diff_stats_format_t} bitmask}

::cffi::enum define git_diff_format_t {
    GIT_DIFF_FORMAT_PATCH        1
    GIT_DIFF_FORMAT_PATCH_HEADER 2
    GIT_DIFF_FORMAT_RAW          3
    GIT_DIFF_FORMAT_NAME_ONLY    4
    GIT_DIFF_FORMAT_NAME_STATUS  5
    GIT_DIFF_FORMAT_PATCH_ID     6
}
::cffi::alias define GIT_DIFF_FORMAT_T {int {enum git_diff_format_t}}

# NOTE: mode cannot be GIT_FILEMODE_T because that is an int.
# Explicitly use enum git_filemode_t.
::cffi::Struct create git_diff_file {
    id         struct.git_oid
    path       STRING
    size       git_object_size_t
    flags      uint32_t
    mode       {uint16_t {enum git_filemode_t}}
    id_abbrev  uint16_t
}

::cffi::Struct create git_diff_delta {
    status     GIT_DELTA_T
    flags      uint32_t
    similarity uint16_t
    nfiles     uint16_t
    old_file   struct.git_diff_file
    new_file   struct.git_diff_file
} -clear

::cffi::prototype function git_diff_notify_cb int {
    diff_so_far      PDIFF
    delta_to_add     {struct.git_diff_delta byref}
    matched_pathspec STRING
    payload          CB_PAYLOAD
}

::cffi::prototype function git_diff_progress_cb int {
    diff_so_far PDIFF
    old_path    STRING
    new_path    STRING
    payload     {pointer unsafe}
}

::cffi::prototype function git_diff_file_cb int {
    delta    {struct.git_diff_delta byref}
    progress float
    payload  CB_PAYLOAD
}

::cffi::Struct create git_diff_options {
    version            int
    flags              uint32_t
    ignore_submodules  GIT_SUBMODULE_IGNORE_T
    pathspec           struct.lg2_strarray
    notify_cb          {pointer.git_diff_notify_cb    nullok {default NULL}}
    progress_cb        {pointer.git_diff_progress_cb  nullok {default NULL}}
    payload            CB_PAYLOAD
    context_lines      uint32_t
    interhunk_lines    uint32_t
    id_abbrev          uint16_t
    max_size           git_off_t
    old_prefix         STRING
    new_prefix         STRING
} -clear

::cffi::Struct create git_diff_patchid_options {
    version uint
}

::cffi::Struct create git_diff_binary_file {
    type        GIT_DIFF_BINARY_T
    data        {pointer unsafe}
    datalen     size_t
    inflatedlen size_t
}

::cffi::Struct create git_diff_binary {
    contains_data uint
    old_file struct.git_diff_binary_file
    new_file struct.git_diff_binary_file
}

::cffi::prototype function git_diff_binary_cb int {
    delta {struct.git_diff_delta byref}
    binary {struct.git_diff_binary byref}
    payload CB_PAYLOAD
}

::cffi::Struct create git_diff_hunk {
    old_start int
    old_lines int
    new_start int
    new_lines int
    header_len size_t
    header chars.utf-8[128]
} -clear

::cffi::prototype function git_diff_hunk_cb int {
    delta {struct.git_diff_delta byref}
    hunk  {struct.git_diff_hunk byref}
    payload CB_PAYLOAD
}

::cffi::enum define git_diff_line_t {
    GIT_DIFF_LINE_CONTEXT   32
    GIT_DIFF_LINE_ADDITION  43
    GIT_DIFF_LINE_DELETION  45
    GIT_DIFF_LINE_CONTEXT_EOFNL 61
    GIT_DIFF_LINE_ADD_EOFNL 62
    GIT_DIFF_LINE_DEL_EOFNL 60
    GIT_DIFF_LINE_FILE_HDR  70
    GIT_DIFF_LINE_HUNK_HDR  72
    GIT_DIFF_LINE_BINARY    66
}
# Do not define above enum as alias as it is not always used as int

::cffi::Struct create git_diff_line {
    origin         {uchar {enum git_diff_line_t}}
    old_lineno     int
    new_lineno     int
    num_lines      int
    content_len    size_t
    content_offset git_off_t
    content        {pointer unsafe}
} -clear

::cffi::prototype function git_diff_line_cb int {
    delta {struct.git_diff_delta byref nullok}
    hunk  {struct.git_diff_hunk byref nullok}
    line  {struct.git_diff_line byref nullok}
    payload CB_PAYLOAD
}

::cffi::enum define git_diff_find_t {
    GIT_DIFF_FIND_BY_CONFIG                  0
    GIT_DIFF_FIND_RENAMES                    0x00001
    GIT_DIFF_FIND_RENAMES_FROM_REWRITES      0x00002
    GIT_DIFF_FIND_COPIES                     0x00004
    GIT_DIFF_FIND_COPIES_FROM_UNMODIFIED     0x00008
    GIT_DIFF_FIND_REWRITES                   0x00010
    GIT_DIFF_BREAK_REWRITES                  0x00020
    GIT_DIFF_FIND_AND_BREAK_REWRITES         0x00030
    GIT_DIFF_FIND_FOR_UNTRACKED              0x00040
    GIT_DIFF_FIND_ALL                        0x000ff
    GIT_DIFF_FIND_IGNORE_LEADING_WHITESPACE  0
    GIT_DIFF_FIND_IGNORE_WHITESPACE          0x01000
    GIT_DIFF_FIND_DONT_IGNORE_WHITESPACE     0x02000
    GIT_DIFF_FIND_EXACT_MATCH_ONLY           0x04000
    GIT_DIFF_BREAK_REWRITES_FOR_RENAMES_ONLY 0x08000
    GIT_DIFF_FIND_REMOVE_UNMODIFIED          0x10000
}


cffi::prototype function file_signature_cb int {
    pPointer     {pointer unsafe}
    file     {struct.git_diff_file byref}
    fullpath STRING
    payload  CB_PAYLOAD
}

cffi::prototype function buffer_signature_cb int {
    pPointer    {pointer unsafe}
    file    {struct.git_diff_file byref}
    buf     {pointer unsafe}
    buflen  size_t
    payload CB_PAYLOAD
}

cffi::prototype function free_signature_cb void {
    pSig    {pointer unsafe}
    payload CB_PAYLOAD
}

cffi::prototype function similarity_cb int {
    score   {pointer.int unsafe}
    pSigA   {pointer unsafe}
    pSigB   {pointer unsafe}
    payload CB_PAYLOAD

}

::cffi::Struct create git_diff_similarity_metric {
    file_signature pointer.file_signature_cb
    buffer_signature pointer.buffer_signature_cb
    free_signature pointer.free_signature_cb
    similarity pointer.similarity_cb
}

::cffi::Struct create git_diff_find_options {
    version uint
    flags uint32_t
    rename_threshold uint16_t
    rename_from_write_threshold uint16_t
    copy_threshold uint16_t
    break_rewrite_threshold uint16_t
    rename_limit size_t
    metric {pointer.git_diff_similarity_metric unsafe}
}


libgit2 functions {
    git_diff_options_init GIT_ERROR_CODE {
        pOpts   {struct.git_diff_options retval}
        version {uint {default 1}}
    }
    git_diff_find_options_init GIT_ERROR_CODE {
        pOpts   {struct.git_diff_find_options retval}
        version {uint {default 1}}
    }
    git_diff_free void {
        pDiff {PDIFF nullok dispose}
    }
    git_diff_tree_to_tree GIT_ERROR_CODE {
        pDiff {PDIFF retval}
        pRepo PREPOSITORY
        pOldTree {PTREE nullok}
        pNewTree {PTREE nullok}
        pOpts {struct.git_diff_options byref}
    }
    git_diff_tree_to_index GIT_ERROR_CODE {
        pDiff {PDIFF retval}
        pRepo PREPOSITORY
        pOldTree {PTREE nullok}
        pIndex {PINDEX nullok}
        pOpts {struct.git_diff_options byref}
    }
    git_diff_index_to_workdir GIT_ERROR_CODE {
        pDiff {PDIFF retval}
        pRepo PREPOSITORY
        pIndex {PINDEX nullok}
        pOpts {struct.git_diff_options byref}
    }
    git_diff_tree_to_workdir GIT_ERROR_CODE {
        pDiff    {PDIFF retval}
        pRepo    PREPOSITORY
        pOldTree {PTREE nullok}
        pOpts    {struct.git_diff_options  byref}
    }
    git_diff_tree_to_workdir_with_index GIT_ERROR_CODE {
        pDiff    {PDIFF retval}
        pRepo    PREPOSITORY
        pOldTree {PTREE nullok}
        pOpts    {struct.git_diff_options byref}
    }
    git_diff_index_to_index GIT_ERROR_CODE {
        pDiff     {PDIFF retval}
        pRepo     PREPOSITORY
        pOldIndex PINDEX
        pNewIndex PINDEX
        pOpts     {struct.git_diff_options byref}
    }
    git_diff_merge GIT_ERROR_CODE {
        pOnto PDIFF
        pFrom PDIFF
    }
    git_diff_find_similar GIT_ERROR_CODE {
        pDiff PDIFF
        pOpts {struct.git_diff_find_options byref}
    }
    git_diff_num_deltas size_t {
        pDiff PDIFF
    }
    git_diff_num_deltas_of_type size_t {
        pDiff PDIFF
        type  GIT_DELTA_T
    }
    git_diff_get_delta {pointer.git_diff_delta unsafe} {
        pDiff PDIFF
        idx   size_t
    }
    git_diff_is_sorted_icase int {
        pDiff PDIFF
    }
    git_diff_foreach GIT_ERROR_CODE {
        pDiff PDIFF
        file_cb pointer.git_diff_file_cb
        binary_cb {pointer.git_diff_binary_cb nullok}
        hunk_cb {pointer.git_diff_hunk_cb nullok}
        line_cb {pointer.git_diff_line_cb nullok}
        payload CB_PAYLOAD
    }
    git_diff_status_char uchar {
        status GIT_DELTA_T
    }
    git_diff_print GIT_ERROR_CODE {
        pDiff    PDIFF
        format   GIT_DIFF_FORMAT_T
        print_cb {pointer.git_diff_line_cb}
        payload  CB_PAYLOAD
    }
    git_diff_to_buf GIT_ERROR_CODE {
        pBuf PBUF
        pDiff PDIFF
        format GIT_DIFF_FORMAT_T
    }
    git_diff_blobs GIT_ERROR_CODE {
        pOldBlob    {PBLOB nullok}
        old_as_path {STRING nullifempty nullok}
        pNewBlob    {PBLOB nullok}
        new_as_path {STRING nullifempty nullok}
        pOpts       {pointer.git_diff_options unsafe nullok {default NULL}}
        file_cb     {pointer.git_diff_file_cb nullok        {default NULL}}
        binary_cb   {pointer.git_diff_binary_cb nullok      {default NULL}}
        hunk_cb     {pointer.git_diff_hunk_cb nullok        {default NULL}}
        line_cb     {pointer.git_diff_line_cb nullok        {default NULL}}
        payload     CB_PAYLOAD
    }
    git_diff_blob_to_buffer GIT_ERROR_CODE {
        pOldBlob       {PBLOB nullok}
        old_as_path    {STRING nullifempty nullok}
        buffer         {pointer unsafe nullok}
        buffer_len     size_t
        buffer_as_path {STRING nullifempty nullok}
        pOpts          {pointer.git_diff_options unsafe nullok {default NULL}}
        file_cb        {pointer.git_diff_file_cb nullok        {default NULL}}
        binary_cb      {pointer.git_diff_binary_cb nullok      {default NULL}}
        hunk_cb        {pointer.git_diff_hunk_cb nullok        {default NULL}}
        line_cb        {pointer.git_diff_line_cb nullok        {default NULL}}
        payload        CB_PAYLOAD
    }
    git_diff_buffers GIT_ERROR_CODE {
        pOldBuf     {pointer unsafe nullok}
        old_len     size_t
        old_as_path {STRING nullifempty nullok}
        pNewBuf     {pointer unsafe nullok}
        new_len     size_t
        new_as_path {STRING nullifempty nullok}
        pOpts       {pointer.git_diff_options unsafe nullok {default NULL}}
        file_cb     {pointer.git_diff_file_cb nullok        {default NULL}}
        binary_cb   {pointer.git_diff_binary_cb nullok      {default NULL}}
        hunk_cb     {pointer.git_diff_hunk_cb nullok        {default NULL}}
        line_cb     {pointer.git_diff_line_cb nullok        {default NULL}}
        payload     CB_PAYLOAD
    }
    git_diff_from_buffer GIT_ERROR_CODE {
        pDiff       {PDIFF   retval}
        content     {pointer unsafe}
        content_len size_t
    }
    git_diff_get_stats GIT_ERROR_CODE {
        pStats {PDIFF_STATS retval}
        pDiff  PDIFF
    }
    git_diff_stats_files_changed size_t {
        pStats PDIFF_STATS
    }
    git_diff_stats_insertions size_t {
        pStats PDIFF_STATS
    }
    git_diff_stats_deletions size_t {
        pStats PDIFF_STATS
    }
    git_diff_stats_to_buf GIT_ERROR_CODE {
        pBuf    PBUF
        pStats  PDIFF_STATS
        format  GIT_DIFF_STATS_FORMAT_T
        width   size_t
    }
    git_diff_stats_free void {
        pStats {PDIFF_STATS nullok dispose}
    }
    git_diff_patchid_options_init GIT_ERROR_CODE {
        pOpts   {struct.git_diff_patchid_options retval}
        version uint
    }
    git_diff_patchid GIT_ERROR_CODE {
        oid {struct.git_oid retval}
        pDiff PDIFF
        pOpts {struct.git_diff_patchid_options byref}
    }
}
