/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#define TCLH_IMPL
#define TCLH_EMBEDDER PACKAGE_NAME
#include "tclCffiInt.h"


typedef struct Tclh_SubCommand {
    const char *cmdName;
    int minargs;
    int maxargs;
    const char *message;
    int (*cmdFn)();
    int flags; /* Command specific usage */
} Tclh_SubCommand;

typedef struct CffiParam {
    Tcl_Obj *nameObj;           /* Parameter name */
    CffiTypeAndAttrs typeAttrs;
} CffiParam;

typedef struct CffiProto {
    CffiLibCtx *libCtxP;         /* Context for the call */
    void *fnP;                    /* Pointer to the function to call */
    int nParams;                  /* Number of params, sizeof params array */
    CffiParam returnType; /* Name and return type of function */
    CffiParam params[1];  /* Real size depends on nparams which
                              may even be 0!*/
} CffiProto;


/* Function: Tclh_SubCommandNameToIndex
 * Looks up a subcommand table and returns index of a matching entry.
 *
 * The function looks up the subcommand table to find the entry with
 * name given by *nameObj*. Each entry in the table is a <Tclh_SubCommand>
 * structure and the name is matched against the *cmdName* field of that
 * structure. Unique prefixes of subcommands are accepted as a match.
 * The end of the table is indicated by an entry whose *cmdName* field is NULL.
 *
 * This function only matches against the name and does not concern itself
 * with any of the other fields in an entry.
 *
 * The function is a wrapper around the Tcl *Tcl_GetIndexFromObjStruct*
 * and subject to its constraints. In particular, *cmdTableP* must point
 * to static storage as a pointer to it is retained.
 *
 * Parameters:
 * ip - Interpreter
 * nameObj - name of method to lookup
 * cmdTableP - pointer to command table
 * indexP - location to store index of matched entry
 *
 * Returns:
 * *TCL_OK* on success with index of match stored in *indexP*; otherwise,
 * *TCL_ERROR* with error message in *ip*.
 */
CffiResult
Tclh_SubCommandNameToIndex(Tcl_Interp *ip,
                           Tcl_Obj *nameObj,
                           Tclh_SubCommand *cmdTableP,
                           int *indexP)
{
    return Tcl_GetIndexFromObjStruct(
        ip, nameObj, cmdTableP, sizeof(*cmdTableP), "subcommand", 0, indexP);
}

/* Function: Tclh_SubCommandLookup
 * Looks up a subcommand table and returns index of a matching entry after
 * verifying number of arguments.
 *
 * The function looks up the subcommand table to find the entry with
 * name given by *objv[1]*. Each entry in the table is a <Tclh_SubCommand>
 * structure and the name is matched against the *cmdName* field of that
 * structure. Unique prefixes of subcommands are accepted as a match.
 * The end of the table is indicated by an entry whose *cmdName* field is NULL.
 *
 * This function only matches against the name and does not concern itself
 * with any of the other fields in an entry.
 *
 * The function is a wrapper around the Tcl *Tcl_GetIndexFromObjStruct*
 * and subject to its constraints. In particular, *cmdTableP* must point
 * to static storage as a pointer to it is retained.
 *
 * On finding a match, the function verifies that the number of arguments
 * satisfy the number of arguments expected by the subcommand based on
 * the *minargs* and *maxargs* fields of the matching <Tclh_SubCommand>
 * entry. These fields should hold the argument count range for the
 * subcommand (meaning not counting subcommand itself)
 *
 * Parameters:
 * ip - Interpreter
 * cmdTableP - pointer to command table
 * objc - Number of elements in *objv*. Must be at least *1*.
 * objv - Array of *Tcl_Obj* pointers as passed to Tcl commands. *objv[0]*
 *        is expected to be the main command and *objv[1]* the subcommand.
 * indexP - location to store index of matched entry
 *
 * Returns:
 * *TCL_OK* on success with index of match stored in *indexP*; otherwise,
 * *TCL_ERROR* with error message in *ip*.
 */
CffiResult
Tclh_SubCommandLookup(Tcl_Interp *ip,
                      const Tclh_SubCommand *cmdTableP,
                      int objc,
                      Tcl_Obj *const objv[],
                      int *indexP)
{
    TCLH_ASSERT(objc > 1);
    if (objc < 2) {
        return Tclh_ErrorNumArgs(ip, 1, objv, "subcommand ?arg ...?");
    }
    CHECK(Tcl_GetIndexFromObjStruct(
        ip, objv[1], cmdTableP, sizeof(*cmdTableP), "subcommand", 0, indexP));
    cmdTableP += *indexP;
    /*
     * Can't use CHECK_NARGS here because of slightly different semantics.
     */
    if ((objc-2) < cmdTableP->minargs || (objc-2) > cmdTableP->maxargs) {
        return Tclh_ErrorNumArgs(ip, 2, objv, cmdTableP->message);
    }
    return TCL_OK;
}

/* Function: CffiParamCleanup
 * Releases resources associated with a CffiParam
 *
 * Parameters:
 * paramP - pointer to datum to clean up
 */
static void CffiParamCleanup(CffiParam *paramP)
{
    CffiTypeAndAttrsCleanup(&paramP->typeAttrs);
    if (paramP->nameObj)
        Tcl_DecrRefCount(paramP->nameObj);
}

static CffiProto *CffiProtoCkalloc(int nparams)
{
    int         sz;
    CffiProto *protoP;

    sz = offsetof(CffiProto, params) + (nparams * sizeof(protoP->params[0]));
    protoP = ckalloc(sz);
    memset(protoP, 0, sz);
    return protoP;
}

/* Note this does NOT release any resources within *protoP */
CFFI_INLINE void CffiProtoFree(CffiProto *protoP) {
    ckfree(protoP);
}

/* Function: CffiProtoCleanup
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
CffiProtoCleanup(CffiProto *protoP)
{
    int i;
    CffiParamCleanup(&protoP->returnType);
    for (i = 0; i < protoP->nParams; ++i) {
        CffiParamCleanup(&protoP->params[i]);
    }
    protoP->nParams = 0;
}

/* Function: CffiProtoParse
 * Parses a function prototype definition returning an internal representation.
 *
 * Parameters:
 * libContextP - pointer to the context in which function is to be called
 * fnNameObj - name of the function
 * returnTypeObj - return type of the function in format expected by
 *            CffiTypeAndAttrsParse
 * paramsObj - parameter definition list consisting of alternating parameter
 *            names and type definitions in the format expected by
 *            <CffiTypeAndAttrsParse>.
 * fnP      - address of function
 * protoPP  - location to hold a pointer to the allocated internal prototype
 *            representation. The pointer must be freed with
 *            <CffiProtoFree>.
 *
 *
 * Returns:
 * Returns *TCL_OK* on success with a pointer to the internal prototype
 * representation in *protoPP* and *TCL_ERROR* on failure with error message
 * in the interpreter.
 *
 */
static CffiResult
CffiProtoParse(CffiLibCtx *libCtxP,
                Tcl_Obj *fnNameObj,
                Tcl_Obj *returnTypeObj,
                Tcl_Obj *paramsObj,
                void *fnP,
                CffiProto **protoPP)
{
    CffiInterpCtx *ipCtxP = libCtxP->vmCtxP->ipCtxP;
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
    protoP = CffiProtoCkalloc(nobjs / 2);
    if (CffiTypeAndAttrsParse(ipCtxP,
                               returnTypeObj,
                               CFFI_F_TYPE_PARSE_RETURN,
                               &protoP->returnType.typeAttrs) != TCL_OK) {
        CffiProtoFree(protoP);
        return TCL_ERROR;
    }
    Tcl_IncrRefCount(fnNameObj);
    protoP->returnType.nameObj = fnNameObj;

    protoP->nParams = 0; /* Update as we go along  */
    for (i = 0, j = 0; i < nobjs; i += 2, ++j) {
        if (CffiTypeAndAttrsParse(ipCtxP,
                                   objs[i + 1],
                                   CFFI_F_TYPE_PARSE_PARAM,
                                   &protoP->params[j].typeAttrs) != TCL_OK) {
            CffiProtoCleanup(protoP);
            CffiProtoFree(protoP);
            return TCL_ERROR;
        }
        Tcl_IncrRefCount(objs[i]);
        protoP->params[j].nameObj = objs[i];
        protoP->nParams += 1; /* Update incrementally for error cleanup */
    }

    protoP->fnP     = fnP;
    protoP->libCtxP = libCtxP;
    *protoPP        = protoP;

    return TCL_OK;
}


/*
 * Implements the call to a function. The cdata parameter contains the
 * prototype information about the function to call. The objv[] parameter
 * contains the arguments to pass to the function.
 */
static CffiResult
CffiFunctionInstanceCmd(ClientData cdata,
                        Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[])
{
    CffiProto *protoP = (CffiProto *)cdata;
    CffiInterpCtx *ipContextP = protoP->libCtxP->vmCtxP->ipCtxP;
    DCCallVM *vmP              = protoP->libCtxP->vmCtxP->vmP;
    Tcl_Obj *const *argObjs;
    Tcl_Obj *resultObj;
    int i;
    CffiValue *valuesP;
    void *pointer;
    CffiResult ret = TCL_OK;
    Tcl_Obj **varNameObjs;

    CFFI_ASSERT(ip == ipContextP->interp);

    if ((objc - 1) != protoP->nParams) {
        Tcl_Obj *syntaxObj = Tcl_NewListObj(protoP->nParams+2, NULL);
        Tcl_ListObjAppendElement(
            NULL, syntaxObj, Tcl_NewStringObj("Syntax:", -1));
        Tcl_ListObjAppendElement(NULL, syntaxObj, objv[0]);
        for (i = 0; i < protoP->nParams; ++i)
            Tcl_ListObjAppendElement(
                NULL, syntaxObj, protoP->params[i].nameObj);
        (void) Tclh_ErrorGeneric(ip, "NUMARGS", Tcl_GetString(syntaxObj));
        Tcl_DecrRefCount(syntaxObj);
        return TCL_ERROR;
    }

    /*
     * Prepare the call by resetting any previous arguments and setting the
     * call mode for this function
     */

    dcReset(vmP);
    dcMode(vmP, protoP->returnType.typeAttrs.callMode);

    /*
     * We need temporary storage of unknown size for parameter values.
     * CffiArgPrepare will use this storage for scalar value types. For
     * aggregates and variable size, CffiArgPrepare will allocate storage
     * from the memlifo and store a pointer to it in in the argument slot.
     * After the call is made, CffiArgPostProcess processes each, stores into
     * output variables as necessary. CffiArgCleanup is responsible for
     * freeing up any internal resources for each argument. The memlifo
     * memory is freed up when the entire frame is popped at the end.
     *
     * Note the additional slot allocated for the return value.
     */
    valuesP = (CffiValue *)MemLifoPushFrame(
        &ipContextP->memlifo, (protoP->nParams + 1) * sizeof(*valuesP));
    /* Also need storage for variable names for output parameters */
    varNameObjs = (Tcl_Obj **)MemLifoAlloc(
        &ipContextP->memlifo, protoP->nParams * sizeof(*valuesP));

    /* Set up parameters. */
    argObjs = objv + 1;
    for (i = 0; i < protoP->nParams; ++i) {
        if (CffiArgPrepare(protoP->libCtxP->vmCtxP,
                           &protoP->params[i].typeAttrs,
                           argObjs[i],
                           &valuesP[i],
                           &varNameObjs[i])
            != TCL_OK) {
            int j;
            for (j = 0; j < i; ++j)
                CffiArgCleanup(&protoP->params[j].typeAttrs, &valuesP[j]);
            goto pop_and_error;
        }
    }

    /* Set up the return value */
    if (CffiReturnPrepare(protoP->libCtxP->vmCtxP,
                           &protoP->returnType.typeAttrs,
                           &valuesP[protoP->nParams]) != TCL_OK) {
        int j;
        for (j = 0; j < protoP->nParams; ++j)
            CffiArgCleanup(&protoP->params[j].typeAttrs, &valuesP[j]);
        goto pop_and_error;
    }

    /* Currently return values are always by value - enforced in prototype */
    CFFI_ASSERT((protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_BYREF) == 0);

    /*
     * CALLFN should only be used for numerics.
     * IMPORTANT: do not call any system or C functions until check done
     * to prevent GetLastError/errno etc. being overwritten
     */
#define CALLFN(objfn_, dcfn_, fld_)                                           \
    do {                                                                      \
        CffiValue *valP = &valuesP[protoP->nParams];                         \
        valP->fld_     = dcfn_(vmP, protoP->fnP);                           \
        if (protoP->returnType.typeAttrs.flags                                \
            & CFFI_F_ATTR_REQUIREMENT_MASK)                                  \
            ret = CffiCheckNumeric(ip, valP, &protoP->returnType.typeAttrs); \
        if (ret == TCL_OK)                                                    \
            resultObj = objfn_(valP->fld_);                                 \
    } while (0);                                                              \
    break

    /*
     * We are duplicating code here because we could have called
     * CffiBinOneValueToObj. But that would mean again going through a
     * switch that we already switched on. Is performance diff worth
     * duplicating code? TBD
     */
    switch (protoP->returnType.typeAttrs.dataType.baseType) {
    case CFFI_K_TYPE_VOID:
        dcCallVoid(vmP, protoP->fnP);
        resultObj = Tcl_NewObj();
        break;
    case CFFI_K_TYPE_SCHAR: CALLFN(Tcl_NewIntObj, dcCallInt, schar);
    case CFFI_K_TYPE_UCHAR: CALLFN(Tcl_NewIntObj, dcCallInt, uchar);
    case CFFI_K_TYPE_SHORT: CALLFN(Tcl_NewIntObj, dcCallInt, sshort);
    case CFFI_K_TYPE_USHORT: CALLFN(Tcl_NewIntObj, dcCallInt, ushort);
    case CFFI_K_TYPE_INT: CALLFN(Tcl_NewIntObj, dcCallInt, sint);
    case CFFI_K_TYPE_UINT: CALLFN(Tcl_NewWideIntObj, dcCallInt, uint);
    case CFFI_K_TYPE_LONG: CALLFN(Tcl_NewLongObj, dcCallInt, slong);
    case CFFI_K_TYPE_ULONG: CALLFN(Tcl_NewWideIntObj, dcCallInt, ulong);
    case CFFI_K_TYPE_LONGLONG: CALLFN(Tcl_NewWideIntObj, dcCallLongLong, slonglong);
    case CFFI_K_TYPE_ULONGLONG: CALLFN(Tcl_NewWideIntObj, dcCallLongLong, ulonglong);
    case CFFI_K_TYPE_FLOAT: CALLFN(Tcl_NewDoubleObj, dcCallFloat, flt);
    case CFFI_K_TYPE_DOUBLE: CALLFN(Tcl_NewDoubleObj, dcCallDouble, dbl);
    case CFFI_K_TYPE_POINTER:
        pointer = dcCallPointer(vmP, protoP->fnP);
        if (CffiPointerToObj(ip, &protoP->returnType.typeAttrs, pointer, &resultObj)
            != TCL_OK)
            ret = TCL_ERROR;
        break;

    case CFFI_K_TYPE_STRUCT:
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
    case CFFI_K_TYPE_BINARY:
    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
#if 0
        /* Currently BYREF not allowed and DC does not support struct byval */
        CFFI_ASSERT(protoP->returnType.flags & CFFI_F_PARAM_BYREF);
        valuesP[protoP->nParams].u.ptr = dcCallPointer(vmP, protoP->fnP);
        if (CffiBinValueToObj(ipContextP->interp,
                               &protoP->returnType,
                               &valuesP[protoP->nParams].u,
                               &resultObj) != TCL_OK)
            goto pop_and_error;
        break;
#endif
        /* FALLTHRU */
    default:
        /* Really, should not even come here since should have been caught in
         * prototype parsing */
        (void) Tclh_ErrorInvalidValue(
            ipContextP->interp, NULL, "Unsupported type for return.");
        ret = TCL_ERROR;
        break;
    }

    CffiArgCleanup(&protoP->returnType.typeAttrs, &valuesP[protoP->nParams]);

    /*
     * Store any output parameters. Output parameters will have the PARAM_OUT
     * flag set.
     * TBD - what if error on function return? Some might be invalid/garbage
     */
    for (i = 0; i < protoP->nParams; ++i) {
        /* Even on error we keep looping as we have to clean up parameters. */
        if (ret == TCL_OK) {
            ret = CffiArgPostProcess(ipContextP->interp,
                                     &protoP->params[i].typeAttrs,
                                     &valuesP[i],
                                     varNameObjs[i]);
        }
        CffiArgCleanup(&protoP->params[i].typeAttrs, &valuesP[i]);
    }

    if (ret == TCL_OK) {
        Tcl_SetObjResult(ip, resultObj);
        MemLifoPopFrame(&ipContextP->memlifo);
        return TCL_OK;
    }

pop_and_error:
    /* Jump for error return after popping memlifo with error in interp */
    MemLifoPopFrame(&ipContextP->memlifo);
    return TCL_ERROR;
}


/* Function: CffiFunctionInstanceDeleter
 * Called by Tcl to cleanup resources associated with a ffi function
 * definition when the corresponding command is deleted.
 *
 * Parameters:
 * cdata - the prototype structure for the function.
 */
static void
CffiFunctionInstanceDeleter(ClientData cdata)
{
    CffiProto *protoP = (CffiProto *)cdata;
    /* Note protoP->cdlP->vmP and protoP->cdlP->dlP not to be deleted here. */
    CffiProtoCleanup(protoP);
    CffiProtoFree(protoP);
}

/* Function: CffiOneFunction
 * Creates a single command mapped to a function in a DLL.
 *
 * Parameters:
 *    ctxP - pointer to the context in which function is to be called
 *    nameObj - pair function name and optional Tcl name
 *    returnTypeObj - function return type definition
 *    paramsObj - list of parameter type definitions
 *    callMode - a dyncall call mode that overrides one specified
 *               in the return type definition if anything other
 *               than DC_CALL_C_DEFAULT
 *
 * *paramsObj* is a list of alternating parameter name and
 * type definitions. The return and parameter type definitions are in the
 * form expected by CffiTypeAndAttrsParse.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
static CffiResult
CffiOneFunction(Tcl_Interp *ip,
                 CffiLibCtx *libCtxP,
                 Tcl_Obj *nameObj,
                 Tcl_Obj *returnTypeObj,
                 Tcl_Obj *paramsObj,
                 int callMode)
{
    CffiProto *protoP = NULL;
    void *fn;
    Tcl_Obj *fqnObj;
    Tcl_Obj **nameObjs;    /* C name and optional Tcl name */
    int nNames;         /* # elements in nameObjs */

    CHECK(Tcl_ListObjGetElements(ip, nameObj, &nNames, &nameObjs));
    if (nNames == 0 || nNames > 2)
        return Tclh_ErrorInvalidValue(ip, nameObj, "Empty or invalid function name specification.");

    fn = dlFindSymbol(libCtxP->dlP, Tcl_GetString(nameObjs[0]));
    if (fn == NULL)
        return Tclh_ErrorNotFound(ip, "Symbol", nameObjs[0], NULL);

    CHECK(CffiProtoParse(libCtxP, nameObjs[0], returnTypeObj, paramsObj, fn, &protoP));
    if (callMode != DC_CALL_C_DEFAULT)
        protoP->returnType.typeAttrs.callMode = callMode;

    if (nNames < 2 || ! strcmp("", Tcl_GetString(nameObjs[1])))
        fqnObj = CffiQualifyName(ip, nameObjs[0]);
    else
        fqnObj = CffiQualifyName(ip, nameObjs[1]);
    Tcl_IncrRefCount(fqnObj);

    Tcl_CreateObjCommand(ip,
                         Tcl_GetString(fqnObj),
                         CffiFunctionInstanceCmd,
                         protoP,
                         CffiFunctionInstanceDeleter);

    Tcl_DecrRefCount(fqnObj);
    return TCL_OK;

}

/* Function: CffiDyncallFunctionCmd
 * Creates a command mapped to a function in a DLL.
 *
 * Parameters:
 * ip   - interpreter
 * ctxP - pointer to the context in which function is to be called
 * objc - number of elements in *objv*
 * objv - array containing the function definition
 *
 * The *objv[2-4]* array contains the elements function name, return type,
 * and parameters. The parameters is a list of alternating parameter name
 * and type definitions. The return and parameter type definitions are in
 * the form expected by CffiTypeAndAttrsParse.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
static CffiResult
CffiDyncallFunctionCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP)
{
    CFFI_ASSERT(objc == 5);

    return CffiOneFunction(
        ip, ctxP, objv[2], objv[3], objv[4], DC_CALL_C_DEFAULT);
}

/* Function: CffiDyncallStdcallCmd
 * Creates a command mapped to a stdcall function in a DLL.
 *
 * Parameters:
 * ip   - interpreter
 * ctxP - pointer to the context in which function is to be called
 * objc - number of elements in *objv*
 * objv - array containing the function definition
 *
 * The *objv[2-4]* array contains the elements function name, return type,
 * and parameters. The parameters is a list of alternating parameter name
 * and type definitions. The return and parameter type definitions are in
 * the form expected by CffiTypeAndAttrsParse.
 *
 * Irrespective of the function return type definition, the call mode
 * is always set to stdcall.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
static CffiResult
CffiDyncallStdcallCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP)
{
    CFFI_ASSERT(objc == 5);

    return CffiOneFunction(
        ip, ctxP, objv[2], objv[3], objv[4], DC_CALL_C_X86_WIN32_STD);
}


/* Function: CffiDyncallManyFunctionsCmd
 * Creates commands mapped to functions in a DLL.
 *
 * Parameters:
 * ip   - interpreter
 * ctxP - pointer to the context in which function is to be called
 * objc - number of elements in *objv*
 * objv - array containing the function definition list
 * callMode - a dyncall call mode that overrides one specified
 *            in the return type definition if anything other
 *            than DC_CALL_C_DEFAULT
 *
 * The *objv[2]* element contains the function definition list.
 * This is a flat list of function name, type, parameter definitions.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
static CffiResult
CffiDyncallManyFunctionsCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP,
                    int callMode)
{
    Tcl_Obj **objs;
    int nobjs;
    int i;
    int ret;

    CFFI_ASSERT(objc == 3);

    CHECK(Tcl_ListObjGetElements(ip, objv[2], &nobjs, &objs));
    if (nobjs % 3) {
        return Tclh_ErrorInvalidValue(
            ip, objv[2], "Incomplete function definition list.");
    }

    for (i = 0; i < nobjs; i += 3) {
        ret = CffiOneFunction(ip, ctxP, objs[i], objs[i + 1], objs[i + 2], callMode);
        /* TBD - if one fails, rest are not defined but prior ones are */
        if (ret != TCL_OK)
            return ret;
    }
    return TCL_OK;
}

/* Function: CffiDyncallFunctionsCmd
 * Creates commands mapped to functions in a DLL.
 *
 * Parameters:
 * ip   - interpreter
 * objc - number of elements in *objv*
 * objv - array containing the function definition list
 * ctxP - pointer to the context in which function is to be called
 *
 * The *objv[2]* element contains the function definition list.
 * This is a flat list of function name, type, parameter definitions.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
static CffiResult
CffiDyncallFunctionsCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP)
{
    return CffiDyncallManyFunctionsCmd(ip, objc, objv, ctxP, DC_CALL_C_DEFAULT);
}

/* Function: CffiDyncallStdcallsCmd
 * Creates commands mapped to stdcall functions in a DLL.
 *
 * Parameters:
 * ip   - interpreter
 * objc - number of elements in *objv*
 * objv - array containing the function definition list
 * ctxP - pointer to the context in which function is to be called
 *
 * The *objv[2]* element contains the function definition list.
 * This is a flat list of function name, type, parameter definitions.
 *
 * Irrespective of the function return type definition, the call mode
 * is always set to stdcall for the functions.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
static CffiResult
CffiDyncallStdcallsCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP)
{
    return CffiDyncallManyFunctionsCmd(ip, objc, objv, ctxP, DC_CALL_C_X86_WIN32_STD);
}

static CffiResult
CffiDyncallDestroyCmd(Tcl_Interp *ip,
                int objc,
                Tcl_Obj *const objv[],
                CffiLibCtx *ctxP)
{
    /*
    * objv[0] is the command name for the DLL. Deleteing
    * the command will also release associated resources
    */
    if (Tcl_DeleteCommand(ip, Tcl_GetString(objv[0])) == 0)
        return TCL_OK;
    else
        return Tclh_ErrorOperFailed(ip, "delete", objv[0], NULL);
}

static CffiResult
CffiDyncallPathCmd(Tcl_Interp *ip,
                int objc,
                Tcl_Obj *const objv[],
                CffiLibCtx *ctxP)
{
    int len;
    char buf[1024 + 1]; /* TBD - what size ? */

    len = dlGetLibraryPath(ctxP->dlP, buf, sizeof(buf) / sizeof(buf[0]));
    /*
     * Workarounds for bugs in dyncall 1.2 on some platforms when dcLoad was
     * called with null argument.
     */
    if (len <= 0)
        return TCL_OK; /* Return empty string */
    if (buf[len-1] == '\0')
        --len;
    Tcl_SetObjResult(ip, Tcl_NewStringObj(buf, len));
    return TCL_OK;
}

static CffiResult
CffiDyncallAddressOfCmd(Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[],
                  CffiLibCtx *ctxP)
{
    void *addr;

    CFFI_ASSERT(objc == 3);
    addr = dlFindSymbol(ctxP->dlP, Tcl_GetString(objv[2]));
    if (addr == NULL)
        return Tclh_ErrorNotFound(ip, "Symbol", objv[2], NULL);
    Tcl_SetObjResult(ip, Tclh_ObjFromAddress(addr));
    return TCL_OK;
}

static CffiResult
CffiDyncallInstanceCmd(ClientData cdata,
                   Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[])
{
    CffiLibCtx *ctxP = (CffiLibCtx *)cdata;
    static const Tclh_SubCommand subCommands[] = {
        {"addressof", 1, 1, "SYMBOL", CffiDyncallAddressOfCmd},
        {"destroy", 0, 0, "", CffiDyncallDestroyCmd},
        {"function", 3, 3, "NAME RETURNTYPE PARAMDEFS", CffiDyncallFunctionCmd},
        {"functions", 1, 1, "FUNCTIONLIST", CffiDyncallFunctionsCmd},
        {"path", 0, 0, "", CffiDyncallPathCmd},
        {"stdcall", 3, 3, "NAME RETURNTYPE PARAMDEFS", CffiDyncallStdcallCmd},
        {"stdcalls", 1, 1, "FUNCTIONLIST", CffiDyncallStdcallsCmd},
        {NULL}};
    int cmdIndex;

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ip, objc, objv, ctxP);
}

static void CffiDyncallInstanceDeleter(ClientData cdata)
{
    CffiLibCtx *ctxP = (CffiLibCtx *)cdata;
    /* Note cdlP->vmCtxP is interp-specific and not to be deleted here. */
    dlFreeLibrary(ctxP->dlP);
    ckfree(ctxP);
}

/* Function: CffiDyncallLibraryObjCmd
 * Implements the script level *dll* command.
 *
 * Parameters:
 * cdata - not used
 * ip - interpreter
 * objc - count of elements in objv
 * objv - aguments
 *
 * Returns:
 * TCL_OK on success with result in interpreter or TCL_ERROR with error
 * message in interpreter.
 */
static CffiResult CffiDyncallLibraryObjCmd(ClientData  cdata,
                                    Tcl_Interp *ip,
                                    int         objc,
                                    Tcl_Obj *const objv[])
{
    DLLib *dlP;
    CffiLibCtx *ctxP;
    Tcl_Obj *nameObj;
    Tcl_Obj *pathObj;
    const char *path;
    static const Tclh_SubCommand subCommands[] = {
        {"new", 0, 1, "?DLLPATH?", NULL},
        {"create", 1, 2, "OBJNAME ?DLLPATH?", NULL},
        {NULL}
    };
    int cmdIndex;
    int ret;
    static unsigned int name_generator; /* No worries about thread safety as
                                           generated names are interp-local */

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));

    if (cmdIndex == 0) {
        /* new */
        nameObj = Tcl_ObjPrintf("::" CFFI_NAMESPACE "::dll%u", ++name_generator);
        pathObj  = objc > 2 ? objv[2] : NULL;
    }
    else {
        /* create */
        nameObj = CffiQualifyName(ip, objv[2]);
        pathObj  = objc > 3 ? objv[3] : NULL;
    }
    Tcl_IncrRefCount(nameObj);

    if (pathObj) {
        path = Tcl_GetString(pathObj);
        if (path[0] == '\0')
            path = NULL;
    }
    else
        path = NULL;

    dlP = dlLoadLibrary(pathObj ? Tcl_GetString(pathObj) : NULL);
    if (dlP == NULL) {
        ret = Tclh_ErrorNotFound(
            ip, "Shared library", pathObj, "Could not load shared library.");
    }
    else {
        ctxP         = ckalloc(sizeof(*ctxP));
        ctxP->vmCtxP = (CffiCallVmCtx *)cdata;
        ctxP->dlP    = dlP;

        Tcl_CreateObjCommand(ip,
                             Tcl_GetString(nameObj),
                             CffiDyncallInstanceCmd,
                             ctxP,
                             CffiDyncallInstanceDeleter);

        Tcl_SetObjResult(ip, nameObj);
        ret = TCL_OK;
    }

    Tcl_DecrRefCount(nameObj);
    return ret;
}

static CffiResult
CffiSymbolsDestroyCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    DLSyms *dlsP)
{
    /*
    * objv[0] is the command name for the loaded symbols file. Deleteing
    * the command will also release associated resources like dlsP
    */
    if (Tcl_DeleteCommand(ip, Tcl_GetString(objv[0])) == 0)
        return TCL_OK;
    else
        return Tclh_ErrorOperFailed(ip, "delete", objv[0], NULL);
}

static CffiResult
CffiSymbolsCountCmd(Tcl_Interp *ip, int objc, Tcl_Obj *const objv[], DLSyms *dlsP)
{
    Tcl_SetObjResult(ip, Tcl_NewIntObj(dlSymsCount(dlsP)));
    return TCL_OK;
}

static CffiResult
CffiSymbolsIndexCmd(Tcl_Interp *ip, int objc, Tcl_Obj *const objv[], DLSyms *dlsP)
{
    int ival;
    const char *symName;

    CHECK(Tcl_GetIntFromObj(ip, objv[2], &ival));
    /*
     * For at least one executable format (PE), dyncall 1.2 does not check
     * index range so do so ourselves.
     */
    if (ival < 0 || ival >= dlSymsCount(dlsP)) {
        return Tclh_ErrorNotFound(
            ip, "Symbol index", objv[2], "No symbol at specified index.");
    }
    symName = dlSymsName(dlsP, ival);
    if (symName != NULL) {
        Tcl_SetResult(ip, (char *)symName, TCL_VOLATILE);
    }
    return TCL_OK;
}

static CffiResult
CffiSymbolsAtAddressCmd(Tcl_Interp *ip, int objc, Tcl_Obj *const objv[], DLSyms *dlsP)
{
    Tcl_WideInt wide;
    const char *symName;

    /* TBD - use pointer type instead of WideInt? */
    CHECK(Tcl_GetWideIntFromObj(ip, objv[2], &wide));

    symName = dlSymsNameFromValue(dlsP, (void *) (intptr_t) wide);
    if (symName == NULL)
        return Tclh_ErrorNotFound(
            ip,
            "Address",
            objv[2],
            "No symbol at specified address or library not loaded.");
    Tcl_SetResult(ip, (char *)symName, TCL_VOLATILE);
    return TCL_OK;
}

static CffiResult
CffiSymbolsInstanceCmd(ClientData cdata,
                        Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[])
{
    DLSyms *dlsP = (DLSyms *)cdata;
    static const Tclh_SubCommand subCommands[] = {
        {"destroy", 0, 0, "", CffiSymbolsDestroyCmd},
        {"count", 0, 0, "", CffiSymbolsCountCmd},
        {"index", 1, 1, "INDEX", CffiSymbolsIndexCmd},
        {"ataddress", 1, 1, "ADDRESS", CffiSymbolsAtAddressCmd},
        {NULL},
    };
    int cmdIndex;

    /* TBD - check dlsP validity?*/

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ip, objc, objv, dlsP);
}

static void
CffiSymbolsInstanceDeleter(ClientData cdata)
{
    dlSymsCleanup((DLSyms*)cdata);
}

/* Function: CffiSymbolsObjCmd
 * Implements the script level *dll* command.
 *
 * Parameters:
 * cdata - not used
 * ip - interpreter
 * objc - count of elements in objv
 * objv - aguments
 *
 * Returns:
 * TCL_OK on success with result in interpreter or TCL_ERROR with error
 * message in interpreter.
 */
static CffiResult
CffiSymbolsObjCmd(ClientData cdata,
                   Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[])
{
    DLSyms *dlsP;
    Tcl_Obj *nameObj;
    Tcl_Obj *pathObj;
    static const Tclh_SubCommand subCommands[] = {
        {"new", 0, 1, "?DLLPATH?", NULL},
        {"create", 1, 2, "OBJNAME ?DLLPATH?", NULL},
        {NULL}
    };
    int cmdIndex;
    int ret;
    static unsigned int  name_generator; /* No worries about thread safety as
                                            generated names are interp-local */

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));

    pathObj = NULL;
    if (cmdIndex == 0) {
        /* new */
        nameObj = Tcl_ObjPrintf("::" CFFI_NAMESPACE "::syms%u", ++name_generator);
        if (objc > 2)
            pathObj = objv[2];
    }
    else {
        /* create */
        nameObj = CffiQualifyName(ip, objv[2]);
        if (objc > 3)
            pathObj = objv[3];
    }
    Tcl_IncrRefCount(nameObj);

    dlsP = dlSymsInit(pathObj ? Tcl_GetString(pathObj) : NULL);
#if defined(_WIN32)
    /* dyncall 1.2 does not protect against missing exports table in PE files */
    /* TBD this is hardcoded to dyncall 1.2 internals! */
    struct MyDLSyms_ {
        DLLib *pLib;
        const char *pBase;
        const DWORD *pNames;
        const DWORD *pFuncs;
        const unsigned short *pOrds;
        size_t count;
    };

    if (dlsP) {
        const char *base = (const char *)((struct MyDLSyms_ *)dlsP)->pLib;
        if (base) {
            IMAGE_DOS_HEADER *pDOSHeader;
            IMAGE_NT_HEADERS *pNTHeader;
            IMAGE_DATA_DIRECTORY *pExportsDataDir;
            pDOSHeader      = (IMAGE_DOS_HEADER *)base;
            pNTHeader       = (IMAGE_NT_HEADERS *)(base + pDOSHeader->e_lfanew);
            if (pNTHeader->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) {
                /* This image doesn't have an export directory table. */
                base = NULL;
            }
            else {
                /* No offset to table */
                pExportsDataDir =
                    &pNTHeader->OptionalHeader
                         .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
                if (pExportsDataDir->VirtualAddress == 0)
                    base = NULL;
            }
        }
        if (base == NULL) {
            dlSymsCleanup(dlsP);
            dlsP = NULL;
        }
    }
#endif
    if (dlsP == NULL) {
        ret = Tclh_ErrorNotFound(
            ip, "Symbols container", pathObj, "Could not find file or export table in file.");
    }
    else {
        Tcl_CreateObjCommand(ip,
                             Tcl_GetString(nameObj),
                             CffiSymbolsInstanceCmd,
                             dlsP,
                             CffiSymbolsInstanceDeleter);
        Tcl_SetObjResult(ip, nameObj);
        ret = TCL_OK;
    }
    Tcl_DecrRefCount(nameObj);
    return ret;
}


/* Function: CffiStructAllocateCmd
 * Allocates memory for one or more structures.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[].
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level, and an optional count
 * scructCtxP - pointer to struct context
 *
 * The single optional argument specifies the number of structures to
 * allocate. By default, space for a single structure is allocated.
 * The allocated space is not initialized.
 *
 * Returns:
 * *TCL_OK* on success with the wrapped pointer as the interpreter result.
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructAllocateCmd(Tcl_Interp *ip,
               int objc,
               Tcl_Obj *const objv[],
               CffiStructCtx *structCtxP)
{
    CffiStruct *structP = structCtxP->structP;
    Tcl_Obj *resultObj;
    void *resultP;
    int count;

    if (objc == 2)
        count = 1;
    else {
        CHECK(Tclh_ObjToInt(ip, objv[2], &count));
        if (count <= 0)
            return Tclh_ErrorInvalidValue(
                ip, NULL, "Array size must be a positive integer.");
    }
    resultP = ckalloc(count * structP->size);

    if (Tclh_PointerRegister(ip, resultP, structP->name, &resultObj) != TCL_OK) {
        ckfree(resultP);
        return TCL_ERROR;
    }
    Tcl_SetObjResult(ip, resultObj);
    return TCL_OK;
}

/* Function: CffiStructFromNativeCmd
 * Returns a Tcl dictionary value from a C struct in memory.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 3-4 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * scructCtxP - pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - pointer to memory
 * objv[3] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with the dictionary value as interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructFromNativeCmd(Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[],
                        CffiStructCtx *structCtxP)
{
    CffiStruct *structP = structCtxP->structP;
    void *valueP;
    int index;
    Tcl_Obj *resultObj;

    CHECK(Tclh_PointerObjVerify(ip, objv[2], &valueP, structP->name));

    /* TBD - check alignment of valueP */

    if (objc == 4) {
        Tcl_WideInt wide;
        CHECK(Tclh_ObjToRangedInt(ip, objv[3], 0, INT_MAX, &wide));
        index = (int) wide;
    }
    else
        index = 0;

    /* TBD - check addition does not cause valueP to overflow */
    CHECK(CffiStructToObj(
        ip, structP, (index * structP->size) + (char *)valueP, &resultObj));

    Tcl_SetObjResult(ip, resultObj);
    return TCL_OK;
}

/* Function: CffiStructToNativeCmd
 * Initializes memory as a C struct from a Tcl dictionary representation.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 4-5 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * scructCtxP - pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - pointer to memory
 * objv[3] - dictionary value to use as initializer
 * objv[4] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructToNativeCmd(Tcl_Interp *ip,
                      int objc,
                      Tcl_Obj *const objv[],
                      CffiStructCtx *structCtxP)
{
    CffiStruct *structP = structCtxP->structP;
    void *valueP;
    int index;

    CHECK(Tclh_PointerObjVerify(ip, objv[2], &valueP, structP->name));

    /* TBD - check alignment of valueP */

    if (objc == 5) {
        Tcl_WideInt wide;
        CHECK(Tclh_ObjToRangedInt(ip, objv[4], 0, INT_MAX, &wide));
        index = (int) wide;
    }
    else
        index = 0;

    /* TBD - check addition does not cause valueP to overflow */
    CHECK(CffiStructFromObj(
        ip, structP, objv[3], (index * structP->size) + (char *)valueP));

    return TCL_OK;
}

#ifdef OBSOLETE

/* Function: CffiStructEncodeCmd
 * Allocates and initializes a structure or array of structures in memory.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 2-4 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * scructCtxP - pointer to struct context
 *
 * Without any arguments, a single zeroed-out structure is allocated and
 * returned. If a single argument is passed, it is the initializer for the
 * struct. If two arguments are passed, the first is a list of initializers
 * and the second is a count. If there are more initializers, the additional
 * ones are ignored. If there are fewer than the specified count, the
 * remaining structs are zeroed.
 *
 * Returns:
 * *TCL_OK* on success with the wrapped pointer as the interpreter result.
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructEncodeCmd(Tcl_Interp *ip,
               int objc,
               Tcl_Obj *const objv[],
               CffiStructCtx *structCtxP)
{
    CffiStruct *structP = structCtxP->structP;
    Tcl_Obj *resultObj;
    void *resultP;
    Tcl_Obj *initObj;
    Tcl_Obj **initObjs;
    int nInit;
    int count;
    int i;

    if (objc < 4) {
        /* struct new ?INITIALIZER? */
        count = 1;
        if (objc < 3) {
            nInit = 0;
            initObjs = NULL;
        }
        else {
            nInit = 1;
            initObj  = objv[2];
            initObjs = &initObj;
        }
    }
    else {
        /* struct new INITIALIZERS COUNT */
        CHECK(Tcl_ListObjGetElements(ip, objv[2], &nInit, &initObjs));
        CHECK(Tclh_ObjToInt(ip, objv[3], &count));
        if (count <= 0)
            return Tclh_ErrorInvalidValue(
                ip, NULL, "Negative or zero specified for struct constructor array size.");
    }

    resultP = ckalloc(count * structP->size);
    if (nInit > count)
        nInit = count;
    for (i = 0; i < nInit; ++i) {
        void *valueP = (i * structP->size) + (char *) resultP;
        if (CffiStructFromObj(ip, structP, initObjs[i], valueP) != TCL_OK) {
            ckfree(resultP);
            return TCL_ERROR;
        }
    }

    /* Fill any uninitializers with zeroes */
    if (count > nInit) {
        memset((nInit * structP->size) + (char *)resultP,
               0,
               (count - nInit) * structP->size);
    }

    if (Tclh_PointerRegister(ip, resultP, structP->name, &resultObj) != TCL_OK) {
        ckfree(resultP);
        return TCL_ERROR;
    }
    Tcl_SetObjResult(ip, resultObj);
    return TCL_OK;
}
#endif /* OBSOLETE */

/* Function: CffiStructFreeCmd
 * Releases the memory allocated for a struct instance.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        a single argument.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * scructCtxP - pointer to struct context
 *
 * The function verifies validity of the passed pointer and unregisters it
 * before freeing it.
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on failure with message in interpreter.
 */
static CffiResult
CffiStructFreeCmd(Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[],
                   CffiStructCtx *structCtxP)
{
    void *valueP;
    CffiResult ret;

    ret = Tclh_PointerObjUnregister(
        ip, objv[2], &valueP, structCtxP->structP->name);
    if (ret == TCL_OK && valueP)
        ckfree(valueP);
    return ret;
}

#ifdef OBSOLETE
/* Function: CffiStructDecodeCmd
 * Dereferences a pointer to a struct and returns the *Tcl_Obj* representation.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 3-4 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * scructCtxP - pointer to struct context
 *
 * If a single argument is provided, it is a pointer to a struct. The
 * function then returns the dictionary representation of the struct as the
 * interpreter result. If two arguments are provided, the first is a pointer
 * to an array of structs and the second is the number of elements of that
 * array. In this case the function stores a list containing the
 * dictionaries into the interpreter result.
 *
 * Returns:
 * *TCL_OK* on success with the dictionar(ies) in the interpreter result and
 * *TCL_ERROR* on failure with the error message in the interpreter result.
 */
static CffiResult
CffiStructDecodeCmd(Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[],
                  CffiStructCtx *structCtxP)
{
    void *valueP;
    int count;
    int ret;
    Tcl_Obj *resultObj;
    CffiStruct *structP = structCtxP->structP;

    CHECK(Tclh_PointerObjVerify(ip, objv[2], &valueP, structP->name));
    if (objc == 4) {
        Tcl_WideInt wide;
        CHECK(Tclh_ObjToRangedInt(ip, objv[3], 0, INT_MAX, &wide));
        count = (int) wide;
    }
    else
        count = 0;
    if (count == 0) {
        /* Single value */
        ret = CffiStructToObj(ip, structP, valueP, &resultObj);
    }
    else {
        /* Array of values, even if count == 1. Return as list */
        int i;
        resultObj = Tcl_NewListObj(count, NULL);
        for (i = 0; i < count; ++i) {
            Tcl_Obj *valueObj;
            ret = CffiStructToObj(
                ip, structP, (i * structP->size) + (char *)valueP, &valueObj);
            if (ret != TCL_OK) {
                Tcl_DecrRefCount(resultObj);
                break;
            }
            Tcl_ListObjAppendElement(NULL, resultObj, valueObj);
        }
    }
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, resultObj);
    return ret;
}
#endif /* OBSOLETE */

/* Function: CffiStructToBinaryCmd
 * Implements the *STRUCT tobinary* command converting a dictionary to a
 * native struct in a Tcl byte array object.
 *
 * Parameters:
 * ip - interpreter
 * objc - number of elements in *objv*
 * objv - argument array. Caller should have checked it has exactly 3
 *        3 elements with objv[2] holding the dictionary representation.
 * scructCtxP - pointer to struct context
 *
 * Returns:
 * *TCL_OK* on success with the *Tcl_Obj* byte array in the interpreter
 * result and *TCL_ERROR* on failure with the error message in the
 * interpreter result.
 */
static CffiResult
CffiStructToBinaryCmd(Tcl_Interp *ip,
                       int objc,
                       Tcl_Obj *const objv[],
                       CffiStructCtx *structCtxP)
{
    void *valueP;
    CffiResult ret;
    Tcl_Obj *resultObj;
    CffiStruct *structP = structCtxP->structP;

    resultObj = Tcl_NewByteArrayObj(NULL, structP->size);
    valueP    = Tcl_GetByteArrayFromObj(resultObj, NULL);
    ret       = CffiStructFromObj(ip, structP, objv[2], valueP);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, resultObj);
    else
        Tcl_DecrRefCount(resultObj);
    return ret;
}

/* Function: CffiStructFromBinaryCmd
 * Implements the *STRUCT frombinary* command that converts a native
 * structure stored in a Tcl byte array to a dictionary.
 *
 * Parameters:
 * ip - interpreter
 * objc - number of elements in *objv*. Must be 3.
 * objv - argument array. Caller should have checked it has 3-4 elements
 *        with objv[2] holding the byte array representation and optional
 *        objv[3] holding the offset into it.
 * scructCtxP - pointer to struct context
 *
 * Returns:
 * *TCL_OK* on success with the *Tcl_Obj* dictionary in the interpreter
 * result and *TCL_ERROR* on failure with the error message in the
 * interpreter result.
 */
static CffiResult
CffiStructFromBinaryCmd(Tcl_Interp *ip,
                         int objc,
                         Tcl_Obj *const objv[],
                         CffiStructCtx *structCtxP)
{
    unsigned char *valueP;
    int len;
    unsigned int offset;
    CffiResult ret;
    Tcl_Obj *resultObj;
    CffiStruct *structP = structCtxP->structP;

    valueP = Tcl_GetByteArrayFromObj(objv[2], &len);
    if (objc == 4)
        CHECK(Tclh_ObjToUInt(ip, objv[3], &offset));
    else
        offset = 0;

    if (len < (int) structP->size || (len - structP->size) < offset) {
        /* Do not pass objv[2] to error function -> binary values not printable */
        return Tclh_ErrorInvalidValue(
            ip, NULL, "Truncated structure binary value.");
    }
    ret = CffiStructToObj(ip, structP, offset + valueP, &resultObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, resultObj);
    return ret;
}

/* Function: CffiStructNameCmd
 * Implements the *STRUCT name* command to retrieve a struct name.
 *
 * Parameters:
 * ip - interpreter
 * objc - number of elements in *objv*
 * objv - argument array. Caller should have checked it has exactly 2 elements
 * scructCtxP - pointer to struct context
 *
 * Returns:
 * *TCL_OK* on success with the struct name as the interpreter
 * result and *TCL_ERROR* on failure with the error message in the
 * interpreter result.
 */
static CffiResult
CffiStructNameCmd(Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[],
                   CffiStructCtx *structCtxP)
{
    Tcl_SetObjResult(ip, structCtxP->structP->name);
    return TCL_OK;
}

static CffiResult
CffiStructDestroyCmd(Tcl_Interp *ip,
                      int objc,
                      Tcl_Obj *const objv[],
                      CffiStructCtx *structCtxP)
{
    /*
    * objv[0] is the command name for the struct instance. Deleteing
    * the command will also release associated resources including structP.
    */
    if (Tcl_DeleteCommand(ip, Tcl_GetString(objv[0])) == 0)
        return TCL_OK;
    else
        return Tclh_ErrorOperFailed(ip, "delete", objv[0], NULL);
}

/* Function: CffiStructInstanceCmd
 * Implements the script level command for struct instances.
 *
 * Parameters:
 * cdata - not used
 * ip - interpreter
 * objc - argument count
 * objv - argument array
 *
 * Returns:
 * TCL_OK or TCL_ERROR, with result in interpreter.
 */
static CffiResult
CffiStructInstanceCmd(ClientData cdata,
                        Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[])
{
    CffiStructCtx *structCtxP = (CffiStructCtx *)cdata;
    static const Tclh_SubCommand subCommands[] = {
        {"allocate", 0, 1, "?COUNT?", CffiStructAllocateCmd}, /* Same command as Encode */
        {"destroy", 0, 0, "", CffiStructDestroyCmd},
        {"fromnative", 1, 2, "POINTER ?INDEX?", CffiStructFromNativeCmd},
        {"describe", 0, 0, "", CffiStructDescribeCmd},
        {"tonative", 2, 3, "POINTER INITIALIZER ?INDEX?", CffiStructToNativeCmd},
        {"free", 1, 1, "POINTER", CffiStructFreeCmd},
        {"frombinary", 1, 1, "BINARY", CffiStructFromBinaryCmd},
        {"info", 0, 0, "", CffiStructInfoCmd},
        {"name", 0, 0, "", CffiStructNameCmd},
        {"tobinary", 1, 1, "DICTIONARY", CffiStructToBinaryCmd},
        {NULL}
    };
    int cmdIndex;

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ip, objc, objv, structCtxP);
}

static void
CffiStructInstanceDeleter(ClientData cdata)
{
    CffiStructCtx *ctxP = (CffiStructCtx *)cdata;
    if (ctxP->structP)
        CffiStructUnref(ctxP->structP);
    /* Note ctxP->ipCtxP is interp-wide and not to be freed here */
    ckfree(ctxP);
}

static CffiResult
CffiStructObjCmd(ClientData cdata,
                   Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    CffiStruct *structP;
    CffiStructCtx *structCtxP;
    static const Tclh_SubCommand subCommands[] = {
        {"new", 1, 1, "STRUCTDEF", NULL},
        {"create", 2, 2, "OBJNAME STRUCTDEF", NULL},
        {NULL}
    };
    Tcl_Obj *structNameObj;
    Tcl_Obj *cmdNameObj;
    Tcl_Obj *defObj;
    int cmdIndex;
    int ret;
    static unsigned int name_generator; /* No worries about thread safety as
                                           generated names are interp-local */

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));

    if (cmdIndex == 0) {
        /* new */
        cmdNameObj = Tcl_ObjPrintf("::" CFFI_NAMESPACE "::struct%u", ++name_generator);
        defObj  = objv[2];
    }
    else {
        /* create */
        cmdNameObj = CffiQualifyName(ip, objv[2]);
        defObj  = objv[3];
    }
    Tcl_IncrRefCount(cmdNameObj);
    /* struct name does not have preceding :: for cosmetic reasons. */
    structNameObj = Tcl_ObjPrintf("%s", 2 + Tcl_GetString(cmdNameObj));
    Tcl_IncrRefCount(structNameObj);

    ret = CffiStructParse(ipCtxP, structNameObj, defObj, &structP);
    if (ret == TCL_OK) {
        structCtxP          = ckalloc(sizeof(*structCtxP));
        structCtxP->ipCtxP  = ipCtxP;
        CffiStructRef(structP);
        structCtxP->structP = structP;

        Tcl_CreateObjCommand(ip,
                             Tcl_GetString(cmdNameObj),
                             CffiStructInstanceCmd,
                             structCtxP,
                             CffiStructInstanceDeleter);
        Tcl_SetObjResult(ip, cmdNameObj);
    }
    Tcl_DecrRefCount(cmdNameObj);
    Tcl_DecrRefCount(structNameObj);
    return ret;
}

/* Function: CffiMemoryAllocateCmd
 * Implements the *pointer allocate* script level command.
 *
 * Parameters:
 * ip - interpreter
 * objc - count of elements in objv[]. Should be 3 or 4 including command
 *        and subcommand.
 * objv - argument array.
 * flags - unused
 *
 * Allocated memory using *ckalloc* and returns a wrapped pointer to it.
 * The *objv[2]* argument contains the allocation size. Optionally, the
 * *objv[3]* argument may be passed as the pointer type tag.
 *
 * Returns:
 * *TCL_OK* on success with wrapped pointer as interpreter result,
 * *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemoryAllocateCmd(Tcl_Interp *ip, int objc, Tcl_Obj *const objv[], int flags)
{
    Tcl_WideInt size;
    CffiResult ret;
    Tcl_Obj *ptrObj;
    void *p;

    CHECK(Tclh_ObjToRangedInt(ip, objv[2], 1, INT_MAX, &size));
    p = ckalloc((int)size);
    ret = Tclh_PointerRegister(ip, p, objc == 4 ? objv[3] : NULL, &ptrObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, ptrObj);
    else
        ckfree(p);
    return ret;
}

/* Function: CffiMemoryFreeCmd
 * Implements the *pointer free* script level command.
 *
 * Parameters:
 * ip - interpreter
 * objc - count of elements in objv[]. Should be 3 including command
 *        and subcommand.
 * objv - argument array.
 * flags - unused
 *
 * Unregisters the wrapped pointer in *objv[2]* and frees the memory.
 * The pointer must have been previously allocated with one of
 * the extensions allocation calls.
 *
 * The function will take no action if the pointer is NULL.
 *
 * Returns:
 *
 */
static CffiResult
CffiMemoryFreeCmd(Tcl_Interp *ip, int objc, Tcl_Obj *const objv[], int flags)
{
    void *pv;
    CffiResult ret;

    CHECK(Tclh_PointerUnwrap(ip, objv[2], &pv, NULL));
    if (pv == NULL)
        return TCL_OK;
    ret = Tclh_PointerUnregister(ip, pv, NULL);
    if (ret == TCL_OK)
        ckfree(pv);
    return ret;
}

/* Function: CffiMemoryFromBinaryCmd
 * Implements the *pointer frombinary* script level command.
 *
 * Parameters:
 * ip - interpreter
 * objc - count of elements in objv[]. Should be 3 or 4 including command
 *        and subcommand.
 * objv - argument array.
 * flags - unused
 *
 * Allocates memory and copies contents of *objv[2]* (as a byte array) to
 * it and returns a wrapped pointer to the memory in the interpreter.
 * The optional *objv[3]* argument is the pointer type tag.
 *
 * Returns:
 * *TCL_OK* on success with wrapped pointer as interpreter result,
 * *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemoryFromBinaryCmd(Tcl_Interp *ip, int objc, Tcl_Obj *const objv[], int flags)
{
    CffiResult ret;
    Tcl_Obj *ptrObj;
    void *p;
    unsigned char *bytes;
    int len;

    bytes = Tcl_GetByteArrayFromObj(objv[2], &len);
    p = ckalloc(len);
    memmove(p, bytes, len);
    ret = Tclh_PointerRegister(ip, p, objc == 4 ? objv[3] : NULL, &ptrObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, ptrObj);
    else
        ckfree(p);
    return ret;
}

/* Function: CffiMemoryToBinaryCmd
 * Implements the *pointer tobinary* script level command.
 *
 * Parameters:
 * ip - interpreter
 * objc - count of elements in objv[]. Should be 3 or 4 including command
 *        and subcommand.
 * objv - argument array.
 * flags - if the low bit is set, the pointer is treated as unsafe and not
 *        checked for validity.
 *
 * Returns the *objv[3]* bytes of memory referenced by the wrapped pointer
 * in *objv[2]* as a *Tcl_Obj* byte array. The passed in pointer should be
 * registered unless the low bit of *flags* is set.
 * If the pointer is *NULL*, the returned byte array is empty.
 *
 * Returns:
 * *TCL_OK* on success with wrapped pointer as interpreter result,
 * *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemoryToBinaryCmd(Tcl_Interp *ip, int objc, Tcl_Obj *const objv[], int flags)
{
    void *pv;
    unsigned int len;

    if (flags & 1)
        CHECK(Tclh_PointerUnwrap(ip, objv[2], &pv, NULL));
    else
        CHECK(Tclh_PointerObjVerify(ip, objv[2], &pv, NULL));
    CHECK(Tclh_ObjToUInt(ip, objv[3], &len));
    Tcl_SetObjResult(ip, Tcl_NewByteArrayObj(pv, len));
    return TCL_OK;
}

static CffiResult
CffiMemoryObjCmd(ClientData cdata,
                   Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[])
{
    /* The flags field low bit is set for unsafe pointer operation */
    static const Tclh_SubCommand subCommands[] = {
        {"allocate", 1, 2, "SIZE ?TYPETAG?", CffiMemoryAllocateCmd, 0},
        {"free", 1, 1, "POINTER", CffiMemoryFreeCmd, 0},
        {"frombinary", 1, 2, "BINARY ?TYPETAG?", CffiMemoryFromBinaryCmd, 0},
        {"tobinary", 2, 2, "POINTER SIZE", CffiMemoryToBinaryCmd, 0},
        {"tobinary!", 2, 2, "POINTER SIZE", CffiMemoryToBinaryCmd, 1},
        {NULL}
    };
    int cmdIndex;

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ip, objc, objv, subCommands[cmdIndex].flags);
}

static void
CffiAliasesCleanup(Tcl_HashTable *typeAliasTableP)
{
    Tcl_HashEntry *heP;
    Tcl_HashSearch hSearch;
    for (heP = Tcl_FirstHashEntry(typeAliasTableP, &hSearch);
         heP != NULL; heP = Tcl_NextHashEntry(&hSearch)) {
        CffiTypeAndAttrs *typeAndAttrsP = Tcl_GetHashValue(heP);
        CffiTypeAndAttrsCleanup(typeAndAttrsP);
        ckfree(typeAndAttrsP);
    }
    Tcl_DeleteHashTable(typeAliasTableP);
}

static CffiResult
CffiAliasDefineCmd(CffiInterpCtx *ipCtxP,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;

    CFFI_ASSERT(objc == 4);

    heP = Tcl_FindHashEntry(&ipCtxP->typeAliases, objv[2]);
    if (heP)
        return Tclh_ErrorExists(ip, "Type or alias", objv[2], NULL);
    return CffiAliasAdd(ipCtxP, objv[2], objv[3]);
}

static CffiResult
CffiAliasBodyCmd(CffiInterpCtx *ipCtxP,
                 Tcl_Interp *ip,
                 int objc,
                 Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;
    CffiTypeAndAttrs *typeAttrsP;

    CFFI_ASSERT(objc == 3);

    heP = Tcl_FindHashEntry(&ipCtxP->typeAliases, objv[2]);
    if (heP == NULL)
        return Tclh_ErrorNotFound(ip, "Type or alias", objv[2], NULL);
    typeAttrsP = Tcl_GetHashValue(heP);
    Tcl_SetObjResult(ip, CffiTypeAndAttrsUnparse(typeAttrsP));
    return TCL_OK;
}

static CffiResult
CffiAliasListCmd(CffiInterpCtx *ipCtxP,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;
    Tcl_HashSearch hSearch;
    const char *pattern;
    Tcl_HashTable *typedefsP = &ipCtxP->typeAliases;
    Tcl_Obj *resultObj = Tcl_NewListObj(0, NULL);

    pattern = objc > 2 ? Tcl_GetString(objv[2]) : NULL;
    for (heP = Tcl_FirstHashEntry(typedefsP, &hSearch);
         heP != NULL; heP = Tcl_NextHashEntry(&hSearch)) {
        /* If a pattern was specified, only return those */
        if (pattern) {
            Tcl_Obj *key = Tcl_GetHashKey(typedefsP, heP);
            if (! Tcl_StringMatch(Tcl_GetString(key), pattern))
                continue;
        }

        Tcl_ListObjAppendElement(ipCtxP->interp,
                                 resultObj,
                                 (Tcl_Obj *)Tcl_GetHashKey(typedefsP, heP));
    }
    Tcl_SetObjResult(ipCtxP->interp, resultObj);
    return TCL_OK;
}

static CffiResult
CffiAliasLoadCmd(CffiInterpCtx *ipCtxP,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    CFFI_ASSERT(objc == 3);
    return CffiAddBuiltinAliases(ipCtxP, objv[2]);
}

static CffiResult
CffiAliasDeleteCmd(CffiInterpCtx *ipCtxP,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;
    Tcl_HashSearch hSearch;
    const char *pattern;
    Tcl_HashTable *tableP = &ipCtxP->typeAliases;

    CFFI_ASSERT(objc == 3);
    heP = Tcl_FindHashEntry(tableP, objv[2]);
    if (heP) {
        CffiTypeAndAttrs *typeAttrsP;
        typeAttrsP = Tcl_GetHashValue(heP);
        CffiTypeAndAttrsCleanup(typeAttrsP);
        ckfree(typeAttrsP);
        Tcl_DeleteHashEntry(heP);
        return TCL_OK;
    }

    /* Check if glob pattern */
    pattern = Tcl_GetString(objv[2]);
    for (heP = Tcl_FirstHashEntry(tableP, &hSearch);
         heP != NULL; heP = Tcl_NextHashEntry(&hSearch)) {
        Tcl_Obj *key = Tcl_GetHashKey(tableP, heP);
        if (Tcl_StringMatch(Tcl_GetString(key), pattern)) {
            CffiTypeAndAttrs *typeAndAttrsP = Tcl_GetHashValue(heP);
            CffiTypeAndAttrsCleanup(typeAndAttrsP);
            ckfree(typeAndAttrsP);
            Tcl_DeleteHashEntry(heP);
        }
    }

    return TCL_OK;
}

static CffiResult
CffiAliasObjCmd(ClientData cdata,
                Tcl_Interp *ip,
                int objc,
                Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    int cmdIndex;
    static const Tclh_SubCommand subCommands[] = {
        {"body", 1, 1, "ALIAS", CffiAliasBodyCmd},
        {"define", 2, 2, "ALIAS DEFINITION", CffiAliasDefineCmd},
        {"delete", 1, 1, "PATTERN", CffiAliasDeleteCmd},
        {"list", 0, 1, "?PATTERN?", CffiAliasListCmd},
        {"load", 1, 1, "ALIASSET", CffiAliasLoadCmd},
        {NULL}
    };

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ipCtxP, ip, objc, objv);
}

static CffiResult
CffiTypeObjCmd(ClientData cdata,
                Tcl_Interp *ip,
                int objc,
                Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    CffiTypeAndAttrs typeAttrs;
    CffiResult ret;
    enum cmdIndex { INFO, SIZE, COUNT };
    int cmdIndex;
    int size, alignment;
    int parse_mode;
    static const Tclh_SubCommand subCommands[] = {
        {"info", 1, 2, "TYPE ?PARSEMODE?", NULL},
        {"size", 1, 1, "TYPE", NULL},
        {"count", 1, 1, "TYPE", NULL},
        {NULL}
    };

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));

    /* For type info, check if a parse mode is specified */
    parse_mode = -1;
    if (cmdIndex == INFO && objc == 4) {
        const char *s = Tcl_GetString(objv[3]);
        if (*s == '\0')
            parse_mode = -1;
        else if (!strcmp(s, "param"))
            parse_mode = CFFI_F_TYPE_PARSE_PARAM;
        else if (!strcmp(s, "return"))
            parse_mode = CFFI_F_TYPE_PARSE_RETURN;
        else if (!strcmp(s, "field"))
            parse_mode = CFFI_F_TYPE_PARSE_FIELD;
        else
            return Tclh_ErrorInvalidValue(ip, objv[3], "Invalid parse mode.");
    }
    ret = CffiTypeAndAttrsParse(ipCtxP, objv[2], parse_mode, &typeAttrs);
    if (ret == TCL_ERROR)
        return ret;

    if (cmdIndex == COUNT) {
        /* type nelems */
        Tcl_SetObjResult(ip, Tcl_NewIntObj(typeAttrs.dataType.count));
    }
    else {
        CffiTypeLayoutInfo(&typeAttrs.dataType, NULL, &size, &alignment);
        if (cmdIndex == SIZE)
            Tcl_SetObjResult(ip, Tcl_NewIntObj(size));
        else {
            /* type info */
            Tcl_Obj *objs[8];
            objs[0] = Tcl_NewStringObj("size", 4);
            objs[1] = Tcl_NewIntObj(size);
            objs[2] = Tcl_NewStringObj("count", 5);
            objs[3] = Tcl_NewIntObj(typeAttrs.dataType.count);
            objs[4] = Tcl_NewStringObj("alignment", 9);
            objs[5] = Tcl_NewIntObj(alignment);
            objs[6] = Tcl_NewStringObj("definition", 10);
            objs[7] = CffiTypeAndAttrsUnparse(&typeAttrs);
            Tcl_SetObjResult(ip, Tcl_NewListObj(8, objs));
        }
    }

    CffiTypeAndAttrsCleanup(&typeAttrs);
    return ret;
}

static CffiResult
CffiPointerObjCmd(ClientData cdata,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[])
{
    int cmdIndex;
    void *pv;
    CffiResult ret;
    Tcl_Obj *tagObj;

    static const Tclh_SubCommand subCommands[] = {
        {"address", 1, 1, "POINTER", NULL},
        {"list", 0, 1, "?TAG?", NULL},
        {"check", 1, 1, "POINTER", NULL},
        {"isvalid", 1, 1, "POINTER", NULL},
        {"tag", 1, 1, "POINTER", NULL},
        {"safe", 1, 1, "POINTER", NULL},
        {"counted", 1, 1, "POINTER", NULL},
        {"dispose", 1, 1, "POINTER", NULL},
        {"isnull", 1, 1, "POINTER", NULL},
        {NULL}
    };
    enum cmdIndex { ADDRESS, LIST, CHECK, ISVALID, TAG, SAFE, COUNTED, DISPOSE, ISNULL };

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));

    /* LIST does not take a pointer arg like the others */
    if (cmdIndex == LIST) {
        Tcl_Obj *resultObj;
        resultObj = Tclh_PointerEnumerate(ip, objc == 3 ? objv[2] : NULL);
        Tcl_SetObjResult(ip, resultObj);
        return TCL_OK;
    }

    ret = Tclh_PointerUnwrap(ip, objv[2], &pv, NULL);
    if (ret != TCL_OK)
        return ret;

    if (cmdIndex == ISNULL) {
        Tcl_SetObjResult(ip, Tcl_NewBooleanObj(pv == NULL));
        return TCL_OK;
    } else if (cmdIndex == ADDRESS) {
        Tcl_SetObjResult(ip, Tclh_ObjFromAddress(pv));
        return TCL_OK;
    }

    if (pv == NULL && cmdIndex != ISVALID)
        return Tclh_ErrorInvalidValue(ip, objv[2], "Pointer is NULL.");

    ret = Tclh_PointerObjGetTag(ip, objv[2], &tagObj);
    if (ret != TCL_OK)
        return ret;

    switch (cmdIndex) {
    case TAG:
        if (ret == TCL_OK && tagObj)
            Tcl_SetObjResult(ip, tagObj);
        return ret;
    case CHECK:
        return Tclh_PointerVerify(ip, pv, tagObj);
    case ISVALID:
        ret = Tclh_PointerVerify(ip, pv, tagObj);
        Tcl_SetObjResult(ip, Tcl_NewBooleanObj(ret == TCL_OK));
        return TCL_OK;
    case SAFE:
        return Tclh_PointerRegister(ip, pv, tagObj, NULL);
    case COUNTED:
        return Tclh_PointerRegisterCounted(ip, pv, tagObj, NULL);
    case DISPOSE:
        return Tclh_PointerUnregister(ip, pv, tagObj);
    default: /* Just to keep compiler happy */
        Tcl_SetResult(
            ip, "Internal error: unexpected pointer subcommand", TCL_STATIC);
        return TCL_ERROR; /* Just to keep compiler happy */
    }
}


static CffiResult
CffiSandboxObjCmd(ClientData cdata,
                   Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[])
{
    unsigned long long ull;

    if (Tclh_ObjToULongLong(ip, objv[1], &ull) != TCL_OK)
        return TCL_ERROR;
    else {
        char buf[40];
#ifdef _WIN32
        _snprintf(buf, 40, "%I64u", ull);
#else
        snprintf(buf, 40, "%llu", ull);
#endif
        Tcl_SetResult(ip, buf, TCL_VOLATILE);
        return TCL_OK;
    }
}

/* Function: CffiFinit
 * Cleans up interpreter-wide resources when extension is unloaded from
 * interpreter.
 *
 * Parameters:
 * ip - interpreter
 * cdata - CallVm context for the interpreter.
 */
void CffiFinit(ClientData cdata, Tcl_Interp *ip)
{
    CffiCallVmCtx *vmCtxP = (CffiCallVmCtx *)cdata;
    if (vmCtxP->vmP)
        dcFree(vmCtxP->vmP);
    if (vmCtxP->ipCtxP) {
        CffiAliasesCleanup(&vmCtxP->ipCtxP->typeAliases);
        MemLifoClose(&vmCtxP->ipCtxP->memlifo);
        ckfree(vmCtxP->ipCtxP);
    }
    ckfree(vmCtxP);
}

DLLEXPORT int
Cffi_Init(Tcl_Interp *ip)
{
    CffiInterpCtx *ipCtxP;
    CffiCallVmCtx *vmCtxP;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(ip, "8.6", 0) == NULL) {
        return TCL_ERROR;
    }
#else
    if (Tcl_PkgRequire(ip, "Tcl", "8.6", 0) == NULL) {
        return TCL_ERROR;
    }
#endif

    if (Tclh_BaseLibInit(ip) != TCL_OK)
        return TCL_ERROR;
    if (Tclh_PointerLibInit(ip) != TCL_OK)
        return TCL_ERROR;

    ipCtxP = ckalloc(sizeof(*ipCtxP));
    ipCtxP->interp = ip;
    Tcl_InitObjHashTable(&ipCtxP->typeAliases);

    /* TBD - size 16000 too much? */
    if (MemLifoInit(
            &ipCtxP->memlifo, NULL, NULL, 16000, MEMLIFO_F_PANIC_ON_FAIL) !=
        MEMLIFO_E_SUCCESS) {
        return Tclh_ErrorAllocation(ip, "Memlifo", NULL);
    }

    vmCtxP = ckalloc(sizeof(*vmCtxP));
    vmCtxP->ipCtxP = ipCtxP;
    vmCtxP->vmP    = dcNewCallVM(4096); /* TBD - size? */

    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::dyncall::Library", CffiDyncallLibraryObjCmd, vmCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::dyncall::Symbols", CffiSymbolsObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::Struct", CffiStructObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::memory", CffiMemoryObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::type", CffiTypeObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::alias", CffiAliasObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::pointer", CffiPointerObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::sandbox", CffiSandboxObjCmd, NULL, NULL);

    Tcl_CallWhenDeleted(ip, CffiFinit, vmCtxP);

    Tcl_PkgProvide(ip, PACKAGE_NAME, PACKAGE_VERSION);
    return TCL_OK;
}

/* Function: CffiQualifyName
 * Fully qualifes a name that is relative.
 *
 * If the name is already fully qualified, it is returned as is. Otherwise
 * it is qualified with the name of the current namespace.
 *
 * Parameters:
 * ip - interpreter
 * nameObj - name
 *
 * Returns:
 * A Tcl_Obj with the qualified name. This may be *nameObj* or a new Tcl_Obj.
 * In either case, no manipulation of reference counts is done.
 */
Tcl_Obj *
CffiQualifyName(Tcl_Interp *ip, Tcl_Obj *nameObj)
{
    const char *name = Tcl_GetString(nameObj);

    if (name[0] == ':' && name[1] == ':')
        return nameObj; /* Already fully qualified */
    else {
        Tcl_Obj *fqnObj;
        Tcl_Namespace *nsP;
        nsP = Tcl_GetCurrentNamespace(ip);
        if (nsP) {
            fqnObj = Tcl_NewStringObj(nsP->fullName, -1);
            if (strcmp("::", nsP->fullName))
                Tcl_AppendToObj(fqnObj, "::", 2);
        }
        else
            fqnObj = Tcl_NewStringObj("::", 2);
        Tcl_AppendObjToObj(fqnObj, nameObj);
        return fqnObj;
    }
}
