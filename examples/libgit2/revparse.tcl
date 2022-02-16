# Based on libgit2/include/revparse.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum flags git_revspec_t {
    GIT_REVSPEC_SINGLE
    GIT_REVSPEC_RANGE
    GIT_REVSPEC_MERGE_BASE
}
::cffi::alias define GIT_REVSPEC_T {int {enum git_revspec_t} bitmask}

::cffi::Struct create git_revspec {
    from POBJECT
    to   POBJECT
    flags GIT_REVSPEC_T
}
libgit2 functions {
    git_revparse_single GIT_ERROR_CODE {
        obj   {POBJECT out}
        pRepo PREPOSITORY
        spec  STRING
    }
    git_revparse_ext GIT_ERROR_CODE {
        obj   {POBJECT    out}
        ref   {PREFERENCE nullok out}
        pRepo PREPOSITORY
        spec  STRING
    }
    git_revparse GIT_ERROR_CODE {
        revspec {struct.git_revspec out}
        pRepo   PREPOSITORY
        spec    STRING
    }
}
