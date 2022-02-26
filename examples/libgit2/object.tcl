# Based on libgit2/include/object.h
#
# This file should be sourced into whatever namespace commands should
# be created in.

# git_object_owner -
# TBD - {PREPOSITORY is marked unsafe because repository is one already allocated}

libgit2 functions {
    git_object_lookup GIT_ERROR_CODE {
            pObj   {POBJECT retval}
            pRepo  PREPOSITORY
            id     {struct.git_oid byref}
            type   GIT_OBJECT_T
    }
    git_object_lookup_prefix GIT_ERROR_CODE {
            pObj   {POBJECT retval}
            pRepo  PREPOSITORY
            id     {struct.git_oid out}
            len    size_t
            type   GIT_OBJECT_T
    }
    git_object_lookup_bypath GIT_ERROR_CODE {
            pObj    {POBJECT retval}
            treeish POBJECT
            path    STRING
            type    GIT_OBJECT_T
    }
    git_object_id {struct.git_oid byref} {
            pObj POBJECT
    }
    git_object_short_id GIT_ERROR_CODE {
            pBuf PBUF
            pObj POBJECT
    }
    git_object_type GIT_OBJECT_T {
            pObj POBJECT
    }
    git_object_owner PREPOSITORY {
            pObj POBJECT
    }
    git_object_free void {
            pObj {POBJECT nullok dispose}
    }
    git_object_type2string STRING {
            type GIT_OBJECT_T
    }
    git_object_string2type GIT_OBJECT_T {
            str STRING
    }
    git_object_typeisloose int {
            type GIT_OBJECT_T
    }
    git_object_peel GIT_ERROR_CODE {
            pPeeledObj {POBJECT retval}
            pObj       POBJECT
            target_type GIT_OBJECT_T
    }
    git_object_dup GIT_ERROR_CODE {
            pDest   {POBJECT retval}
            pSource POBJECT
    }
}
