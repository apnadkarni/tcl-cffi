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

static CffiProto *CffiProtoAllocate(Tcl_Size nparams)
{
    size_t         sz;
    CffiProto *protoP;

    sz = offsetof(CffiProto, params) + (nparams * sizeof(protoP->params[0]));
    protoP = ckalloc(sz);
    memset(protoP, 0, sz);
    protoP->abi = CffiDefaultABI();
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

/* Function: CffiFindDynamicCountParam
 * Returns the index of the parameter holding a dynamic count
 *
 * Parameters:
 * ip - interpreter
 * protoP - function prototype whose parameters are to be searched
 * nameObj - name of the dynamic count parameter
 *
 * Returns:
 * The index of the parameter or -1 if not found with error message
 * in the interpreter
 */
static int
CffiFindDynamicCountParam(Tcl_Interp *ip, CffiProto *protoP, Tcl_Obj *nameObj)
{
    int i;
    const char *name = Tcl_GetString(nameObj);
    /* Note only searching fixed params as varargs are not named */
    for (i = 0; i < protoP->nParams; ++i) {
        CffiParam *paramP = &protoP->params[i];
        if (CffiTypeIsNotArray(&paramP->typeAttrs.dataType)
            && CffiTypeIsInteger(paramP->typeAttrs.dataType.baseType)
            && (paramP->typeAttrs.flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT))
            && !strcmp(name, Tcl_GetString(paramP->nameObj))) {
            return i;
        }
    }
    (void)Tclh_ErrorNotFound(ip,
                             "Parameter",
                             nameObj,
                             "Could not find referenced count for dynamic "
                             "array, possibly wrong type or not scalar.");
    return -1;
}

/* Function: CffiProtoParse
 * Parses a function prototype definition returning an internal representation.
 *
 * Parameters:
 * ipCtxP - pointer to the context in which prototype is defined
 * fnNameObj - name of the function or prototype definition
 * returnTypeObj - return type of the function in format expected by
 *            CffiTypeAndAttrsParse
 * numParamElements - size of paramElements
 * paramElements - parameter definition array consisting of alternating 
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
                   CffiABIProtocol abi,
                   Tcl_Obj *fnNameObj,
                   Tcl_Obj *returnTypeObj,
                   Tcl_Size numParamElements,
                   Tcl_Obj **paramElements,
                   CffiProto **protoPP)
{
    Tcl_Interp *ip = ipCtxP->interp;
    CffiProto *protoP;
    Tcl_Size i, j;
    int need_pass2;
    int isVarArgs;

    if (numParamElements & 1) {
        /*
         * Odd number. Only allowed if last param is varargs.
         * And in that case, there has to be at least one fixed param
         * (libffi restriction)
         */
        if (strcmp(Tcl_GetString(paramElements[numParamElements-1]), "...")) {
            return Tclh_ErrorInvalidValue(
                ip, paramElements[numParamElements-1], "Parameter type missing.");
        }
#ifdef CFFI_HAVE_VARARGS
        if (abi != CffiDefaultABI()) {
            return Tclh_ErrorGeneric(
                ip, NULL, "Varargs not supported for this calling convention.");
        }
        numParamElements -= 1; /* Ignore the ... */
        if (numParamElements == 0) {
            /* The ... is the only parameter */
            return Tclh_ErrorInvalidValue(
                ip,
                NULL,
                "No fixed parameters present in varargs function definition.");
        }
        isVarArgs = 1;
#else
        return Tclh_ErrorGeneric(
            ip,
            NULL,
            "Varargs functions not supported by the backend.");
#endif
    }
    else
        isVarArgs = 0;

    /*
     * Parameter list is alternating name, type elements. Thus number of
     * parameters is numParamElements/2
     */
    protoP = CffiProtoAllocate(numParamElements / 2);
    protoP->abi = abi;
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
    if (isVarArgs) {
        protoP->flags |= CFFI_F_PROTO_VARARGS;
    }

    protoP->nParams = 0; /* Update as we go along  */
    need_pass2      = 0;
    /* Params are list {name type name type ...} */
    for (i = 0, j = 0; i < numParamElements; i += 2, ++j) {
        if (CffiTypeAndAttrsParse(ipCtxP,
                                  paramElements[i + 1],
                                  CFFI_F_TYPE_PARSE_PARAM,
                                  &protoP->params[j].typeAttrs)
            != TCL_OK) {
            CffiProtoUnref(protoP);
            return TCL_ERROR;
        }
        if (isVarArgs && protoP->params[j].typeAttrs.parseModeSpecificObj) {
            CffiProtoUnref(protoP);
            return Tclh_ErrorGeneric(
                ip,
                NULL,
                "Parameters in varargs functions cannot have default values.");
        }
        if (protoP->params[j].typeAttrs.flags & CFFI_F_ATTR_RETVAL) {
            if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_RETVAL) {
                CffiProtoUnref(protoP);
                return Tclh_ErrorGeneric(
                    ip,
                    NULL,
                    "The \"retval\" annotation must not be placed on more than "
                    "one parameter definition.");
            }
            if (protoP->returnType.typeAttrs.dataType.baseType != CFFI_K_TYPE_VOID) {
                if (!CffiTypeIsInteger(
                        protoP->returnType.typeAttrs.dataType.baseType)
                    || !(protoP->returnType.typeAttrs.flags
                         & (CFFI_F_ATTR_REQUIREMENT_MASK))) {
                    CffiProtoUnref(protoP);
                    return Tclh_ErrorGeneric(
                        ip,
                        NULL,
                        "The \"retval\" annotation can only be used "
                        "in parameter definitions in functions with void or integer "
                        "return types with error checking annotations.");
                }
            }
            /* Mark that return value is through a parameter */
            protoP->returnType.typeAttrs.flags |= CFFI_F_ATTR_RETVAL;
        }
        Tcl_IncrRefCount(paramElements[i]);
        protoP->params[j].nameObj = paramElements[i];
        protoP->nParams += 1; /* Update incrementally for error cleanup after return */
        if (CffiTypeIsVLA(&protoP->params[j].typeAttrs.dataType))
            need_pass2 = 1;
    }

    /* Check type definitions for dynamic arrays */
    if (need_pass2) {
        for (i = 0; i < protoP->nParams; ++i) {
            if (CffiTypeIsVLA(
                    &protoP->params[i].typeAttrs.dataType)) {
                int dynamicParamIndex = CffiFindDynamicCountParam(
                    ip,
                    protoP,
                    protoP->params[i].typeAttrs.dataType.countHolderObj);
                if (dynamicParamIndex < 0) {
                    /* Dynamic count parameter not found or wrong type */
                    CffiProtoUnref(protoP);
                    return TCL_ERROR;
                }
                protoP->params[i].arraySizeParamIndex = dynamicParamIndex;
            }
        }
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
                         "Prototype",
                         CFFI_F_SKIP_ERROR_MESSAGES,
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
                  CffiABIProtocol abi)
{
    CffiProto *protoP;
    Tcl_Obj *fqnObj;

    CFFI_ASSERT(objc == 5);

    CHECK(CffiNameSyntaxCheck(ipCtxP->interp, objv[2]));

    Tcl_Obj **paramObjs;
    Tcl_Size nparams;

    CHECK(Tcl_ListObjGetElements(ip, objv[4], &nparams, &paramObjs));

    CHECK(CffiPrototypeParse(
        ipCtxP, abi, objv[2], objv[3], nparams, paramObjs, &protoP));
    CffiProtoRef(protoP);
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
CffiPrototypeClearCmd(CffiInterpCtx *ipCtxP,
                      Tcl_Interp *ip,
                      int objc,
                      Tcl_Obj *const objv[])
{
    CFFI_ASSERT(objc == 2);
    return CffiNameDeleteNames(ipCtxP->interp,
                               &ipCtxP->scope.prototypes,
                               NULL,
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
    CffiNameTableFinit(ipCtxP->interp,
                       &ipCtxP->scope.prototypes,
                       CffiPrototypeNameDeleteCallback);
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
        {"clear", 0, 0, "", CffiPrototypeClearCmd},
        {"delete", 1, 1, "PATTERN", CffiPrototypeDeleteCmd},
        {"list", 0, 1, "?PATTERN?", CffiPrototypeListCmd},
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
