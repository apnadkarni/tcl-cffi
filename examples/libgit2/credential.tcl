# Based on libgit2/include/credential.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::enum flags git_credential_t {
    GIT_CREDENTIAL_USERPASS_PLAINTEXT
    GIT_CREDENTIAL_SSH_KEY
    GIT_CREDENTIAL_SSH_CUSTOM
    GIT_CREDENTIAL_DEFAULT
    GIT_CREDENTIAL_SSH_INTERACTIVE
    GIT_CREDENTIAL_USERNAME
    GIT_CREDENTIAL_SSH_MEMORY
}
::cffi::alias define GIT_CREDENTIAL_T {int {enum git_credential_t}}

::cffi::alias define PCREDENTIAL {pointer.git_credential}
::cffi::alias define PCREDENTIAL_USERPASS_PLAINTEXT {pointer.git_credential_userpass_plaintext}
::cffi::alias define PCREDENTIAL_DEFAULT {pointer.git_credential_default}
::cffi::alias define PCREDENTIAL_SSH_KEY {pointer.git_credential_ssh_key}
::cffi::alias define PCREDENTIAL_SSH_INTERACTIVE {pointer.git_credential_ssh_interactive}
::cffi::alias define PCREDENTIAL_SSH_CUSTOM {pointer.git_credential_ssh_custom}

::cffi::prototype function git_credential_acquire_cb int {
    ppCred             {pointer unsafe}
    url               STRING
    username_from_url {STRING nullok}
    allowed_types     {GIT_CREDENTIAL_T bitmask}
    payload           {pointer unsafe}
}

::cffi::prototype function git_credentials_ssh_interactive_cb void {
    name            STRING
    name_len        int
    instruction     STRING
    instruction_len int
    num_prompts     int
    prompts         {pointer.LIBSSH2_USERAUTH_KBDINT_PROMPT   unsafe}
    responses       {pointer.LIBSSH2_USERAUTH_KBDINT_RESPONSE unsafe}
    abstract        {pointer unsafe}
}

::cffi::prototype function git_credential_sign_cb int {
    session {pointer.LIBSSH2_SESSION unsafe}
    pSig {pointer unsafe}
    pSig_len {pointer.size_t unsafe}
    data {pointer unsafe}
    data_len size_t
    abstract {pointer unsafe}
}

libgit2 functions {
    git_credential_free void {
        pCred {PCREDENTIAL nullok dispose}
    }
    git_credential_has_username int {
        pCred PCREDENTIAL
    }
    git_credential_get_username {STRING nullok} {
        pCred PCREDENTIAL
    }
    git_credential_userpass_plaintext_new GIT_ERROR_CODE {
        ppCred {PCREDENTIAL out}
        username STRING
        password STRING
    }
    git_credential_default_new GIT_ERROR_CODE {
        ppCred {PCREDENTIAL out}
    }
    git_credential_username_new GIT_ERROR_CODE {
        ppCred {PCREDENTIAL out}
        username STRING
    }
    git_credential_ssh_key_new GIT_ERROR_CODE {
        ppCred {PCREDENTIAL out}
        username STRING
        publickeypath STRING
        privatekeypath STRING
        passphrase STRING
    }
    git_credential_ssh_key_memory_new GIT_ERROR_CODE {
        ppCred {PCREDENTIAL out}
        username STRING
        publickey STRING
        privatekey STRING
        passphrase STRING
    }
    git_credential_ssh_interactive_new GIT_ERROR_CODE {
        ppCred {PCREDENTIAL out}
        username STRING
        prompt_cb pointer.git_credential_ssh_interactive_cb
        payload {pointer unsafe}
    }
    git_credential_ssh_key_from_agent GIT_ERROR_CODE {
        ppCred {PCREDENTIAL out}
        username STRING
    }
    git_credential_ssh_custom_new GIT_ERROR_CODE {
        ppCred {PCREDENTIAL out}
        username STRING
        publickey binary
        publickey_len size_t
        sign_cb pointer.git_credential_sign_cb
        payload {pointer unsafe}
    }
}
