# Based on libgit2/include/pathspec.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::alias define PPATHSPEC {pointer.git_pathspec counted}
::cffi::alias define PPATHSPEC_MATCH_LIST pointer.git_pathspec_match_list

::cffi::enum define git_pathspec_flag_t {
    GIT_PATHSPEC_DEFAULT        0
    GIT_PATHSPEC_IGNORE_CASE    1
    GIT_PATHSPEC_USE_CASE       2
    GIT_PATHSPEC_NO_GLOB        4
    GIT_PATHSPEC_NO_MATCH_ERROR 8
    GIT_PATHSPEC_FIND_FAILURES  16
    GIT_PATHSPEC_FAILURES_ONLY  32
}
::cffi::alias define GIT_PATHSPEC_FLAG_T {int {enum git_pathspec_flag_t} bitmask}

libgit2 functions {
    git_pathspec_new GIT_ERROR_CODE {
        ps    {PPATHSPEC retval}
        paths PSTRARRAYIN
    }
    git_pathspec_free void {
        ps {PPATHSPEC nullok dispose}
    }
    git_pathspec_matches_path int {
        ps    PPATHSPEC
        flags GIT_PATHSPEC_FLAG_T
        path  STRING
    }
    git_pathspec_match_workdir GIT_ERROR_CODE {
        psmatches {PPATHSPEC_MATCH_LIST retval}
        pRepo     PREPOSITORY
        flags     GIT_PATHSPEC_FLAG_T
        ps        PPATHSPEC
    }
    git_pathspec_match_index GIT_ERROR_CODE {
        psmatches {PPATHSPEC_MATCH_LIST retval}
        index     PINDEX
        flags     GIT_PATHSPEC_FLAG_T
        ps        PPATHSPEC
    }
    git_pathspec_match_tree GIT_ERROR_CODE {
        psmatches {PPATHSPEC_MATCH_LIST retval}
        tree      PTREE
        flags     GIT_PATHSPEC_FLAG_T
        ps        PPATHSPEC
    }
    git_pathspec_match_diff GIT_ERROR_CODE {
        psmatches {PPATHSPEC_MATCH_LIST retval}
        diff      PDIFF
        flags     GIT_PATHSPEC_FLAG_T
        ps        PPATHSPEC
    }
    git_pathspec_match_list_free void {
        psmatches {PPATHSPEC_MATCH_LIST nullok dispose}
    }
    git_pathspec_match_list_entrycount size_t {
        psmatches PPATHSPEC_MATCH_LIST
    }
    git_pathspec_match_list_entry STRING {
        psmatches PPATHSPEC_MATCH_LIST
        pos       size_t
    }
    git_pathspec_match_list_diff_entry {struct.git_diff_delta byref} {
        psmatches PPATHSPEC_MATCH_LIST
        pos       size_t
    }
    git_pathspec_match_list_failed_entrycount size_t {
        psmatches PPATHSPEC_MATCH_LIST
    }
    git_pathspec_match_list_failed_entry STRING {
        psmatches PPATHSPEC_MATCH_LIST
        pos       size_t
    }
}
