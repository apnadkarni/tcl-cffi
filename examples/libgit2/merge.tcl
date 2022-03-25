# Based on libgit2/include/merge.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_merge_file_input {
    version {uint {default 1}}
    ptr     {pointer unsafe}
    size    size_t
    path    {STRING  nullifempty}
    mode    uint
}

::cffi::enum flags git_merge_flag_t {
    GIT_MERGE_FIND_RENAMES
    GIT_MERGE_FAIL_ON_CONFLICT
    GIT_MERGE_SKIP_REUC
    GIT_MERGE_NO_RECURSIVE
    GIT_MERGE_VIRTUAL_BASE
}
::cffi::alias define GIT_MERGE_FLAG_T {int {enum git_merge_flag_t} bitmask}

::cffi::enum sequence git_merge_file_favor_t {
    GIT_MERGE_FILE_FAVOR_NORMAL
    GIT_MERGE_FILE_FAVOR_OURS
    GIT_MERGE_FILE_FAVOR_THEIRS
    GIT_MERGE_FILE_FAVOR_UNION
}
::cffi::alias define GIT_MERGE_FILE_FAVOR_T {int {enum git_merge_file_favor_t}}

::cffi::enum define git_merge_file_flag_t {
    GIT_MERGE_FILE_DEFAULT                  0
    GIT_MERGE_FILE_STYLE_MERGE              1
    GIT_MERGE_FILE_STYLE_DIFF3              2
    GIT_MERGE_FILE_SIMPLIFY_ALNUM           4
    GIT_MERGE_FILE_IGNORE_WHITESPACE        8
    GIT_MERGE_FILE_IGNORE_WHITESPACE_CHANGE 16
    GIT_MERGE_FILE_IGNORE_WHITESPACE_EOL    32
    GIT_MERGE_FILE_DIFF_PATIENCE            64
    GIT_MERGE_FILE_DIFF_MINIMAL             128
    GIT_MERGE_FILE_STYLE_ZDIIF3             256
    GIT_MERGE_FILE_ACCEPT_CONFLICTS         512
}
::cffi::alias define GIT_MERGE_FILE_FLAG_T {int {enum git_merge_file_flag_t} bitmask}

::cffi::enum define git_merge_analysis_t {
    GIT_MERGE_ANALYSIS_NONE        0
    GIT_MERGE_ANALYSIS_NORMAL      1
    GIT_MERGE_ANALYSIS_UP_TO_DATE  2
    GIT_MERGE_ANALYSIS_FASTFORWARD 4
    GIT_MERGE_ANALYSIS_UNBORN      8
}
::cffi::alias define GIT_MERGE_ANALYSIS_T {int {enum git_merge_analysis_t} bitmask}

::cffi::enum define git_merge_preference_t {
    GIT_MERGE_PREFERENCE_NONE             0
    GIT_MERGE_PREFERENCE_NO_FASTFORWARD   1
    GIT_MERGE_PREFERENCE_FASTFORWARD_ONLY 2
}
::cffi::alias define GIT_MERGE_PREFERENCE_T {int {enum git_merge_preference_t} bitmask}

::cffi::Struct create git_merge_file_options {
    version        {uint {default 1}}
    ancestor_label STRING
    our_label      STRING
    their_label    STRING
    favor          GIT_MERGE_FILE_FAVOR_T
    flags          GIT_MERGE_FILE_FLAG_T
    marker_size    ushort
}

# NOTE: path and ptr are pointer, not string because they point to internal
# libgit storage that is freed by libgit.
::cffi::Struct create git_merge_file_result {
    automergeable uint
    path          {pointer unsafe}
    mode          uint
    ptr           {pointer unsafe}
    len           size_t
}

::cffi::Struct create git_merge_options {
    version          {uint {default 1}}
    flags            {GIT_MERGE_FLAG_T {default GIT_MERGE_FIND_RENAMES}}
    rename_threshold uint
    target_limit     uint
    metric           {pointer.git_diff_similarity_metric nullok}
    recursion_limit  uint
    default_driver   {STRING nullifempty}
    file_favor       GIT_MERGE_FILE_FAVOR_T
    file_flags       GIT_MERGE_FILE_FLAG_T
} -clear

libgit2 functions {
    git_merge_file_input_init GIT_ERROR_CODE {
        opts    {struct.git_merge_file_input retval}
        version uint
    }
    git_merge_file_options_init GIT_ERROR_CODE {
        opts    {struct.git_merge_file_options retval}
        version {uint {default 1}}
    }
    git_merge_options_init GIT_ERROR_CODE {
        opts    {struct.git_merge_options retval}
        version {uint {default 1}}
    }
    git_merge_analysis GIT_ERROR_CODE {
        analysis        {GIT_MERGE_ANALYSIS_T   out}
        preference      {GIT_MERGE_PREFERENCE_T out}
        pRepo           PREPOSITORY
        their_heads     pointer.git_annotated_commit[their_heads_len]
        their_heads_len size_t
    }
    git_merge_analysis_for_ref GIT_ERROR_CODE {
        analysis        {GIT_MERGE_ANALYSIS_T   out}
        preference      {GIT_MERGE_PREFERENCE_T out}
        pRepo           PREPOSITORY
        our_ref         PREFERENCE
        their_heads     pointer.git_annotated_commit[their_heads_len]
        their_heads_len size_t
    }
    git_merge_base GIT_ERROR_CODE {
        oid {struct.git_oid retval}
        pRepo PREPOSITORY
        one {struct.git_oid byref}
        two {struct.git_oid byref}
    }
    git_merge_bases GIT_ERROR_CODE {
        oids {struct.git_oidarray retval}
        pRepo PREPOSITORY
        one {struct.git_oid byref}
        two {struct.git_oid byref}
    }
    git_merge_base_many GIT_ERROR_CODE {
        oid         {struct.git_oid retval}
        pRepo       PREPOSITORY
        length      size_t
        input_array {struct.git_oid[length]}
    }
    git_merge_bases_many GIT_ERROR_CODE {
        oids        {struct.git_oidarray retval}
        pRepo       PREPOSITORY
        length      size_t
        input_array {struct.git_oid[length]}
    }
    git_merge_base_octopus GIT_ERROR_CODE {
        oid         {struct.git_oid retval}
        pRepo       PREPOSITORY
        length      size_t
        input_array {struct.git_oid[length]}
    }
    git_merge_file GIT_ERROR_CODE {
        result   {struct.git_merge_file_result  retval}
        ancestor {struct.git_merge_file_input   byref}
        ours     {struct.git_merge_file_input   byref}
        theirs   {struct.git_merge_file_input   byref}
        opts     {struct.git_merge_file_options byref}
    }
    git_merge_file_from_index GIT_ERROR_CODE {
        result   {struct.git_merge_file_result  retval}
        pRepo    PREPOSITORY
        ancestor {struct.git_index_entry        byref}
        ours     {struct.git_index_entry        byref}
        theirs   {struct.git_index_entry        byref}
        opts     {struct.git_merge_file_options byref}
    }
    git_merge_file_result_free void {
        result {struct.git_merge_file_result inout}
    }
    git_merge_trees GIT_ERROR_CODE {
        index         {PINDEX retval}
        pRepo         PREPOSITORY
        ancestor_tree PTREE
        our_tree      PTREE
        their_tree    PTREE
        opts          {struct.git_merge_options byref}
    }
    git_merge_commits GIT_ERROR_CODE {
        index        {PINDEX retval}
        pRepo        PREPOSITORY
        our_commit   PCOMMIT
        their_commit PCOMMIT
        opts         {struct.git_merge_options byref}
    }
    git_merge GIT_ERROR_CODE {
        pRepo           PREPOSITORY
        their_heads     pointer.git_annotated_commit[their_heads_len]
        their_heads_len size_t
        merge_opts      {struct.git_merge_options    byref}
        checkout_opts   {struct.git_checkout_options byref}
    }
}
