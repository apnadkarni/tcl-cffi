# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/reset.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum sequence git_reset_t {
    GIT_RESET_SOFT
    GIT_RESET_MIXED
    GIT_RESET_HARD
} 1
::cffi::alias define GIT_RESET_T {int {enum git_reset_t}}

libgit2 functions {
    git_reset GIT_ERROR_CODE {
        pRepo         PREPOSITORY
        target        POBJECT
        reset_type    GIT_RESET_T
        checkout_opts {struct.git_checkout_options byref}
    }
    git_reset_from_annotated GIT_ERROR_CODE {
        pRepo         PREPOSITORY
        commit        PANNOTATED_COMMIT
        reset_type    GIT_RESET_T
        checkout_opts {struct.git_checkout_options byref}
    }
    git_reset_default GIT_ERROR_CODE {
        pRepo PREPOSITORY
        target POBJECT
        pathspecs PSTRARRAYIN
    }
}
