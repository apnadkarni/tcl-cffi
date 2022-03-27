# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

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

::cffi::Struct create git_remote_head {
    local         int
    oid           struct.git_oid
    loid          struct.git_oid
    name          {pointer unsafe}
    symref_target {pointer unsafe}
}
