# Based on libgit2/include/net.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

variable GIT_DEFAULT_PORT 9418

::cffi::enum sequence git_direction {
    GIT_DIRECTION_FETCH
    GIT_DIRECTION_PUSH
}
::cffi::alias define GIT_DIRECTION {int {enum git_direction}}
