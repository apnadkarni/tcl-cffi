/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

#ifdef CFFI_ENABLE_CALLBACKS

static void
CffiCallbackCleanup(CffiCallback *cbP)
{
    if (cbP) {
        if (cbP->protoP)
            CffiProtoUnref(cbP->protoP);
        if (cbP->cmdObj)
            Tcl_DecrRefCount(cbP->cmdObj);
        if (cbP->errorResultObj)
            Tcl_DecrRefCount(cbP->errorResultObj);
        if (cbP->ffiExecutableAddress) {
            Tcl_HashEntry *heP;
            heP = Tcl_FindHashEntry(&cbP->ipCtxP->callbackClosures,
                                    cbP->ffiExecutableAddress);
            CFFI_ASSERT(Tcl_GetHashValue(heP) == cbP);
            if (heP)
                Tcl_DeleteHashEntry(heP);
        }
        if (cbP->ffiClosureP)
            ffi_closure_free(cbP->ffiClosureP);
    }
}

void CffiCallbackCleanupAndFree(CffiCallback *cbP)
{
    CffiCallbackCleanup(cbP);
    ckfree(cbP);
}

static CffiCallback *
CffiCallbackAllocAndInit(CffiInterpCtx *ipCtxP,
                         CffiProto *protoP,
                         Tcl_Obj *cmdObj,
                         Tcl_Obj *errorResultObj)
{
    CffiCallback *cbP;

    cbP = ckalloc(sizeof(*cbP));
    cbP->ipCtxP = ipCtxP;
    cbP->protoP = protoP;
    protoP->nRefs += 1;
    cbP->cmdObj = cmdObj;
    Tcl_IncrRefCount(cmdObj);
    cbP->ffiClosureP = NULL;
    cbP->ffiExecutableAddress = NULL;
    cbP->errorResultObj = errorResultObj;
    if (errorResultObj)
        Tcl_IncrRefCount(errorResultObj);
    return cbP;
}

/* Function: CffiCallbackCheckType
 * Checks whether a type is suitable for use in a callback
 *
 * Parameters:
 * ipCtxP - interpreter context
 * paramP - parameter.
 * isReturn - 0 if param, non-0 if return type
 * valueObj - if non-NULL, checked against the type if isReturn is true.
 *
 * Returns:
 * *TCL_OK* if type is ok, else *TCL_ERROR*
 */
static CffiResult
CffiCallbackCheckType(CffiInterpCtx *ipCtxP,
                      const CffiParam *paramP,
                      int isReturn,
                      Tcl_Obj *valueObj)
{
    const CffiTypeAndAttrs *typeAttrsP = &paramP->typeAttrs;

#define CFFI_INVALID_CALLBACK_ATTR_FLAGS                                \
    (CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT | CFFI_F_ATTR_REQUIREMENT_MASK \
     | (CFFI_F_ATTR_SAFETY_MASK & ~CFFI_F_ATTR_UNSAFE)                  \
     | CFFI_F_ATTR_ERROR_MASK | CFFI_F_ATTR_STOREONERROR                \
     | CFFI_F_ATTR_STOREALWAYS | CFFI_F_ATTR_STRUCTSIZE)

    if (typeAttrsP->flags & CFFI_INVALID_CALLBACK_ATTR_FLAGS) {
        return Tclh_ErrorInvalidValue(
            ipCtxP->interp,
            paramP->nameObj,
            "An annotation in the type definition is not "
            "suitable for use in callbacks.");
    }

    if (typeAttrsP->dataType.count != 0) {
        return Tclh_ErrorInvalidValue(
            ipCtxP->interp,
            paramP->nameObj,
            "Array parameters not permitted in callback functions.");
    }
    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_SCHAR:
    case CFFI_K_TYPE_UCHAR:
    case CFFI_K_TYPE_SHORT:
    case CFFI_K_TYPE_USHORT:
    case CFFI_K_TYPE_INT:
    case CFFI_K_TYPE_UINT:
    case CFFI_K_TYPE_LONG:
    case CFFI_K_TYPE_ULONG:
    case CFFI_K_TYPE_LONGLONG:
    case CFFI_K_TYPE_ULONGLONG:
        if (isReturn && valueObj)  {
            /* Check that the value is valid for the type */
            Tcl_WideInt wide;
            CHECK(CffiIntValueFromObj(ipCtxP, typeAttrsP, valueObj, &wide));
        }
        return TCL_OK;
    case CFFI_K_TYPE_FLOAT:
    case CFFI_K_TYPE_DOUBLE:
        if (isReturn && valueObj)  {
            double dbl;
            CHECK(Tcl_GetDoubleFromObj(ipCtxP->interp, valueObj, &dbl));
        }
        return TCL_OK;
    case CFFI_K_TYPE_POINTER:
        if (! (typeAttrsP->flags & CFFI_F_ATTR_UNSAFE)) {
            return Tclh_ErrorInvalidValue(
                ipCtxP->interp,
                paramP->nameObj,
                "Pointer types in callbacks must have the unsafe annotation.");
        }
        if (isReturn && valueObj) {
            void *pv;
            CHECK(Tclh_PointerUnwrap(ipCtxP->interp, valueObj, &pv, NULL));
        }
        return TCL_OK;

    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
    case CFFI_K_TYPE_STRUCT:
        if (isReturn) {
            return Tclh_ErrorInvalidValue(
                ipCtxP->interp,
                NULL,
                "Non-scalar parameter type not permitted as callback return "
                "value.");
        }
        if (typeAttrsP->dataType.baseType == CFFI_K_TYPE_STRUCT
            && !(typeAttrsP->flags & CFFI_F_ATTR_BYREF)) {
            return Tclh_ErrorInvalidValue(
                ipCtxP->interp,
                paramP->nameObj,
                "Struct parameter types in callbacks must be byref.");
        }
        return TCL_OK;

    case CFFI_K_TYPE_VOID:
        if (isReturn)
            return TCL_OK;
        /* Else FALLTHRU - not allowed as params */

    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
    case CFFI_K_TYPE_BINARY:
    default:
        return Tclh_ErrorInvalidValue(
            ipCtxP->interp, NULL, "Invalid type for use in callbacks.");
    }
}

/* Function: CffiCallbackCheckProto
 * Checks if a prototype definition is suitable for use as a callback.
 *
 * Parameters:
 * ip - interpreter. May be NULL if no error messages are needed.
 * protoP - prototype definition
 * errorReturnObj - value to return from callback in case of errors
 *
 * Callbacks only support a subset of argument types and annotations.
 *
 * Returns:
 * Returns *TCL_OK* if the prototype can be used for a callback, or
 * *TCL_ERROR* with an error message in the interpreter.
 */
static CffiResult
CffiCallbackCheckProto(CffiInterpCtx *ipCtxP,
                       const CffiProto *protoP,
                       Tcl_Obj *errorReturnObj)
{
    int i;

    /* Check the parameters */
    for (i = 0; i < protoP->nParams; ++i) {
        const CffiParam *paramP = &protoP->params[i];
        CHECK(CffiCallbackCheckType(ipCtxP, paramP, 0, NULL));
    }
    /* Check return type */
    CHECK(CffiCallbackCheckType(ipCtxP, &protoP->returnType, 1, errorReturnObj));

    return TCL_OK;
}

static CffiResult
CffiCallbackFind(CffiInterpCtx *ipCtxP,
                 void *executableAddress,
                 CffiCallback **cbPP)
{
#ifdef CFFI_USE_DYNCALL
    Tcl_SetResult(
        ip, "Callbacks not supported by the dyncall back end.", TCL_STATIC);
    return TCL_ERROR;
#endif

#ifdef CFFI_USE_LIBFFI
    Tcl_HashEntry *heP;
    heP = Tcl_FindHashEntry(&ipCtxP->callbackClosures, executableAddress);
    if (heP) {
        *cbPP = Tcl_GetHashValue(heP);
        return TCL_OK;
    }
    else
        return Tclh_ErrorNotFound(
            ipCtxP->interp, "Callback", NULL, "Callback entry not found.");
#endif
}


CffiResult
CffiCallbackFreeObjCmd(ClientData cdata,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    void *pv;
    CffiCallback *cbP = NULL;
    CffiResult ret;
    Tcl_Obj *tagObj;

    CHECK_NARGS(ip, 2, 2, "CALLBACKPTR");

    CHECK(Tclh_PointerUnwrap(ip, objv[1], &pv, NULL));
    if (pv == NULL)
        return TCL_OK;

    /* Map the function trampoline address to our callback structure */
    CHECK(CffiCallbackFind(ipCtxP, pv, &cbP));
    CFFI_ASSERT(cbP->ffiExecutableAddress == pv);

    CHECK(Tclh_PointerObjGetTag(ip, objv[1], &tagObj));
    if (tagObj == NULL)
        return Tclh_ErrorInvalidValue(ip, objv[1], "Not a callback function pointer.");
    ret = Tclh_PointerUnregister(ip, pv, tagObj);
    if (ret == TCL_OK)
        CffiCallbackCleanupAndFree(cbP);

    return ret;
}

CffiResult
CffiCallbackObjCmd(ClientData cdata,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    CffiCallback *cbP = NULL;
    CffiProto *protoP;
    Tcl_Obj **cmdObjs;
    int nCmdObjs;
    Tcl_Obj *cbObj;
    CffiResult ret;
    void *closureP;
    void *executableAddr;

    CHECK_NARGS(ip, 3, 4, "PROTOTYPENAME CMDPREFIX ?ERROR_RESULT?");

    CHECK(Tcl_ListObjGetElements(ip, objv[2], &nCmdObjs, &cmdObjs));
    if (nCmdObjs == 0)
        return Tclh_ErrorInvalidValue(ip, NULL, "Empty command specified.");

    protoP = CffiProtoGet(CffiScopeGet(ipCtxP, NULL), objv[1]);
    if (protoP == NULL) {
        return Tclh_ErrorNotFound(ip, "Prototype", objv[1], NULL);
    }

    CHECK(CffiCallbackCheckProto(ipCtxP, protoP, objc < 4 ? NULL : objv[3]));

#ifdef CFFI_USE_DYNCALL
    Tcl_SetResult(
        ip, "Callbacks not supported by the dyncall back end.", TCL_STATIC);
    return TCL_ERROR;
#endif

#ifdef CFFI_USE_LIBFFI
    CHECK(CffiLibffiInitProtoCif(ip, protoP));

    cbP = CffiCallbackAllocAndInit(
        ipCtxP, protoP, objv[2], objc < 4 ? NULL : objv[3]);
    if (cbP == NULL)
        return TCL_ERROR;

    /* Allocate the libffi closure */
    closureP = ffi_closure_alloc(sizeof(ffi_closure), &executableAddr);
    if (closureP == NULL)
        Tclh_ErrorAllocation(ip, "ffi_closure", NULL);
    else {
        ffi_status ffiStatus;

        cbP->ffiClosureP = closureP;

        ffiStatus = ffi_prep_closure_loc(
            closureP, protoP->cifP, CffiLibffiCallback, cbP, executableAddr);
        if (ffiStatus == FFI_OK) {
            cbP->ffiExecutableAddress = executableAddr;
            /*
             * Construct return function pointer value. This pointer is passed
             * as the callback function address.
             */
            ret = Tclh_PointerRegister(ip, executableAddr, objv[1], &cbObj);
            if (ret == TCL_OK) {
                /* We need to map from the function pointer to callback context */
                Tcl_HashEntry *heP;
                int isNew;
                heP = Tcl_CreateHashEntry(
                    &ipCtxP->callbackClosures, executableAddr, &isNew);
                if (isNew) {
                    Tcl_SetHashValue(heP, cbP);
                    Tcl_SetObjResult(ip, cbObj);
                    return TCL_OK;
                }
                /* Entry exists? Something wrong */
                Tcl_SetResult(ip,
                              "Internal error: callback entry already exists.",
                              TCL_STATIC);
            }
        }
        else {
            Tcl_SetObjResult(
                ip, Tcl_ObjPrintf("libffi returned error %d", ffiStatus));
        }
    }

    CFFI_ASSERT(cbP);
    CffiCallbackCleanupAndFree(cbP);
    return TCL_ERROR;
#endif
}

#endif /* CFFI_ENABLE_CALLBACKS */
