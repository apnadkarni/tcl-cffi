# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/email.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum define git_email_create_flags_t {
    GIT_EMAIL_CREATE_DEFAULT       0
    GIT_EMAIL_CREATE_OMIT_NUMBERS  1
    GIT_EMAIL_CREATE_ALWAYS_NUMBER 2
    GIT_EMAIL_CREATE_NO_RENAMES    4
}
::cffi::alias define GIT_EMAIL_CREATE_FLAGS_T {int {enum git_email_create_flags_t} bitmask}

::cffi::Struct create git_email_create_options {
    version        {uint {default 1}}
    flags          GIT_EMAIL_CREATE_FLAGS_T
    diff_opts      struct.git_diff_options
    diff_find_opts struct.git_diff_find_options
    subject_prefix STRING
    start_number   size_t
    reroll_number  size_t
}

libgit2 functions {
    git_email_create_from_diff GIT_ERROR_CODE {
        buf PBUF
        diff PDIFF
        patch_idx size_t
        patch_count size_t
        commit_id {struct.git_oid byref}
        summary STRING
        body STRING
        author {struct.git_signature byref}
        opts {struct.git_email_create_options byref}
    }
    git_email_create_from_commit GIT_ERROR_CODE {
        buf PBUF
        commit PCOMMIT
        opts {struct.git_email_create_options byref}
    }
}

