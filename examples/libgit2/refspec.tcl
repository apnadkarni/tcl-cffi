# Based on libgit2/include/refspec.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

libgit2 functions {
    git_refspec_parse GIT_ERROR_CODE {
        pRefSpec {PREFSPEC retval}
        input    STRING
        is_fetch int
    }
    git_refspec_free void {
        pRefSpec {PREFSPEC dispose}
    }
    git_refspec_src STRING {
        pRefSpec PREFSPEC
    }
    git_refspec_dst STRING {
        pRefSpec PREFSPEC
    }
    git_refspec_string STRING {
        pRefSpec PREFSPEC
    }
    git_refspec_force int {
        pRefSpec PREFSPEC
    }
    git_refspec_direction GIT_DIRECTION {
        pRefSpec PREFSPEC
    }
    git_refspec_src_matches int {
        refSpecP PREFSPEC
        refName  STRING
    }
    git_refspec_dst_matches int {
        refSpecP PREFSPEC
        refName  STRING
    }
    git_refspec_transform GIT_ERROR_CODE {
        pBuf     PBUF
        pRefSpec PREFSPEC
        name     STRING
    }
    git_refspec_rtransform GIT_ERROR_CODE {
        pBuf     PBUF
        pRefSpec PREFSPEC
        name     STRING
    }
}
