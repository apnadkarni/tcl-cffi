# Based on libgit2/include/credential_helpers.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_credential_userpass_payload {
    username STRING
    password STRING
}

libgit2 function git_credential_userpass GIT_ERROR_CODE {
    creds         {PCREDENTIAL retval}
    url           STRING
    user_from_url STRING
    allowed_types uint
    payload       {struct.git_credential_userpass_payload byref}
}
