# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/global.h

# This file should be sourced into whatever namespace commands should
# be created in.


libgit2 functions {
    git_libgit2_init GIT_ERROR_CODE {}
    git_libgit2_shutdown int {}
}
