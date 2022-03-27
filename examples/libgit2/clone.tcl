# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/clone.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum sequence git_clone_local_t {
    GIT_CLONE_LOCAL_AUTO
    GIT_CLONE_LOCAL
    GIT_CLONE_NO_LOCAL
    GIT_CLONE_LOCAL_NO_LINKS
}
::cffi::alias define GIT_CLONE_LOCAL_T {int {enum git_clone_local_t}}

::cffi::prototype function git_remote_create_cb int {
    pRemote {pointer.PREMOTE unsafe}
    pRepo   PREPOSITORY
    name    STRING
    url     STRING
    payload {pointer unsafe}
}

::cffi::prototype function git_repository_create_cb int {
    pRepo    {pointer.PREPOSITORY unsafe}
    path     STRING
    bare     int
    payload  {pointer unsafe}
}

::cffi::Struct create git_clone_options {
    version               uint
    checkout_opts         struct.git_checkout_options
    fetch_opts            struct.git_fetch_options
    bare                  int
    local                 GIT_CLONE_LOCAL_T
    checkout_branch       {STRING nullifempty}
    repository_cb         {pointer.git_repository_create_cb nullok}
    repository_cb_payload {pointer unsafe nullok}
    remote_cb             {pointer.git_remote_create_cb nullok}
    remote_cb_payload     {pointer unsafe nullok}
}

libgit2 functions {
    git_clone_options_init GIT_ERROR_CODE {
        opts    {struct.git_clone_options retval}
        version {uint {default 1}}
    }
    git_clone GIT_ERROR_CODE {
        pRepo      {PREPOSITORY retval}
        url        STRING
        local_path STRING
        opts       {struct.git_clone_options byref nullifempty}
    }
}
