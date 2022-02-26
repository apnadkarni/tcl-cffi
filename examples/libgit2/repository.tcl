# Based on libgit2/include/repository.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum flags git_repository_open_flag_t {
    GIT_REPOSITORY_OPEN_NO_SEARCH
    GIT_REPOSITORY_OPEN_CROSS_FS
    GIT_REPOSITORY_OPEN_BARE
    GIT_REPOSITORY_OPEN_NO_DOTGIT
    GIT_REPOSITORY_OPEN_FROM_ENV
}
::cffi::alias define GIT_REPOSITORY_OPEN_FLAG_T {uint {enum git_repository_open_flag_t}}

# https://libgit2.org/libgit2/#v1.3.0/type/git_repository_init_flag_t
::cffi::enum flags git_repository_init_flag_t {
	GIT_REPOSITORY_INIT_BARE
	GIT_REPOSITORY_INIT_NO_REINIT
	GIT_REPOSITORY_INIT_NO_DOTGIT_DIR
	GIT_REPOSITORY_INIT_MKDIR
	GIT_REPOSITORY_INIT_MKPATH
	GIT_REPOSITORY_INIT_EXTERNAL_TEMPLATE
	GIT_REPOSITORY_INIT_RELATIVE_GITLINK
}

# https://libgit2.org/libgit2/#v1.3.0/type/git_repository_init_mode_t
::cffi::enum define git_repository_init_mode_t {
    GIT_REPOSITORY_INIT_SHARED_UMASK 0
    GIT_REPOSITORY_INIT_SHARED_GROUP 0002775
    GIT_REPOSITORY_INIT_SHARED_ALL   0002777
}

::cffi::Struct create git_repository_init_options {
    version  {uint {default 1}}
    flags    {uint32_t bitmask {enum git_repository_init_flag_t}}
    mode     {uint32_t bitmask {enum git_repository_init_mode_t}}
    workdir_path   {STRING {default ""} nullifempty}
    description    {STRING {default ""} nullifempty}
    template_path  {STRING {default ""} nullifempty}
    initial_head   {STRING {default ""} nullifempty}
    origin_url     {STRING {default ""} nullifempty}
}

::cffi::enum sequence git_repository_item_t {
    GIT_REPOSITORY_ITEM_GITDIR
    GIT_REPOSITORY_ITEM_WORKDIR
    GIT_REPOSITORY_ITEM_COMMONDIR
    GIT_REPOSITORY_ITEM_INDEX
    GIT_REPOSITORY_ITEM_OBJECTS
    GIT_REPOSITORY_ITEM_REFS
    GIT_REPOSITORY_ITEM_PACKED_REFS
    GIT_REPOSITORY_ITEM_REMOTES
    GIT_REPOSITORY_ITEM_CONFIG
    GIT_REPOSITORY_ITEM_INFO
    GIT_REPOSITORY_ITEM_HOOKS
    GIT_REPOSITORY_ITEM_LOGS
    GIT_REPOSITORY_ITEM_MODULES
    GIT_REPOSITORY_ITEM_WORKTREES
    GIT_REPOSITORY_ITEM__LAST
}
::cffi::alias define GIT_REPOSITORY_ITEM_T {int {enum git_repository_item_t}}

::cffi::prototype function git_repository_fetchhead_foreach_cb int {
    ref_name   STRING
    remote_url STRING
    oid        {struct.git_oid byref}
    is_merge   uint
    payload    {pointer unsafe}
}
::cffi::prototype function git_repository_mergehead_foreach_cb int {
    oid     {struct.git_oid byref}
    payload {pointer unsafe}
}

::cffi::enum sequence git_repository_state_t {
    GIT_REPOSITORY_STATE_NONE
    GIT_REPOSITORY_STATE_MERGE
    GIT_REPOSITORY_STATE_REVERT
    GIT_REPOSITORY_STATE_REVERT_SEQUENCE
    GIT_REPOSITORY_STATE_CHERRYPICK
    GIT_REPOSITORY_STATE_CHERRYPICK_SEQUENCE
    GIT_REPOSITORY_STATE_BISECT
    GIT_REPOSITORY_STATE_REBASE
    GIT_REPOSITORY_STATE_REBASE_INTERACTIVE
    GIT_REPOSITORY_STATE_REBASE_MERGE
    GIT_REPOSITORY_STATE_APPLY_MAILBOX
    GIT_REPOSITORY_STATE_APPLY_MAILBOX_OR_REBASE

}
::cffi::alias define GIT_REPOSITORY_STATE_T {int {enum git_repository_state_t}}

libgit2 functions {
    git_repository_open GIT_ERROR_CODE {
        ppRep {PREPOSITORY retval}
        path STRING
    }
    git_repository_open_from_worktree GIT_ERROR_CODE {
        ppRep      {PREPOSITORY retval}
        pWorkTree PWORKTREE
    }
    git_repository_wrap_odb GIT_ERROR_CODE {
        ppRep {PREPOSITORY retval}
        pOdb PODB
    }
    git_repository_discover GIT_ERROR_CODE {
        buffer PBUF
        start_path STRING
        across_fs int
        ceiling_dirs {STRING {default ""}}
    }
    git_repository_open_ext  GIT_ERROR_CODE {
        ppRep {PREPOSITORY retval}
        path  STRING
        flags {GIT_REPOSITORY_OPEN_FLAG_T {default 0}}
        ceiling_dirs {STRING nullifempty {default ""}}
    }
    git_repository_open_bare GIT_ERROR_CODE {
        ppRep     {PREPOSITORY retval}
        bare_path STRING
    }
    git_repository_free void {pRep {PREPOSITORY nullok dispose}}
    git_repository_init GIT_ERROR_CODE {
        ppRep  {PREPOSITORY retval}
        path   STRING
        is_bare uint
    }
    git_repository_init_options_init {int zero} {
        opts {struct.git_repository_init_options retval}
        version {uint {default 1}}
    }
    git_repository_init_ext GIT_ERROR_CODE {
        ppRep {PREPOSITORY retval}
        repo_path STRING
        opts      {struct.git_repository_init_options byref}
    }
    git_repository_head GIT_ERROR_CODE {
        ppRef {PREFERENCE retval}
        pRepo PREPOSITORY
    }
    git_repository_head_for_worktree GIT_ERROR_CODE {
        ppRef {PREFERENCE retval}
        pRepo PREPOSITORY
        name STRING
    }
    git_repository_head_detached int {
        pRepo PREPOSITORY
    }
    git_repository_head_detached_for_worktree int {
        pRepo PREPOSITORY
        name STRING
    }
    git_repository_head_unborn int {
        pRepo PREPOSITORY
    }
    git_repository_is_empty int {
        pRepo PREPOSITORY
    }
    git_repository_item_path GIT_ERROR_CODE {
        pBuf  PBUF
        pRepo PREPOSITORY
        item  GIT_REPOSITORY_ITEM_T
    }
    git_repository_path STRING {
        pRepo PREPOSITORY
    }
    git_repository_workdir {STRING nullok} {
        pRepo PREPOSITORY
    }
    git_repository_commondir STRING {
        pRepo PREPOSITORY
    }
    git_repository_set_workdir GIT_ERROR_CODE {
        pRepo   PREPOSITORY
        workdir STRING
        update_gitlink int
    }
    git_repository_is_bare int {
        pRepo PREPOSITORY
    }
    git_repository_is_worktree int {
        pRepo PREPOSITORY
    }
    git_repository_config GIT_ERROR_CODE {
        ppConfig {PCONFIG retval}
        pRepo PREPOSITORY
    }
    git_repository_config_snapshot GIT_ERROR_CODE {
        ppConfig {PCONFIG retval}
        pRepo PREPOSITORY
    }
    git_repository_odb GIT_ERROR_CODE {
        pOdb {PODB retval}
        pRepo PREPOSITORY
    }
    git_repository_refdb GIT_ERROR_CODE {
        pRefdb {PREFDB retval}
        pRepo PREPOSITORY
    }
    git_repository_index GIT_ERROR_CODE {
        pIndex {PINDEX retval}
        pRepo PREPOSITORY
    }
    git_repository_message GIT_ERROR_CODE {
        pBuf PBUF
        pRepo PREPOSITORY
    }
    git_repository_message_remove int {
        pRepo PREPOSITORY
    }
    git_repository_state_cleanup GIT_ERROR_CODE {
        pRepo PREPOSITORY
    }
    git_repository_fetchhead_foreach int {
        pRepo    PREPOSITORY
        callback {pointer.git_repository_fetchhead_foreach_cb unsafe}
        payload  {pointer unsafe}
    }
    git_repository_mergehead_foreach int {
        pRepo    PREPOSITORY
        callback {pointer.git_repository_mergehead_foreach_cb unsafe}
        payload  {pointer unsafe}
    }
    git_repository_hashfile GIT_ERROR_CODE {
        oid   {struct.git_oid out byref}
        pRepo PREPOSITORY
        path  STRING
        type  GIT_OBJECT_T
        as_path STRING
    }
    git_repository_set_head GIT_ERROR_CODE {
        pRepo   PREPOSITORY
        refname STRING
    }
    git_repository_set_head_detached GIT_ERROR_CODE {
        pRepo       PREPOSITORY
        commitish   {struct.git_oid byref}
    }
    git_repository_set_head_detached_from_annotated GIT_ERROR_CODE {
        pRepo       PREPOSITORY
        commitish   PANNOTATED_COMMIT
    }
    git_repository_detach_head GIT_ERROR_CODE {
        pRepo PREPOSITORY
    }
    git_repository_state GIT_REPOSITORY_STATE_T {
        pRepo PREPOSITORY
    }
    git_repository_set_namespace GIT_ERROR_CODE {
        pRepo PREPOSITORY
        nmspace STRING
    }
    git_repository_get_namespace STRING {
        pRepo PREPOSITORY
    }
    git_repository_is_shallow int {
        pRepo PREPOSITORY
    }
    git_repository_ident GIT_ERROR_CODE {
        name {STRING out}
        email {STRING out}
        pRepo PREPOSITORY
    }
    git_repository_set_ident GIT_ERROR_CODE {
        pRepo PREPOSITORY
        name  {STRING nullifempty}
        email {STRING nullifempty}
    }
}
