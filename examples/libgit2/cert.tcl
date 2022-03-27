# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/net.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum sequence git_cert_t {
    GIT_CERT_NONE
    GIT_CERT_X509
    GIT_CERT_HOSTKEY_LIBSSH2
    GIT_CERT_STRARRAY
}
::cffi::alias define GIT_CERT_T {int {enum git_cert_t}}

::cffi::Struct create git_cert {
    cert_type GIT_CERT_T
}

::cffi::enum flags git_cert_ssh_t {
    GIT_CERT_SSH_MD5
    GIT_CERT_SSH_SHA1
    GIT_CERT_SSH_SHA256
    GIT_CERT_SSH_RAW
}
::cffi::alias define GIT_CERT_SSH_T {int {enum git_cert_ssh_t}}

::cffi::prototype function git_transport_certificate_check_cb int {
    cert    {struct.git_cert byref}
    valid   int
    host    STRING
    payload {pointer  unsafe}
}

::cffi::enum sequence git_cert_ssh_raw_type_t {
    GIT_CERT_SSH_RAW_TYPE_UNKNOWN
    GIT_CERT_SSH_RAW_TYPE_RSA
    GIT_CERT_SSH_RAW_TYPE_DSS
    GIT_CERT_SSH_RAW_TYPE_KEY_ECDSA_256
    GIT_CERT_SSH_RAW_TYPE_KEY_ECDSA_384
    GIT_CERT_SSH_RAW_TYPE_KEY_ECDSA_521
    GIT_CERT_SSH_RAW_TYPE_KEY_ED25519
}
::cffi::alias define GIT_CERT_SSH_RAW_TYPE_T {int {enum git_cert_ssh_raw_type_t}}

::cffi::Struct create git_cert_hostkey {
    parent      struct.git_cert
    type        GIT_CERT_SSH_T
    hash_md5    uchar[16]
    hash_sha1   uchar[20]
    hash_sha256 uchar[32]
    raw_type    GIT_CERT_SSH_RAW_TYPE_T
    hostkey     {pointer unsafe}
    hostkey_len size_t
}

::cffi::Struct create git_cert_x509 {
    parent struct.git_cert
    data   {pointer unsafe}
    len    size_t
}
