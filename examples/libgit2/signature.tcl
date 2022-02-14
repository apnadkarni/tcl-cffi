# Based on libgit2/include/signature.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

libgit2 functions {
    git_signature_new GIT_ERROR_CODE {
        vsig   {PSIGNATURE out}
        name   STRING
        email  STRING
        time   git_time_t
        offset int
    }
    git_signature_now GIT_ERROR_CODE {
        vsig   {PSIGNATURE out}
        name   STRING
        email  STRING
    }
    git_signature_default GIT_ERROR_CODE {
        vsig  {PSIGNATURE out}
        pRepo PREPOSITORY
    }
    git_signature_from_buffer GIT_ERROR_CODE {
        vsig {PSIGNATURE out}
        buf  STRING
    }
    git_signature_dup GIT_ERROR_CODE {
        vdest {PSIGNATURE out}
        sig   PSIGNATURE
    }
    git_signature_free void {
        sig {PSIGNATURE nullok dispose}
    }
}
