/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

#ifdef CFFI_HAVE_CALLBACKS

#ifdef CFFI_USE_LIBFFI
# define EXEFLD ffiExecutableAddress
#endif
#ifdef CFFI_USE_DYNCALL
# define EXEFLD dcCallbackP
#endif

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
        if (cbP->EXEFLD) {
            Tcl_HashEntry *heP;
            heP = Tcl_FindHashEntry(&cbP->ipCtxP->callbackClosures,
                                    cbP->EXEFLD);
            CFFI_ASSERT(Tcl_GetHashValue(heP) == cbP);
            if (heP)
                Tcl_DeleteHashEntry(heP);
        }
#ifdef CFFI_USE_LIBFFI
        CffiLibffiCallbackCleanup(cbP);
#endif
#ifdef CFFI_USE_DYNCALL
        CffiDyncallCallbackCleanup(cbP);
#endif
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
#ifdef CFFI_USE_LIBFFI
    cbP->ffiClosureP = NULL;
    cbP->ffiExecutableAddress = NULL;
#endif
#ifdef CFFI_USE_DYNCALL
    cbP->dcCallbackP = NULL;
    cbP->dcCallbackSig = NULL;
#endif
    cbP->errorResultObj = errorResultObj;
    if (errorResultObj)
        Tcl_IncrRefCount(errorResultObj);
    cbP->depth = 0;
    return cbP;
}

/* Function: CffiCallbackCheckType
 * Checks whether a type is suitable for use in a callback
 *
 * Parameters:
 * ipCtxP - interpreter context
 * paramP - parameter.
 * isReturn - 0 if param, non-0 if return type
 * valueObj - checked against the type if isReturn is true.
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

    if (CffiTypeIsArray(&typeAttrsP->dataType)) {
        return Tclh_ErrorInvalidValue(
            ipCtxP->interp,
            paramP->nameObj,
            "Array parameters not permitted in callback functions.");
    }

    if (isReturn && typeAttrsP->dataType.baseType != CFFI_K_TYPE_VOID
        && valueObj == NULL) {
        return Tclh_ErrorInvalidValue(
            ipCtxP->interp,
            NULL,
            "A default error value must be specified in a callback if return "
            "type is not void.");
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
        if (isReturn)  {
            /* Check that the value is valid for the type */
            Tcl_WideInt wide;
            CHECK(CffiIntValueFromObj(
                ipCtxP->interp, typeAttrsP, valueObj, &wide));
        }
        return TCL_OK;
    case CFFI_K_TYPE_FLOAT:
    case CFFI_K_TYPE_DOUBLE:
        if (isReturn)  {
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
        if (isReturn) {
            void *pv;
            CHECK(Tclh_PointerUnwrap(ipCtxP->interp, valueObj, &pv));
        }
        return TCL_OK;

    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
#ifdef _WIN32
    case CFFI_K_TYPE_WINSTRING:
#endif
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
#ifdef _WIN32
    case CFFI_K_TYPE_WINCHAR_ARRAY:
#endif
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

    /* Cannot be a varargs funciton */
    if (protoP->flags & CFFI_F_PROTO_VARARGS) {
        return Tclh_ErrorGeneric(
            ipCtxP->interp,
            NULL,
            "Callbacks cannot have a variable number of parameters.");
    }

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
    Tcl_HashEntry *heP;
    heP = Tcl_FindHashEntry(&ipCtxP->callbackClosures, executableAddress);
    if (heP) {
        *cbPP = Tcl_GetHashValue(heP);
        return TCL_OK;
    }
    else
        return Tclh_ErrorNotFound(
            ipCtxP->interp, "Callback", NULL, "Callback entry not found.");
}

static CffiResult
CffiCallbackFreeCmd(CffiInterpCtx *ipCtxP,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    void *pv;
    CffiCallback *cbP = NULL;
    CffiResult ret;
    Tcl_Obj *tagObj;

    CFFI_ASSERT(objc == 3);

    CHECK(Tclh_PointerUnwrap(ip, objv[2], &pv));
    if (pv == NULL)
        return TCL_OK;

    /* Map the function trampoline address to our callback structure */
    CHECK(CffiCallbackFind(ipCtxP, pv, &cbP));
#ifdef CFFI_USE_LIBFFI
    CFFI_ASSERT(cbP->ffiExecutableAddress == pv);
#endif
#ifdef CFFI_USE_DYNCALL
    CFFI_ASSERT(cbP->dcCallbackP == pv);
#endif

    if (cbP->depth != 0) {
        return Tclh_ErrorGeneric(
            ip, NULL, "Attempt to delete callback while still active.");
    }

    CHECK(Tclh_PointerObjGetTag(ip, objv[2], &tagObj));
    if (tagObj == NULL)
        return Tclh_ErrorInvalidValue(ip, objv[2], "Not a callback function pointer.");
    ret = Tclh_PointerUnregisterTagged(ip, ipCtxP->tclhCtxP, pv, tagObj);
    if (ret == TCL_OK)
        CffiCallbackCleanupAndFree(cbP);

    return ret;
}

static CffiResult
CffiCallbackNewCmd(CffiInterpCtx *ipCtxP,
                   Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[])
{
    CffiCallback *cbP = NULL;
    CffiProto *protoP;
    Tcl_Obj *protoFqnObj = NULL;
    Tcl_Obj **cmdObjs;
    Tcl_Size nCmdObjs;
    Tcl_Obj *cbObj;
    CffiResult ret;
    void *executableAddr;

    CFFI_ASSERT(objc == 4 || objc == 5);

    CHECK(Tcl_ListObjGetElements(ip, objv[3], &nCmdObjs, &cmdObjs));
    if (nCmdObjs == 0)
        return Tclh_ErrorInvalidValue(ip, NULL, "Empty command specified.");

    /* We will need fqn for tagging pointer */
    protoFqnObj = Tclh_NsQualifyNameObj(ip, objv[2], NULL);
    Tcl_IncrRefCount(protoFqnObj);
    protoP = CffiProtoGet(ipCtxP, protoFqnObj);

    if (protoP == NULL) {
        Tclh_ErrorNotFound(ip, "Prototype", objv[2], NULL);
        goto error_handler;
    }

    /* Verify prototype is usable as a callback */
    if (CffiCallbackCheckProto(ipCtxP, protoP, objc < 5 ? NULL : objv[4])
            != TCL_OK)
        goto error_handler;

    cbP = CffiCallbackAllocAndInit(
        ipCtxP, protoP, objv[3], objc < 5 ? NULL : objv[4]);
    if (cbP == NULL)
        goto error_handler;

#ifdef CFFI_USE_LIBFFI
    if (CffiLibffiCallbackInit(ipCtxP, protoP, cbP) != TCL_OK)
        goto error_handler;
    executableAddr = cbP->ffiExecutableAddress;
#endif
#ifdef CFFI_USE_DYNCALL
    if (CffiDyncallCallbackInit(ipCtxP, protoP, cbP) != TCL_OK)
        goto error_handler;
    executableAddr = cbP->dcCallbackP;
#endif
    /*
     * Construct return function pointer value. This pointer is passed
     * as the callback function address.
     */
    ret = Tclh_PointerRegister(
        ip, ipCtxP->tclhCtxP, executableAddr, protoFqnObj, &cbObj);
    if (ret == TCL_OK) {
        /* We need to map from the function pointer to callback context */
        Tcl_HashEntry *heP;
        int isNew;
        heP = Tcl_CreateHashEntry(
            &ipCtxP->callbackClosures, executableAddr, &isNew);
        if (isNew) {
            Tcl_SetHashValue(heP, cbP);
            Tcl_SetObjResult(ip, cbObj);
            Tcl_DecrRefCount(protoFqnObj);
            return TCL_OK;
        }
        /* Entry exists? Something wrong */
        Tcl_SetResult(
            ip, "Internal error: callback entry already exists.", TCL_STATIC);
    }

error_handler:
    if (protoFqnObj)
        Tcl_DecrRefCount(protoFqnObj);
    if (cbP)
        CffiCallbackCleanupAndFree(cbP);
    return TCL_ERROR;

}

CffiResult
CffiCallbackObjCmd(ClientData cdata,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    /* The flags field CFFI_F_ALLOW_UNSAFE is set for unsafe pointer operation */
    static const Tclh_SubCommand subCommands[] = {
        {"new", 2, 3, "PROTOTYPENAME CMDPREFIX ?ERROR_RESULT?", CffiCallbackNewCmd, 0},
        {"free", 1, 1, "CALLBACKPTR", CffiCallbackFreeCmd, 0},
        {NULL}
    };
    int cmdIndex;

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ipCtxP, ip, objc, objv);
}

#endif /* CFFI_HAVE_CALLBACKS */
