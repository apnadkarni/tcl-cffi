# Based on libgit2/include/remote.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum flags git_remote_create_flags {
    GIT_REMOTE_CREATE_SKIP_INSTEADOF
    GIT_REMOTE_CREATE_SKIP_DEFAULT_FETCHSPEC
}

::cffi::Struct create git_remote_create_options {
    version uint
    pRepo   {PREPOSITORY nullok}
    name    {STRING nullok nullifempty}
    fetchspec STRING
    flags   uint
}

::cffi::enum sequence git_remote_completion_t {
    GIT_REMOTE_COMPLETION_DOWNLOAD
    GIT_REMOTE_COMPLETION_INDEXING
    GIT_REMOTE_COMPLETION_ERROR
}
::cffi::alias define GIT_REMOTE_COMPLETION_T {int {enum git_remote_completion_t}}

::cffi::enum sequence git_fetch_prune_t {
    GIT_FETCH_PRUNE_UNSPECIFIED
    GIT_FETCH_PRUNE
    GIT_FETCH_NO_PRUNE
}
::cffi::alias define GIT_FETCH_PRUNE_T {int {enum git_fetch_prune_t}}

::cffi::enum sequence git_remote_autotag_option_t {
    GIT_REMOTE_DOWNLOAD_TAGS_UNSPECIFIED
    GIT_REMOTE_DOWNLOAD_TAGS_AUTO
    GIT_REMOTE_DOWNLOAD_TAGS_NONE
    GIT_REMOTE_DOWNLOAD_TAGS_ALL
}
::cffi::alias define GIT_REMOTE_AUTOTAG_OPTION_T {int {enum git_remote_autotag_option_t}}

::cffi::prototype function git_push_transfer_progress_cb int {
    current uint
    total   uint
    bytes   size_t
    payload CB_PAYLOAD
}

::cffi::Struct create git_push_update {
    src_refname STRING
    dst_refname STRING
    src struct.git_oid
    dst struct.git_oid
}

::cffi::prototype function git_push_negotiation int {
    updates {pointer unsafe}
    len     size_t
    payload CB_PAYLOAD
}
::cffi::prototype function git_push_update_reference_cb GIT_ERROR_CODE {
    refname STRING
    status  {STRING nullok nullifempty}
    data    {pointer unsafe}
}
::cffi::prototype function git_remote_ready_cb GIT_ERROR_CODE {
    pRemote   PREMOTE
    direction GIT_DIRECTION
    payload   CB_PAYLOAD
}
::cffi::prototype function git_remote_completion_cb int {
    type GIT_REMOTE_COMPLETION_T
    payload CB_PAYLOAD
}
::cffi::prototype function git_remote_update_tips_cb int {
    refname STRING
    oid_old {struct.git_oid byref}
    oid_new {struct.git_oid byref}
    payload CB_PAYLOAD
}

::cffi::Struct create git_remote_callbacks {
    version                uint
    sideband_progress      {pointer.git_transport_message_cb nullok}
    completion             {pointer.git_remote_completion_cb nullok}
    credentials            {pointer.git_credential_acquire_cb nullok}
    certificate_check      {pointer.git_transport_certificate_check_cb nullok}
    transfer_progress      {pointer.git_indexer_progress_cb nullok}
    update_tips            {pointer.git_remote_update_tips_cb nullok}
    pack_progrss           {pointer.git_packbuilder_progress nullok}
    push_transfer_progress {pointer.git_push_transfer_progress_cb nullok}
    push_update_reference  {pointer.git_push_update_reference_cb nullok}
    push_negotiation       {pointer.git_push_negotiation nullok}
    transport              {pointer.git_transport_cb nullok}
    remote_ready           {pointer.git_remote_ready_cb nullok}
    payload                CB_PAYLOAD
    reserved               {pointer unsafe nullok}
} -clear


if {[lg2_abi_vsatisfies 1.4]} {
    ::cffi::Struct create git_fetch_options {
        version          int
        callbacks        struct.git_remote_callbacks
        prune            GIT_FETCH_PRUNE_T
        update_fetchhead int
        download_tags    GIT_REMOTE_AUTOTAG_OPTION_T
        proxy_opts       struct.git_proxy_options
        follow_redirects GIT_REMOTE_REDIRECT_T
        custom_headers   struct.lg2_strarray
    }
    ::cffi::Struct create git_push_options {
        version uint
        pb_parallelism uint
        callbacks struct.git_remote_callbacks
        proxy_opts struct.git_proxy_options
        follow_redirects GIT_REMOTE_REDIRECT_T
        custom_headers   struct.lg2_strarray
    }
} else {
    ::cffi::Struct create git_fetch_options {
        version          int
        callbacks        struct.git_remote_callbacks
        prune            GIT_FETCH_PRUNE_T
        update_fetchhead int
        download_tags    GIT_REMOTE_AUTOTAG_OPTION_T
        proxy_opts       struct.git_proxy_options
        custom_headers   struct.lg2_strarray
    }
    ::cffi::Struct create git_push_options {
        version uint
        pb_parallelism uint
        callbacks struct.git_remote_callbacks
        proxy_opts struct.git_proxy_options
        custom_headers   struct.lg2_strarray
    }
}


libgit2 functions {
    git_remote_create GIT_ERROR_CODE {
        pRemote {PREMOTE retval}
        pRepo   PREPOSITORY
        name    STRING
        url     STRING
    }
    git_remote_create_options_init GIT_ERROR_CODE {
        opts {struct.git_remote_create_options retval}
        version uint
    }
    git_remote_create_with_opts GIT_ERROR_CODE {
        pRemote {PREMOTE retval}
        url     STRING
        opts    {struct.git_remote_create_options byref}
    }
    git_remote_create_with_fetchspec GIT_ERROR_CODE {
        pRemote {PREMOTE retval}
        pRepo   PREPOSITORY
        name    STRING
        url     STRING
        fetch   STRING
    }
    git_remote_create_anonymous GIT_ERROR_CODE {
        pRemote {PREMOTE retval}
        pRepo   PREPOSITORY
        url     STRING
    }
    git_remote_create_detached GIT_ERROR_CODE {
        pRemote {PREMOTE retval}
        url     STRING
    }
    git_remote_lookup GIT_ERROR_CODE {
        pRemote {PREMOTE retval}
        pRepo   PREPOSITORY
        name    STRING
    }
    git_remote_dup GIT_ERROR_CODE {
        pRemote {PREMOTE retval}
        psource PREMOTE
    }
    git_remote_owner PREPOSITORY {
        pRemote PREMOTE
    }
    git_remote_name STRING {
        pRemote PREMOTE
    }
    git_remote_url STRING {
        pRemote PREMOTE
    }
    git_remote_pushurl STRING {
        pRemote PREMOTE
    }
    git_remote_set_url GIT_ERROR_CODE {
        pRepo PREPOSITORY
        name  STRING
        url   STRING
    }
    git_remote_set_pushurl GIT_ERROR_CODE {
        pRepo PREPOSITORY
        name  STRING
        url   STRING
    }
    git_remote_set_instance_url GIT_ERROR_CODE {
        pRemote PREMOTE
        url   STRING
    }
    git_remote_set_instance_pushurl GIT_ERROR_CODE {
        pRemote PREMOTE
        url   STRING
    }
    git_remote_add_fetch GIT_ERROR_CODE {
        pRepo PREPOSITORY
        name  STRING
        refspec STRING
    }
    git_remote_get_fetch_refspecs GIT_ERROR_CODE {
        pArray PSTRARRAY
        pRemote PREMOTE
    }
    git_remote_add_push GIT_ERROR_CODE {
        pRepo PREPOSITORY
        name  STRING
        refspec STRING
    }
    git_remote_get_push_refspecs GIT_ERROR_CODE {
        pArray PSTRARRAY
        pRemote PREMOTE
    }
    git_remote_refspec_count size_t {
        pRemote PREMOTE
    }
    git_remote_get_refspec {PREFSPEC unsafe} {
        pRemote PREMOTE
        n       size_t
    }
    git_remote_connect GIT_ERROR_CODE {
        pRemote PREMOTE
        direction GIT_DIRECTION
        callbacks {struct.git_remote_callbacks byref nullifempty nullok}
        proxy_opts {struct.git_proxy_options byref nullifempty nullok}
        pCustomHeaders {PSTRARRAYIN nullok}
    }
    git_remote_ls GIT_ERROR_CODE {
        heads   {pointer retval unsafe}
        size    {size_t out}
        pRemote PREMOTE
    }
    git_remote_connected int {
        pRemote PREMOTE
    }
    git_remote_stop GIT_ERROR_CODE {
        pRemote PREMOTE
    }
    git_remote_disconnect GIT_ERROR_CODE {
        pRemote PREMOTE
    }
    git_remote_free void {
        pRemote {PREMOTE nullok dispose}
    }
    git_remote_list GIT_ERROR_CODE {
        pArray PSTRARRAY
        pRepo  PREPOSITORY
    }
    git_remote_init_callbacks GIT_ERROR_CODE {
        opts    {struct.git_remote_callbacks retval}
        version {uint {default 1}}
    }
    git_fetch_options_init GIT_ERROR_CODE {
        opts    {struct.git_fetch_options retval}
        version {uint {default 1}}
    }
    git_push_options_init GIT_ERROR_CODE {
        opts    {struct.git_push_options retval}
        version {uint {default 1}}
    }
    git_remote_download GIT_ERROR_CODE {
        pRemote  PREMOTE
        refspecs {PSTRARRAYIN nullok}
        opts     {struct.git_fetch_options byref}
    }
    git_remote_upload GIT_ERROR_CODE {
        pRemote  PREMOTE
        refspecs {PSTRARRAYIN nullok}
        opts     {struct.git_push_options byref}
    }
    git_remote_update_tips GIT_ERROR_CODE {
        pRemote          PREMOTE
        callbacks        pointer.git_remote_callbacks
        update_fetchhead int
        download_tags    GIT_REMOTE_AUTOTAG_OPTION_T
        reflog_message   {STRING nullok nullifempty}
    }
    git_remote_fetch GIT_ERROR_CODE {
        pRemote        PREMOTE
        refspecs       {PSTRARRAYIN nullok}
        opts           {struct.git_fetch_options byref}
        reflog_message {STRING nullok nullifempty}
    }
    git_remote_prune GIT_ERROR_CODE {
        pRemote   PREMOTE
        callbacks {pointer.git_remote_callbacks}
    }
    git_remote_push GIT_ERROR_CODE {
        pRemote  PREMOTE
        refspecs {PSTRARRAYIN nullok}
        opts     {struct.git_push_options byref}
    }
    git_remote_stats {struct.git_indexer_progress byref} {
        pRemote PREMOTE
    }
    git_remote_autotag GIT_REMOTE_AUTOTAG_OPTION_T {
        pRemote PREMOTE
    }
    git_remote_set_autotag GIT_ERROR_CODE {
        pRepo  PREPOSITORY
        remote STRING
        value  GIT_REMOTE_AUTOTAG_OPTION_T
    }
    git_remote_prune_refs int {
        pRemote PREMOTE
    }
    git_remote_rename GIT_ERROR_CODE {
        problems PSTRARRAY
        pRepo    PREPOSITORY
        name     STRING
        new_name STRING
    }
    git_remote_name_is_valid GIT_ERROR_CODE {
        valid       {int retval}
        remote_name STRING
    }
    git_remote_delete GIT_ERROR_CODE {
        pRepo PREPOSITORY
        name  STRING
    }
    git_remote_default_branch GIT_ERROR_CODE {
        pBuf    PBUF
        pRemote PREMOTE
    }
}

if {[lg2_abi_vsatisfies 1.4]} {
    ::cffi::enum flags git_remote_redirect_t {
        GIT_REMOTE_REDIRECT_NONE
        GIT_REMOTE_REDIRECT_INITIAL
        GIT_REMOTE_REDIRECT_ALL
    }
    ::cffi::alias define GIT_REMOTE_REDIRECT_T {int {enum git_remote_redirect_t}}

    ::cffi::Struct create git_remote_connect_options {
        version uint
        callbacks struct.git_remote_callbacks
        proxy_opts struct.git_proxy_options
        follow_redirects GIT_REMOTE_REDIRECT_T
        custom_headers   struct.lg2_strarray
    }

    libgit2 function git_remote_connect_options_init GIT_ERROR_CODE {
        opts {struct.git_remote_connect_options byref}
        version {uint {default 1}}
    }
    libgit2 function git_remote_connect_ext GIT_ERROR_CODE {
        pRemote PREMOTE
        direction GIT_DIRECTION
        opts {struct.git_remote_connect_options byref nullifempty nullok}
    }
}

