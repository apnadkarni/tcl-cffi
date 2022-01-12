/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

/* Function: CffiParamCleanup
 * Releases resources associated with a CffiParam
 *
 * Parameters:
 * paramP - pointer to datum to clean up
 */
static void CffiParamCleanup(CffiParam *paramP)
{
    CffiTypeAndAttrsCleanup(&paramP->typeAttrs);
    Tclh_ObjClearPtr(&paramP->nameObj);
}

static CffiProto *CffiProtoAllocate(int nparams)
{
    int         sz;
    CffiProto *protoP;

    sz = offsetof(CffiProto, params) + (nparams * sizeof(protoP->params[0]));
    protoP = ckalloc(sz);
    memset(protoP, 0, sz);
    return protoP;
}

/* Function: CffiProtoUnref
 * Cleans up resources associated with a prototype representation.
 *
 * Parameters:
 * protoP - pointer to a prototype representation. This must be in a
 *          consistent state.
 *
 * Returns:
 * Nothing. Any associated resources are released. Note that *protoP*
 * itself is not freed.
 */
void
CffiProtoUnref(CffiProto *protoP)
{
    if (protoP->nRefs <= 1) {
        int i;
        CffiParamCleanup(&protoP->returnType);
        for (i = 0; i < protoP->nParams; ++i) {
            CffiParamCleanup(&protoP->params[i]);
        }
#ifdef CFFI_USE_LIBFFI
        if (protoP->cifP)
            ckfree(protoP->cifP);
#endif
        ckfree(protoP);
    }
    else
        protoP->nRefs -= 1;
}

/* Function: CffiProtoParse
 * Parses a function prototype definition returning an internal representation.
 *
 * Parameters:
 * ipCtxP - pointer to the context in which prototype is defined
 * fnNameObj - name of the function or prototype definition
 * returnTypeObj - return type of the function in format expected by
 *            CffiTypeAndAttrsParse
 * paramsObj - parameter definition list consisting of alternating parameter
 *            names and type definitions in the format expected by
 *            <CffiTypeAndAttrsParse>.
 * protoPP   - location to hold a pointer to the allocated internal prototype
 *            representation with reference count set to 0.
 *
 * Returns:
 * Returns *TCL_OK* on success with a pointer to the internal prototype
 * representation in *protoPP* and *TCL_ERROR* on failure with error message
 * in the interpreter.
 *
 */
CffiResult
CffiPrototypeParse(CffiInterpCtx *ipCtxP,
                   Tcl_Obj *fnNameObj,
                   Tcl_Obj *returnTypeObj,
                   Tcl_Obj *paramsObj,
                   CffiProto **protoPP)
{
    Tcl_Interp *ip = ipCtxP->interp;
    CffiProto *protoP;
    Tcl_Obj **objs;
    int nobjs;
    int i, j;

    CHECK(Tcl_ListObjGetElements(ip, paramsObj, &nobjs, &objs));
    if (nobjs & 1)
        return Tclh_ErrorInvalidValue(ip, paramsObj, "Parameter type missing.");

    /*
     * Parameter list is alternating name, type elements. Thus number of
     * parameters is nobjs/2
     */
    protoP = CffiProtoAllocate(nobjs / 2);
    if (CffiTypeAndAttrsParse(ipCtxP,
                              returnTypeObj,
                              CFFI_F_TYPE_PARSE_RETURN,
                              &protoP->returnType.typeAttrs)
        != TCL_OK) {
        CffiProtoUnref(protoP);
        return TCL_ERROR;
    }
    Tcl_IncrRefCount(fnNameObj);
    protoP->returnType.nameObj = fnNameObj;

    protoP->nParams = 0; /* Update as we go along  */
    for (i = 0, j = 0; i < nobjs; i += 2, ++j) {
        if (CffiTypeAndAttrsParse(ipCtxP,
                                  objs[i + 1],
                                  CFFI_F_TYPE_PARSE_PARAM,
                                  &protoP->params[j].typeAttrs)
            != TCL_OK) {
            CffiProtoUnref(protoP);
            return TCL_ERROR;
        }
        Tcl_IncrRefCount(objs[i]);
        protoP->params[j].nameObj = objs[i];
        protoP->nParams += 1; /* Update incrementally for error cleanup */
    }

    *protoPP = protoP;
    return TCL_OK;
}

/* Function: CffiProtoGet
 * Checks for the existence of a prototype definition and returns its internal
 * form.
 *
 * Parameters:
 *   scopeP - scope
 *   protoNameObj - prototype name
 *
 * Note that the reference count of the returned structure is not incremented
 * Caller should do that if it is going to be held on to.
 *
 * Returns:
 * If a prototype of the specified name is found, a pointer to it
 * is returned otherwise *NULL*.
 */
CffiProto *
CffiProtoGet(CffiInterpCtx *ipCtxP, Tcl_Obj *protoNameObj)
{
    CffiProto *protoP;
    CffiResult ret;
    ret = CffiNameLookup(ipCtxP->interp,
                         &ipCtxP->scope.prototypes,
                         Tcl_GetString(protoNameObj),
                         "Enum",
                         CFFI_F_NAME_SKIP_MESSAGES,
                         (ClientData *)&protoP,
                         NULL);
    return ret == TCL_OK ? protoP : NULL;
}

/* Function: CffiPrototypeDefineCmd
 * Implements the core of the *prototype define* command.
 *
 * Parameters:
 *  ipCtxP - context
 *  ip - interpreter
 *  objc - size of *objv*
 *  objv - *prototype* *function|stdcall* NAME RETURN PARAMS
 *  callMode - calling convention
 *
 * Returns:
 * TCL_OK on success, TCL_ERROR on failure.
 */
static CffiResult
CffiPrototypeDefineCmd(CffiInterpCtx *ipCtxP,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[],
                  CffiABIProtocol callMode)
{
    CffiProto *protoP;
    Tcl_Obj *fqnObj;

    CFFI_ASSERT(objc == 5);

    CHECK(CffiNameSyntaxCheck(ipCtxP->interp, objv[2]));

    CHECK(CffiPrototypeParse(ipCtxP, objv[2], objv[3], objv[4], &protoP));
    protoP->abi = callMode;
    if (CffiNameObjAdd(ip,
                       &ipCtxP->scope.prototypes,
                       objv[2],
                       "Prototype",
                       protoP,
                       &fqnObj)
        != TCL_OK) {
        CffiProtoUnref(protoP);
        return TCL_ERROR;
    }
    else {
        Tcl_SetObjResult(ip, fqnObj);
        return TCL_OK;
    }
}

static void CffiPrototypeNameDeleteCallback(ClientData clientData)
{
    CffiProto *protoP = (CffiProto *)clientData;
    if (protoP)
        CffiProtoUnref(protoP);
}

static CffiResult
CffiPrototypeDeleteCmd(CffiInterpCtx *ipCtxP,
                       Tcl_Interp *ip,
                       int objc,
                       Tcl_Obj *const objv[])
{
    CFFI_ASSERT(objc == 3);
    return CffiNameDeleteNames(ipCtxP->interp,
                               &ipCtxP->scope.prototypes,
                               Tcl_GetString(objv[2]),
                               CffiPrototypeNameDeleteCallback);
}

static CffiResult
CffiPrototypeListCmd(CffiInterpCtx *ipCtxP,
                     Tcl_Interp *ip,
                     int objc,
                     Tcl_Obj *const objv[])
{
    const char *pattern;
    Tcl_Obj *namesObj;
    CffiResult ret;

    /*
     * Default pattern to "*", not NULL as the latter will list all scopes
     * we only want the ones in current namespace.
     */
    pattern = objc > 2 ? Tcl_GetString(objv[2]) : "*";
    ret = CffiNameListNames(
        ipCtxP->interp, &ipCtxP->scope.prototypes, pattern, &namesObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ipCtxP->interp, namesObj);
    return ret;
}

void
CffiPrototypesCleanup(CffiInterpCtx *ipCtxP)
{
    CffiNameDeleteNames(ipCtxP->interp,
                        &ipCtxP->scope.prototypes,
                        NULL,
                        CffiPrototypeNameDeleteCallback);
    Tcl_DeleteHashTable(&ipCtxP->scope.prototypes);
}

CffiResult
CffiPrototypeObjCmd(ClientData cdata,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    int cmdIndex;
    static const Tclh_SubCommand subCommands[] = {
        {"function", 3, 3, "NAME RETURNTYPE PARAMDEFS", NULL},
        {"stdcall", 3, 3, "NAME RETURNTYPE PARAMDEFS", NULL},
        {"delete", 1, 1, "PATTERN", CffiPrototypeDeleteCmd},
        {"list", 0, 1, "?PATTERN?", CffiPrototypeListCmd},
#ifdef NOTYET
        {"body", 1, 1, "ALIAS", NULL},
        {"bind", 2, 2, "NAME FNPTR", NULL},
#endif
        {NULL}
    };

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    if (cmdIndex == 0 || cmdIndex == 1) {
        return CffiPrototypeDefineCmd(ipCtxP,
                                      ip,
                                      objc,
                                      objv,
                                      cmdIndex == 0 ? CffiDefaultABI()
                                                    : CffiStdcallABI());
    }
    else
        return subCommands[cmdIndex].cmdFn(ipCtxP, ip, objc, objv);
}
