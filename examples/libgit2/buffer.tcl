# Copyright (c) 2022 Ashok P. Nadkarni
# All rights reserved.
# See LICENSE file for details.

# Based on libgit2/include/buffer.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_buf {
    ptr      {pointer unsafe nullok {default NULL}}
    reserved {size_t {default 0}}
    size     {size_t {default 0}}
}

::cffi::alias define PBUF pointer.git_buf

libgit2 function git_buf_dispose void {buffer PBUF}

if {! [lg2_abi_vsatisfies 1.4]} {
    # These functions are NOT supported in 1.4 and later

    # In the functions below, we could pass a struct.git_buf byref so it is seen
    # as a dictionary at the Tcl level. However, that is likely to be error-prone
    # if the application is not careful about buffer management and value duplication.
    # So the parameters are typed as pointers to make them more opaque and
    # reference as opposed to value based.

    libgit2 functions {
        git_buf_grow GIT_ERROR_CODE {
            buffer      PBUF
            target_size size_t
        }
        git_buf_set GIT_ERROR_CODE {
            buffer  PBUF
            data    bytes[datalen]
            datalen size_t
        }
        git_buf_is_binary int {
            buffer  PBUF
        }
        git_buf_contains_nul int {
            buffer  PBUF
        }
    }
}

proc lg2_buf_tostr {pBuf} {
    set buf [git_buf fromnative! $pBuf]
    dict with buf {
        if {$size == 0 || [::cffi::pointer isnull $ptr]} {
            return ""
        }
        return [encoding convertfrom utf-8 [::cffi::memory tobinary! $ptr $size]]
    }
}
