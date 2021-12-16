/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

static CffiResult
CffiTypeToLibffiType(Tcl_Interp *ip,
                     CffiTypeParseMode parseMode,
                     CffiTypeAndAttrs *typeAttrsP,
                     ffi_type **ffiTypePP)
{
    if (typeAttrsP->flags & CFFI_F_ATTR_BYREF) {
        *ffiTypePP = &ffi_type_pointer;
        return TCL_OK;
    }
    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_VOID:
        if (parseMode != CFFI_F_TYPE_PARSE_RETURN)
            goto invalid_type;
        *ffiTypePP = &ffi_type_void;
        break;
    case CFFI_K_TYPE_SCHAR: *ffiTypePP = &ffi_type_schar; break;
    case CFFI_K_TYPE_UCHAR: *ffiTypePP = &ffi_type_uchar; break;
    case CFFI_K_TYPE_SHORT: *ffiTypePP = &ffi_type_sshort; break;
    case CFFI_K_TYPE_USHORT: *ffiTypePP = &ffi_type_ushort; break;
    case CFFI_K_TYPE_INT: *ffiTypePP = &ffi_type_sint; break;
    case CFFI_K_TYPE_UINT: *ffiTypePP = &ffi_type_uint; break;
    case CFFI_K_TYPE_LONG: *ffiTypePP = &ffi_type_slong; break;
    case CFFI_K_TYPE_ULONG: *ffiTypePP = &ffi_type_ulong; break;
    case CFFI_K_TYPE_LONGLONG:
        if (sizeof(long long) == 8)
            *ffiTypePP = &ffi_type_sint64;
        else if (sizeof(long long) == 4)
            *ffiTypePP = &ffi_type_sint32;
        else
            goto invalid_type;
        break;
    case CFFI_K_TYPE_ULONGLONG:
        if (sizeof(unsigned long long) == 8)
            *ffiTypePP = &ffi_type_uint64;
        else if (sizeof(unsigned long long) == 4)
            *ffiTypePP = &ffi_type_uint32;
        else
            goto invalid_type;
        break;
    case CFFI_K_TYPE_FLOAT: *ffiTypePP = &ffi_type_float; break;
    case CFFI_K_TYPE_DOUBLE: *ffiTypePP = &ffi_type_double; break;
    case CFFI_K_TYPE_ASTRING: /* Fallthru - treated as pointer */
    case CFFI_K_TYPE_UNISTRING: /* Fallthru - treated as pointer */
    case CFFI_K_TYPE_BINARY: /* Fallthru - treated as pointer */
    case CFFI_K_TYPE_POINTER: *ffiTypePP = &ffi_type_pointer; return TCL_OK;
    case CFFI_K_TYPE_STRUCT:
        /* TBD - pass by value not supported yet. BYREF handled earlier */
    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
    default:
        /* Either BYREF should have been set or some unknown type */
        goto invalid_type;
    }

    return TCL_OK;

invalid_type:
    return Tclh_ErrorInvalidValue(
        ip, NULL, "Unknown type or invalid type for context.");
}

CffiResult CffiLibffiInitProtoCif(Tcl_Interp *ip, CffiProto *protoP)
{
    ffi_cif *cifP;
    ffi_type **ffiTypePP;
    CffiResult ret;
    int i;

    if (protoP->cifP)
        return TCL_OK; /* Already done */

    cifP = ckalloc(sizeof(*cifP)
                   + protoP->nParams * sizeof(ffi_type *) /* Argument types */
                   + sizeof(ffi_type *)                   /* Return type slot */
    );

    /* Map argument CFFI types to libffi types */
    ffiTypePP = (ffi_type **)(cifP + 1);
    for (i = 0; i < protoP->nParams; ++i) {
        ret = CffiTypeToLibffiType(ip,
                                   CFFI_F_TYPE_PARSE_PARAM,
                                   &protoP->params[i].typeAttrs,
                                   &ffiTypePP[i]);
        if (ret != TCL_OK)
            goto error_handler;
    }
    /* Ditto for the return type */
    ret = CffiTypeToLibffiType(ip,
                               CFFI_F_TYPE_PARSE_RETURN,
                               &protoP->returnType.typeAttrs,
                               &ffiTypePP[protoP->nParams]);
    if (ret != TCL_OK)
        goto error_handler;

    if (ffi_prep_cif(cifP,
                     protoP->abi,
                     protoP->nParams,
                     ffiTypePP[protoP->nParams],
                     ffiTypePP)
        != FFI_OK) {
        if (ip) {
            Tcl_SetResult(ip, "Internal error: Could not intialize libffi cif.", TCL_STATIC);
        }
        goto error_handler;
    }

    protoP->cifP = cifP;
    return TCL_OK;

error_handler:
    ckfree(cifP);
    return TCL_ERROR;
}


