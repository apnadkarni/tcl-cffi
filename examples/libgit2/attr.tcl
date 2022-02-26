# Based on libgit2/include/attr.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

# NOTES:
# The attribute api is slightly weird. The retrieval functions return attribute
# values as char*. However, these cannot directly be treated as strings because
# TRUE and FALSE values are represented by addresses of specific variables in
# libgit2 internals. Thus these pointers are to be checked with git_attr_value
# which returns the value type (slightly misnamed). Then if the type is string
# the pointer can be dereferenced to retrieve the value.
proc git_decode_value {pointer {default ""}} {
    if {[::cffi::pointer isnull $pointer]} {
        return $default
    }
    switch -exact -- [git_attr_value $pointer] {
        GIT_ATTR_VALUE_TRUE { return true }
        GIT_ATTR_VALUE_FALSE { return false }
        GIT_ATTR_VALUE_STRING { return [::cffi::memory tostring! $pointer utf-8]}
        GIT_ATTR_VALUE_UNSPECIFIC -
        default { return $default }
    }
}

::cffi::enum sequence git_attr_value_t {
    GIT_ATTR_VALUE_UNSPECIFIED
    GIT_ATTR_VALUE_TRUE
    GIT_ATTR_VALUE_FALSE
    GIT_ATTR_VALUE_STRING
}
::cffi::alias define GIT_ATTR_VALUE_T {int {enum git_attr_value_t}}

::cffi::enum define git_attr_check_t {
    GIT_ATTR_CHECK_FILE_THEN_INDEX 0
    GIT_ATTR_CHECK_INDEX_THEN_FILE 1
    GIT_ATTR_CHECK_INDEX_ONLY      2
    GIT_ATTR_CHECK_NO_SYSTEM       4
    GIT_ATTR_CHECK_INCLUDE_HEAD    8
    GIT_ATTR_CHECK_INCLUDE_COMMIT  16
}
::cffi::alias define GIT_ATTR_CHECK_T {uint {enum git_attr_check_t}}

::cffi::Struct create git_attr_options {
    version        {uint    {default 1}}
    flags          GIT_ATTR_CHECK_T
    reserved       {pointer nullok   {default NULL}}
    attr_commit_id struct.git_oid
} -clear

::cffi::prototype function git_attr_foreach_cb int {
    name    STRING
    value   {pointer unsafe nullok}
    payload CB_PAYLOAD
}

libgit2 functions {
    git_attr_value GIT_ATTR_VALUE_T {
        attr {pointer unsafe}
    }
    git_attr_get GIT_ERROR_CODE {
        value_out {pointer          unsafe retval}
        pRepo     PREPOSITORY
        flags     {GIT_ATTR_CHECK_T bitmask}
        path      STRING
        name      STRING
    }
    git_attr_get_ext GIT_ERROR_CODE {
        value_out {pointer                 unsafe retval}
        pRepo     PREPOSITORY
        opts      {struct.git_attr_options byref}
        path      STRING
        name      STRING
    }
    git_attr_get_many GIT_ERROR_CODE {
        values_out {pointer[num_attr] unsafe retval}
        pRepo      PREPOSITORY
        flags      {GIT_ATTR_CHECK_T  bitmask}
        path       STRING
        num_attr   size_t
        names      {pointer[num_attr]}
    }
    git_attr_get_many_ext GIT_ERROR_CODE {
        values_out {pointer[num_attr]       unsafe retval}
        pRepo      PREPOSITORY
        opts       {struct.git_attr_options byref}
        path       STRING
        num_attr   size_t
        names      {pointer[num_attr]       unsafe}
    }
    git_attr_foreach GIT_ERROR_CODE {
        pRepo    PREPOSITORY
        flags    {GIT_ATTR_CHECK_T bitmask}
        path     STRING
        callback pointer.git_attr_foreach_cb
        payload  CB_PAYLOAD
    }
    git_attr_foreach_ext GIT_ERROR_CODE {
        pRepo    PREPOSITORY
        opts     {struct.git_attr_options byref}
        path     STRING
        callback pointer.git_attr_foreach_cb
        payload  CB_PAYLOAD
    }
    git_attr_cache_flush GIT_ERROR_CODE {
        pRepo PREPOSITORY
    }
    git_attr_add_macro GIT_ERROR_CODE {
        pRepo  PREPOSITORY
        name   STRING
        values STRING
    }
}
