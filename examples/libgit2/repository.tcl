# Based on libgit2/include/repository.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

AddFunctions {
    git_repository_open {
        GIT_ERROR_CODE
        {
            ppRep {PREPOSITORY out}
            path string
        }
    }
    git_repository_open_from_worktree {
        GIT_ERROR_CODE
        {
            ppRep      {PREPOSITORY out}
            pWorkTree PWORKTREE
        }
    }
    git_repository_wrap_odb {
        GIT_ERROR_CODE
        {
            ppRep {PREPOSITORY out}
            pOdb PODB
        }
    }
    git_repository_discover {
        GIT_ERROR_CODE
        {
            buffer PBUF
            start_path string
            across_fs int
            ceiling_dirs {string {default ""}}
        }
    }
}

::cffi::enum flags git_repository_open_flag_t {
    GIT_REPOSITORY_OPEN_NO_SEARCH
    GIT_REPOSITORY_OPEN_CROSS_FS
    GIT_REPOSITORY_OPEN_BARE
    GIT_REPOSITORY_OPEN_NO_DOTGIT
    GIT_REPOSITORY_OPEN_FROM_ENV
}

AddFunctions {
    git_repository_open_ext  {
        GIT_ERROR_CODE
        {
            ppRep {PREPOSITORY out}
            path  string
            flags uint
            ceiling_dirs {string {default ""}}
        }
    }
    git_repository_open_bare {
        GIT_ERROR_CODE
        {
            ppRep     {PREPOSITORY out}
            bare_path string
        }
    }
    git_repository_free {
        void
        {pRep {PREPOSITORY dispose}}
    }
    git_repository_init {
        GIT_ERROR_CODE
        {
            ppRep  {PREPOSITORY out}
            path   string
            is_bare uint
        }
    }
}

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
    workdir_path   {string {default ""} nullifempty}
    description    {string {default ""} nullifempty}
    template_path  {string {default ""} nullifempty}
    initial_head   {string {default ""} nullifempty}
    origin_url     {string {default ""} nullifempty}
}

AddFunctions {
    git_repository_init_options_init {
        {int zero}
        {
            opts {struct.git_repository_init_options out}
            version {uint {default 1}}
        }
    }
    git_repository_init_ext {
        GIT_ERROR_CODE
        {
            ppRep {PREPOSITORY out}
            repo_path string
            opts      {struct.git_repository_init_options byref}
        }
    }
    git_repository_head {
        GIT_ERROR_CODE
        {
            ppRef {PREFERENCE out}
            pRepo PREPOSITORY
        }
    }
    git_repository_head_for_worktree {
        GIT_ERROR_CODE
        {
            ppRef {PREFERENCE out}
            pRepo PREPOSITORY
            name string
        }
    }
    git_repository_head_detached {
        int
        {
            pRepo PREPOSITORY
        }
    }
    git_repository_head_detached_for_worktree {
        int
        {
            pRepo PREPOSITORY
            name string
        }
    }
    git_repository_head_unborn {
        int
        {
            pRepo PREPOSITORY
        }
    }
    git_repository_is_empty {
        int
        {
            pRepo PREPOSITORY
        }
    }
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

AddFunctions {
    git_repository_item_path {
        GIT_ERROR_CODE
        {
            pBuf  PBUF
            pRepo PREPOSITORY
            item  GIT_REPOSITORY_ITEM_T
        }
    }
    git_repository_path {
        string
        {
            pRepo PREPOSITORY
        }
    }
    git_repository_workdir {
        string
        {
            pRepo PREPOSITORY
        }
    }
    git_repository_commondir {
        string
        {
            pRepo PREPOSITORY
        }
    }
    git_repository_set_workdir {
        GIT_ERROR_CODE
        {
            pRepo   PREPOSITORY
            workdir string
            update_gitlink int
        }
    }
    git_repository_is_bare {
        int
        {
            pRepo PREPOSITORY
        }
    }
    git_repository_is_worktree {
        int
        {
            pRepo PREPOSITORY
        }
    }
    git_repository_config {
        GIT_ERROR_CODE
        {
            ppConfig {PCONFIG out}
            pRepo PREPOSITORY
        }
    }
    git_repository_config_snapshot {
        GIT_ERROR_CODE
        {
            ppConfig {PCONFIG out}
            pRepo PREPOSITORY
        }
    }
    git_repository_odb {
        GIT_ERROR_CODE
        {
            pOdb {PODB out}
            pRepo PREPOSITORY
        }
    }
    git_repository_refdb {
        GIT_ERROR_CODE
        {
            pRefdb {PREFDB out}
            pRepo PREPOSITORY
        }
    }
    git_repository_index {
        GIT_ERROR_CODE
        {
            pIndex {PINDEX out}
            pRepo PREPOSITORY
        }
    }
    git_repository_message {
        GIT_ERROR_CODE
        {
            pBuf PBUF
            pRepo PREPOSITORY
        }
    }
    git_repository_message_remove {
        int
        {
            pRepo PREPOSITORY
        }
    }
    git_repository_state_cleanup {
        GIT_ERROR_CODE
        {
            pRepo PREPOSITORY
        }
    }
}

# TODO - pending implementation of callbacks
# typedef int GIT_CALLBACK(git_repository_fetchhead_foreach_cb)(const char *ref_name,
# GIT_EXTERN(int) git_repository_fetchhead_foreach(
# git_repository *repo,
# git_repository_fetchhead_foreach_cb callback,
# void *payload);
# typedef int GIT_CALLBACK(git_repository_mergehead_foreach_cb)(const git_oid *oid,
# void *payload);
# GIT_EXTERN(int) git_repository_mergehead_foreach(
# git_repository *repo,
# git_repository_mergehead_foreach_cb callback,
# void *payload);

AddFunctions {
    git_repository_hashfile {
        GIT_ERROR_CODE
        {
            oid   {struct.git_oid out byref}
            pRepo PREPOSITORY
            path  string
            type  GIT_OBJECT_T
            as_path string
        }
    }
    git_repository_set_head {
        GIT_ERROR_CODE
        {
            pRepo   PREPOSITORY
            refname string
        }
    }
    git_repository_set_head_detached {
        GIT_ERROR_CODE
        {
            pRepo       PREPOSITORY
            commitish   {struct.git_oid byref}
        }
    }
    git_repository_set_head_detached_from_annotated {
        GIT_ERROR_CODE
        {
            pRepo       PREPOSITORY
            commitish   PANNOTATED_COMMIT
        }
    }
    git_repository_detach_head {
        GIT_ERROR_CODE
        {
            pRepo PREPOSITORY
        }
    }
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

AddFunctions {
    git_repository_state {
        GIT_REPOSITORY_STATE_T
        {
            pRepo PREPOSITORY
        }
    }
    git_repository_set_namespace {
        GIT_ERROR_CODE
        {
            pRepo PREPOSITORY
            nmspace string
        }
    }
    git_repository_get_namespace {
        string
        {
            pRepo PREPOSITORY
        }
    }
    git_repository_is_shallow {
        int
        {
            pRepo PREPOSITORY
        }
    }
    git_repository_ident {
        GIT_ERROR_CODE
        {
            name {string out}
            email {string out}
            pRepo PREPOSITORY
        }
    }
    git_repository_set_ident {
        GIT_ERROR_CODE
        {
            pRepo PREPOSITORY
            name  {string nullifempty}
            email {string nullifempty}
        }
    }
}
