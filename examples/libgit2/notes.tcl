# Based on libgit2/include/notes.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::prototype function git_note_foreach_cb {int {enum git_error_code}} {
    blob_id             {struct.git_oid byref}
    annotated_object_id {struct.git_oid byref}
    payload             CB_PAYLOAD
}

::cffi::alias define PNOTE_ITERATOR pointer.git_note_iterator

libgit2 functions {
    git_note_iterator_new GIT_ERROR_CODE {
        iterator  {PNOTE_ITERATOR out}
        pRepo     PREPOSITORY
        notes_ref {STRING         nullifempty}
    }
    git_note_commit_iterator_new GIT_ERROR_CODE {
        iterator     {PNOTE_ITERATOR out}
        notes_commit PCOMMIT
    }
    git_note_iterator_free void {
        iterator {PNOTE_ITERATOR dispose}
    }
    git_note_next {int {enum git_error_code}} {
        note_id      {struct.git_oid out}
        annotated_id {struct.git_oid out}
        iterator     PNOTE_ITERATOR
    }
    git_note_read GIT_ERROR_CODE {
        note {PNOTE out}
        pRepo PREPOSITORY
        notes_commit PCOMMIT
        oid {struct.git_oid byref}
    }
    git_note_commit_read GIT_ERROR_CODE {
        note {PNOTE out}
        pRepo PREPOSITORY
        notes_commit PCOMMIT
        oid {struct.git_oid byref}
    }
    git_note_author {struct.git_signature byref} {
        note PNOTE
    }
    git_note_committer {struct.git_signature byref} {
        note PNOTE
    }
    git_note_message STRING {
        note PNOTE
    }
    git_note_id {struct.git_oid byref} {
        note PNOTE
    }
    git_note_create GIT_ERROR_CODE {
        id        {struct.git_oid       out}
        pRepo     PREPOSITORY
        notes_ref {STRING               nullifempty}
        author    {struct.git_signature byref}
        committer {struct.git_signature byref}
        oid       {struct.git_oid       byref}
        note      STRING
        force     int
    }
    git_note_commit_create GIT_ERROR_CODE {
        notes_commit_id      {struct.git_oid       out}
        notes_blob_out       {struct.git_oid       out}
        pRepo                PREPOSITORY
        parent               PCOMMIT
        author               {struct.git_signature byref}
        committer            {struct.git_signature byref}
        oid                  {struct.git_oid       byref}
        note                 STRING
        allow_note_overwrite int
    }
    git_note_remove GIT_ERROR_CODE {
        pRepo     PREPOSITORY
        notes_ref {STRING               nullifempty}
        author    {struct.git_signature byref}
        committer {struct.git_signature byref}
        oid       {struct.git_oid       byref}
    }
    git_note_commit_remove GIT_ERROR_CODE {
        notes_commit_oid {struct.git_oid byref}
        pRepo PREPOSITORY
        notes_commit PCOMMIT
        author    {struct.git_signature byref}
        committer {struct.git_signature byref}
        oid       {struct.git_oid       byref}
    }
    git_note_free void {
        note {PNOTE dispose}
    }
    git_note_default_ref GIT_ERROR_CODE {
        buf PBUF
        pRepo PREPOSITORY
    }
    git_note_foreach {int {enum git_error_code}} {
        pRepo PREPOSITORY
        notes_ref {STRING nullifempty}
        note_cb pointer.git_note_foreach_cb
        payload CB_PAYLOAD
    }
}
