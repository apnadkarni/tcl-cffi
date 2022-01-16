# Based on libgit2/include/strarray.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_strarray {
    strings {pointer.strings unsafe}
    count   size_t
} -clear

libgit2 functions {
    git_strarray_dispose void {
        array pointer.git_strarray
    }
    git_strarray_copy GIT_ERROR_CODE {
        pTarget pointer.git_strarray
        pSource pointer.git_strarray
    }
}
