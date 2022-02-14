# Based on libgit2/include/mailmap.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

libgit2 functions {
    git_mailmap_new GIT_ERROR_CODE {
        mm {PMAILMAP out}
    }
    git_mailmap_free void {
        mm {PMAILMAP nullok dispose}
    }
    git_mailmap_add_entry GIT_ERROR_CODE {
        mm            PMAILMAP
        real_name     {STRING nullifempty}
        real_email    {STRING nullifempty}
        replace_name  {STRING nullifempty}
        replace_email {STRING nullifempty}
    }
    git_mailmap_from_buffer GIT_ERROR_CODE {
        mm  {PMAILMAP out}
        buf binary
        len size_t
    }
    git_mailmap_from_repository GIT_ERROR_CODE {
        mm    {PMAILMAP out}
        pRepo PREPOSITORY
    }
    git_mailmap_resolve GIT_ERROR_CODE {
        real_name  {STRING   out}
        real_email {STRING   out}
        mm         {PMAILMAP nullok}
        name       STRING
        email      STRING
    }
    git_mailmap_resolve_signature GIT_ERROR_CODE {
        pSig {pointer.git_signature out}
        mm   PMAILMAP
        sig  {struct.git_signature  byref}
    }
}
