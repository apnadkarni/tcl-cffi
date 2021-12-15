/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

/* Function: CffiFunctionCleanup
 * Releases resources associated with a function definition.
 *
 * Note that *fnP* itself is not freed.
 *
 * Parameters:
 * fnP - function descriptor
 */
void CffiFunctionCleanup(CffiFunction *fnP)
{
    if (fnP->libCtxP)
        CffiLibCtxUnref(fnP->libCtxP);
    if (fnP->protoP)
        CffiProtoUnref(fnP->protoP);
    if (fnP->cmdNameObj)
        Tcl_DecrRefCount(fnP->cmdNameObj);
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
                       Tcl_Obj *cmdNameObj,
                       Tcl_Obj **argObjs,
                       CffiArgument *argsP,
                       Tcl_Obj *valueObj)
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Obj **onErrorObjs;
    Tcl_Obj **evalObjs;
    Tcl_Obj *inputArgsObj;
    Tcl_Obj *outputArgsObj;
    Tcl_Obj *callInfoObj;
    CffiResult ret;
    int nOnErrorObjs;
    int nEvalObjs;
    int i;

    CFFI_ASSERT(protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_ONERROR);
    CFFI_ASSERT(protoP->returnType.typeAttrs.parseModeSpecificObj);
    CHECK(Tcl_ListObjGetElements(
        ip, protoP->returnType.typeAttrs.parseModeSpecificObj, &nOnErrorObjs, &onErrorObjs));

    nEvalObjs = nOnErrorObjs + 1; /* We will tack on the call dictionary */
    evalObjs =
        MemLifoAlloc(&ipCtxP->memlifo, nEvalObjs * sizeof(Tcl_Obj *));

    /*
     * Construct the dictionary of arguments that were input to the function.
     * Built as a list for efficiency as the handler may or may not access it.
     */
    callInfoObj = Tcl_NewListObj(0, NULL);
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

    Tcl_ListObjAppendElement(NULL, callInfoObj, Tcl_NewStringObj("In", 2));
    Tcl_ListObjAppendElement(NULL, callInfoObj, inputArgsObj);
    Tcl_ListObjAppendElement(NULL, callInfoObj, Tcl_NewStringObj("Out", 3));
    Tcl_ListObjAppendElement(NULL, callInfoObj, outputArgsObj);
    if (valueObj) {
        Tcl_ListObjAppendElement(NULL, callInfoObj, Tcl_NewStringObj("Result", 6));
        Tcl_ListObjAppendElement(NULL, callInfoObj, valueObj);
    }
    if (cmdNameObj) {
        Tcl_ListObjAppendElement(NULL, callInfoObj, Tcl_NewStringObj("Command", 7));
        Tcl_ListObjAppendElement(NULL, callInfoObj, cmdNameObj);
    }

    /* Must protect before call as Eval may or may not release objects */
    for (i = 0; i < nOnErrorObjs; ++i) {
        evalObjs[i] = onErrorObjs[i];
        /* Increment ref count in case underlying list shimmers away */
        Tcl_IncrRefCount(evalObjs[i]);
    }
    Tcl_IncrRefCount(callInfoObj);
    evalObjs[nOnErrorObjs] = callInfoObj;

    ret = Tcl_EvalObjv(ip, nEvalObjs, evalObjs, 0);

    /* Undo the protection. */
    CFFI_ASSERT(nEvalObjs == (nOnErrorObjs + 1));
    for (i = 0; i < nEvalObjs; ++i) {
        Tcl_DecrRefCount(evalObjs[i]);
    }

    return ret;
}


/* Function: CffiFunctionSetupArgs
 * Prepares the call stack needed for a function call.
 *
 * Parameters:
 * callP - call context
 * nArgObjs - size of argObjs
 * argObjs - function arguments passed by the script
 *
 * The function resets the call context and sets up the arguments.
 *
 * As part of setting up the call stack, the function may allocate memory
 * from the context memlifo. Caller responsible for freeing.
 *
 * Returns:
 * TCL_OK on success with the call stack set up, TCL_ERROR on error with an
 * error message in the interpreter.
 */
CffiResult
CffiFunctionSetupArgs(CffiCall *callP, int nArgObjs, Tcl_Obj *const *argObjs)
{
    int i;
    int need_pass2;
    CffiArgument *argsP;
    CffiProto *protoP;
    Tcl_Interp *ip;

    protoP = callP->fnP->protoP;
    ip     = callP->fnP->vmCtxP->ipCtxP->interp;

    /* Resets the context for the call */
    if (CffiResetCall(ip, callP) != TCL_OK)
        goto cleanup_and_error;

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
    if (CffiResetCall(ip, callP) != TCL_OK)
        goto cleanup_and_error;

    for (i = 0; i < callP->nArgs; ++i) {
        const char *name;
        int j;
        long long actualCount;
        CffiValue *actualValueP;
        actualCount = protoP->params[i].typeAttrs.dataType.count;
        if (actualCount >= 0) {
            /* This arg already been parsed successfully. Just load it. */
            CFFI_ASSERT(argsP[i].flags & CFFI_F_ARG_INITIALIZED);
            CffiLoadArg(callP, &argsP[i], &protoP->params[i].typeAttrs);
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
#ifdef CFFI_USE_DYNCALL
    DCCallVM *vmP         = fnP->vmCtxP->vmP;
#endif
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

#ifndef CFFI_USE_DYNCALL
    /* protoP->cifP is lazy-initialized */
    ret = CffiLibffiInitProtoCif(ip, protoP);
    if (ret != TCL_OK)
        return ret;
#endif

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

    /* Set up arguments if any */
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
        /* Set up stack. This also does CffiResetCall so we don't need to */
        if (CffiFunctionSetupArgs(&callCtx, protoP->nParams, argObjs) != TCL_OK)
            goto pop_and_error;
        /* callCtx.argsP will have been set up by above call */
    }
    else {
        /* Prepare for the call. */
        if (CffiResetCall(ip, &callCtx) != TCL_OK)
            goto pop_and_error;
    }

    /* Set up the return value */
    if (CffiReturnPrepare(&callCtx) != TCL_OK)
        goto pop_and_error;

    /* Currently return values are always by value - enforced in prototype */
    CFFI_ASSERT((protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_BYREF) == 0);


    /*
     * A note on pointer disposal - pointers must be disposed of AFTER the
     * function is invoked (since success/fail control disposal) but BEFORE
     * wrapping of return values and output arguments that are pointers
     * since a returned pointer may be the same as on just disposed. Not
     * disposing first would cause pointer registration to fail. Hence
     * the repeated CffiPointerArgsDispose calls below instead of a single
     * one at the end.
     */

    /*
     * CALLFN should only be used for numerics.
     * IMPORTANT: do not call any system or C functions until check done
     * to prevent GetLastError/errno etc. being overwritten
     */
#define CALLFN(objfn_, dcfn_, fld_)                                            \
    do {                                                                       \
        CffiValue retval;                                                      \
        retval.u.fld_ = dcfn_(&callCtx);                                       \
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_REQUIREMENT_MASK) \
            fnCheckRet = CffiCheckNumeric(                                     \
                ip, &protoP->returnType.typeAttrs, &retval, &sysError);        \
        CffiPointerArgsDispose(ip, protoP, callCtx.argsP, fnCheckRet);         \
        if (fnCheckRet == TCL_OK                                               \
            && (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_ENUM)         \
            && protoP->returnType.typeAttrs.dataType.u.tagObj) {               \
            CffiEnumFindReverse(                                               \
                ipCtxP,                                                        \
                protoP->returnType.typeAttrs.dataType.u.tagObj,                \
                (Tcl_WideInt)retval.u.fld_,                                    \
                0,                                                             \
                &resultObj);                                                   \
        }                                                                      \
        else {                                                                 \
            /* AFTER above Check to not lose GetLastError */                   \
            resultObj = objfn_(retval.u.fld_);                                 \
        }                                                                      \
    } while (0);                                                               \
    break

    switch (protoP->returnType.typeAttrs.dataType.baseType) {
    case CFFI_K_TYPE_VOID:
        CffiCallVoidFunc(&callCtx);
        CffiPointerArgsDispose(ip, protoP, callCtx.argsP, fnCheckRet);
        resultObj = Tcl_NewObj();
        break;
    case CFFI_K_TYPE_SCHAR: CALLFN(Tcl_NewIntObj, CffiCallSCharFunc, schar);
    case CFFI_K_TYPE_UCHAR: CALLFN(Tcl_NewIntObj, CffiCallUCharFunc, uchar);
    case CFFI_K_TYPE_SHORT: CALLFN(Tcl_NewIntObj, CffiCallShortFunc, sshort);
    case CFFI_K_TYPE_USHORT: CALLFN(Tcl_NewIntObj, CffiCallUShortFunc, ushort);
    case CFFI_K_TYPE_INT: CALLFN(Tcl_NewIntObj, CffiCallIntFunc, sint);
    case CFFI_K_TYPE_UINT: CALLFN(Tcl_NewWideIntObj, CffiCallUIntFunc, uint);
    case CFFI_K_TYPE_LONG: CALLFN(Tcl_NewLongObj, CffiCallLongFunc, slong);
    case CFFI_K_TYPE_ULONG: CALLFN(Tcl_NewWideIntObj, CffiCallULongFunc, ulong);
    case CFFI_K_TYPE_LONGLONG: CALLFN(Tcl_NewWideIntObj, CffiCallLongLongFunc, slonglong);
    case CFFI_K_TYPE_ULONGLONG: CALLFN(Tcl_NewWideIntObj, CffiCallULongLongFunc, ulonglong);
    case CFFI_K_TYPE_FLOAT: CALLFN(Tcl_NewDoubleObj, CffiCallFloatFunc, flt);
    case CFFI_K_TYPE_DOUBLE: CALLFN(Tcl_NewDoubleObj, CffiCallDoubleFunc, dbl);
    case CFFI_K_TYPE_POINTER:
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
        pointer = CffiCallPointerFunc(&callCtx);
        /* Do IMMEDIATELY so as to not lose GetLastError */
        fnCheckRet = CffiCheckPointer(
            ip, &protoP->returnType.typeAttrs, pointer, &sysError);
        CffiPointerArgsDispose(ip, protoP, callCtx.argsP, fnCheckRet);         \
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
                    ipCtxP, protoP, fnP->cmdNameObj, argObjs, callCtx.argsP, resultObj);
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
void
CffiFunctionInstanceDeleter(ClientData cdata)
{
    CffiFunction *fnP = (CffiFunction *)cdata;
    CffiFunctionCleanup(fnP);
    ckfree(fnP);
}

CffiResult
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
 *    vmCtxP - pointer to the context in which function is to be called
 *    libCtxP - containing library, NULL for free standing function
 *    fnAddr - address of function
 *    cmdNameObj - name to give to command
 *    returnTypeObj - function return type definition
 *    paramsObj - list of parameter type definitions
 *    callMode - a calling convention that overrides one specified
 *               in the return type definition if anything other
 *               than the default abi
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
                      CffiLibCtx *libCtxP,
                      void *fnAddr,
                      Tcl_Obj *cmdNameObj,
                      Tcl_Obj *returnTypeObj,
                      Tcl_Obj *paramsObj,
                      CffiABIProtocol callMode)
{
    Tcl_Obj *fqnObj;
    CffiProto *protoP = NULL;
    CffiFunction *fnP = NULL;

    CHECK(CffiPrototypeParse(
        vmCtxP->ipCtxP, cmdNameObj, returnTypeObj, paramsObj, &protoP));
    /* TBD - comment in func header says only override if default */
    protoP->abi = callMode;
#ifndef CFFI_USE_DYNCALL
    protoP->cifP = NULL;
#endif

    fnP = ckalloc(sizeof(*fnP));
    fnP->fnAddr = fnAddr;
    fnP->vmCtxP = vmCtxP;
    fnP->libCtxP = libCtxP;
    if (libCtxP)
        CffiLibCtxRef(libCtxP);
    CffiProtoRef(protoP);
    fnP->protoP = protoP;
    fqnObj = CffiQualifyName(ip, cmdNameObj);
    Tcl_IncrRefCount(fqnObj);
    fnP->cmdNameObj = fqnObj;

    Tcl_CreateObjCommand(ip,
                         Tcl_GetString(fqnObj),
                         CffiFunctionInstanceCmd,
                         fnP,
                         CffiFunctionInstanceDeleter);

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
 *               than default
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
                             CffiABIProtocol callMode)
{
    void *fn;
    Tcl_Obj *cmdNameObj;
    Tcl_Obj **nameObjs;    /* C name and optional Tcl name */
    int nNames;         /* # elements in nameObjs */

    CHECK(Tcl_ListObjGetElements(ip, nameObj, &nNames, &nameObjs));
    if (nNames == 0 || nNames > 2)
        return Tclh_ErrorInvalidValue(ip, nameObj, "Empty or invalid function name specification.");

    fn = CffiLibFindSymbol(ip, libCtxP->libH, nameObjs[0]);
    if (fn == NULL)
        return Tclh_ErrorNotFound(ip, "Symbol", nameObjs[0], NULL);

    if (nNames < 2 || ! strcmp("", Tcl_GetString(nameObjs[1])))
        cmdNameObj = nameObjs[0];
    else
        cmdNameObj = nameObjs[1];

    return CffiDefineOneFunction(ip,
                                 libCtxP->vmCtxP,
                                 libCtxP,
                                 fn,
                                 cmdNameObj,
                                 returnTypeObj,
                                 paramsObj,
                                 callMode);
}


/* Function: CffiLibraryFunctionCmd
 * Creates a command mapped to a function in a DLL.
 *
 * Parameters:
 * ip   - interpreter
 * ctxP - library context in which functions are being defined
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
CffiLibraryFunctionCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP)
{
    CFFI_ASSERT(objc == 5);

    return CffiDefineOneFunctionFromLib(
        ip, ctxP, objv[2], objv[3], objv[4], CffiDefaultABI());
}

/* Function: CffiLibraryStdcallCmd
 * Creates a command mapped to a stdcall function in a DLL.
 *
 * Parameters:
 * ip   - interpreter
 * ctxP - library context in which functions are being defined
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
CffiLibraryStdcallCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP)
{
    CFFI_ASSERT(objc == 5);

    return CffiDefineOneFunctionFromLib(
        ip, ctxP, objv[2], objv[3], objv[4], CffiStdcallABI());
}


/* Function: CffiLibraryManyFunctionsCmd
 * Creates commands mapped to functions in a DLL.
 *
 * Parameters:
 * ip   - interpreter
 * ctxP - library context in which functions are being defined
 * objc - number of elements in *objv*
 * objv - array containing the function definition list
 * callMode - the calling convention
 *
 * The *objv[2]* element contains the function definition list.
 * This is a flat list of function name, type, parameter definitions.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
static CffiResult
CffiLibraryManyFunctionsCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP,
                    CffiABIProtocol callMode)
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
        ret = CffiDefineOneFunctionFromLib(
            ip, ctxP, objs[i], objs[i + 1], objs[i + 2], callMode);
        /* TBD - if one fails, rest are not defined but prior ones are */
        if (ret != TCL_OK)
            return ret;
    }
    return TCL_OK;
}

/* Function: CffiLibraryFunctionsCmd
 * Creates commands mapped to functions in a DLL.
 *
 * Parameters:
 * ip   - interpreter
 * objc - number of elements in *objv*
 * objv - array containing the function definition list
 * ctxP - library context in which functions are defined
 *
 * The *objv[2]* element contains the function definition list.
 * This is a flat list of function name, type, parameter definitions.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
static CffiResult
CffiLibraryFunctionsCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP)
{
    return CffiLibraryManyFunctionsCmd(ip, objc, objv, ctxP, CffiDefaultABI());
}


/* Function: CffiLibraryStdcallsCmd
 * Creates commands mapped to stdcall functions in a DLL.
 *
 * Parameters:
 * ip   - interpreter
 * objc - number of elements in *objv*
 * objv - array containing the function definition list
 * ctxP - library context in which function is defined
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
CffiLibraryStdcallsCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP)
{
    return CffiLibraryManyFunctionsCmd(
        ip, objc, objv, ctxP, CffiStdcallABI());
}

static CffiResult
CffiLibraryDestroyCmd(Tcl_Interp *ip,
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
CffiLibraryPathCmd(Tcl_Interp *ip,
                int objc,
                Tcl_Obj *const objv[],
                CffiLibCtx *ctxP)
{
    Tcl_SetObjResult(ip, CffiLibPath(ip, ctxP));
    return TCL_OK;
}

static CffiResult
CffiLibraryAddressOfCmd(Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[],
                  CffiLibCtx *ctxP)
{
    void *addr;

    CFFI_ASSERT(objc == 3);
    addr = CffiLibFindSymbol(ip, ctxP->libH, objv[2]);
    if (addr) {
        Tcl_SetObjResult(ip, Tclh_ObjFromAddress(addr));
        return TCL_OK;
    }
    else
        return TCL_ERROR; /* ip already contains error message */
}


static CffiResult
CffiLibraryInstanceCmd(ClientData cdata,
                   Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[])
{
    CffiLibCtx *ctxP = (CffiLibCtx *)cdata;
    static const Tclh_SubCommand subCommands[] = {
        {"addressof", 1, 1, "SYMBOL", CffiLibraryAddressOfCmd},
        {"destroy", 0, 0, "", CffiLibraryDestroyCmd},
        {"function", 3, 3, "NAME RETURNTYPE PARAMDEFS", CffiLibraryFunctionCmd},
        {"functions", 1, 1, "FUNCTIONLIST", CffiLibraryFunctionsCmd},
        {"path", 0, 0, "", CffiLibraryPathCmd},
        {"stdcall", 3, 3, "NAME RETURNTYPE PARAMDEFS", CffiLibraryStdcallCmd},
        {"stdcalls", 1, 1, "FUNCTIONLIST", CffiLibraryStdcallsCmd},
        {NULL}};
    int cmdIndex;

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ip, objc, objv, ctxP);
}

static void CffiLibraryInstanceDeleter(ClientData cdata)
{
    CffiLibCtxUnref((CffiLibCtx *)cdata);
}


/* Function: CffiLibraryObjCmd
 * Implements the script level *Library* command.
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
CffiLibraryObjCmd(ClientData cdata,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[])
{
    CffiLibCtx *ctxP;
    Tcl_Obj *nameObj;
    Tcl_Obj *pathObj;
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

    ret = CffiLibLoad(ip, pathObj, &ctxP);
    if (ret == TCL_OK) {
        ctxP->vmCtxP = (CffiCallVmCtx *)cdata;
        Tcl_CreateObjCommand(ip,
                             Tcl_GetString(nameObj),
                             CffiLibraryInstanceCmd,
                             ctxP,
                             CffiLibraryInstanceDeleter);
        Tcl_SetObjResult(ip, nameObj);
    }

    Tcl_DecrRefCount(nameObj);
    return ret;
}
