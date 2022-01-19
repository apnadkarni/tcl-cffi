# Based on libgit2/include/trace.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum sequence git_trace_level_t {
    GIT_TRACE_NONE
    GIT_TRACE_FATAL
    GIT_TRACE_ERROR
    GIT_TRACE_WARN
    GIT_TRACE_INFO
    GIT_TRACE_DEBUG
    GIT_TRACE_TRACE
}
::cffi::alias define GIT_TRACE_LEVEL_T {int {enum git_trace_level_t}}

::cffi::prototype function git_trace_cb void {
    level GIT_TRACE_LEVEL_T
    msg   STRING
}

libgit2 function git_trace_set GIT_ERROR_CODE {
    level GIT_TRACE_LEVEL_T
    cb    pointer.git_trace_cb
}
