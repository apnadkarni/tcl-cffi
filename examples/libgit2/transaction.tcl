# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/transaction.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

libgit2 functions {
    git_transaction_new GIT_ERROR_CODE {
        vtx   {PTRANSACTION retval}
        pRepo PREPOSITORY
    }
    git_transaction_lock_ref GIT_ERROR_CODE {
        tx      PTRANSACTION
        refname STRING
    }
    git_transaction_set_target GIT_ERROR_CODE {
        tx      PTRANSACTION
        refname STRING
        target  {struct.git_oid byref}
        sig     {struct.git_signature byref nullifempty}
        msg     STRING
    }
    git_transaction_set_symbolic_target GIT_ERROR_CODE {
        tx      PTRANSACTION
        refname STRING
        target  STRING
        sig     {struct.git_signature byref nullifempty}
        msg     STRING
    }
    git_transaction_set_reflog GIT_ERROR_CODE {
        tx      PTRANSACTION
        regname STRING
        reglog  PREFLOG
    }
    git_transaction_remove GIT_ERROR_CODE {
        tx      PTRANSACTION
        refname STRING
    }
    git_transaction_commit GIT_ERROR_CODE {
        tx PTRANSACTION
    }
    git_transaction_free void {
        tx {PTRANSACTION nullok dispose}
    }
}
