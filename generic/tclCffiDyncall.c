/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

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
CffiResult
CffiDyncallSymbolsObjCmd(ClientData cdata,
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

/*
 * Implements the call to a function. The cdata parameter contains the
 * prototype information about the function to call. The objv[] parameter
 * contains the arguments to pass to the function.
 */
CffiResult
CffiFunctionCall(ClientData cdata,
                 Tcl_Interp *ip,
                 int objArgIndex, /* Where in objv[] args start */
                 int objc,
                 Tcl_Obj *const objv[])
{
    CffiFunction *fnP     = (CffiFunction *)cdata;
    CffiProto *protoP     = fnP->protoP;
    CffiInterpCtx *ipCtxP = fnP->vmCtxP->ipCtxP;
    DCCallVM *vmP         = fnP->vmCtxP->vmP;
    Tcl_Obj *const *argObjs;
    Tcl_Obj *resultObj;
    int i;
    CffiValue *valuesP;
    void *pointer;
    CffiResult ret = TCL_OK;
    Tcl_Obj **varNameObjs;

    CFFI_ASSERT(ip == ipCtxP->interp);
    CFFI_ASSERT(objc >= objArgIndex);

    /* TBD - check memory executable permissions */
    if ((uintptr_t) fnP->fnAddr < 0xffff)
        return Tclh_ErrorInvalidValue(ip, NULL, "Function pointer not in executable page.");

    if ((objc - objArgIndex) != protoP->nParams) {
        Tcl_Obj *syntaxObj = Tcl_NewListObj(protoP->nParams+2, NULL);
        Tcl_ListObjAppendElement(
            NULL, syntaxObj, Tcl_NewStringObj("Syntax:", -1));
        for (i = 0; i < objArgIndex; ++i)
            Tcl_ListObjAppendElement(NULL, syntaxObj, objv[i]);
        for (i = 0; i < protoP->nParams; ++i)
            Tcl_ListObjAppendElement(
                NULL, syntaxObj, protoP->params[i].nameObj);
        (void) Tclh_ErrorGeneric(ip, "NUMARGS", Tcl_GetString(syntaxObj));
        Tcl_DecrRefCount(syntaxObj);
        return TCL_ERROR;
    }

    argObjs = objv + objArgIndex;

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
        &ipCtxP->memlifo, (protoP->nParams + 1) * sizeof(*valuesP));
    /* Also need storage for variable names for output parameters */
    varNameObjs = (Tcl_Obj **)MemLifoAlloc(
        &ipCtxP->memlifo, protoP->nParams * sizeof(*valuesP));

    /* Set up parameters. */
    for (i = 0; i < protoP->nParams; ++i) {
        if (CffiArgPrepare(fnP->vmCtxP,
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
    if (CffiReturnPrepare(fnP->vmCtxP,
                          &protoP->returnType.typeAttrs,
                          &valuesP[protoP->nParams])
        != TCL_OK) {
        int j;
        for (j = 0; j < protoP->nParams; ++j)
            CffiArgCleanup(&protoP->params[j].typeAttrs, &valuesP[j]);
        goto pop_and_error;
    }

    /* Only dispose of pointers AFTER all above param checks pass */
    for (i = 0; i < protoP->nParams; ++i) {
        CffiTypeAndAttrs *typeAttrsP = &protoP->params[i].typeAttrs;
        if (typeAttrsP->dataType.baseType == CFFI_K_TYPE_POINTER
            && (typeAttrsP->flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT))
            && (typeAttrsP->flags & CFFI_F_ATTR_DISPOSE)) {
            int nptrs = typeAttrsP->dataType.count;
            /* Note no error checks because the CffiArgPrepare calls
               above would have already done validation */
            if (nptrs <= 1) {
                if (valuesP[i].ptr != NULL)
                    Tclh_PointerUnregister(ip, valuesP[i].ptr, NULL);
            }
            else {
                int j;
                void **ptrArray = valuesP[i].ptr;
                for (j = 0; j < nptrs; ++j) {
                    if (ptrArray[j] != NULL)
                        Tclh_PointerUnregister(ip, ptrArray[j], NULL);
                }
            }
        }
    }

    /* Currently return values are always by value - enforced in prototype */
    CFFI_ASSERT((protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_BYREF) == 0);

    /*
     * CALLFN should only be used for numerics.
     * IMPORTANT: do not call any system or C functions until check done
     * to prevent GetLastError/errno etc. being overwritten
     */
#define CALLFN(objfn_, dcfn_, fld_)                                            \
    do {                                                                       \
        CffiValue *valP = &valuesP[protoP->nParams];                           \
        valP->fld_      = dcfn_(vmP, fnP->fnAddr);                             \
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_REQUIREMENT_MASK) \
            ret = CffiCheckNumeric(ip, valP, &protoP->returnType.typeAttrs);   \
        if (ret == TCL_OK)                                                     \
            resultObj = objfn_(valP->fld_);                                    \
    } while (0);                                                               \
    break

    /*
     * We are duplicating code here because we could have called
     * CffiBinOneValueToObj. But that would mean again going through a
     * switch that we already switched on. Is performance diff worth
     * duplicating code? TBD
     */
    switch (protoP->returnType.typeAttrs.dataType.baseType) {
    case CFFI_K_TYPE_VOID:
        dcCallVoid(vmP, fnP->fnAddr);
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
        pointer = dcCallPointer(vmP, fnP->fnAddr);
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
        if (CffiBinValueToObj(ipCtxP->interp,
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
            ipCtxP->interp, NULL, "Unsupported type for return.");
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
            ret = CffiArgPostProcess(ipCtxP->interp,
                                     &protoP->params[i].typeAttrs,
                                     &valuesP[i],
                                     varNameObjs[i]);
        }
        CffiArgCleanup(&protoP->params[i].typeAttrs, &valuesP[i]);
    }

    if (ret == TCL_OK) {
        Tcl_SetObjResult(ip, resultObj);
        MemLifoPopFrame(&ipCtxP->memlifo);
        return TCL_OK;
    }

pop_and_error:
    /* Jump for error return after popping memlifo with error in interp */
    MemLifoPopFrame(&ipCtxP->memlifo);
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
    CffiFunction *fnP = (CffiFunction *)cdata;
    CffiProtoUnref(fnP->protoP);
    ckfree(fnP);
}

static CffiResult
CffiFunctionInstanceCmd(ClientData cdata,
                        Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[])
{
    return CffiFunctionCall(cdata, ip, 1, objc, objv);
}

/* Function: CffiDefineOneFunction
 * Creates a single command mapped to a function.
 *
 * Parameters:
 *    ctxP - pointer to the context in which function is to be called
 *    fnAddr - address of function
 *    cmdNameObj - name to give to command
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
CffiDefineOneFunction(Tcl_Interp *ip,
                      CffiCallVmCtx *vmCtxP,
                      void *fnAddr,
                      Tcl_Obj *cmdNameObj,
                      Tcl_Obj *returnTypeObj,
                      Tcl_Obj *paramsObj,
                      int callMode)
{
    Tcl_Obj *fqnObj;
    CffiProto *protoP = NULL;
    CffiFunction *fnP = NULL;

    CHECK(CffiPrototypeParse(
        vmCtxP->ipCtxP, cmdNameObj, returnTypeObj, paramsObj, &protoP));
    if (callMode != DC_CALL_C_DEFAULT)
        protoP->returnType.typeAttrs.callMode = callMode;

    fnP = ckalloc(sizeof(*fnP));
    fnP->fnAddr = fnAddr;
    fnP->vmCtxP = vmCtxP;
    CffiProtoRef(protoP);
    fnP->protoP = protoP;
    fqnObj = CffiQualifyName(ip, cmdNameObj);
    Tcl_IncrRefCount(fqnObj);

    Tcl_CreateObjCommand(ip,
                         Tcl_GetString(fqnObj),
                         CffiFunctionInstanceCmd,
                         fnP,
                         CffiFunctionInstanceDeleter);

    Tcl_DecrRefCount(fqnObj);
    return TCL_OK;
}

/* Function: CffiDefineOneFunctionFromLib
 * Creates a single command mapped to a function in a DLL.
 *
 * Parameters:
 *    ctxP - pointer to the library context
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
CffiDefineOneFunctionFromLib(Tcl_Interp *ip,
                             CffiLibCtx *libCtxP,
                             Tcl_Obj *nameObj,
                             Tcl_Obj *returnTypeObj,
                             Tcl_Obj *paramsObj,
                             int callMode)
{
    void *fn;
    Tcl_Obj *cmdNameObj;
    Tcl_Obj **nameObjs;    /* C name and optional Tcl name */
    int nNames;         /* # elements in nameObjs */

    CHECK(Tcl_ListObjGetElements(ip, nameObj, &nNames, &nameObjs));
    if (nNames == 0 || nNames > 2)
        return Tclh_ErrorInvalidValue(ip, nameObj, "Empty or invalid function name specification.");

    fn = dlFindSymbol(libCtxP->dlP, Tcl_GetString(nameObjs[0]));
    if (fn == NULL)
        return Tclh_ErrorNotFound(ip, "Symbol", nameObjs[0], NULL);

    if (nNames < 2 || ! strcmp("", Tcl_GetString(nameObjs[1])))
        cmdNameObj = nameObjs[0];
    else
        cmdNameObj = nameObjs[1];

    return CffiDefineOneFunction(ip,
                                 libCtxP->vmCtxP,
                                 fn,
                                 cmdNameObj,
                                 returnTypeObj,
                                 paramsObj,
                                 callMode);
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

    return CffiDefineOneFunctionFromLib(
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

    return CffiDefineOneFunctionFromLib(
        ip, ctxP, objv[2], objv[3], objv[4],
#if defined(_WIN32) && !defined(_WIN64)
        DC_CALL_C_X86_WIN32_STD
#else
        DC_CALL_C_DEFAULT
#endif
        );

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
        ret = CffiDefineOneFunctionFromLib(ip, ctxP, objs[i], objs[i + 1], objs[i + 2], callMode);
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
CffiResult CffiDyncallLibraryObjCmd(ClientData  cdata,
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

