# Based on libgit2/include/transport.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::prototype function git_transport_message_cb int {
    str {pointer unsafe}
    len int
    payload CB_PAYLOAD
}

::cffi::prototype function git_transport_cb int {
    pTransport {pointer unsafe}
    pOwner     PREMOTE
    param      {pointer unsafe}
}

