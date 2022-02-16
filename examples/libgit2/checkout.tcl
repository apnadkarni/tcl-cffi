# Based on libgit2/include/checkout.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum define git_checkout_strategy_t {
    GIT_CHECKOUT_NONE                    0x0000000
    GIT_CHECKOUT_SAFE                    0x0000001
    GIT_CHECKOUT_FORCE                   0x0000002
    GIT_CHECKOUT_RECREATE_MISSING        0x0000004
    GIT_CHECKOUT_ALLOW_CONFLICTS         0x0000010
    GIT_CHECKOUT_REMOVE_UNTRACKED        0x0000020
    GIT_CHECKOUT_REMOVE_IGNORED          0x0000040
    GIT_CHECKOUT_UPDATE_ONLY             0x0000080
    GIT_CHECKOUT_DONT_UPDATE_INDEX       0x0000100
    GIT_CHECKOUT_NO_REFRESH              0x0000200
    GIT_CHECKOUT_SKIP_UNMERGED           0x0000400
    GIT_CHECKOUT_USE_OURS                0x0000800
    GIT_CHECKOUT_USE_THEIRS              0x0001000
    GIT_CHECKOUT_DISABLE_PATHSPEC_MATCH  0x0002000
    GIT_CHECKOUT_SKIP_LOCKED_DIRECTORIES 0x0040000
    GIT_CHECKOUT_DONT_OVERWRITE_IGNORED  0x0080000
    GIT_CHECKOUT_CONFLICT_STYLE_MERGE    0x0100000
    GIT_CHECKOUT_CONFLICT_STYLE_DIFF3    0x0200000
    GIT_CHECKOUT_DONT_REMOVE_EXISTING    0x0400000
    GIT_CHECKOUT_DONT_WRITE_INDEX        0x0800000
    GIT_CHECKOUT_DRY_RUN                 0x1000000
}
::cffi::alias define GIT_CHECKOUT_STRATEGY_T {int {enum git_checkout_strategy_t}}

::cffi::enum define git_checkout_notify_t {
    GIT_CHECKOUT_NOTIFY_NONE       0x00000
    GIT_CHECKOUT_NOTIFY_CONFLICT   0x00001
    GIT_CHECKOUT_NOTIFY_DIRTY      0x00002
    GIT_CHECKOUT_NOTIFY_UPDATED    0x00004
    GIT_CHECKOUT_NOTIFY_UNTRACKED  0x00008
    GIT_CHECKOUT_NOTIFY_IGNORED    0x00010
    GIT_CHECKOUT_NOTIFY_ALL        0x0FFFF
}
::cffi::alias define GIT_CHECKOUT_NOTIFY_T {int {enum git_checkout_notify_t}}

::cffi::Struct create git_checkout_perfdata {
    mkdir_calls size_t
    stat_calls  size_t
    chmod_calls size_t
}

::cffi::prototype function git_checkout_notify_cb int {
    why GIT_CHECKOUT_NOTIFY_T
    path {STRING nullok}
    baseline {struct.git_diff_file byref}
    target {struct.git_diff_file byref}
    workdir {struct.git_diff_file byref}
    payload {pointer unsafe}
}

::cffi::prototype function git_checkout_progress_cb void {
    path {STRING nullok}
    completed_steps size_t
    total_steps size_t
    payload {pointer unsafe}
}

::cffi::prototype function git_checkout_perfdata_cb void {
    perfdata {struct.git_checkout_perfdata byref}
    payload {pointer unsafe}
}

::cffi::Struct create git_checkout_options {
    version           uint
    checkout_strategy GIT_CHECKOUT_STRATEGY_T
    disable_filters   int
    dir_mode          uint
    file_mode         uint
    file_open_flags   int
    notify_flags      int
    notify_cb         {pointer.git_checkout_notify_cb   nullok}
    notify_payload    CB_PAYLOAD
    progress_cb       {pointer.git_checkout_progress_cb nullok}
    progress_payload  CB_PAYLOAD
    paths             struct.git_strarray
    pBaseline         {PTREE nullok}
    pBaselineIndex    {PINDEX nullok}
    target_directory  {STRING nullifempty}
    ancestor_label    {STRING nullifempty}
    our_label         {STRING nullifempty}
    their_label       {STRING nullifempty}
    perfdata_cb       {pointer.git_checkout_perfdata_cb nullok}
    perfdata_payload  CB_PAYLOAD
}

# Notes
# git_checkout_tree pTreeish is untyped because it takes several
# different types of objects

libgit2 functions {
    git_checkout_options_init int {
        pOpts   {struct.git_checkout_options out}
        version {uint {default 1}}
    }
    git_checkout_head GIT_ERROR_CODE {
        pRepo PREPOSITORY
        opts  {struct.git_checkout_options byref}
    }
    git_checkout_index GIT_ERROR_CODE {
        pRepo PREPOSITORY
        pIndex PINDEX
        opts  {struct.git_checkout_options byref}
    }
    git_checkout_tree GIT_ERROR_CODE {
        pRepo PREPOSITORY
        pTreeish pointer
        opts  {struct.git_checkout_options byref}
    }
}
