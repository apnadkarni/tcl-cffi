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
    pRemote {PREMOTE out}
    pRepo   PREPOSITORY
    name    STRING
    url     STRING
    payload {pointer unsafe}
}

::cffi::prototype function git_repository_create_cb int {
    pRepo    {PREPOSITORY out}
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
    checkout_branch       STRING
    repository_cb         {pointer.git_repository_create_cb nullok}
    repository_cb_payload {pointer unsafe}
    remote_cb             {pointer.git_remote_create_cb nullok}
    remote_cb_payload     {pointer unsafe}
}

libgit2 functions {
    git_clone_options_init GIT_ERROR_CODE {
        opts    {struct.git_clone_options out}
        version uint
    }
    git_clone GIT_ERROR_CODE {
        pRepo      {PREPOSITORY out}
        url        STRING
        local_path STRING
        opts       {struct.git_clone_options byref}
    }
}
