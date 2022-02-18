/*
 * Copyright (c) 2021-2022 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#define TCLH_SHORTNAMES
#include "tclCffiInt.h"

static CffiResult CffiArgPrepare(CffiCall *callP, int arg_index, Tcl_Obj *valueObj);
static CffiResult CffiArgPostProcess(CffiCall *callP, int arg_index);
static void CffiArgCleanup(CffiCall *callP, int arg_index);
static CffiResult CffiReturnPrepare(CffiCall *callP);
static CffiResult CffiReturnCleanup(CffiCall *callP);
static void CffiPointerArgsDispose(Tcl_Interp *ip,
                            CffiProto *protoP,
                            CffiArgument *argsP,
                            int callSucceeded);

Tcl_WideInt
CffiGrabSystemError(const CffiTypeAndAttrs *typeAttrsP,
                    Tcl_WideInt winError)
{
    Tcl_WideInt sysError = 0;
    if (typeAttrsP->flags & CFFI_F_ATTR_ERRNO)
        sysError = errno;
#ifdef _WIN32
    else if (typeAttrsP->flags & CFFI_F_ATTR_LASTERROR)
        sysError = GetLastError();
    else if (typeAttrsP->flags & CFFI_F_ATTR_WINERROR)
        sysError = winError;
#endif
    return sysError;
}


/* Function: CffiPointerArgsDispose
 * Disposes pointer arguments
 *
 * Parameters:
 * ip - interpreter
 * protoP - prototype structure with argument descriptors
 * argsP - array of argument values
 * callFailed - 0 if the function invocation had succeeded, else non-0
 *
 * The function loops through all argument values that are pointers
 * and annotated as *dispose* or *disposeonsuccess*. Any such pointers
 * are unregistered.
 *
 * Returns:
 * Nothing.
 */
void
CffiPointerArgsDispose(Tcl_Interp *ip,
                       CffiProto *protoP,
                       CffiArgument *argsP,
                       int callFailed)
{
    int i;
    for (i = 0; i < protoP->nParams; ++i) {
        CffiTypeAndAttrs *typeAttrsP = &protoP->params[i].typeAttrs;
        if (typeAttrsP->dataType.baseType == CFFI_K_TYPE_POINTER
            && (typeAttrsP->flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT))) {
            /*
             * DISPOSE - always dispose of pointer
             * DISPOSEONSUCCESS - dispose if the call had returned successfully
             */
            if ((typeAttrsP->flags & CFFI_F_ATTR_DISPOSE)
                || ((typeAttrsP->flags & CFFI_F_ATTR_DISPOSEONSUCCESS)
                    && !callFailed)) {
                int nptrs = argsP[i].arraySize;
                /* Note no error checks because the CffiFunctionSetup calls
                   above would have already done validation */
                if (nptrs < 0) {
                    /* Scalar */
                    if (argsP[i].savedValue.u.ptr != NULL)
                        Tclh_PointerUnregister(
                            ip, argsP[i].savedValue.u.ptr, NULL);
                }
                else {
                    /* Array */
                    int j;
                    void **ptrArray = argsP[i].savedValue.u.ptr;
                    CFFI_ASSERT(ptrArray);
                    for (j = 0; j < nptrs; ++j) {
                        if (ptrArray[j] != NULL)
                            Tclh_PointerUnregister(ip, ptrArray[j], NULL);
                    }
                }
            }
        }
    }
}

/* Function: CffiArgPrepareChars
 * Initializes a CffiValue to pass a chars argument.
 *
 * Parameters:
 * callP - the call context
 * arg_index - index into argument array of call context. Caller must
 * ensure this is an array of size > 0.
 * valueObj - the value passed in from script. May be NULL for pure output.
 * valueP - location of native value to store
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with pointer to chars in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
static CffiResult
CffiArgPrepareChars(CffiCall *callP,
                    int arg_index,
                    Tcl_Obj *valueObj,
                    CffiValue *valueP)
{
    CffiInterpCtx *ipCtxP = callP->fnP->ipCtxP;
    CffiArgument *argP    = &callP->argsP[arg_index];
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_CHAR_ARRAY);
    CFFI_ASSERT(argP->arraySize > 0);

    valueP->u.ptr =
        MemLifoAlloc(&ipCtxP->memlifo, argP->arraySize);

    /* If input, we need to encode appropriately */
    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT))
        return CffiCharsFromObj(ipCtxP->interp,
                                typeAttrsP->dataType.u.tagObj,
                                valueObj,
                                valueP->u.ptr,
                                argP->arraySize);
    else {
        /*
         * To protect against the called C function leaving the output
         * argument unmodified on error which would result in our
         * processing garbage in CffiArgPostProcess, set null terminator.
         */
        *(char *)valueP->u.ptr = '\0';
        /* In case encoding employs double nulls */
        if (argP->arraySize > 1)
            *(1 + (char *)valueP->u.ptr) = '\0';
        return TCL_OK;
    }
}

/* Function: CffiArgPrepareUniChars
 * Initializes a CffiValue to pass a unichars argument.
 *
 * Parameters:
 * callP - the call context
 * arg_index - index into argument array of call context. Caller must
 * ensure this is an array of size > 0.
 * valueObj - the value passed in from script. May be NULL for pure output.
 * valueP - location of native value to store
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with pointer to chars in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
static CffiResult
CffiArgPrepareUniChars(CffiCall *callP,
                       int arg_index,
                       Tcl_Obj *valueObj,
                       CffiValue *valueP)
{
    CffiInterpCtx *ipCtxP = callP->fnP->ipCtxP;
    CffiArgument *argP    = &callP->argsP[arg_index];
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;

    CFFI_ASSERT(argP->arraySize > 0);

    valueP->u.ptr =
        MemLifoAlloc(&ipCtxP->memlifo, argP->arraySize * sizeof(Tcl_UniChar));
    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_UNICHAR_ARRAY);

    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
        return CffiUniCharsFromObjSafe(
            ipCtxP->interp, valueObj, valueP->u.ptr, argP->arraySize);
    }
    else {
        /*
         * To protect against the called C function leaving the output
         * argument unmodified on error which would result in our
         * processing garbage in CffiArgPostProcess, set null terminator.
         */
        *(Tcl_UniChar *)valueP->u.ptr = 0;
        return TCL_OK;
    }
}

/* Function: CffiArgCleanup
 * Releases any resources stored within a CffiValue
 *
 * Parameters:
 * valueP - pointer to a value
 */
void CffiArgCleanup(CffiCall *callP, int arg_index)
{
    /*
     * No argument types need to be any resource clean up.
     * In the future, if any clean up is needed, remember to check
     * the INITIALIZE flag first as follows.
     * if ((callP->argsP[arg_index].flags & CFFI_F_ARG_INITIALIZED) == 0)
     *     return;
     */

}

/* Function: CffiArgPrepare
 * Prepares a argument for a DCCall
 *
 * Parameters:
 * callP - function call context
 * arg_index - the index of the argument. The corresponding slot should
 *            have been initialized so the its flags field is 0.
 * valueObj - the Tcl_Obj containing the value or variable name for out
 *             parameters
 *
 * The function modifies the following:
 *
 * callP->argsP[arg_index].flags - sets CFFI_F_ARG_INITIALIZED
 *
 * callP->argsP[arg_index].value - the native value.
 *
 * callP->argsP[arg_index].savedValue - copy of above for some types only
 *
 * callP->argsP[arg_index].varNameObj - will store the variable name
 *            for byref parameters. This will be valueObj[0] for byref
 *            parameters and NULL for byval parameters. The reference
 *            count is unchanged from what is passed in so do NOT call
 *            an *additional* Tcl_DecrRefCount on it.
 *
 * In addition to storing the native value in the *value* field, the
 * function also stores it as a function argument. The function may
 * allocate additional dynamic storage from the call context that is the
 * caller's responsibility to free.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
CffiResult
CffiArgPrepare(CffiCall *callP, int arg_index, Tcl_Obj *valueObj)
{
    CffiInterpCtx *ipCtxP = callP->fnP->ipCtxP;
    Tcl_Interp *ip        = ipCtxP->interp;
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;
    CffiArgument *argP    = &callP->argsP[arg_index];
    Tcl_Obj **varNameObjP = &argP->varNameObj;
    enum CffiBaseType baseType;
    CffiAttrFlags flags;
    int len;
    char *p;

    /* Expected initialization to virgin state */
    CFFI_ASSERT(callP->argsP[arg_index].flags == 0);

    /*
     * IMPORTANT: the logic here must be consistent with CffiArgPostProcess
     * and CffiArgCleanup. Any changes here should be reflected there too.
     */

    flags = typeAttrsP->flags;
    baseType = typeAttrsP->dataType.baseType;
    if (CffiTypeIsArray(&typeAttrsP->dataType)) {
        switch (baseType) {
        case CFFI_K_TYPE_BINARY:
            return ErrorInvalidValue(ip,
                                     NULL,
                                     "Arrays not supported for "
                                     "binary types.");
        default:
            break;
        }
    }

    /*
     * out/inout parameters are always expected to be byref. Prototype
     * parser should have ensured that.
     */
    CFFI_ASSERT((typeAttrsP->flags & (CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT)) == 0
                || (typeAttrsP->flags & CFFI_F_ATTR_BYREF) != 0);

    /*
     * For pure in parameters, valueObj provides the value itself. For out
     * and inout parameters, valueObj is the variable name. If the parameter
     * is an inout parameter, the variable must exist since the value passed
     * to the called function is taken from there. For pure out parameters,
     * the variable need not exist and will be created if necessary. For
     * both in and inout, on return from the function the corresponding
     * content is stored in that variable.
     */
    *varNameObjP = NULL;
    if (flags & (CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT)) {
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        *varNameObjP = valueObj;
        valueObj = Tcl_ObjGetVar2(ip, valueObj, NULL, TCL_LEAVE_ERR_MSG);
        if (valueObj == NULL && (flags & CFFI_F_ATTR_INOUT)) {
            return ErrorInvalidValue(
                ip,
                *varNameObjP,
                "Variable specified as inout argument does not exist.");
        }
        /* TBD - check if existing variable is an array and error out? */
    }

    /*
     * Type parsing should have validated out/inout params specify size if
     * not a fixed size type. Note chars/unichars/bytes are fixed size since
     * their array size is required to be specified.
     */

    /* Non-scalars need to be passed byref. Parsing should have checked */
    CFFI_ASSERT((flags & CFFI_F_ATTR_BYREF)
                || (CffiTypeIsNotArray(&typeAttrsP->dataType)
#ifndef CFFI_USE_LIBFFI
                    && baseType != CFFI_K_TYPE_STRUCT
#endif
                    && baseType != CFFI_K_TYPE_CHAR_ARRAY
                    && baseType != CFFI_K_TYPE_UNICHAR_ARRAY
                    && baseType != CFFI_K_TYPE_BYTE_ARRAY));

    /*
     * STORENUM - for storing numerics only.
     * For storing an argument, if the parameter is byval, extract the
     * value into native form and pass it to dyncall via the specified
     * dcfn_ function. Note byval parameter are ALWAYS in-only.
     * For in and inout, the value is extracted into native storage.
     * For byval parameters (can only be in, never out/inout), this
     * value is passed in to dyncall via the argument specific function
     * specified by dcfn_. For byref parameters, the argument is always
     * passed via the dcArgPointer function with a pointer to the
     * native value.
     *
     * NOTE: We explicitly pass in objfn_ and dcfn_ instead of using
     * conversion functions from cffiBaseTypes as it is more efficient
     * (compile time function binding)
     */

    /* May the programming gods forgive me for these macro */

#ifdef CFFI_USE_DYNCALL
#define STOREARGBYVAL(storefn_, fld_)                   \
    do {                                                \
        storefn_(callP, arg_index, argP->value.u.fld_); \
    } while (0)
#define STOREARGBYREF(fld_)                                         \
    do {                                                            \
        CffiStoreArgPointer(callP, arg_index, &argP->value.u.fld_); \
    } while (0)
#endif /*  CFFI_USE_DYNCALL */

#ifdef CFFI_USE_LIBFFI
#define STOREARGBYVAL(storefn_, fld_)                        \
    do {                                                     \
        callP->argValuesPP[arg_index] = &argP->value.u.fld_; \
    } while (0)
#define STOREARGBYREF(fld_)                                  \
    do {                                                     \
        argP->valueP                  = &argP->value.u.fld_; \
        callP->argValuesPP[arg_index] = &argP->valueP;       \
    } while (0)

#endif /* CFFI_USE_LIBFFI */

#define PUSHARG(dcfn_, fld_)            \
    do {                                \
        if (flags & CFFI_F_ATTR_BYREF)  \
            STOREARGBYREF(fld_);        \
        else                            \
            STOREARGBYVAL(dcfn_, fld_); \
    } while (0)

    switch (baseType) {
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
    case CFFI_K_TYPE_FLOAT:
    case CFFI_K_TYPE_DOUBLE:
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
        if (argP->arraySize < 0) {
            if (flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT)) {
                /* NOTE - &argP->value is start of all field values */
                CHECK(CffiNativeScalarFromObj(ipCtxP,
                                              typeAttrsP,
                                              valueObj,
                                              0,
                                              &argP->value,
                                              0,
                                              &ipCtxP->memlifo));
            }
            else {
                /* Set pure OUT to 0. Not necessary but may catch some pointer errors */
                memset(&argP->value, 0, sizeof(argP->value));
            }
            switch (baseType) {
            case CFFI_K_TYPE_SCHAR: PUSHARG(CffiStoreArgSChar, schar); break;
            case CFFI_K_TYPE_UCHAR: PUSHARG(CffiStoreArgUChar, uchar); break;
            case CFFI_K_TYPE_SHORT: PUSHARG(CffiStoreArgShort, sshort); break;
            case CFFI_K_TYPE_USHORT: PUSHARG(CffiStoreArgUShort, ushort); break;
            case CFFI_K_TYPE_INT: PUSHARG(CffiStoreArgInt, sint); break;
            case CFFI_K_TYPE_UINT: PUSHARG(CffiStoreArgUInt, uint); break;
            case CFFI_K_TYPE_LONG: PUSHARG(CffiStoreArgLong, slong); break;
            case CFFI_K_TYPE_ULONG: PUSHARG(CffiStoreArgULong, ulong); break;
            case CFFI_K_TYPE_LONGLONG: PUSHARG(CffiStoreArgLongLong, slonglong); break;
            case CFFI_K_TYPE_ULONGLONG: PUSHARG(CffiStoreArgULongLong, ulonglong); break;
            case CFFI_K_TYPE_FLOAT: PUSHARG(CffiStoreArgFloat, flt); break;
            case CFFI_K_TYPE_DOUBLE: PUSHARG(CffiStoreArgDouble, dbl); break;
            case CFFI_K_TYPE_ASTRING: PUSHARG(CffiStoreArgPointer, ptr); break;
            case CFFI_K_TYPE_UNISTRING: PUSHARG(CffiStoreArgPointer, ptr); break;
            default:
                return Tclh_ErrorGeneric(
                    ip, NULL, "Internal error: type not handled.");
            }
        }
        else if (argP->arraySize == 0) {
            /* Zero size array, pass as NULL */
            goto pass_null_array;
        }
        else {
            void *valuesP;
            CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
            if (flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT)) {
                valuesP = MemLifoAlloc(&ipCtxP->memlifo,
                                       argP->arraySize
                                           * typeAttrsP->dataType.baseTypeSize);
                CHECK(CffiNativeValueFromObj(ipCtxP,
                                             typeAttrsP,
                                             argP->arraySize,
                                             valueObj,
                                             0,
                                             valuesP,
                                             0,
                                             &ipCtxP->memlifo));
            }
            else {
                valuesP = MemLifoZeroes(
                    &ipCtxP->memlifo,
                    argP->arraySize * typeAttrsP->dataType.baseTypeSize);
            }
            argP->value.u.ptr = valuesP;
            /* BYREF but really a pointer to array so STOREARGBYVAL, not STOREARGBYREF */
            STOREARGBYVAL(CffiStoreArgPointer, ptr);
        }
        break;
    case CFFI_K_TYPE_STRUCT:
        CFFI_ASSERT(argP->arraySize != 0);
        if (argP->arraySize < 0) {
            /* Single struct */
            void *structValueP;
            if (typeAttrsP->flags & CFFI_F_ATTR_NULLIFEMPTY) {
                int dict_size;
                CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_BYREF);
                CHECK(Tcl_DictObjSize(ip, valueObj, &dict_size));
                if (dict_size == 0) {
                    /* Empty dictionary AND NULLIFEMPTY set */
                    argP->value.u.ptr = NULL;
                    /* BYREF but really a pointer so STOREARG, not STOREARGBYREF */
                    STOREARGBYVAL(CffiStoreArgPointer, ptr);
                    break;
                }
                /* NULLIFEMPTY but dictionary has elements */
            }
            /* TBD - check out struct size matches Libffi's */
            structValueP = MemLifoAlloc(&ipCtxP->memlifo,
                                        typeAttrsP->dataType.u.structP->size);
            if (flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT)) {
                CHECK(CffiStructFromObj(ipCtxP,
                                        typeAttrsP->dataType.u.structP,
                                        valueObj, 0,
                                        structValueP, &ipCtxP->memlifo));
            }
            if (typeAttrsP->flags & CFFI_F_ATTR_BYREF) {
                argP->value.u.ptr = structValueP;
                /* BYREF but really a pointer so STOREARG, not STOREARGBYREF */
                STOREARGBYVAL(CffiStoreArgPointer, ptr);
            } else {
#ifdef CFFI_USE_DYNCALL
                CFFI_ASSERT(0); /* Should not reach here for dyncall */
#endif
#ifdef CFFI_USE_LIBFFI
                argP->value.u.ptr             = NULL;/* Not used */
                callP->argValuesPP[arg_index] = structValueP;
#endif
            }
        }
        else {
            /* Array of structs */
            char *valueArray;
            char *toP;
            int struct_size = typeAttrsP->dataType.u.structP->size;
            CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
            if (argP->arraySize == 0)
                goto pass_null_array;
            valueArray =
                MemLifoAlloc(&ipCtxP->memlifo, argP->arraySize * struct_size);
            if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
                /* IN or INOUT */
                Tcl_Obj **valueObjList;
                int i, nvalues;
                CHECK(Tcl_ListObjGetElements(
                    ip, valueObj, &nvalues, &valueObjList));
                if (nvalues > argP->arraySize)
                    nvalues = argP->arraySize;
                for (toP = valueArray, i = 0; i < nvalues;
                     toP += struct_size, ++i) {
                    CHECK(CffiStructFromObj(ipCtxP,
                                            typeAttrsP->dataType.u.structP,
                                            valueObjList[i], 0,
                                            toP, &ipCtxP->memlifo));
                }
                if (i < argP->arraySize) {
                    /* Fill uninitialized with 0 */
                    memset(toP, 0, (argP->arraySize - i) * struct_size);
                }
            }
            argP->value.u.ptr = valueArray;
            STOREARGBYVAL(CffiStoreArgPointer, ptr);
        }
        break;

    case CFFI_K_TYPE_POINTER:
        CFFI_ASSERT(argP->arraySize != 0);
        if (argP->arraySize < 0) {
            if (flags & CFFI_F_ATTR_OUT)
                argP->value.u.ptr = NULL;
            else {
                CHECK(
                    CffiPointerFromObj(ip, typeAttrsP, valueObj, &argP->value.u.ptr));
                if (flags & (CFFI_F_ATTR_DISPOSE | CFFI_F_ATTR_DISPOSEONSUCCESS))
                    argP->savedValue.u.ptr = argP->value.u.ptr;
            }
            if (flags & CFFI_F_ATTR_BYREF)
                STOREARGBYREF(ptr);
            else
                STOREARGBYVAL(CffiStoreArgPointer, ptr);
        }
        else {
            void **valueArray;
            CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
            if (argP->arraySize == 0)
                goto pass_null_array;
            valueArray = MemLifoAlloc(&ipCtxP->memlifo,
                                      argP->arraySize * sizeof(void *));
            if (flags & CFFI_F_ATTR_OUT)
                argP->value.u.ptr = valueArray;
            else {
                Tcl_Obj **valueObjList;
                int i, nvalues;
                CHECK(Tcl_ListObjGetElements(
                    ip, valueObj, &nvalues, &valueObjList));
                if (nvalues > argP->arraySize)
                    nvalues = argP->arraySize;
                for (i = 0; i < nvalues; ++i) {
                    CHECK(CffiPointerFromObj(
                        ip, typeAttrsP, valueObjList[i], &valueArray[i]));
                }
                CFFI_ASSERT(i == nvalues);
                for ( ; i < argP->arraySize; ++i)
                    valueArray[i] = NULL;/* Fill additional elements */
                argP->value.u.ptr = valueArray;
                if (flags & (CFFI_F_ATTR_DISPOSE | CFFI_F_ATTR_DISPOSEONSUCCESS)) {
                    /* Save pointers to dispose after call completion */
                    void **savedValueArray;
                    savedValueArray = MemLifoAlloc(
                        &ipCtxP->memlifo, argP->arraySize * sizeof(void *));
                    for (i = 0; i < argP->arraySize; ++i)
                        savedValueArray[i] = valueArray[i];
                    argP->savedValue.u.ptr = savedValueArray;
                }
            }
            /* BYREF but really a pointer so STOREARG, not STOREARGBYREF */
            STOREARGBYVAL(CffiStoreArgPointer, ptr);
        }
        break;

    case CFFI_K_TYPE_CHAR_ARRAY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        if (argP->arraySize == 0)
            goto pass_null_array;
        CHECK(CffiArgPrepareChars(callP, arg_index, valueObj, &argP->value));
        /* BYREF but really a pointer so STOREARG, not STOREARGBYREF */
        STOREARGBYVAL(CffiStoreArgPointer, ptr);
        break;
    case CFFI_K_TYPE_UNICHAR_ARRAY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        if (argP->arraySize == 0)
            goto pass_null_array;
        CHECK(CffiArgPrepareUniChars(callP, arg_index, valueObj, &argP->value));
        /* BYREF but really a pointer so STOREARG, not STOREARGBYREF */
        STOREARGBYVAL(CffiStoreArgPointer, ptr);
        break;

    case CFFI_K_TYPE_BINARY:
        CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_IN);
        /* Pure input but could still shimmer so copy to memlifo */
        p = (char *)Tcl_GetByteArrayFromObj(valueObj, &len);
        /* If zero length, always store null pointer regardless of nullifempty */
        if (len)
            argP->value.u.ptr = MemLifoCopy(&ipCtxP->memlifo, p, len);
        else
            argP->value.u.ptr = NULL;
        if (flags & CFFI_F_ATTR_BYREF)
            STOREARGBYREF(ptr);
        else
            STOREARGBYVAL(CffiStoreArgPointer, ptr);
        break;

    case CFFI_K_TYPE_BYTE_ARRAY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        if (argP->arraySize <= 0)
            goto pass_null_array;
        argP->value.u.ptr = MemLifoAlloc(&ipCtxP->memlifo, argP->arraySize);
        if (flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT)) {
            /* NOTE: because of shimmering possibility, we need to copy */
            CHECK(CffiBytesFromObjSafe(
                ipCtxP->interp, valueObj, argP->value.u.ptr, argP->arraySize));
        }
        /* BYREF but really a pointer so STOREARG, not STOREARGBYREF */
        STOREARGBYVAL(CffiStoreArgPointer, ptr);
        break;

    default:
        return ErrorInvalidValue( ip, NULL, "Unsupported type.");
    }

success:
    callP->argsP[arg_index].flags |= CFFI_F_ARG_INITIALIZED;
    return TCL_OK;

pass_null_array:
    /* Zero size array, pass as NULL */
    CFFI_ASSERT(argP->arraySize == 0);
    CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
    if ((flags & CFFI_F_ATTR_NULLOK) == 0) {
        return Tclh_ErrorGeneric(ip,
                                 NULL,
                                 "Passing a zero size array requires "
                                 "the nullok annotation.");
    }
    argP->value.u.ptr = NULL;
    STOREARGBYVAL(CffiStoreArgPointer, ptr);
    goto success;

#undef PUSHARG
#undef STOREARGBYVAL
#undef STOREARGBYREF
}

/* Function: CffiArgPostProcess
 * Does the post processing of an argument after a call
 *
 * Post processing of the argument consists of checking if the parameter
 * was an *out* or *inout* parameter and storing it in the output Tcl variable.
 * Note no cleanup of argument storage is done.
 *
 * In the case of arrays, if the output array was specified as zero size,
 * the output variable is not modified.
 *
 * Parameters:
 * callP - the call context
 * arg_index - index of argument to do post processing
 *
 * Returns:
 * *TCL_OK* on success,
 * *TCL_ERROR* on error with message stored in the interpreter.
 */
CffiResult
CffiArgPostProcess(CffiCall *callP, int arg_index)
{
    Tcl_Interp *ip = callP->fnP->ipCtxP->interp;
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;
    CffiArgument *argP = &callP->argsP[arg_index];
    Tcl_Obj *varObjP   = argP->varNameObj;
    CffiValue *valueP;
    Tcl_Obj *valueObj;
    int ret;

    CFFI_ASSERT(callP->argsP[arg_index].flags & CFFI_F_ARG_INITIALIZED);

    if (typeAttrsP->flags & CFFI_F_ATTR_IN)
        return TCL_OK;

    CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_BYREF);

    /* If output array size was zero, no output is to be stored */
    if (argP->arraySize == 0)
        return TCL_OK;

    /*
     * There are three categories:
     *  - scalar values are directly stored in *valueP
     *  - structs and arrays of scalars are stored in at the location
     *    pointed to by valueP->u.ptr
     *  - strings/unistring are actually pointers and stored in valueP->u.ptr
     */
    valueP = &argP->value;

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
    case CFFI_K_TYPE_FLOAT:
    case CFFI_K_TYPE_DOUBLE:
    case CFFI_K_TYPE_POINTER:
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
        /* Scalars stored at valueP, arrays of scalars at valueP->u.ptr */
        /* TBD - this might be broken for small integers on big-endian
        since they are promoted by libffi to ffi_arg */
        if (argP->arraySize < 0)
            ret = CffiNativeValueToObj(
                ip, typeAttrsP, valueP, 0, argP->arraySize, &valueObj);
        else
            ret = CffiNativeValueToObj(
                ip, typeAttrsP, valueP->u.ptr, 0, argP->arraySize, &valueObj);
        break;

    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
        ret = CffiNativeValueToObj(
            ip, typeAttrsP, valueP->u.ptr, 0, argP->arraySize, &valueObj);
        break;

    case CFFI_K_TYPE_STRUCT:
        ret = CffiNativeValueToObj(
            ip, typeAttrsP, valueP->u.ptr, 0, argP->arraySize, &valueObj);
        break;

    case CFFI_K_TYPE_BINARY:
    default:
        /* Should not happen */
        ret = Tclh_ErrorInvalidValue(ip, NULL, "Unsupported argument type");
        break;
    }

    if (ret != TCL_OK)
        return ret;

    /*
     * Check if value is to be converted to a enum name. This is slightly
     * inefficient since we have to convert back from a Tcl_Obj to an
     * integer but currently the required context is not being passed down
     * to the lower level functions that extract scalar values.
     */
    if ((typeAttrsP->flags & (CFFI_F_ATTR_ENUM|CFFI_F_ATTR_BITMASK))
        && typeAttrsP->dataType.u.tagObj) {
        Tcl_Obj *enumValueObj;
        Tcl_WideInt wide;
        if (CffiTypeIsNotArray(&typeAttrsP->dataType)) {
            /* Note on error, keep the value */
            if (Tcl_GetWideIntFromObj(NULL, valueObj, &wide) == TCL_OK) {
                enumValueObj = CffiIntValueToObj(typeAttrsP, wide);
                if (enumValueObj)
                    valueObj = enumValueObj;
            }
        }
        else {
            /* Array of integers */
            Tcl_Obj **elemObjs;
            int nelems;
            /* Note on error, keep the value */
            if (Tcl_ListObjGetElements(NULL, valueObj, &nelems, &elemObjs)
                == TCL_OK) {
                Tcl_Obj *enumValuesObj;
                int i;
                enumValuesObj = Tcl_NewListObj(nelems, NULL);
                for (i = 0; i < nelems; ++i) {
                    if (Tcl_GetWideIntFromObj(NULL, elemObjs[i], &wide)
                        != TCL_OK) {
                        break;
                    }
                    enumValueObj = CffiIntValueToObj(typeAttrsP, wide);
                    if (enumValueObj)
                        Tcl_ListObjAppendElement(
                            NULL, enumValuesObj, enumValueObj);
                    else
                        Tcl_ListObjAppendElement(
                            NULL, enumValuesObj, elemObjs[i]);
                }
                if (i == nelems) {
                    /* All converted successfully */
                    Tcl_DecrRefCount(valueObj);
                    valueObj = enumValuesObj;
                }
                else {
                    /* Keep original */
                    Tcl_DecrRefCount(enumValuesObj);
                }
            }
        }
    }

    /*
     * Tcl_ObjSetVar2 will release valueObj if its ref count is 0
     * preventing us from trying again after deleting the array so
     * preserve the value obj.
     */
    Tcl_IncrRefCount(valueObj);
    if (Tcl_ObjSetVar2(ip, varObjP, NULL, valueObj, 0) == NULL) {
        /* Perhaps it is an array in which case we need to delete first */
        Tcl_UnsetVar(ip, Tcl_GetString(varObjP), 0);
        /* Retry */
        if (Tcl_ObjSetVar2(ip, varObjP, NULL, valueObj, TCL_LEAVE_ERR_MSG)
            == NULL) {
            Tcl_DecrRefCount(valueObj);
            return TCL_ERROR;
        }
    }
    Tcl_DecrRefCount(valueObj);
    return TCL_OK;
}

/* Function: CffiReturnPrepare
 * Prepares storage for DCCall function return value
 *
 * Parameters:
 * callP - call context
 *
 * The function may allocate additional dynamic storage from
 * *ipContextP->memlifo* that is the caller's responsibility to free.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
CffiResult
CffiReturnPrepare(CffiCall *callP)
{
#ifdef CFFI_USE_DYNCALL
    /*
     * Nothing to do as no allocations needed.
     *  arrays, struct, chars[], unichars[], bytes and anything that requires
     * non-scalar storage is either not supported by C or by dyncall.
     */
#endif /* CFFI_USE_DYNCALL */

#ifdef CFFI_USE_LIBFFI
    CffiTypeAndAttrs *retTypeAttrsP = &callP->fnP->protoP->returnType.typeAttrs;
    /* Byref return values are basically pointers irrespective of base type */
    if (retTypeAttrsP->flags & CFFI_F_ATTR_BYREF) {
        callP->retValueP = &callP->retValue.u.ptr;
        return TCL_OK;
    }

    /*
     * For *integer* types, libffi has a quirk in how it returns values
     * via promotion. Values small enough to fit in a register (ffi_arg)
     * are promoted to ffi_arg.
     * * TBD - not clear this is really needed since in any case the
     * target is a union of all possible types. and effectively, the
     * resulting pointer value is the same.
     */
#define INITRETPTR(type_, fld_) \
    do { \
        if (sizeof(type_) <= sizeof(ffi_arg)) { \
            callP->retValueP = &callP->retValue.u.ffi_val; \
        } else { \
            callP->retValueP = &callP->retValue.u.fld_; \
        } \
    } while (0)

    /* Set up the pointer for the return storage.  */
    switch (retTypeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_VOID: callP->retValueP = NULL; break;
    case CFFI_K_TYPE_SCHAR: INITRETPTR(signed char, schar); break;
    case CFFI_K_TYPE_UCHAR: INITRETPTR(signed char, schar); break;
    case CFFI_K_TYPE_SHORT: INITRETPTR(signed char, schar); break;
    case CFFI_K_TYPE_USHORT: INITRETPTR(signed char, schar); break;
    case CFFI_K_TYPE_INT: INITRETPTR(signed char, schar); break;
    case CFFI_K_TYPE_UINT: INITRETPTR(signed char, schar); break;
    case CFFI_K_TYPE_LONG: INITRETPTR(signed char, schar); break;
    case CFFI_K_TYPE_ULONG: INITRETPTR(signed char, schar); break;
    case CFFI_K_TYPE_LONGLONG: INITRETPTR(signed char, schar); break;
    case CFFI_K_TYPE_ULONGLONG: INITRETPTR(signed char, schar); break;
    case CFFI_K_TYPE_FLOAT: callP->retValueP = &callP->retValue.u.flt; break;
    case CFFI_K_TYPE_DOUBLE: callP->retValueP = &callP->retValue.u.dbl; break;
    case CFFI_K_TYPE_ASTRING: /* Fallthru - treat as pointer */
    case CFFI_K_TYPE_UNISTRING: /* Fallthru - treat as pointer */
    case CFFI_K_TYPE_POINTER: callP->retValueP = &callP->retValue.u.ptr; break;
    case CFFI_K_TYPE_STRUCT:
        callP->retValueP = MemLifoAlloc(
            &callP->fnP->libCtxP->ipCtxP->memlifo,
            retTypeAttrsP->dataType.u.structP->size);
        break;
    default:
        Tcl_SetResult(callP->fnP->ipCtxP->interp,
                      "Invalid return type.",
                      TCL_STATIC);
        return TCL_ERROR;
    }
#undef INITRETPTR
#endif /* CFFI_USE_LIBFFI */

    return TCL_OK;
}

CffiResult CffiReturnCleanup(CffiCall *callP)
{
    /*
     * No clean up needed for any types. Any type that needs non-scalar
     * storage is allocated from the memlifo area.
     */
    return TCL_OK;
}



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
    CffiAttrFlags flags = typeAttrsP->flags;

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
        CffiAttrFlags flags          = typeAttrsP->flags;
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
 * callP - initialized call context.
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
    CffiInterpCtx *ipCtxP;

    protoP = callP->fnP->protoP;
    ipCtxP = callP->fnP->ipCtxP;
    ip     = ipCtxP->interp;

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
        &ipCtxP->memlifo, callP->nArgs * sizeof(CffiArgument));
    callP->argsP = argsP;
    for (i = 0; i < callP->nArgs; ++i)
        argsP[i].flags = 0; /* Mark as uninitialized */
#ifdef CFFI_USE_LIBFFI
    callP->argValuesPP = (void **)MemLifoAlloc(
        &ipCtxP->memlifo, callP->nArgs * sizeof(void *));
#endif

    /*
     * Arguments are set up in two phases - first set up those arguments that
     * are not dependent on other argument values. Then loop again to set up
     * the latter. Currently only dynamically sized arrays are dependent on
     * other arguments.
     */
    need_pass2 = 0;
    for (i = 0; i < callP->nArgs; ++i) {
        CffiTypeAndAttrs *typeAttrsP = &protoP->params[i].typeAttrs;
        if (CffiTypeIsVariableSizeArray(&typeAttrsP->dataType)) {
            /* Dynamic array. */
            need_pass2 = 1;
            continue;
        }

        /* Scalar or fixed size array. Type decl should have ensured size!=0 */
        argsP[i].arraySize = typeAttrsP->dataType.arraySize;
        CFFI_ASSERT(argsP[i].arraySize != 0); /* Must be scalar or array of size > 0 */
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
        CffiTypeAndAttrs *typeAttrsP = &protoP->params[i].typeAttrs;

        if (! CffiTypeIsVariableSizeArray(&typeAttrsP->dataType)) {
            /* This arg already been parsed successfully. Just load it. */
            CFFI_ASSERT(argsP[i].flags & CFFI_F_ARG_INITIALIZED);
            CffiReloadArg(callP, &argsP[i], typeAttrsP);
            continue;
        }
        CFFI_ASSERT((argsP[i].flags & CFFI_F_ARG_INITIALIZED) == 0);

        /* Need to find the parameter corresponding to this dynamic count */
        CFFI_ASSERT(typeAttrsP->dataType.countHolderObj);
        name = Tcl_GetString(typeAttrsP->dataType.countHolderObj);

        /*
         * Loop through initialized parameters looking for a match. A match
         * must have been initialized (ie. not dynamic), must be a scalar
         * and having the referenced name.
         */
        for (j = 0; j < callP->nArgs; ++j) {
            if ((argsP[j].flags & CFFI_F_ARG_INITIALIZED)
                && CffiTypeIsNotArray(&protoP->params[j].typeAttrs.dataType) &&
                !strcmp(name, Tcl_GetString(protoP->params[j].nameObj)))
                break;
        }
        if (j == callP->nArgs) {
            (void)Tclh_ErrorNotFound(
                ip,
                "Parameter",
                typeAttrsP->dataType.countHolderObj,
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

        if (actualCount < 0 || actualCount > INT_MAX) {
            (void) Tclh_ErrorRange(ip, argObjs[j], 1, INT_MAX);
            goto cleanup_and_error;
        }

        argsP[i].arraySize = (int) actualCount;
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
    CffiInterpCtx *ipCtxP = fnP->ipCtxP;
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

#ifdef CFFI_USE_LIBFFI
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
#ifdef CFFI_USE_LIBFFI
    callCtx.argValuesPP = NULL;
    callCtx.retValueP   = NULL;
#endif

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
#define CALLFN(objfn_, dcfn_, fld_, type_)                                     \
    do {                                                                       \
        CffiValue retval;                                                      \
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_BYREF) {          \
            type_ *p = CffiCallPointerFunc(&callCtx);                          \
            if (p)                                                             \
                retval.u.fld_ = *p;                                            \
            else {                                                             \
                fnCheckRet = Tclh_ErrorInvalidValue(                           \
                    ip, NULL, "Function returned NULL pointer");               \
                ret = TCL_ERROR;                                               \
                break; /* Skip rest of macro */                                \
            }                                                                  \
        }                                                                      \
        else                                                                   \
            retval.u.fld_ = dcfn_(&callCtx);                                   \
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_REQUIREMENT_MASK) \
            fnCheckRet = CffiCheckNumeric(                                     \
                ip, &protoP->returnType.typeAttrs, &retval, &sysError);        \
        CffiPointerArgsDispose(ip, protoP, callCtx.argsP, fnCheckRet);         \
        if (fnCheckRet == TCL_OK) {                                            \
            /* First try converting as enums, bitmasks etc. */                 \
            resultObj = CffiIntValueToObj(&protoP->returnType.typeAttrs,       \
                                          (Tcl_WideInt)retval.u.fld_);         \
            /* If that did not work just use native value */                   \
            if (resultObj == NULL)                                             \
                resultObj = objfn_(retval.u.fld_);                             \
        }                                                                      \
        else {                                                                 \
            /* AFTER above Check to not lose GetLastError */                   \
            resultObj = objfn_(retval.u.fld_);                                 \
        }                                                                      \
    } while (0)

    switch (protoP->returnType.typeAttrs.dataType.baseType) {
    case CFFI_K_TYPE_VOID:
        CffiCallVoidFunc(&callCtx);
        CffiPointerArgsDispose(ip, protoP, callCtx.argsP, fnCheckRet);
        resultObj = Tcl_NewObj();
        break;
    case CFFI_K_TYPE_SCHAR:
        CALLFN(Tcl_NewIntObj, CffiCallSCharFunc, schar, signed char);
        break;
    case CFFI_K_TYPE_UCHAR:
        CALLFN(Tcl_NewIntObj, CffiCallUCharFunc, uchar, unsigned char);
        break;
    case CFFI_K_TYPE_SHORT:
        CALLFN(Tcl_NewIntObj, CffiCallShortFunc, sshort, short);
        break;
    case CFFI_K_TYPE_USHORT:
        CALLFN(Tcl_NewIntObj, CffiCallUShortFunc, ushort, unsigned short);
        break;
    case CFFI_K_TYPE_INT:
        CALLFN(Tcl_NewIntObj, CffiCallIntFunc, sint, int);
        break;
    case CFFI_K_TYPE_UINT:
        CALLFN(Tcl_NewWideIntObj, CffiCallUIntFunc, uint, unsigned int);
        break;
    case CFFI_K_TYPE_LONG:
        CALLFN(Tcl_NewLongObj, CffiCallLongFunc, slong, long);
        break;
    case CFFI_K_TYPE_ULONG:
        CALLFN(Tclh_ObjFromULong, CffiCallULongFunc, ulong, unsigned long);
        break;
    case CFFI_K_TYPE_LONGLONG:
        CALLFN(Tcl_NewWideIntObj, CffiCallLongLongFunc, slonglong, long long);
        break;
    case CFFI_K_TYPE_ULONGLONG:
        CALLFN(Tclh_ObjFromULongLong, CffiCallULongLongFunc, ulonglong, unsigned long long);
        break;
    case CFFI_K_TYPE_FLOAT:
        CALLFN(Tcl_NewDoubleObj, CffiCallFloatFunc, flt, float);
        break;
    case CFFI_K_TYPE_DOUBLE:
        CALLFN(Tcl_NewDoubleObj, CffiCallDoubleFunc, dbl, double);
        break;
    case CFFI_K_TYPE_POINTER:
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
        pointer = CffiCallPointerFunc(&callCtx);
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_BYREF) {
            if (pointer)
                pointer = *(void **)pointer; /* By ref so dereference */
            else {
                fnCheckRet = Tclh_ErrorInvalidValue(
                    ip, NULL, "Function returned NULL pointer");
                ret = TCL_ERROR;
                break;
            }
        }
        /* Do check IMMEDIATELY so as to not lose GetLastError */
        fnCheckRet = CffiCheckPointer(
            ip, &protoP->returnType.typeAttrs, pointer, &sysError);
        CffiPointerArgsDispose(ip, protoP, callCtx.argsP, fnCheckRet);         \
        switch (protoP->returnType.typeAttrs.dataType.baseType) {
            case CFFI_K_TYPE_POINTER:
                ret = CffiPointerToObj(
                    ip, &protoP->returnType.typeAttrs, pointer, &resultObj);
                break;
            case CFFI_K_TYPE_ASTRING:
                ret = CffiCharsToObj(
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
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_BYREF) {
            pointer = CffiCallPointerFunc(&callCtx);
            if (pointer == NULL) {
                fnCheckRet = Tclh_ErrorInvalidValue(
                    ip, NULL, "Function returned NULL pointer");
                ret = TCL_ERROR;
                break;
            }
        }
        else {
#ifdef CFFI_USE_LIBFFI
            CffiLibffiCall(&callCtx);
            pointer = callCtx.retValueP;
#endif
#ifdef CFFI_USE_DYNCALL
            /* Should not really happen as checks made at definition time */
            (void)Tclh_ErrorInvalidValue(
                ipCtxP->interp, NULL, "Unsupported type for return.");
            ret = TCL_ERROR;
            break;
#endif
        }
        ret = CffiStructToObj(ip,
                              protoP->returnType.typeAttrs.dataType.u.structP,
                              pointer,
                              &resultObj);
        break;
    case CFFI_K_TYPE_BINARY:
    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
#if 0
        /* Currently BYREF not allowed for arrays and DC does not support struct byval */
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
            CffiAttrFlags flags = protoP->params[i].typeAttrs.flags;
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
 *    ip - interpreter
 *    ipCtxP - interpreter context
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
                      CffiInterpCtx *ipCtxP,
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

    CHECK(CffiPrototypeParse(ipCtxP,
                             cmdNameObj,
                             returnTypeObj,
                             paramsObj,
                             &protoP));
    /* TBD - comment in func header says only override if default */
    protoP->abi = callMode;
#ifdef CFFI_USE_LIBFFI
    protoP->cifP = NULL;
#endif

    fnP = ckalloc(sizeof(*fnP));
    fnP->fnAddr = fnAddr;
    fnP->ipCtxP = ipCtxP;
    fnP->libCtxP = libCtxP;
    if (libCtxP)
        CffiLibCtxRef(libCtxP);
    CffiProtoRef(protoP);
    fnP->protoP = protoP;
    fqnObj = Tclh_NsQualifyNameObj(ip, cmdNameObj, NULL);

    Tcl_IncrRefCount(fqnObj);
    fnP->cmdNameObj = fqnObj;

    Tcl_CreateObjCommand(ip,
                         Tcl_GetString(fqnObj),
                         CffiFunctionInstanceCmd,
                         fnP,
                         CffiFunctionInstanceDeleter);
    Tcl_SetObjResult(ip, fqnObj);
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
CffiResult
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
                                 libCtxP->ipCtxP,
                                 libCtxP,
                                 fn,
                                 cmdNameObj,
                                 returnTypeObj,
                                 paramsObj,
                                 callMode);
}

