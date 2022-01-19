# Based on libgit2/include/refs.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum define git_reference_format_t {
    GIT_REFERENCE_FORMAT_NORMAL            0
    GIT_REFERENCE_FORMAT_ALLOW_ONELEVEL    1
    GIT_REFERENCE_FORMAT_REFSPEC_PATTERN   2
    GIT_REFERENCE_FORMAT_REFSPEC_SHORTHAND 4
}
::cffi::alias define GIT_REFERENCE_FORMAT_T {int {enum git_reference_format_t}}

::cffi::prototype function git_reference_foreach_cb GIT_ERROR_CODE {
    ref     PREFERENCE
    payload CB_PAYLOAD
}
::cffi::prototype function git_reference_foreach_name_cb GIT_ERROR_CODE {
    name STRING
    payload CB_PAYLOAD
}

libgit2 functions {
    git_reference_lookup GIT_ERROR_CODE {
        vref   {PREFERENCE out}
        pRepo PREPOSITORY
        name  STRING
    }
    git_reference_name_to_id GIT_ERROR_CODE {
        vid   {struct.git_oid out}
        pRepo PREPOSITORY
        name  STRING
    }
    git_reference_dwim GIT_ERROR_CODE {
        vref      {PREFERENCE out}
        pRepo     PREPOSITORY
        shorthand STRING
    }
    git_reference_symbolic_create_matching GIT_ERROR_CODE {
        vref          {PREFERENCE out}
        pRepo         PREPOSITORY
        name          STRING
        target        STRING
        force         int
        current_value STRING
        log_message   STRING
    }
    git_reference_symbolic_create GIT_ERROR_CODE {
        vref          {PREFERENCE out}
        pRepo         PREPOSITORY
        name          STRING
        target        STRING
        force         int
        log_message   STRING
    }
    git_reference_create GIT_ERROR_CODE {
        vref        {PREFERENCE     out}
        pRepo       PREPOSITORY
        name        STRING
        id          {struct.git_oid byref}
        force       int
        log_message STRING
    }
    git_reference_create_matching GIT_ERROR_CODE {
        vref        {PREFERENCE     out}
        pRepo       PREPOSITORY
        name        STRING
        id          {struct.git_oid byref}
        force       int
        current_id  {struct.git_oid byref}
        log_message STRING
    }
    git_reference_target {struct.git_oid byref} {
        ref PREFERENCE
    }
    git_reference_target_peel {struct.git_oid byref} {
        ref PREFERENCE
    }
    git_reference_symbolic_target STRING {
        ref PREFERENCE
    }
    git_reference_type GIT_REFERENCE_T {
        ref PREFERENCE
    }
    git_reference_name STRING {
        ref PREFERENCE
    }
    git_reference_resolve GIT_ERROR_CODE {
        vref {PREFERENCE out}
        ref  PREFERENCE
    }
    git_reference_owner PREPOSITORY {
        ref PREFERENCE
    }
    git_reference_symbolic_set_target GIT_ERROR_CODE {
        vref        {PREFERENCE out}
        ref         PREFERENCE
        target      STRING
        log_message STRING
    }
    git_reference_set_target GIT_ERROR_CODE {
        vref        {PREFERENCE     out}
        ref         PREFERENCE
        id          {struct.git_oid byref}
        log_message STRING
    }
    git_reference_rename GIT_ERROR_CODE {
        vref        {PREFERENCE out}
        ref         PREFERENCE
        new_name    STRING
        force       int
        log_message STRING
    }
    git_reference_delete GIT_ERROR_CODE {
        ref PREFERENCE
    }
    git_reference_remove GIT_ERROR_CODE {
        pRepo PREPOSITORY
        name  STRING
    }
    git_reference_list GIT_ERROR_CODE {
        array PSTRARRAY
        pRepo PREPOSITORY
    }
    git_reference_foreach GIT_ERROR_CODE {
        pRepo    PREPOSITORY
        callback pointer.git_reference_foreach_cb
        payload  CB_PAYLOAD
    }
    git_reference_foreach_name GIT_ERROR_CODE {
        pRepo    PREPOSITORY
        callback pointer.git_reference_foreach_name_cb
        payload  CB_PAYLOAD
    }
    git_reference_dup GIT_ERROR_CODE {
        ref PREFERENCE
    }
    git_reference_free void {
        ref {PREFERENCE dispose}
    }
    git_reference_cmp int {
        ref1 PREFERENCE
        ref2 PREFERENCE
    }
    git_reference_iterator_new GIT_ERROR_CODE {
        viter {PREFERENCE_ITERATOR out}
        pRepo PREPOSITORY
    }
    git_reference_iterator_glob_new GIT_ERROR_CODE {
        viter {PREFERENCE_ITERATOR out}
        pRepo PREPOSITORY
        glob STRING
    }
    git_reference_next {int {enum git_error_code}} {
        vref {PREFERENCE out}
        iter PREFERENCE_ITERATOR
    }
    git_reference_next_name {int {enum git_error_code}} {
        name {STRING out}
        iter PREFERENCE_ITERATOR
    }
    git_reference_iterator_free void {
        iter {PREFERENCE_ITERATOR dispose}
    }
    git_reference_foreach_glob GIT_ERROR_CODE {
        pRepo    PREPOSITORY
        glob     STRING
        callback pointer.git_reference_foreach_name_cb
        payload  CB_PAYLOAD
    }
    git_reference_has_log int {
        pRepo   PREPOSITORY
        refname STRING
    }
    git_reference_ensure_log GIT_ERROR_CODE {
        pRepo   PREPOSITORY
        refname STRING
    }
    git_reference_is_branch int {
        ref PREFERENCE
    }
    git_reference_is_remote int {
        ref PREFERENCE
    }
    git_reference_is_tag int {
        ref PREFERENCE
    }
    git_reference_is_note int {
        ref PREFERENCE
    }
    git_reference_normalize_name GIT_ERROR_CODE {
        buffer      chars[buffer_size]
        buffer_size size_t
        name        STRING
        flags       GIT_REFERENCE_FORMAT_T
    }
    git_reference_peel GIT_ERROR_CODE {
        vobj {POBJECT out}
        ref  PREFERENCE
        type GIT_OBJECT_T
    }
    git_reference_name_is_valid int {
        ref PREFERENCE
    }
    git_reference_shorthand STRING {
        ref PREFERENCE
    }
}
