# Based on libgit2/include/proxy.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum sequence git_proxy_t {
    GIT_PROXY_NONE
    GIT_PROXY_AUTO
    GIT_PROXY_SPECIFIED
}
::cffi::alias define GIT_PROXY_T {int {enum git_proxy_t}}

::cffi::Struct create git_proxy_options {
    version uint
    type GIT_PROXY_T
    url {STRING nullifempty}
    credentials {pointer.git_credential_acquire_cb nullok}
    certificate_check {pointer.git_transport_certificate_check_cb nullok}
    payload CB_PAYLOAD
} -clear

libgit2 function git_proxy_options_init GIT_ERROR_CODE {
    opts {struct.git_proxy_options retval}
    version uint
}

