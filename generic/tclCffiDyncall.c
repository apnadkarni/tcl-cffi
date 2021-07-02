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

/* Function: CffiFunctionLoadArg
 * Loads a value into the dyncall argument context
 *
 * Parameters:
 *
 * vmP - dyncall vm context
 * argP - argument value to load
 * typeAttrsP - argument type descriptor
 */
void CffiLoadArg(DCCallVM *vmP, CffiArgument *argP, CffiTypeAndAttrs *typeAttrsP)
{
    CFFI_ASSERT(argP->flags & CFFI_F_ARG_INITIALIZED);

#define STORESCALAR(dcfn_, fld_)                                      \
    do {                                                           \
        if (argP->actualCount == 0) {                              \
            if (typeAttrsP->flags & CFFI_F_ATTR_BYREF)             \
                dcArgPointer(vmP, (DCpointer)&argP->value.u.fld_); \
            else                                                   \
                dcfn_(vmP, argP->value.u.fld_);                    \
        }                                                          \
        else {                                                     \
            CFFI_ASSERT(typeAttrsP->flags &CFFI_F_ATTR_BYREF);     \
            dcArgPointer(vmP, (DCpointer)argP->value.u.ptr);       \
        }                                                          \
    } while (0)
    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_SCHAR: STORESCALAR(dcArgChar, schar); break;
    case CFFI_K_TYPE_UCHAR: STORESCALAR(dcArgChar, uchar); break;
    case CFFI_K_TYPE_SHORT: STORESCALAR(dcArgShort, sshort); break;
    case CFFI_K_TYPE_USHORT: STORESCALAR(dcArgShort, ushort); break;
    case CFFI_K_TYPE_INT: STORESCALAR(dcArgInt, sint); break;
    case CFFI_K_TYPE_UINT: STORESCALAR(dcArgInt, uint); break;
    case CFFI_K_TYPE_LONG: STORESCALAR(dcArgLong, slong); break;
    case CFFI_K_TYPE_ULONG: STORESCALAR(dcArgLong, ulong); break;
    case CFFI_K_TYPE_LONGLONG: STORESCALAR(dcArgLongLong, slonglong); break;
    case CFFI_K_TYPE_ULONGLONG: STORESCALAR(dcArgLongLong, ulonglong); break;
    case CFFI_K_TYPE_FLOAT: STORESCALAR(dcArgFloat, flt); break;
    case CFFI_K_TYPE_DOUBLE: STORESCALAR(dcArgDouble, dbl); break;
    case CFFI_K_TYPE_POINTER: STORESCALAR(dcArgDouble, dbl); break;
    case CFFI_K_TYPE_STRUCT: /* FALLTHRU */
    case CFFI_K_TYPE_CHAR_ARRAY: /* FALLTHRU */
    case CFFI_K_TYPE_BYTE_ARRAY: /* FALLTHRU */
    case CFFI_K_TYPE_UNICHAR_ARRAY:
        dcArgPointer(vmP, argP->value.u.ptr);
        break;
    case CFFI_K_TYPE_ASTRING:/* FALLTHRU */
    case CFFI_K_TYPE_UNISTRING: /* FALLTHRU */
    case CFFI_K_TYPE_BINARY:
        if (typeAttrsP->flags & CFFI_F_ATTR_BYREF)
            dcArgPointer(vmP, &argP->value.u.ptr);
        else
            dcArgPointer(vmP, argP->value.u.ptr);
        break;
    default:
        CFFI_PANIC("CffiLoadArg: unknown typei %d",
                   typeAttrsP->dataType.baseType);
        break;
    }
#undef STORESCALAR
}

/* Function: CffiFunctionSetupArgs
 * Prepares the call stack needed for a function call.
 *
 * Parameters:
 * callP - call context
 * nArgObjs - size of argObjs
 * argObjs - function arguments passed by the script
 *
 * As part of setting up the call stack, the function may allocate memory
 * from the context memlifo. Caller responsible for freeing.
 *
 * Returns:
 * TCL_OK on success with the call stack set up, TCL_ERROR on error with an
 * error message in the interpreter.
 */
static CffiResult
CffiFunctionSetupArgs(CffiCall *callP, int nArgObjs, Tcl_Obj *const *argObjs)
{
    int i;
    int need_pass2;
    CffiArgument *argsP;
    CffiProto *protoP;
    Tcl_Interp *ip;
    DCCallVM *vmP;

    protoP = callP->fnP->protoP;
    ip     = callP->fnP->vmCtxP->ipCtxP->interp;

    /*
     * We need temporary storage of unknown size for parameter values.
     * CffiArgPrepare will use this storage for scalar value types. For
     * aggregates and variable size, CffiArgPrepare will allocate storage
     * from the memlifo and store a pointer to it in in the argument slot.
     * After the call is made, CffiArgPostProcess processes each, stores into
     * output variables as necessary. CffiArgCleanup is responsible for
     * freeing up any internal resources for each argument. The memlifo
     * memory is freed up when the entire frame is popped at the end.
     */
    callP->nArgs = protoP->nParams;
    if (callP->nArgs == 0)
        return TCL_OK;
    argsP = (CffiArgument *)MemLifoAlloc(
        &callP->fnP->vmCtxP->ipCtxP->memlifo, callP->nArgs * sizeof(CffiArgument));
    callP->argsP = argsP;
    for (i = 0; i < callP->nArgs; ++i)
        argsP[i].flags = 0; /* Mark as uninitialized */

    /*
     * Arguments are set up in two phases - first set up those arguments that
     * are not dependent on other argument values. Then loop again to set up
     * the latter. Currently only dynamically sized arrays are dependent on
     * other arguments.
     */
    need_pass2 = 0;
    for (i = 0; i < callP->nArgs; ++i) {
        int actualCount;
        actualCount = protoP->params[i].typeAttrs.dataType.count;
        if (actualCount < 0) {
            /* Dynamic array. */
            need_pass2 = 1;
            continue;
        }
        argsP[i].actualCount = actualCount;
        if (CffiArgPrepare(callP, i, argObjs[i]) != TCL_OK)
            goto cleanup_and_error;
    }

    if (need_pass2 == 0)
        return TCL_OK;

    /*
     * Blast it. We need a second pass since some arguments were unresolved.
     * Need to reset the dyncall arg stack since some arguments may have
     * already been loaded.
     */
    vmP = callP->fnP->vmCtxP->vmP;
    dcReset(vmP);
    dcMode(vmP, protoP->callMode);

    for (i = 0; i < callP->nArgs; ++i) {
        const char *name;
        int j;
        long long actualCount;
        CffiValue *actualValueP;
        actualCount = protoP->params[i].typeAttrs.dataType.count;
        if (actualCount >= 0) {
            /* This arg already been parsed successfully. Just load it. */
            CFFI_ASSERT(argsP[i].flags & CFFI_F_ARG_INITIALIZED);
            CffiLoadArg(
                vmP, &argsP[i], &protoP->params[i].typeAttrs);
            continue;
        }
        CFFI_ASSERT((argsP[i].flags & CFFI_F_ARG_INITIALIZED) == 0);


        /* Need to find the parameter corresponding to this dynamic count */
        CFFI_ASSERT(
            protoP->params[i].typeAttrs.dataType.countHolderObj);
        name = Tcl_GetString(
            protoP->params[i].typeAttrs.dataType.countHolderObj);

        /*
         * Loop through initialized parameters looking for a match. A match
         * must have been initialized (ie. not dynamic), must be a scalar
         * and having the referenced name.
         */
        for (j = 0; j < callP->nArgs; ++j) {
            if ((argsP[j].flags & CFFI_F_ARG_INITIALIZED)
                && argsP[j].actualCount == 0 &&
                !strcmp(name, Tcl_GetString(protoP->params[j].nameObj)))
                break;
        }
        if (j == callP->nArgs) {
            (void)Tclh_ErrorNotFound(
                ip,
                "Parameter",
                protoP->params[i].typeAttrs.dataType.countHolderObj,
                "Could not find referenced count for dynamic array, possibly wrong type or not scalar.");
            goto cleanup_and_error;
        }

        /* Dynamic element count is at index j. */
        actualValueP = &argsP[j].value;
        switch (protoP->params[j].typeAttrs.dataType.baseType) {
        case CFFI_K_TYPE_SCHAR: actualCount = actualValueP->u.schar; break;
        case CFFI_K_TYPE_UCHAR: actualCount = actualValueP->u.uchar; break;
        case CFFI_K_TYPE_SHORT: actualCount = actualValueP->u.sshort; break;
        case CFFI_K_TYPE_USHORT: actualCount = actualValueP->u.ushort; break;
        case CFFI_K_TYPE_INT: actualCount = actualValueP->u.sint; break;
        case CFFI_K_TYPE_UINT: actualCount = actualValueP->u.uint; break;
        case CFFI_K_TYPE_LONG: actualCount = actualValueP->u.slong; break;
        case CFFI_K_TYPE_ULONG: actualCount = actualValueP->u.ulong; break;
        case CFFI_K_TYPE_LONGLONG: actualCount = actualValueP->u.slonglong; break;
        case CFFI_K_TYPE_ULONGLONG: actualCount = actualValueP->u.ulonglong; break;
        default:
            (void) Tclh_ErrorWrongType(ip, NULL, "Wrong type for dynamic array count value.");
            goto cleanup_and_error;
        }

        if (actualCount <= 0 || actualCount > INT_MAX) {
            (void) Tclh_ErrorRange(ip, argObjs[j], 0, INT_MAX);
            goto cleanup_and_error;
        }

        argsP[i].actualCount = (int) actualCount;
        if (CffiArgPrepare(callP, i, argObjs[i]) != TCL_OK)
            goto cleanup_and_error;
    }

    return TCL_OK;

    cleanup_and_error:
        for (i = 0; i < callP->nArgs; ++i) {
            if (callP->argsP[i].flags & CFFI_F_ARG_INITIALIZED)
                CffiArgCleanup(callP, i);
        }

        return TCL_ERROR;
}


/* Function: CffiDefaultErrorHandler
 * Stores an error message in the interpreter based on the error reporting
 * mechanism for the type
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - type descriptor
 * valueObj - the value, as an *Tcl_Obj* that failed requirements (may be NULL)
 * sysError - system error (from errno, GetLastError() etc.)
 *
 * Returns:
 * Always returns *TCL_ERROR*
 */
static CffiResult
CffiDefaultErrorHandler(Tcl_Interp *ip,
                        const CffiTypeAndAttrs *typeAttrsP,
                        Tcl_Obj *valueObj,
                        Tcl_WideInt sysError)
{
    int flags = typeAttrsP->flags;

#ifdef _WIN32
    if (flags & (CFFI_F_ATTR_LASTERROR | CFFI_F_ATTR_WINERROR)) {
        return Tclh_ErrorWindowsError(ip, (unsigned int) sysError, NULL);
    }
#endif

    if (flags & CFFI_F_ATTR_ERRNO) {
        char buf[256];
        char *bufP;
        buf[0] = 0;             /* Safety check against misconfig */
#ifdef _WIN32
        strerror_s(buf, sizeof(buf) / sizeof(buf[0]), (int) sysError);
        bufP = buf;
#else
        /*
         * This is tricky. See manpage for gcc/linux for the strerror_r
         * to be selected. BUT Apple for example does not follow the same
         * feature test macro conventions.
         */
#if _GNU_SOURCE || (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE < 200112L)
        bufP = strerror_r((int) sysError, buf, sizeof(buf) / sizeof(buf[0]));
        /* Returned bufP may or may not be buf! */
#else
        /* XSI/POSIX standard version */
        strerror_r((int) sysError, buf, sizeof(buf) / sizeof(buf[0]));
        bufP = buf;
#endif
#endif
        Tcl_SetResult(ip, bufP, TCL_VOLATILE);
        return TCL_ERROR;
    }

    /* Generic error */
    Tclh_ErrorInvalidValue(ip, valueObj, "Function returned an error value.");
    return TCL_ERROR;
}

/* Function: CffiCustomErrorHandler
 * Calls the handler specified by the `onerror` annotation.
 *
 * Parameters:
 * ipCtxP - interp context
 * protoP - declaration of function that returned error
 * argObjs - arguments for the function (protoP->nParams size)
 * argsP - array of native values corresponding to argObjs
 * valueObj - the value triggering the error. Caller should be holding a
 *     reference count if it wants to access after the function returns.
 *
 * The `onerror` handler is passed three arguments, `valueObj`, a dictionary
 * of inputs into the function and a dictionary of outputs from the function.
 *
 * Returns:
 * *TCL_OK* with result to be returned in interp or *TCL_ERROR* with error
 * message in interpreter.
 */
static CffiResult
CffiCustomErrorHandler(CffiInterpCtx *ipCtxP,
                       CffiProto *protoP,
                       Tcl_Obj **argObjs,
                       CffiArgument *argsP,
                       Tcl_Obj *valueObj)
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Obj **onErrorObjs;
    Tcl_Obj **evalObjs;
    Tcl_Obj *inputArgsObj;
    Tcl_Obj *outputArgsObj;
    CffiResult ret;
    int nOnErrorObjs;
    int nEvalObjs;
    int i;

    CFFI_ASSERT(protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_ONERROR);
    CFFI_ASSERT(protoP->returnType.typeAttrs.parseModeSpecificObj);
    CHECK(Tcl_ListObjGetElements(
        ip, protoP->returnType.typeAttrs.parseModeSpecificObj, &nOnErrorObjs, &onErrorObjs));

    nEvalObjs = nOnErrorObjs + 3; /* value, input dict, output dict */
    evalObjs =
        MemLifoAlloc(&ipCtxP->memlifo, nEvalObjs * sizeof(Tcl_Obj *));

    /*
     * Construct the dictionary of arguments that were input to the function.
     * Built as a list for efficiency as the handler may or may not access it.
     */
    inputArgsObj = Tcl_NewListObj(protoP->nParams, NULL);
    outputArgsObj = Tcl_NewListObj(protoP->nParams, NULL);
    for (i = 0; i < protoP->nParams; ++i) {
        CffiTypeAndAttrs *typeAttrsP = &protoP->params[i].typeAttrs;
        int flags                    = typeAttrsP->flags;
        if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
            Tcl_ListObjAppendElement(
                NULL, inputArgsObj, protoP->params[i].nameObj);
            Tcl_ListObjAppendElement(NULL, inputArgsObj, argObjs[i]);
        }
        /* Only append outputs if stored on error */
        if ((flags & (CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT))
            && (flags & (CFFI_F_ATTR_STOREONERROR | CFFI_F_ATTR_STOREALWAYS))
            && argsP[i].varNameObj != NULL) {
            Tcl_Obj *outValObj = Tcl_ObjGetVar2(ip, argsP[i].varNameObj, NULL, 0);
            if (outValObj) {
                Tcl_ListObjAppendElement(
                    NULL, outputArgsObj, protoP->params[i].nameObj);
                Tcl_ListObjAppendElement(NULL, outputArgsObj, outValObj);
            }
        }
    }

    /* Must protect before call as Eval may or may not release objects */
    for (i = 0; i < nOnErrorObjs; ++i) {
        evalObjs[i] = onErrorObjs[i];
        /* Increment ref count in case underlying list shimmers away */
        Tcl_IncrRefCount(evalObjs[i]);
    }
    Tcl_IncrRefCount(valueObj);
    evalObjs[nOnErrorObjs] = valueObj;
    Tcl_IncrRefCount(inputArgsObj);
    evalObjs[nOnErrorObjs + 1] = inputArgsObj;
    Tcl_IncrRefCount(outputArgsObj);
    evalObjs[nOnErrorObjs + 2] = outputArgsObj;

    ret = Tcl_EvalObjv(ip, nEvalObjs, evalObjs, 0);

    /* Undo the protection */
    for (i = 0; i < nEvalObjs; ++i) {
        Tcl_DecrRefCount(evalObjs[i]);
    }

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
    Tcl_Obj **argObjs = NULL;
    int nArgObjs;
    Tcl_Obj *resultObj = NULL;
    int i;
    void *pointer;
    CffiCall callCtx;
    MemLifoMarkHandle mark;
    CffiResult ret = TCL_OK;
    CffiResult fnCheckRet = TCL_OK; /* Whether function return check passed */
    Tcl_WideInt sysError;  /* Error retrieved from system */

    CFFI_ASSERT(ip == ipCtxP->interp);

    /* TBD - check memory executable permissions */
    if ((uintptr_t) fnP->fnAddr < 0xffff)
        return Tclh_ErrorInvalidValue(ip, NULL, "Function pointer not in executable page.");

    mark = MemLifoPushMark(&ipCtxP->memlifo);

    /* IMPORTANT - mark has to be popped even on errors before returning */

    /* nArgObjs is supplied arguments. Remaining have to come from defaults */
    CFFI_ASSERT(objc >= objArgIndex);
    nArgObjs = objc - objArgIndex;
    if (nArgObjs > protoP->nParams)
        goto numargs_error; /* More args than params */

    callCtx.fnP = fnP;
    callCtx.nArgs = 0;
    callCtx.argsP = NULL;

    /*
     * Prepare the call by resetting any previous arguments and setting the
     * call mode for this function. Do this BEFORE setting up arguments.
     */

    dcReset(vmP);
    dcMode(vmP, protoP->callMode);

    if (protoP->nParams) {
        /* Allocate space to hold argument values */
        argObjs = (Tcl_Obj **)MemLifoAlloc(
            &ipCtxP->memlifo, protoP->nParams * sizeof(Tcl_Obj *));

        /* Fill in argument value from those supplied. */
        for (i = 0; i < nArgObjs; ++i)
            argObjs[i] = objv[objArgIndex + i];

        /* Fill remaining from defaults, erroring if no default */
        while (i < protoP->nParams) {
            if (protoP->params[i].typeAttrs.parseModeSpecificObj == NULL)
                goto numargs_error;
            argObjs[i] = protoP->params[i].typeAttrs.parseModeSpecificObj;
            ++i;
        }
        /* Set up the call stack */
        if (CffiFunctionSetupArgs(&callCtx, protoP->nParams, argObjs) != TCL_OK)
            goto pop_and_error;
        /* callCtx.argsP will have been set up by above call */

        /* Only dispose of pointers AFTER all above param checks pass */
        for (i = 0; i < protoP->nParams; ++i) {
            CffiTypeAndAttrs *typeAttrsP = &protoP->params[i].typeAttrs;
            if (typeAttrsP->dataType.baseType == CFFI_K_TYPE_POINTER
                && (typeAttrsP->flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT))
                && (typeAttrsP->flags & CFFI_F_ATTR_DISPOSE)) {
                /* TBD - could actualCount be greater than number of pointers
                 * actually passed in ? */
                int nptrs = callCtx.argsP[i].actualCount;
                /* Note no error checks because the CffiFunctionSetup calls
                   above would have already done validation */
                if (nptrs <= 1) {
                    if (callCtx.argsP[i].value.u.ptr != NULL)
                        Tclh_PointerUnregister(
                            ip, callCtx.argsP[i].value.u.ptr, NULL);
                }
                else {
                    int j;
                    void **ptrArray = callCtx.argsP[i].value.u.ptr;
                    for (j = 0; j < nptrs; ++j) {
                        if (ptrArray[j] != NULL)
                            Tclh_PointerUnregister(ip, ptrArray[j], NULL);
                    }
                }
            }
        }
    }

    /* Set up the return value */
    if (CffiReturnPrepare(&callCtx) != TCL_OK) {
        int j;
        for (j = 0; j < callCtx.nArgs; ++j)
            CffiArgCleanup(&callCtx, j);
        goto pop_and_error;
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
        CffiValue retval;                                                      \
        retval.u.fld_ = dcfn_(vmP, fnP->fnAddr);                               \
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_REQUIREMENT_MASK) \
            fnCheckRet = CffiCheckNumeric(                                     \
                ip, &protoP->returnType.typeAttrs, &retval, &sysError);        \
        resultObj = objfn_(                                                    \
            retval.u.fld_); /* AFTER above Check to not lose GetLastError */   \
    } while (0);                                                               \
    break

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
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
        pointer = dcCallPointer(vmP, fnP->fnAddr);
        /* Do IMMEDIATELY so as to not lose GetLastError */
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_REQUIREMENT_MASK)
            fnCheckRet = CffiCheckPointer(
                ip, &protoP->returnType.typeAttrs, pointer, &sysError);
        switch (protoP->returnType.typeAttrs.dataType.baseType) {
            case CFFI_K_TYPE_POINTER:
                ret = CffiPointerToObj(
                    ip, &protoP->returnType.typeAttrs, pointer, &resultObj);
                break;
            case CFFI_K_TYPE_ASTRING:
                ret = CffiExternalCharsToObj(
                    ip, &protoP->returnType.typeAttrs, pointer, &resultObj);
                break;
            case CFFI_K_TYPE_UNISTRING:
                if (pointer)
                    resultObj = Tcl_NewUnicodeObj((Tcl_UniChar *)pointer, -1);
                else
                    resultObj = Tcl_NewObj();
                break;
            default:
                /* Just to keep gcc happy */
                CFFI_PANIC("UNEXPECTED BASE TYPE");
                break;
        }
        break;
    case CFFI_K_TYPE_STRUCT:
    case CFFI_K_TYPE_BINARY:
    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
#if 0
        /* Currently BYREF not allowed and DC does not support struct byval */
        CFFI_ASSERT(protoP->returnType.flags & CFFI_F_PARAM_BYREF);
        callCtx.returnValue.ptr = dcCallPointer(vmP, protoP->fnP);
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


    /*
     * At this point, the state of the call is reflected by the
     * following variables:
     * ret - TCL_OK/TCL_ERROR -> error invoking function or processing its
     *   return value, e.g. string could not be encoded into Tcl's
     *   internal form
     * fnCheckRet - TCL_OK/TCL_ERROR -> return value check annotations
     *   or failed
     * resultObj - if ret is TCL_OK, holds wrapped value irrespective of
     *   value of fnCheckRet. Reference count should be 0.
     */
    CFFI_ASSERT(resultObj != NULL || ret != TCL_OK);
    CFFI_ASSERT(resultObj == NULL || ret == TCL_OK);

    /*
     * Based on the above state, the following actions need to be taken
     * depending on the value of (ret, fnCheckRet)
     *
     * (TCL_OK, TCL_OK) - store out and inout parameters that unmarked or
     * marked as storealways and return resultObj as the command result.
     *
     * (TCL_OK, TCL_ERROR) - no errors at the Tcl level, but the function
     * return value conditions not met indicating an error. Only store output
     * parameters marked with storealways or storeonerror. If a handler is
     * defined (including predefined ones), call it and return its result.
     * Otherwise, raise an generic error with TCL_ERROR.
     *
     * (TCL_ERROR, TCL_OK) - function was called successfully, but error
     * wrapping the result (e.g. pointer was already registered).
     * Raise an error without storing any output parameters. TBD - revisit
     *
     * (TCL_ERROR, TCL_ERROR) - function return value failed conditions
     * and the result could not be wrapped. Raise an error without storing
     * any parameters. TBD - revisit
     */

    if (ret == TCL_OK) {
        /*
         * Store parameters based on function return conditions.
         * Errors storing parameters are ignored (what else to do?)
         */
        for (i = 0; i < protoP->nParams; ++i) {
            int flags = protoP->params[i].typeAttrs.flags;
            if ((flags & (CFFI_F_ATTR_INOUT|CFFI_F_ATTR_OUT))) {
                if ((fnCheckRet == TCL_OK
                     && !(flags & CFFI_F_ATTR_STOREONERROR))
                    || (fnCheckRet != TCL_OK
                        && (flags & CFFI_F_ATTR_STOREONERROR))
                    || (flags & CFFI_F_ATTR_STOREALWAYS)) {
                    /* Parameter needs to be stored */
                    if (CffiArgPostProcess(&callCtx, i) != TCL_OK)
                        ret = TCL_ERROR;
                }
            }
        }
    }
    /* Parameters stored away. Note ret might have changed to error */

    if (ret == TCL_OK) {
        CFFI_ASSERT(resultObj != NULL);
        if (fnCheckRet == TCL_OK) {
            Tcl_SetObjResult(ip, resultObj);
        }
        else {
            /* Call error handler if specified, otherwise default handler */
            if ((protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_ONERROR)
                && protoP->returnType.typeAttrs.parseModeSpecificObj) {
                Tcl_IncrRefCount(resultObj);
                ret = CffiCustomErrorHandler(
                    ipCtxP, protoP, argObjs, callCtx.argsP, resultObj);
                Tclh_ObjClearPtr(&resultObj);
            }
            else {
                ret = CffiDefaultErrorHandler(ip, &protoP->returnType.typeAttrs, resultObj, sysError);
            }
        }
    }

    CffiReturnCleanup(&callCtx);
    for (i = 0; i < protoP->nParams; ++i)
        CffiArgCleanup(&callCtx, i);

pop_and_go:
    MemLifoPopMark(mark);
    return ret;

numargs_error:
    /* Do NOT jump here if args are still to be cleaned up */
    resultObj = Tcl_NewListObj(protoP->nParams + 2, NULL);
    Tcl_ListObjAppendElement(NULL, resultObj, Tcl_NewStringObj("Syntax:", -1));
    for (i = 0; i < objArgIndex; ++i)
        Tcl_ListObjAppendElement(NULL, resultObj, objv[i]);
    for (i = 0; i < protoP->nParams; ++i)
        Tcl_ListObjAppendElement(NULL, resultObj, protoP->params[i].nameObj);
    Tclh_ErrorGeneric(ip, "NUMARGS", Tcl_GetString(resultObj));
    /* Fall thru below */
pop_and_error:
    /* Do NOT jump here if args are still to be cleaned up */
    /* Jump for error return after popping memlifo with error in interp */
    if (resultObj)
        Tcl_DecrRefCount(resultObj);
    ret = TCL_ERROR;
    goto pop_and_go;

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
    protoP->callMode = callMode;

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

