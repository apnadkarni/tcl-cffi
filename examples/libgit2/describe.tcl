# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/describe.tcl
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum sequence git_describe_strategy_t {
    GIT_DESCRIBE_DEFAULT
    GIT_DESCRIBE_TAGS
    GIT_DESCRIBE_ALL
}
::cffi::alias define GIT_DESCRIBE_STRATEGY_T {int {enum git_describe_strategy_t}}

::cffi::Struct create git_describe_options {
    version                     {uint {default 1}}
    max_candidates_tags         {uint {default 10}}
    describe_strategy           GIT_DESCRIBE_STRATEGY_T
    pattern                     {STRING nullifempty}
    only_follow_first_pattern   int
    show_commit_oid_as_fallback int
}
::cffi::Struct create git_describe_format_options {
    version                {uint {default 1}}
    abbreviated_size       {uint {default 7}}
    always_use_long_format int
    dirty_suffix           {STRING nullifempty}
}

::cffi::alias define PDESCRIBE_RESULT pointer.git_describe_result

libgit2 functions {
    git_describe_options_init GIT_ERROR_CODE {
        opts    {struct.git_describe_options retval}
        version {uint {default 1}}
    }
    git_describe_format_options_init GIT_ERROR_CODE {
        opts    {struct.git_describe_format_options retval}
        version {uint {default 1}}
    }
    git_describe_commit GIT_ERROR_CODE {
        result {PDESCRIBE_RESULT retval}
        committish POBJECT
        opts {struct.git_describe_options byref}
    }
    git_describe_workdir GIT_ERROR_CODE {
        result {PDESCRIBE_RESULT retval}
        pRepo PREPOSITORY
        opts {struct.git_describe_options byref}
    }
    git_describe_format GIT_ERROR_CODE {
        buf PBUF
        result PDESCRIBE_RESULT
        opts {struct.git_describe_format_options byref}
    }
    git_describe_result_free void {
        result {PDESCRIBE_RESULT nullok dispose}
    }
}

