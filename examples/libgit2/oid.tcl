# Based on libgit2/include/oid.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

::cffi::Struct create git_oid {
    id bytes[20]
}

::cffi::alias define POID pointer.git_oid

libgit2 functions {
    git_oid_fromstr GIT_ERROR_CODE {
        oid {struct.git_oid out byref}
        str uchar[40]
    }
    git_oid_fromstrp GIT_ERROR_CODE {
        oid {struct.git_oid out byref}
        str STRING
    }
    git_oid_fromstrn GIT_ERROR_CODE {
        oid {struct.git_oid out byref}
        str chars[length]
        length size_t
    }
    git_oid_fromraw GIT_ERROR_CODE {
        oid {struct.git_oid out byref}
        raw bytes[20]
    }
    git_oid_nfmt GIT_ERROR_CODE {
        hex {chars[n] out}
        n   size_t
        oid {struct.git_oid byref}
    }
    git_oid_tostr_s STRING {
        oid {struct.git_oid byref}
    }
    git_oid_tostr STRING {
        str {chars[n] out}
        n   size_t
        oid {struct.git_oid byref}
    }
    git_oid_cpy GIT_ERROR_CODE {
        dst {struct.git_oid out}
        src {struct.git_oid byref}
    }
    git_oid_cmp int {
        a {struct.git_oid byref}
        b {struct.git_oid byref}
    }
    git_oid_equal int {
        a {struct.git_oid byref}
        b {struct.git_oid byref}
    }
    git_oid_ncmp int {
        a {struct.git_oid byref}
        b {struct.git_oid byref}
        len size_t
    }
    git_oid_streq int {
        id {struct.git_oid byref}
        str STRING
    }
    git_oid_strcmp int {
        id {struct.git_oid byref}
        str STRING
    }
    git_oid_is_zero int {
        id {struct.git_oid byref}
    }
}

# Note: git_oid_fmt retuns *exactly* 40 characters but NOT null terminated so
# we cannot type parameter hex as chars[40]
libgit2 function git_oid_fmt GIT_ERROR_CODE {
    hex {bytes[40] out}
    oid {struct.git_oid byref}
}
# Note: git_oid_pathfmt retuns *exactly* 41 characters but NOT null terminated so
# we cannot type parameter hex as chars[41]
libgit2 function git_oid_pathfmt GIT_ERROR_CODE {
    hex {bytes[41] out}
    oid {struct.git_oid byref}
}


::cffi::alias define POID_SHORTEN pointer.git_oid_shorten

libgit2 functions {
    git_oid_shorten_new POID_SHORTEN {min_length size_t}
    git_oid_shorten_add GIT_ERROR_CODE {
        os POID_SHORTEN
        text_id STRING
    }
    git_oid_shorten_free void {
        os {POID_SHORTEN nullok dispose}
    }
}
