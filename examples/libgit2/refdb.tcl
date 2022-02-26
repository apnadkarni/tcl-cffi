# Based on libgit2/include/refdb.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

libgit2 functions {
    git_refdb_new GIT_ERROR_CODE {
        refdb {PREFDB retval}
        pRepo PREPOSITORY
    }
    git_refdb_open GIT_ERROR_CODE {
        refdb {PREFDB retval}
        pRepo PREPOSITORY
    }
    git_refdb_compress GIT_ERROR_CODE {
        refdb PREFDB
    }
    git_refdb_free void {
        refdb {PREFDB nullok dispose}
    }
}
