# Based on libgit2/include/commit.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

# NOTES:
# git_commit_message and similar returns pointer not string because the encoding
# is not known in advance. Application needs to explicitly decode using [memory
# tostring!] after retrieving encoding with git_commit_message_encoding
libgit2 functions {
    git_commit_lookup GIT_ERROR_CODE {
        commit {PCOMMIT        retval}
        pRepo  PREPOSITORY
        id     {struct.git_oid byref}
    }
    git_commit_lookup_prefix GIT_ERROR_CODE {
        commit {PCOMMIT        retval}
        pRepo  PREPOSITORY
        id     {struct.git_oid byref}
        len    size_t
    }
    git_commit_free void {
        commit {PCOMMIT nullok dispose}
    }
    git_commit_id {struct.git_oid byref} {
        commit PCOMMIT
    }
    git_commit_owner {PREPOSITORY unsafe} {
        commit PCOMMIT
    }
    git_commit_message_encoding {STRING nullok} {
        commit PCOMMIT
    }
    git_commit_message {pointer unsafe nullok} {
        commit PCOMMIT
    }
    git_commit_message_raw {pointer unsafe nullok} {
        commit PCOMMIT
    }
    git_commit_summary {pointer unsafe nullok} {
        commit PCOMMIT
    }
    git_commit_body {pointer unsafe nullok} {
        commit PCOMMIT
    }
    git_commit_time git_time_t {
        commit PCOMMIT
    }
    git_commit_time_offset int {
        commit PCOMMIT
    }
    git_commit_committer {struct.git_signature byref nullok} {
        commit PCOMMIT
    }
    git_commit_author {struct.git_signature byref nullok} {
        commit PCOMMIT
    }
    git_commit_committer_with_mailmap GIT_ERROR_CODE {
        pSig    {pointer.git_signature retval}
        commit  PCOMMIT
        mailmap PMAILMAP
    }
    git_commit_author_with_mailmap GIT_ERROR_CODE {
        pSig    {pointer.git_signature retval}
        commit  PCOMMIT
        mailmap PMAILMAP
    }
    git_commit_raw_header STRING {
        commit PCOMMIT
    }
    git_commit_tree GIT_ERROR_CODE {
        tree   {PTREE retval}
        commit PCOMMIT
    }
    git_commit_tree_id {struct.git_oid byref} {
        commit PCOMMIT
    }
    git_commit_parentcount uint {
        commit PCOMMIT
    }
    git_commit_parent GIT_ERROR_CODE {
        parent_commit {PCOMMIT retval}
        commit        PCOMMIT
        n             uint
    }
    git_commit_parent_id {struct.git_oid byref} {
        commit PCOMMIT
        n      uint
    }
    git_commit_nth_gen_ancestor GIT_ERROR_CODE {
        ancestor {PCOMMIT retval}
        commit   PCOMMIT
        n        uint
    }
    git_commit_header_field GIT_ERROR_CODE {
        buf    PBUF
        commit PCOMMIT
        field  STRING
    }
    git_commit_extract_signature GIT_ERROR_CODE {
        signature   PBUF
        signed_data PBUF
        pRepo       PREPOSITORY
        commit_id   {struct.git_oid byref}
        field       {STRING nullifempty nullok}
    }
    git_commit_create GIT_ERROR_CODE {
        id               {struct.git_oid       retval}
        pRepo            PREPOSITORY
        update_ref       {STRING               nullifempty nullok}
        author           {struct.git_signature byref}
        committer        {struct.git_signature byref}
        message_encoding STRING
        encoded_message  pointer
        tree             PTREE
        parent_count     size_t
        parents          {pointer.git_object[parent_count] nullok}
    }
    git_commit_amend GIT_ERROR_CODE {
        id               {struct.git_oid       retval}
        commit_to_amend  PCOMMIT
        update_ref       {STRING               nullifempty nullok}
        author           {struct.git_signature byref}
        committer        {struct.git_signature byref}
        message_encoding STRING
        encoded_message  pointer
        tree             PTREE
    }
    git_commit_create_buffer GIT_ERROR_CODE {
        buf PBUF
        pRepo            PREPOSITORY
        author           {struct.git_signature byref}
        committer        {struct.git_signature byref}
        message_encoding STRING
        encoded_message  pointer
        tree             PTREE
        parent_count     size_t
        parents          {pointer.git_commit[parent_count] nullok}
    }
    git_commit_create_with_signature GIT_ERROR_CODE {
        id               {struct.git_oid retval}
        pRepo            PREPOSITORY
        commit_content   STRING
        signature        {STRING nullifempty nullok}
        signature_fields {STRING nullifempty nullok}
    }
    git_commit_dup GIT_ERROR_CODE {
        dupCommit {PCOMMIT retval}
        commit PCOMMIT
    }
}

::cffi::prototype function git_commit_create_cb int {
    id               {pointer.git_oid unsafe}
    author           {struct.git_signature byref}
    committer        {struct.git_signature byref}
    message_encoding STRING
    encoded_message  pointer
    tree             PTREE
    parent_count     size_t
    parents          pointer.git_commit
    payload          CB_PAYLOAD
}

# Wrappers
proc lg2_commit_message {pCommit} {
    set encoded_message [git_commit_message $pCommit]

    # Get the git encoding which uses IANA names and map to Tcl encoding name
    if {[catch {
        set encoding [lg2_iana_to_tcl_encoding [git_commit_message_encoding $pCommit]]
    }]} {
        set encoding utf-8
    }
    return [cffi::memory tostring! $encoded_message $encoding]
}

# TBD - pending varargs support
# GIT_EXTERN(int) git_commit_create_v(
# git_oid *id,
# git_repository *repo,
# const char *update_ref,
# const git_signature *author,
# const git_signature *committer,
# const char *message_encoding,
# const char *message,
# const git_tree *tree,
# size_t parent_count,
# ...);
