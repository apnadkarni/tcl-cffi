# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/message.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_message_trailer {
    key   {pointer unsafe}
    value {pointer unsafe}
}

::cffi::Struct create git_message_trailer_array {
    trailers {pointer.git_message_trailer unsafe}
    count size_t
    _trailer_block {pointer unsafe}
} -clear

libgit2 functions {
    git_message_prettify GIT_ERROR_CODE {
        buf {PBUF retval}
        message STRING
        strip_comments int
        comment_char uchar
    }
    git_message_trailers GIT_ERROR_CODE {
        parr    pointer.git_message_trailer_array
        message STRING
    }
    git_message_trailer_array_free void {
        parr pointer.git_message_trailer_array
    }
}
