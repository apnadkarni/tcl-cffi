# Based on libgit2/include/transport.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::prototype function git_transport_message_cb int {
    str bytes[len]
    len int
    payload {pointer unsafe}
}

::cffi::prototype function git_transport_cb int {
    pTransport {PTRANSPORT out}
    pOwner     PREMOTE
    param      {pointer unsafe}
}

