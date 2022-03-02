# This file should be sourced into whatever namespace commands should
# be created in.

cffi::enum define git_error_code {
	GIT_OK           0
	GIT_ERROR       -1
	GIT_ENOTFOUND   -3
	GIT_EEXISTS     -4
	GIT_EAMBIGUOUS  -5
	GIT_EBUFS       -6
	GIT_EUSER       -7
	GIT_EBAREREPO         -8
	GIT_EUNBORNBRANCH     -9
	GIT_EUNMERGED        -10
	GIT_ENONFASTFORWARD  -11
	GIT_EINVALIDSPEC     -12
	GIT_ECONFLICT        -13
	GIT_ELOCKED          -14
	GIT_EMODIFIED        -15
	GIT_EAUTH            -16
	GIT_ECERTIFICATE     -17
	GIT_EAPPLIED         -18
	GIT_EPEEL            -19
	GIT_EEOF             -20
	GIT_EINVALID         -21
	GIT_EUNCOMMITTED     -22
	GIT_EDIRECTORY       -23
	GIT_EMERGECONFLICT   -24
	GIT_PASSTHROUGH      -30
	GIT_ITEROVER         -31
	GIT_RETRY            -32
	GIT_EMISMATCH        -33
	GIT_EINDEXDIRTY      -34
	GIT_EAPPLYFAIL       -35
}

# These are error classes (field klass in git_error)
cffi::enum sequence git_error_t {
    GIT_ERROR_NONE
    GIT_ERROR_NOMEMORY
    GIT_ERROR_OS
    GIT_ERROR_INVALID
    GIT_ERROR_REFERENCE
    GIT_ERROR_ZLIB
    GIT_ERROR_REPOSITORY
    GIT_ERROR_CONFIG
    GIT_ERROR_REGEX
    GIT_ERROR_ODB
    GIT_ERROR_INDEX
    GIT_ERROR_OBJECT
    GIT_ERROR_NET
    GIT_ERROR_TAG
    GIT_ERROR_TREE
    GIT_ERROR_INDEXER
    GIT_ERROR_SSL
    GIT_ERROR_SUBMODULE
    GIT_ERROR_THREAD
    GIT_ERROR_STASH
    GIT_ERROR_CHECKOUT
    GIT_ERROR_FETCHHEAD
    GIT_ERROR_MERGE
    GIT_ERROR_SSH
    GIT_ERROR_FILTER
    GIT_ERROR_REVERT
    GIT_ERROR_CALLBACK
    GIT_ERROR_CHERRYPICK
    GIT_ERROR_DESCRIBE
    GIT_ERROR_REBASE
    GIT_ERROR_FILESYSTEM
    GIT_ERROR_PATCH
    GIT_ERROR_WORKTREE
    GIT_ERROR_SHA1
    GIT_ERROR_HTTP
    GIT_ERROR_INTERNAL
}
cffi::alias define git_error_t {int {enum git_error_t}}

cffi::Struct create git_error {
    message {STRING nullok nullifempty}
    klass   {int {enum git_error_t}}
}

libgit2 functions {
    git_error_last {pointer.git_error unsafe nullok} {}
    git_error_clear void {}
    git_error_set_str {int zero} {klass git_error_t message STRING}
    git_error_set_oom void {}
}

proc ErrorCodeHandler {callinfo} {
    lg2_throw_last_error [dict get $callinfo Result]
}

# Iterator error codes need to allow for GIT_ITEROVER as no more items
cffi::alias define GIT_ITER_ERROR_CODE \
    [list int {enum git_error_code} nonnegative [list onerror [namespace current]::IterCodeHandler]]

proc IterCodeHandler {callinfo} {
    set code [dict get $callinfo Result]
    if {$code == -31} {
        return GIT_ITEROVER
    }
    lg2_throw_last_error $code
}
