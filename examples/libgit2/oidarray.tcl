# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/oidarray.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_oidarray {
    ids   pointer.git_oid
    count size_t
}

libgit2 function git_oidarray_dispose void {
    array pointer.git_oidarray
}
