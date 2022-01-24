/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

static CffiResult CffiTypeToLibffiType(Tcl_Interp *ip,
                                       CffiABIProtocol abi,
                                       CffiTypeParseMode parseMode,
                                       CffiTypeAndAttrs *typeAttrsP,
                                       ffi_type **ffiTypePP);
static CffiResult
CffiLibffiTranslateStruct(Tcl_Interp *ip,
                          CffiABIProtocol abi,
                          CffiTypeParseMode parseMode,
                          CffiStruct *structP,
                          ffi_type **ffiTypePP)
{
    CffiLibffiStruct *libffiStructP;
    int i;

    /* See if we have already translated the struct for this ABI. */
    for (libffiStructP = structP->libffiTypes; libffiStructP;
         libffiStructP = libffiStructP->nextP) {
        if (libffiStructP->abi == abi) {
            *ffiTypePP = &libffiStructP->ffiType;
            return TCL_OK;
        }
    }
    /*
     * Do not have a translated struct. Construct one. We allocate
     * additional space to hold the array of element pointers for the type.
     * The size of this array is equal to number of parameters plus a slot
     * for the terminating NULL. Note the struct definition itself already
     * holds one slot so just allocate extra space corresponding to the
     * parameter count.
     */
    libffiStructP = ckalloc(sizeof(*libffiStructP)
                            + (structP->nFields * sizeof(ffi_type *)));
    libffiStructP->ffiType.size = 0;
    libffiStructP->ffiType.alignment = 0;
    libffiStructP->ffiType.type = FFI_TYPE_STRUCT;
    libffiStructP->ffiType.elements = libffiStructP->ffiFieldTypes;

    for (i = 0; i < structP->nFields; ++i) {
        if (CffiTypeToLibffiType(ip,
                                 abi,
                                 parseMode,
                                 &structP->fields[i].fieldType,
                                 &libffiStructP->ffiFieldTypes[i]) != TCL_OK) {
            ckfree(libffiStructP);
            return TCL_ERROR;
        }
    }
    libffiStructP->ffiFieldTypes[structP->nFields] = NULL; /* Terminator */

    *ffiTypePP = &libffiStructP->ffiType;
    return TCL_OK;
}

static CffiResult
CffiTypeToLibffiType(Tcl_Interp *ip,
                     CffiABIProtocol abi,
                     CffiTypeParseMode parseMode,
                     CffiTypeAndAttrs *typeAttrsP,
                     ffi_type **ffiTypePP)
{
    if (typeAttrsP->flags & CFFI_F_ATTR_BYREF) {
        *ffiTypePP = &ffi_type_pointer;
        return TCL_OK;
    }
    if (CffiTypeIsArray(&typeAttrsP->dataType)) {
        return Tclh_ErrorGeneric(
            ip,
            NULL,
            "The libffi backend does not support arrays by value. Define as "
            "struct with corresponding number of fields as a workaround.");
    }
    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_VOID:
        if (parseMode != CFFI_F_TYPE_PARSE_RETURN)
            goto invalid_type;
        *ffiTypePP = &ffi_type_void;
        break;
    case CFFI_K_TYPE_SCHAR : *ffiTypePP = &ffi_type_schar; break;
    case CFFI_K_TYPE_UCHAR : *ffiTypePP = &ffi_type_uchar; break;
    case CFFI_K_TYPE_SHORT : *ffiTypePP = &ffi_type_sshort; break;
    case CFFI_K_TYPE_USHORT: *ffiTypePP = &ffi_type_ushort; break;
    case CFFI_K_TYPE_INT   : *ffiTypePP = &ffi_type_sint; break;
    case CFFI_K_TYPE_UINT  : *ffiTypePP = &ffi_type_uint; break;
    case CFFI_K_TYPE_LONG  : *ffiTypePP = &ffi_type_slong; break;
    case CFFI_K_TYPE_ULONG : *ffiTypePP = &ffi_type_ulong; break;
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
    case CFFI_K_TYPE_FLOAT    : *ffiTypePP = &ffi_type_float; break;
    case CFFI_K_TYPE_DOUBLE   : *ffiTypePP = &ffi_type_double; break;
    case CFFI_K_TYPE_ASTRING  : /* Fallthru - treated as pointer */
    case CFFI_K_TYPE_UNISTRING: /* Fallthru - treated as pointer */
    case CFFI_K_TYPE_BINARY   : /* Fallthru - treated as pointer */
    case CFFI_K_TYPE_POINTER  : *ffiTypePP = &ffi_type_pointer; break;
    case CFFI_K_TYPE_STRUCT:
        if (CffiLibffiTranslateStruct(
                ip, abi, parseMode, typeAttrsP->dataType.u.structP, ffiTypePP)
            != TCL_OK)
            return TCL_ERROR;
        break;
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
                                   protoP->abi,
                                   CFFI_F_TYPE_PARSE_PARAM,
                                   &protoP->params[i].typeAttrs,
                                   &ffiTypePP[i]);
        if (ret != TCL_OK)
            goto error_handler;
    }
    /* Ditto for the return type */
    ret = CffiTypeToLibffiType(ip,
                               protoP->abi,
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

static CffiResult
CffiLibffiCallbackArgToObj(CffiCallback *cbP,
                           ffi_cif *cifP,
                           int argIndex,
                           void **args,
                           Tcl_Obj **argObjP)
{
    void *valueP;
    CffiValue val;
    CffiTypeAndAttrs *typeAttrsP = &cbP->protoP->params[argIndex].typeAttrs;

    CFFI_ASSERT(CffiTypeIsNotArray(&typeAttrsP->dataType));

#define EXTRACTINT_(type_, fld_)                                               \
    do {                                                                       \
        if (typeAttrsP->flags & CFFI_F_ATTR_BYREF) {                           \
            /* arg is actually a pointer to the value, not the value itself */ \
            valueP = *(void **)args[argIndex];                                 \
        }                                                                      \
        else {                                                                 \
            /* libffi promotes smaller integers to ffi_arg */                  \
            if (sizeof(type_) <= sizeof(ffi_arg)) {                            \
                val.u.fld_ = (type_) * (ffi_arg *)args[argIndex];              \
                valueP     = &val.u.fld_;                                      \
            }                                                                  \
            else                                                               \
                valueP = args[argIndex];                                       \
        }                                                                      \
    } while (0)

#define EXTRACT_(type_, fld_)                                                  \
    do {                                                                       \
        if (typeAttrsP->flags & CFFI_F_ATTR_BYREF) {                           \
            /* arg is actually a pointer to the value, not the value itself */ \
            valueP = *(void **)args[argIndex];                                 \
        }                                                                      \
        else {                                                                 \
            valueP = args[argIndex];                                           \
        }                                                                      \
    } while (0)

    /* TBD - big endian? */

    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_SCHAR    : EXTRACTINT_(signed char, schar); break;
    case CFFI_K_TYPE_UCHAR    : EXTRACTINT_(unsigned char, uchar); break;
    case CFFI_K_TYPE_SHORT    : EXTRACTINT_(short, sshort); break;
    case CFFI_K_TYPE_USHORT   : EXTRACTINT_(unsigned short, ushort); break;
    case CFFI_K_TYPE_INT      : EXTRACTINT_(int, sint); break;
    case CFFI_K_TYPE_UINT     : EXTRACTINT_(unsigned int, uint); break;
    case CFFI_K_TYPE_LONG     : EXTRACTINT_(long, slong); break;
    case CFFI_K_TYPE_ULONG    : EXTRACTINT_(unsigned long, ulong); break;
    case CFFI_K_TYPE_LONGLONG : EXTRACTINT_(long long, slonglong); break;
    case CFFI_K_TYPE_ULONGLONG: EXTRACTINT_(unsigned long long, ulonglong); break;

    case CFFI_K_TYPE_FLOAT: EXTRACT_(float, flt); break;
    case CFFI_K_TYPE_DOUBLE: EXTRACT_(double, dbl); break;

    case CFFI_K_TYPE_POINTER:
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
        EXTRACT_(void *, ptr);
        break;

    case CFFI_K_TYPE_STRUCT:
        CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_BYREF);
        /* args[argIndex] is the location of the pointer to the struct */
        valueP = *(void **)args[argIndex];
        return CffiNativeValueToObj(
            cbP->ipCtxP->interp, typeAttrsP, valueP, -1, argObjP);

    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
    case CFFI_K_TYPE_BINARY:
    case CFFI_K_TYPE_VOID:
    default:
        return Tclh_ErrorInvalidValue(
            cbP->ipCtxP->interp, NULL, "Invalid type for use in callbacks.");
    }

    return CffiNativeScalarToObj(
        cbP->ipCtxP->interp, typeAttrsP, valueP, argObjP);

#undef EXTRACT_
#undef EXTRACT_INT_
}


/* Function: CffiLibffiCallbackStoreResult
 * Stores the result of a callback in libffi return location.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * typeAttrsP - type attributes of value
 * valueObj - value to store
 * retP - location where to store
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on error
 */
static CffiResult
CffiLibffiCallbackStoreResult(CffiInterpCtx *ipCtxP,
                              CffiTypeAndAttrs *typeAttrsP,
                              Tcl_Obj *valueObj,
                              void *retP)
{
    double dbl;

#define RETURNINT_(type_)                                                \
    do {                                                                 \
        Tcl_WideInt wide;                                                \
        CHECK(CffiIntValueFromObj(ipCtxP, typeAttrsP, valueObj, &wide)); \
        /* libffi promotes smaller integers to ffi_arg */                \
        if (sizeof(type_) <= sizeof(ffi_arg))                            \
            *(ffi_arg *)retP = (ffi_arg)wide;                            \
        else                                                             \
            *(type_ *)retP = (type_)wide;                                \
    } while (0)

    CFFI_ASSERT(CffiTypeIsNotArray(&typeAttrsP->dataType));
    CFFI_ASSERT((typeAttrsP->flags & CFFI_F_ATTR_BYREF) == 0);

    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_VOID: break;
    case CFFI_K_TYPE_SCHAR    : RETURNINT_(signed char); break;
    case CFFI_K_TYPE_UCHAR    : RETURNINT_(unsigned char); break;
    case CFFI_K_TYPE_SHORT    : RETURNINT_(short); break;
    case CFFI_K_TYPE_USHORT   : RETURNINT_(unsigned short); break;
    case CFFI_K_TYPE_INT      : RETURNINT_(int); break;
    case CFFI_K_TYPE_UINT     : RETURNINT_(unsigned int); break;
    case CFFI_K_TYPE_LONG     : RETURNINT_(long); break;
    case CFFI_K_TYPE_ULONG    : RETURNINT_(unsigned long); break;
    case CFFI_K_TYPE_LONGLONG : RETURNINT_(long long); break;
    case CFFI_K_TYPE_ULONGLONG: RETURNINT_(unsigned long long); break;
    case CFFI_K_TYPE_FLOAT:
        CHECK(Tcl_GetDoubleFromObj(ipCtxP->interp, valueObj, &dbl));
        *(float *)retP = (float)dbl;
        break;
    case CFFI_K_TYPE_DOUBLE:
        CHECK(Tcl_GetDoubleFromObj(ipCtxP->interp, valueObj, (double *)retP));
        break;
    case CFFI_K_TYPE_POINTER:
        CHECK(
            Tclh_PointerUnwrap(ipCtxP->interp, valueObj, (void **)retP, NULL));
        break;

    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
    case CFFI_K_TYPE_STRUCT:
    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
    case CFFI_K_TYPE_BINARY:
    default:
        return Tclh_ErrorInvalidValue(
            ipCtxP->interp, NULL, "Invalid type for use as callback return.");
    }
    return TCL_OK;
#undef RETURNINT_
}

/* Function: CffiLibffiCallback
 * Called from libffi to invoke callback functions
 *
 * Parameters:
 * cifP - libffi call descriptor
 * retP - location to store return value
 * args - arguments to this function
 * userdata - Cffi closure context
 *
 * This prototype should match that specified in libffi ffi_prep_closure_loc
 * function.
 */
void
CffiLibffiCallback(ffi_cif *cifP, void *retP, void **args, void *userdata)
{
    Tcl_Obj **evalObjs;
    int nEvalObjs;
    Tcl_Obj **cmdObjs;
    int nCmdObjs;
    CffiResult ret;
    Tcl_Obj *resultObj;
    int i;
    CffiCallback *cbP = (CffiCallback *)userdata;
    CffiInterpCtx *ipCtxP = cbP->ipCtxP;

    CFFI_ASSERT(cifP->nargs == cbP->protoP->nParams);
    CFFI_ASSERT(cifP == cbP->protoP->cifP);
    if (Tcl_ListObjGetElements(NULL, cbP->cmdObj, &nCmdObjs, &cmdObjs)
                != TCL_OK) {
        /* TBD - set *retP to error by some convention */
        return;
    }
    nEvalObjs = nCmdObjs + cifP->nargs;
    /*
     * Note: CffiFunctionCall would have already set a memlifo mark so
     * we do not have to over here for releasing memlifo memory
     */
    evalObjs =
        MemLifoAlloc(&ipCtxP->memlifo, nEvalObjs * sizeof(Tcl_Obj *));


    /* Translate arguments passed by libffi to Tcl_Objs */
    for (i = 0; i < cbP->protoP->nParams; ++i) {
        ret = CffiLibffiCallbackArgToObj(
            cbP, cifP, i, args, &evalObjs[nCmdObjs + i]);
        if (ret != TCL_OK) {
            int j;
            for (j = 0; j < i; ++j)
                Tcl_DecrRefCount(evalObjs[nCmdObjs + j]);
            goto vamoose;
        }
        Tcl_IncrRefCount(evalObjs[nCmdObjs + i]);
    }

    /*
    * Only incr refs for cmdObjs. argument objs at index nCmdObjs+...
    * already done above
    */
    for (i = 0; i < nCmdObjs; ++i) {
        evalObjs[i] = cmdObjs[i];
        Tcl_IncrRefCount(evalObjs[i]);
    }
    /* Note: evaluating in current context, not global context */
    ret = Tcl_EvalObjv(ipCtxP->interp, nEvalObjs, evalObjs, 0);
    for (i = 0; i < nEvalObjs; ++i) {
        Tcl_DecrRefCount(evalObjs[i]);
    }

vamoose:
    /* May come here on an error or success */
    if (ret == TCL_OK) {
        /* Try converting result to a native value */
        resultObj = Tcl_GetObjResult(ipCtxP->interp);
        ret       = CffiLibffiCallbackStoreResult(
            ipCtxP, &cbP->protoP->returnType.typeAttrs, resultObj, retP);
        /* Do not want callback result percolating up the stack. */
        Tcl_ResetResult(ipCtxP->interp);
    }
    if (ret != TCL_OK) {
        /*
         * Either original eval raised error or could not convert above.
         * Store the designated error value. Should not fail since object
         * value should have been checked at callback definition time. In
         * any case, no way to further signal that something has gone wrong.
         */
        (void)CffiLibffiCallbackStoreResult(ipCtxP,
                                            &cbP->protoP->returnType.typeAttrs,
                                            cbP->errorResultObj,
                                            retP);
    }
}

static int
CffiLibffiClosureDeleteEntry(Tcl_HashTable *htP, Tcl_HashEntry *heP, ClientData unused)
{
    CffiCallback *cbP = Tcl_GetHashValue(heP);
    if (cbP)
        CffiCallbackCleanupAndFree(cbP);
    return 1;
}

void
CffiLibffiFinit(CffiInterpCtx *ipCtxP)
{
    Tclh_HashIterate(
        &ipCtxP->callbackClosures, CffiLibffiClosureDeleteEntry, NULL);
    Tcl_DeleteHashTable(&ipCtxP->callbackClosures);
}

CffiResult
CffiLibffiInit(CffiInterpCtx *ipCtxP)
{
    /* Table mapping callback closure function addresses to CffiCallback */
    Tcl_InitHashTable(&ipCtxP->callbackClosures, TCL_ONE_WORD_KEYS);
    return TCL_OK;
}
