/*
 * Copyright (c) 2021-2022 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#define TCLH_SHORTNAMES
#include "tclCffiInt.h"

static CffiResult CffiArgPrepare(CffiCall *callP, int arg_index, Tcl_Obj *valueObj);
static CffiResult
CffiArgPostProcess(CffiCall *callP, int arg_index, Tcl_Obj **resultObjP);
static void CffiArgCleanup(CffiCall *callP, int arg_index);
static CffiResult CffiReturnPrepare(CffiCall *callP);
static CffiResult CffiReturnCleanup(CffiCall *callP);
static void CffiPointerArgsDispose(CffiInterpCtx *ipCtxP,
                                   int nArgs,
                                   CffiArgument *argsP,
                                   int callSucceeded);
static CffiResult CffiGetCountFromValue(Tcl_Interp *ip,
                                        CffiBaseType valueType,
                                        const CffiValue *valueP,
                                        int *countP);

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
 * nArgs - size of argsP[]
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
static void
CffiPointerArgsDispose(CffiInterpCtx *ipCtxP,
                       int nArgs,
                       CffiArgument *argsP,
                       int callFailed)
{
    Tcl_Interp *ip = ipCtxP->interp;
    int i;
    for (i = 0; i < nArgs; ++i) {
        CffiTypeAndAttrs *typeAttrsP = argsP[i].typeAttrsP;
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
                        Tclh_PointerUnregister(ip,
                                               ipCtxP->tclhCtxP,
                                               argsP[i].savedValue.u.ptr,
                                               NULL);
                }
                else {
                    /* Array */
                    int j;
                    void **ptrArray = argsP[i].savedValue.u.ptr;
                    CFFI_ASSERT(ptrArray);
                    for (j = 0; j < nptrs; ++j) {
                        if (ptrArray[j] != NULL)
                            Tclh_PointerUnregister(
                                ip, ipCtxP->tclhCtxP, ptrArray[j], NULL);
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
    const CffiTypeAndAttrs *typeAttrsP = argP->typeAttrsP;

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_CHAR_ARRAY);
    CFFI_ASSERT(argP->arraySize > 0);

    valueP->u.ptr =
        Tclh_LifoAlloc(&ipCtxP->memlifo, argP->arraySize);

    /* If input, we need to encode appropriately */
    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT))
        return CffiCharsFromObj(ipCtxP->interp,
                                typeAttrsP->dataType.u.encoding,
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
    const CffiTypeAndAttrs *typeAttrsP = argP->typeAttrsP;

    CFFI_ASSERT(argP->arraySize > 0);

    valueP->u.ptr =
        Tclh_LifoAlloc(&ipCtxP->memlifo, argP->arraySize * sizeof(Tcl_UniChar));
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
static
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
 *             parameters. May be NULL in cases where CFFI_F_ATTR_RETVAL
 *             is set for the parameter.
 *
 * The function modifies the following:
 *
 * callP->argsP[arg_index].flags - sets CFFI_F_ARG_INITIALIZED
 *
 * callP->argsP[arg_index].value - the native value.
 *
 * callP->argsP[arg_index].valueP - (libffi) Set to point to the value field
 *
 * callP->argsP[arg_index].typeAttrsP - the type descriptor for the argument
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
static CffiResult
CffiArgPrepare(CffiCall *callP, int arg_index, Tcl_Obj *valueObj)
{
    CffiInterpCtx *ipCtxP = callP->fnP->ipCtxP;
    Tcl_Interp *ip        = ipCtxP->interp;
    CffiArgument *argP    = &callP->argsP[arg_index];
    const CffiTypeAndAttrs *typeAttrsP = argP->typeAttrsP;
    Tcl_Obj **varNameObjP = &argP->varNameObj;
    enum CffiBaseType baseType;
    CffiAttrFlags flags;
    int len;
    char *p;

    /* Expected initialization to virgin state */
    CFFI_ASSERT(argP->flags == 0);

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
     * For pure in parameters, valueObj provides the value itself.
     *
     * For out and inout parameters, valueObj is normally the variable name
     * with the exception noted below. If the parameter is an inout
     * parameter, the variable must exist since the value passed to the
     * called function is taken from there. For pure out parameters, the
     * variable need not exist and will be created if necessary. For both in
     * and inout, on return from the function the corresponding content is
     * stored in that variable.
     *
     * The exception to the above for out parameters is that if the
     * CFFI_F_ATTR_RETVAL flag is set, the value returned in the parameter
     * by the function is forwarded up as the function return so there
     * is not variable name supplied and valueObj will be NULL.
     */
    *varNameObjP = NULL;
    if (flags & (CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT)) {
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        if (flags & CFFI_F_ATTR_RETVAL) {
            CFFI_ASSERT(flags & CFFI_F_ATTR_OUT);
            CFFI_ASSERT(valueObj == NULL);
        }
        else {
            *varNameObjP = valueObj;
            valueObj = Tcl_ObjGetVar2(ip, valueObj, NULL, TCL_LEAVE_ERR_MSG);
            if (valueObj == NULL && (flags & CFFI_F_ATTR_INOUT)) {
                return ErrorInvalidValue(
                    ip,
                    *varNameObjP,
                    "Variable specified as inout argument does not exist.");
        /* TBD - check if existing variable is an array and error out? */
            }
        }
    }

    /*
     * Type parsing should have validated out/inout params specify size if
     * not a fixed size type. Note chars/unichars/bytes are fixed size since
     * their array size is required to be specified.
     */

    /* Non-scalars need to be passed byref. Parsing should have checked */
#ifdef CFFI_USE_LIBFFI
    CFFI_ASSERT((flags & CFFI_F_ATTR_BYREF)
                || (CffiTypeIsNotArray(&typeAttrsP->dataType)
                    && baseType != CFFI_K_TYPE_CHAR_ARRAY
                    && baseType != CFFI_K_TYPE_UNICHAR_ARRAY
                    && baseType != CFFI_K_TYPE_BYTE_ARRAY));
#else
    CFFI_ASSERT((flags & CFFI_F_ATTR_BYREF)
                || (CffiTypeIsNotArray(&typeAttrsP->dataType)
                    && baseType != CFFI_K_TYPE_CHAR_ARRAY
                    && baseType != CFFI_K_TYPE_UNICHAR_ARRAY
                    && baseType != CFFI_K_TYPE_STRUCT
                    && baseType != CFFI_K_TYPE_BYTE_ARRAY));
#endif

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
#ifdef _WIN32
    case CFFI_K_TYPE_WINSTRING:
#endif
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
#ifdef _WIN32
            case CFFI_K_TYPE_WINSTRING: PUSHARG(CffiStoreArgPointer, ptr); break;
#endif
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
                valuesP = Tclh_LifoAlloc(&ipCtxP->memlifo,
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
                Tclh_LifoUSizeT nCopy =
                    argP->arraySize * typeAttrsP->dataType.baseTypeSize;
                valuesP = Tclh_LifoAlloc(&ipCtxP->memlifo, nCopy);
                memset(valuesP, 0, nCopy);
            }
            argP->value.u.ptr = valuesP;
            /* BYREF but really a pointer to array so STOREARGBYVAL, not STOREARGBYREF */
            STOREARGBYVAL(CffiStoreArgPointer, ptr);
        }
        break;
    case CFFI_K_TYPE_STRUCT:
        if (argP->arraySize < 0) {
            /* Single struct */
            void *structValueP;
            if (flags & CFFI_F_ATTR_NULLIFEMPTY) {
                Tcl_Size dict_size;
                CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
                CFFI_ASSERT(flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT));
                CFFI_ASSERT(valueObj);

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
            structValueP = Tclh_LifoAlloc(&ipCtxP->memlifo,
                                        typeAttrsP->dataType.u.structP->size);
            if (flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT)) {
                CHECK(CffiStructFromObj(ipCtxP,
                                        typeAttrsP->dataType.u.structP,
                                        valueObj, 0,
                                        structValueP, &ipCtxP->memlifo));
            }
            if (flags & CFFI_F_ATTR_BYREF) {
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
                Tclh_LifoAlloc(&ipCtxP->memlifo, argP->arraySize * struct_size);
            if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
                /* IN or INOUT */
                Tcl_Obj **valueObjList;
                Tcl_Size i, nvalues;
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
        if (argP->arraySize < 0) {
            if (flags & CFFI_F_ATTR_OUT)
                argP->value.u.ptr = NULL; /* Being paranoid */
            else {
                CHECK(
                    CffiPointerFromObj(ipCtxP, typeAttrsP, valueObj, &argP->value.u.ptr));
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
            valueArray = Tclh_LifoAlloc(&ipCtxP->memlifo,
                                      argP->arraySize * sizeof(void *));
            if (flags & CFFI_F_ATTR_OUT)
                argP->value.u.ptr = valueArray;
            else {
                Tcl_Obj **valueObjList;
                Tcl_Size i, nvalues;
                CHECK(Tcl_ListObjGetElements(
                    ip, valueObj, &nvalues, &valueObjList));
                if (nvalues > argP->arraySize)
                    nvalues = argP->arraySize;
                for (i = 0; i < nvalues; ++i) {
                    CHECK(CffiPointerFromObj(
                        ipCtxP, typeAttrsP, valueObjList[i], &valueArray[i]));
                }
                CFFI_ASSERT(i == nvalues);
                for ( ; i < argP->arraySize; ++i)
                    valueArray[i] = NULL;/* Fill additional elements */
                argP->value.u.ptr = valueArray;
                if (flags & (CFFI_F_ATTR_DISPOSE | CFFI_F_ATTR_DISPOSEONSUCCESS)) {
                    /* Save pointers to dispose after call completion */
                    void **savedValueArray;
                    savedValueArray = Tclh_LifoAlloc(
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
        if (len) {
            argP->value.u.ptr = Tclh_LifoAlloc(&ipCtxP->memlifo, len);
            memmove(argP->value.u.ptr, p, len);
        }
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
        argP->value.u.ptr = Tclh_LifoAlloc(&ipCtxP->memlifo, argP->arraySize);
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
 * was an *out* or *inout* parameter and storing it in the output Tcl variable
 * named by the varNameObj field of the argument descriptor if it is not
 * NULL. If it is NULL, as is the case with the CFFI_F_ATTR_RETVAL attribute
 * set, it is returned in the location pointed by resultObjP. Note the
 * reference count of the returned Tcl_Obj is not incremented before returning.
 *
 * Note no cleanup of argument storage is done.
 *
 * In the case of arrays, if the output array was specified as zero size,
 * the output variable is not modified.
 *
 * Parameters:
 * callP - the call context
 * arg_index - index of argument to do post processing
 * resultObjP - location to store result if the varNameObj field is NULL.
 *
 * Returns:
 * *TCL_OK* on success,
 * *TCL_ERROR* on error with message stored in the interpreter.
 */
static
CffiResult
CffiArgPostProcess(CffiCall *callP, int arg_index, Tcl_Obj **resultObjP)
{
    CffiInterpCtx *ipCtxP = callP->fnP->ipCtxP;
    Tcl_Interp *ip        = ipCtxP->interp;
    CffiArgument *argP    = &callP->argsP[arg_index];
    const CffiTypeAndAttrs *typeAttrsP = argP->typeAttrsP;
    CffiValue *valueP;
    Tcl_Obj *valueObj;
    CffiProto *protoP;
    int arraySize;
    int ret;

    CFFI_ASSERT(argP->flags & CFFI_F_ARG_INITIALIZED);

    if (typeAttrsP->flags & CFFI_F_ATTR_IN)
        return TCL_OK;

    CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_BYREF);

    protoP = callP->fnP->protoP;
    if (arg_index < protoP->nParams
        && protoP->params[arg_index].typeAttrs.dataType.arraySize == 0) {
        /*
         * This is a dynamically sized array. Pick up the array size from
         * the parameter that contains the size. Note that argP->arraySize
         * would have been set to that size at call time but the called
         * function might have modified it to reflect actual length of
         * data returned.
         */
        int sizeParamIndex = protoP->params[arg_index].arraySizeParamIndex;

        ret = CffiGetCountFromValue(
            ip,
            protoP->params[sizeParamIndex].typeAttrs.dataType.baseType,
            &callP->argsP[sizeParamIndex].value,
            &arraySize);
        if (ret != TCL_OK
            || arraySize > argP->arraySize) {
            /* Sanity check. Should not happen but ... */
            arraySize = argP->arraySize;
        }
    }
    else {
        arraySize = argP->arraySize;
    }
    if (arraySize == 0) {
        /* Output array is zero-size */
        valueObj = Tcl_NewObj();
        ret = TCL_OK;
        goto store_value;
    }

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
#ifdef _WIN32
    case CFFI_K_TYPE_WINSTRING:
#endif
        /* Scalars stored at valueP, arrays of scalars at valueP->u.ptr */
        if (arraySize < 0)
            ret = CffiNativeValueToObj(
                ipCtxP, typeAttrsP, valueP, 0, argP->arraySize, &valueObj);
        else
            ret = CffiNativeValueToObj(
                ipCtxP, typeAttrsP, valueP->u.ptr, 0, arraySize, &valueObj);
        break;

    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
        ret = CffiNativeValueToObj(
            ipCtxP, typeAttrsP, valueP->u.ptr, 0, arraySize, &valueObj);
        break;

    case CFFI_K_TYPE_STRUCT:
        ret = CffiNativeValueToObj(
            ipCtxP, typeAttrsP, valueP->u.ptr, 0, arraySize, &valueObj);
        break;

    case CFFI_K_TYPE_BINARY:
    default:
        /* Should not happen */
        ret = Tclh_ErrorInvalidValue(ip, NULL, "Unsupported argument type");
        break;
    }

    if (ret != TCL_OK)
        return ret;

    /* Expect no errors beyond this point */

    /*
     * Check if value is to be converted to a enum name. This is
     * inefficient since we have to convert back from a Tcl_Obj to an
     * integer but currently the required context is not being passed down
     * to the lower level functions that extract scalar values.
     * TBD - is this still true? Does typeAttrsP not contain enum values?
     */
    if ((typeAttrsP->flags & (CFFI_F_ATTR_ENUM|CFFI_F_ATTR_BITMASK))
        && typeAttrsP->dataType.u.tagNameObj) {
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
            Tcl_Size nelems;
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

store_value:
    CFFI_ASSERT(valueObj);

    if (typeAttrsP->flags & CFFI_F_ATTR_RETVAL) {
        CFFI_ASSERT(resultObjP);
        *resultObjP = valueObj;
    }
    else {
        Tcl_Obj *varObjP = argP->varNameObj;
        CFFI_ASSERT(varObjP);

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
    }
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
#ifdef _WIN32
    case CFFI_K_TYPE_WINSTRING:
#endif
    case CFFI_K_TYPE_POINTER: callP->retValueP = &callP->retValue.u.ptr; break;
    case CFFI_K_TYPE_STRUCT:
        callP->retValueP = Tclh_LifoAlloc(
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
    Tcl_Size nOnErrorObjs;
    Tcl_Size nEvalObjs;
    int i;

    CFFI_ASSERT(protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_ONERROR);
    CFFI_ASSERT(protoP->returnType.typeAttrs.parseModeSpecificObj);
    CHECK(Tcl_ListObjGetElements(
        ip, protoP->returnType.typeAttrs.parseModeSpecificObj, &nOnErrorObjs, &onErrorObjs));

    nEvalObjs = nOnErrorObjs + 1; /* We will tack on the call dictionary */
    evalObjs =
        Tclh_LifoAlloc(&ipCtxP->memlifo, nEvalObjs * sizeof(Tcl_Obj *));

    /*
     * Construct the dictionary of arguments that were input to the function.
     * Built as a list for efficiency as the handler may or may not access it.
     * Note: only fixed params passed, not varargs as latter are not named
     */
    callInfoObj = Tcl_NewListObj(0, NULL);
    inputArgsObj = Tcl_NewListObj(protoP->nParams, NULL);
    outputArgsObj = Tcl_NewListObj(protoP->nParams, NULL);
    for (i = 0; i < protoP->nParams; ++i) {
        CffiTypeAndAttrs *typeAttrsP = argsP[i].typeAttrsP;
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

/* Function: CffiGetCountFromValue
 * Extracts an count from a CffiValue.
 *
 * An error is returned if the value is negative or too large.
 *
 * Parameters:
 * ip - interpreter for error messages. May be NULL.
 * valueP - Value from which count is to be extracted
 * countP - Location to store the count
 *
 * Returns:
 * TCL_OK on success, TCL_ERROR on failure with error message in interpreter.
 */
static CffiResult
CffiGetCountFromValue(Tcl_Interp *ip,
                      CffiBaseType valueType,
                      const CffiValue *valueP,
                      int *countP)
{
    long long count;
    switch (valueType) {
    case CFFI_K_TYPE_SCHAR: count = valueP->u.schar; break;
    case CFFI_K_TYPE_UCHAR: count = valueP->u.uchar; break;
    case CFFI_K_TYPE_SHORT: count = valueP->u.sshort; break;
    case CFFI_K_TYPE_USHORT: count = valueP->u.ushort; break;
    case CFFI_K_TYPE_INT: count = valueP->u.sint; break;
    case CFFI_K_TYPE_UINT: count = valueP->u.uint; break;
    case CFFI_K_TYPE_LONG: count = valueP->u.slong; break;
    case CFFI_K_TYPE_ULONG: count = valueP->u.ulong; break;
    case CFFI_K_TYPE_LONGLONG: count = valueP->u.slonglong; break;
    case CFFI_K_TYPE_ULONGLONG: count = valueP->u.ulonglong; break;
    default:
        return Tclh_ErrorWrongType(
            ip, NULL, "Wrong type for dynamic array count value.");
    }

    if (count < 0 || count > INT_MAX) {
        return Tclh_ErrorGeneric(
            ip,
            NULL,
            "Array size must be a positive integer that fits into type int.");
    }

    *countP = (int)count;
    return TCL_OK;
}

/* Function: CffiFunctionSetupArgs
 * Prepares the call stack needed for a function call.
 *
 * Parameters:
 * callP - initialized call context.
 * nArgObjs - size of argObjs.
 * argObjs - function arguments passed by the script. If a parameter has
 *   the RETVAL attribute set, the corresponding element in this array
 *   will be NULL.
 * varArgTypesP - array of type descriptors for the varargs arguments
 *   May be NULL if no varargs.
 * The function resets the call context and sets up the arguments.
 *
 * As part of setting up the call stack, the function may allocate memory
 * from the context memlifo. Caller responsible for freeing.
 *
 * Returns:
 * TCL_OK on success with the call stack set up, TCL_ERROR on error with an
 * error message in the interpreter.
 */
static CffiResult
CffiFunctionSetupArgs(CffiCall *callP,
                      int nArgObjs,
                      Tcl_Obj *const *argObjs,
                      CffiTypeAndAttrs *varArgTypesP)
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
    callP->nArgs = nArgObjs;
    if (callP->nArgs == 0)
        return TCL_OK;
    argsP = (CffiArgument *)Tclh_LifoAlloc(
        &ipCtxP->memlifo, callP->nArgs * sizeof(CffiArgument));
    callP->argsP = argsP;
    for (i = 0; i < callP->nArgs; ++i)
        argsP[i].flags = 0; /* Mark as uninitialized */
#ifdef CFFI_USE_LIBFFI
    callP->argValuesPP = (void **)Tclh_LifoAlloc(
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
        CffiTypeAndAttrs *typeAttrsP;

        if (i < protoP->nParams) {
            /* Fixed param */
            typeAttrsP = &protoP->params[i].typeAttrs;
        }
        else {
            /* Vararg. */
            CFFI_ASSERT(varArgTypesP);
            typeAttrsP = &varArgTypesP[i - protoP->nParams];
        }

        if (CffiTypeIsVariableSizeArray(&typeAttrsP->dataType)) {
            /* Dynamic array. */
            need_pass2 = 1;
            continue;
        }
        argsP[i].typeAttrsP = typeAttrsP;

        /* Scalar or fixed size array. Type decl should have ensured size!=0 */
        argsP[i].arraySize = typeAttrsP->dataType.arraySize;
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
        int dynamicCountIndex;
        int actualCount;
        CffiTypeAndAttrs *typeAttrsP;
        if (i < protoP->nParams) {
            typeAttrsP = &protoP->params[i].typeAttrs;
        }
        else {
            typeAttrsP = &varArgTypesP[i - protoP->nParams];
        }

        if (! CffiTypeIsVariableSizeArray(&typeAttrsP->dataType)) {
            /* This arg already been parsed successfully. Just load it. */
            CFFI_ASSERT(argsP[i].flags & CFFI_F_ARG_INITIALIZED);
            CffiReloadArg(callP, &argsP[i], typeAttrsP);
            continue;
        }
        CFFI_ASSERT((argsP[i].flags & CFFI_F_ARG_INITIALIZED) == 0);

        if (i >= protoP->nParams) {
            Tclh_ErrorWrongType(ip,
                                NULL,
                                "Dynamically sized arrays not permitted for "
                                "varargs arguments.");
            goto cleanup_and_error;
        }

        /* Locate the parameter holding the dynamic count */
        dynamicCountIndex = protoP->params[i].arraySizeParamIndex;
        CFFI_ASSERT(dynamicCountIndex >= 0
                    && dynamicCountIndex < protoP->nParams);
        CFFI_ASSERT(argsP[dynamicCountIndex].flags & CFFI_F_ARG_INITIALIZED);

        if (CffiGetCountFromValue(
                ip,
                protoP->params[dynamicCountIndex].typeAttrs.dataType.baseType,
                &argsP[dynamicCountIndex].value,
                &actualCount)
            != TCL_OK)
            goto cleanup_and_error;


        argsP[i].typeAttrsP = typeAttrsP;
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
    Tcl_Obj *resultObj = NULL;
    Tcl_Obj **argObjs = NULL;
    Tcl_Obj * const *varArgObjs = NULL;
    CffiTypeAndAttrs *varArgTypesP = NULL;
    int nArgObjs;
    int nVarArgs;
    int nActualArgs;
    int varArgsInited = 0;
    int i;
    int argResultIndex; /* If >=0, index of output argument as function result */
    void *pointer;
    CffiCall callCtx;
    Tclh_LifoMark mark;
    CffiResult ret = TCL_OK;
    CffiResult fnCheckRet = TCL_OK; /* Whether function return check passed */
    Tcl_WideInt sysError;  /* Error retrieved from system */

    CFFI_ASSERT(ip == ipCtxP->interp);

    /* nArgObjs is supplied arguments. Remaining have to come from defaults */
    CFFI_ASSERT(objc >= objArgIndex);
    nArgObjs = objc - objArgIndex;

    /* TBD - check memory executable permissions */
    if ((uintptr_t) fnP->fnAddr < 0xffff)
        return Tclh_ErrorInvalidValue(ip, NULL, "Function pointer not in executable page.");

    mark = Tclh_LifoPushMark(&ipCtxP->memlifo);

    /* IMPORTANT - mark has to be popped even on errors before returning */

    /* Check number of arguments passed */
    if (protoP->flags & CFFI_F_PROTO_VARARGS) {
        /*
         * Varargs functions differ from fixed arg functions in that
         * - they do not permit default values for parameters so at least
         *   that many arguments must be present
         * - number of arguments may be more than number in prototype
         */
        int minNumArgs = protoP->nParams;
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_RETVAL)
            minNumArgs -= 1; /* One parameter is return val */
        if (nArgObjs < minNumArgs)
            goto numargs_error; /* More args than params */
        nVarArgs   = nArgObjs - minNumArgs;
        varArgObjs = nVarArgs ? (minNumArgs + objArgIndex + objv) : NULL;
    }
    else {
        /*
         * For normal functions, there may be fewer arguments as defaults
         * may be present (if not, error is caught when setting up args)
         * There should never me more args than formal parameters.
         */
        int maxNumArgs = protoP->nParams;
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_RETVAL)
            maxNumArgs -= 1; /* One param is the return val */
        if (nArgObjs > maxNumArgs)
            goto numargs_error; /* More args than params */
        nVarArgs   = 0;
        varArgObjs = NULL;
    }
    if (nVarArgs) {
        /* Need room for varargs type descriptors */
        varArgTypesP = (CffiTypeAndAttrs *)Tclh_LifoAlloc(
            &ipCtxP->memlifo, nVarArgs * sizeof(CffiTypeAndAttrs));
    }

#ifdef CFFI_USE_LIBFFI
    /* protoP->cifP is lazy-initialized */
    ret = CffiLibffiInitProtoCif(
        ipCtxP, protoP, nVarArgs, varArgObjs, varArgTypesP);
    if (ret != TCL_OK)
        goto pop_and_go;
#endif

    callCtx.fnP = fnP;
    callCtx.nArgs = 0;
    callCtx.argsP = NULL;
#ifdef CFFI_USE_LIBFFI
    callCtx.argValuesPP = NULL;
    callCtx.retValueP   = NULL;
#endif

    /*
     * Set up arguments if any. Remember
     * - for normal functions, there may be fewer arguments than params
     *   because some params may have defaults
     * - for varargs functions, there may be more arguments than params
     *   but never fewer (since defaults not allowed even for fixed params)
     * protoP->nParams is number of fixed params defined. Will be at least
     * 1 for varargs functions.
     * nArgObjs is number of arguments passed in to this function.
     * nVarArgs is number of vararg arguments passed in to this function.
     * For normal functions,
     *   protoP->nParams >= 0, protoP->nParams >= nArgObjs, nVarArgs == 0
     * For vararg functions,
     *   protoP->nParams >=1, protoP->nParams <= nArgObjs, nVarArgs >= 0
     * nActualArgs is calculated taking into account defaulted arguments
     * and varargs.
     */
    nActualArgs = nVarArgs + protoP->nParams;
    argResultIndex = -1;/* Presume function result returned as is */
    if (protoP->nParams) {
        int j;

        /*
         * Allocate space to hold all arguments.  Number of fixed parameters
         * plus vararg parameters. Note this is NOT same as nArgObjs due ti
         * presence of defaults.
         */
        argObjs = (Tcl_Obj **)Tclh_LifoAlloc(
            &ipCtxP->memlifo, nActualArgs * sizeof(Tcl_Obj *));

        /* First do the fixed arguments */
        for (i = 0, j = objArgIndex; i < protoP->nParams; ++i, ++j) {
            if (protoP->params[i].typeAttrs.flags & CFFI_F_ATTR_RETVAL) {
                /* This is a parameter to use as return value. No argument
                   is expected from the caller */
                CFFI_ASSERT(argResultIndex < 0); /* Checked at definition */
                argObjs[i]     = NULL;
                argResultIndex = i; /* Index of param to be used for func result */
                --j; /* Negate loop ++j so same argument used for next param */
            }
            else {
                if (j < objc) {
                    argObjs[i] = objv[j];
                }
                else {
                    /*
                     * No argument, must have a default - parseModeSpecificObj
                     * is used for both defaults and onerror.
                     */
                    if ((protoP->params[i].typeAttrs.parseModeSpecificObj
                         == NULL)
                        || (protoP->params[i].typeAttrs.flags
                            & CFFI_F_ATTR_ONERROR))
                        goto numargs_error;

                    argObjs[i] =
                        protoP->params[i].typeAttrs.parseModeSpecificObj;
                }
            }
        }
        /*
         * Now parse varargs arguments. At this point j is start of varargs
         * within objv[]. i is start of varargs within argObjs[].
         */
        if (nVarArgs) {
            /* Note below assert only holds for varargs functions */
            CFFI_ASSERT((i + nVarArgs) == nArgObjs);
            for (; i < nArgObjs; ++i, ++j) {
                CFFI_ASSERT(j < objc);
                Tcl_Obj **typeAndValueObj;
                Tcl_Size n;
                if (Tcl_ListObjGetElements(NULL, objv[j], &n, &typeAndValueObj) != TCL_OK
                || n != 2) {
                    /* Should not really happen since already checked in
                     * CffiLibInitProtoCif above */
                    Tclh_ErrorInvalidValue(
                        ip,
                        varArgObjs[i],
                        "A vararg must be a type and value pair.");
                    goto pop_and_error;
                }
                argObjs[i] = typeAndValueObj[1];
            }
        }

        /* Set up stack. This also does CffiResetCall so we don't need to */
        if (CffiFunctionSetupArgs(
                &callCtx, nActualArgs, argObjs, varArgTypesP)
            != TCL_OK)
            goto pop_and_error;
        /* callCtx.argsP will have been set up by above call */
        varArgsInited = 1;
        CFFI_ASSERT(callCtx.nArgs == nActualArgs);
    }
    else {
        /* Prepare for the call. */
        if (CffiResetCall(ip, &callCtx) != TCL_OK)
            goto pop_and_error;
    }

    /* Set up the return value */
    if (CffiReturnPrepare(&callCtx) != TCL_OK)
        goto pop_and_error;

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
        CffiValue cretval; /* Actual C function result */                      \
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_BYREF) {          \
            type_ *p = CffiCallPointerFunc(&callCtx);                          \
            if (p)                                                             \
                cretval.u.fld_ = *p;                                           \
            else {                                                             \
                fnCheckRet = Tclh_ErrorInvalidValue(                           \
                    ip, NULL, "Function returned NULL pointer");               \
                ret = TCL_ERROR;                                               \
                break; /* Skip rest of macro */                                \
            }                                                                  \
        }                                                                      \
        else                                                                   \
            cretval.u.fld_ = dcfn_(&callCtx);                                  \
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_REQUIREMENT_MASK) \
            fnCheckRet = CffiCheckNumeric(                                     \
                ip, &protoP->returnType.typeAttrs, &cretval, &sysError);       \
        CffiPointerArgsDispose(ipCtxP, callCtx.nArgs, callCtx.argsP, fnCheckRet);         \
        if (fnCheckRet == TCL_OK) {                                            \
            /* Wrap function return value unless an output argument is */      \
            /* to be returned as the function result */                        \
            if (argResultIndex < 0) {                                          \
                /* First try converting function result as enums, bitmasks */  \
                resultObj = CffiIntValueToObj(&protoP->returnType.typeAttrs,   \
                                              (Tcl_WideInt)cretval.u.fld_);    \
                /* If that did not work just use native value */               \
                if (resultObj == NULL)                                         \
                    resultObj = objfn_(cretval.u.fld_);                        \
            }                                                                  \
        }                                                                      \
        else {                                                                 \
            resultObj = objfn_(cretval.u.fld_);                                \
        }                                                                      \
    } while (0)

    switch (protoP->returnType.typeAttrs.dataType.baseType) {
    case CFFI_K_TYPE_VOID:
        CffiCallVoidFunc(&callCtx);
        CffiPointerArgsDispose(
            ipCtxP, callCtx.nArgs, callCtx.argsP, fnCheckRet);
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
#ifdef _WIN32
    case CFFI_K_TYPE_WINSTRING:
#endif
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
        CffiPointerArgsDispose(ipCtxP, callCtx.nArgs, callCtx.argsP, fnCheckRet);         \
        switch (protoP->returnType.typeAttrs.dataType.baseType) {
            case CFFI_K_TYPE_POINTER:
                ret = CffiPointerToObj(
                    ipCtxP, &protoP->returnType.typeAttrs, pointer, &resultObj);
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
#ifdef _WIN32
            case CFFI_K_TYPE_WINSTRING:
                if (pointer)
                    resultObj = Tclh_ObjFromWinChars(
                        ipCtxP->tclhCtxP, (WCHAR *)pointer, -1);
                else
                    resultObj = Tcl_NewObj();
                break;
#endif
            default:
                /* Just to keep gcc happy */
                CFFI_PANIC("UNEXPECTED BASE TYPE");
                break;
        }
        break;
    case CFFI_K_TYPE_STRUCT:
        if (protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_BYREF) {
            pointer = CffiCallPointerFunc(&callCtx);
            fnCheckRet = CffiCheckPointer(
                ip, &protoP->returnType.typeAttrs, pointer, &sysError);
            CffiPointerArgsDispose(ipCtxP, callCtx.nArgs, callCtx.argsP, fnCheckRet);
            if (pointer == NULL) {
                CffiStruct *structP =
                    protoP->returnType.typeAttrs.dataType.u.structP;
                if (fnCheckRet == TCL_OK) {
                    /* Null pointer but allowed. Construct a default value. */
                    pointer = Tclh_LifoAlloc(&ipCtxP->memlifo, structP->size);
                    ret     = CffiStructObjDefault(ipCtxP, structP, pointer);
                }
                else {
                    fnCheckRet = Tclh_ErrorInvalidValue(
                        ip, NULL, "Function returned NULL pointer.");
                    ret = TCL_ERROR;
                }
                if (ret != TCL_OK)
                    break;
            }
            CFFI_ASSERT(fnCheckRet == TCL_OK); /* Else pointer would be NULL */
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
        ret = CffiStructToObj(ipCtxP,
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
     * ret - TCL_OK/TCL_ERROR -> success/fail invoking function or processing
     *   its return value, e.g. string could not be encoded into Tcl's
     *   internal form
     * fnCheckRet - TCL_OK/TCL_ERROR -> call and conversion above succeeded
     *   but return value check annotations failed
     * resultObj - if ret is TCL_OK, holds wrapped value irrespective of
     *   value of fnCheckRet. Reference count should be 0.
     */

    /* resultObj must not be NULL unless ret is not TCL_OK or an arg value is to
     * be returned as the result */
    CFFI_ASSERT(resultObj || ret != TCL_OK || argResultIndex >= 0);
    /* resultObj must be NULL unless ret is TCL_OK */
    CFFI_ASSERT(resultObj == NULL || ret == TCL_OK);

    /*
     * Based on the above state, the following actions need to be taken
     * depending on the value of (ret, fnCheckRet)
     *
     * (TCL_OK, TCL_OK) - store out and inout parameters that are unmarked or
     * marked as storealways. Return resultObj as the command result unless
     * useArgResult specifies that an output argument is returned
     * as the result.
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

    if (resultObj)
        Tcl_IncrRefCount(resultObj);

    if (ret == TCL_OK) {
        /*
         * Store parameters based on function return conditions.
         * Errors storing parameters are ignored (what else to do?)
         * Note only fixed params considered, not varargs. For now that's
         * fine since varargs are currently never INOUT or OUT parameters.
         */
        for (i = 0; i < protoP->nParams; ++i) {
            /* Skip the index, if any, that is to be returned as function result */
            if (i != argResultIndex) {
                CffiAttrFlags flags = protoP->params[i].typeAttrs.flags;
                if ((flags & (CFFI_F_ATTR_INOUT | CFFI_F_ATTR_OUT))) {
                    if ((fnCheckRet == TCL_OK
                         && !(flags & CFFI_F_ATTR_STOREONERROR))
                        || (fnCheckRet != TCL_OK
                            && (flags & CFFI_F_ATTR_STOREONERROR))
                        || (flags & CFFI_F_ATTR_STOREALWAYS)) {
                        /* Parameter needs to be stored */
                        if (CffiArgPostProcess(&callCtx, i, NULL) != TCL_OK)
                            ret = TCL_ERROR;/* Only update ret on error! */
                    }
                }
            }
        }
    }
    /* Parameters stored away. Note ret might have changed to error */

    /* See if a parameter output value is to be returned as function result */
    if (ret == TCL_OK && fnCheckRet == TCL_OK && argResultIndex >= 0) {
        if (resultObj)
            Tclh_ObjClearPtr(&resultObj);
        ret = CffiArgPostProcess(&callCtx, argResultIndex, &resultObj);
        if (ret == TCL_OK && resultObj)
            Tcl_IncrRefCount(resultObj);
    }

    if (ret == TCL_OK) {
        CFFI_ASSERT(resultObj != NULL);
        if (fnCheckRet == TCL_OK) {
            Tcl_SetObjResult(ip, resultObj);
        }
        else {
            /* Call error handler if specified, otherwise default handler */
            if ((protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_ONERROR)
                && protoP->returnType.typeAttrs.parseModeSpecificObj) {
                ret = CffiCustomErrorHandler(ipCtxP,
                                             protoP,
                                             fnP->cmdNameObj,
                                             argObjs,
                                             callCtx.argsP,
                                             resultObj);
            }
            else {
                ret = CffiDefaultErrorHandler(
                    ip, &protoP->returnType.typeAttrs, resultObj, sysError);
            }
        }
    }
    Tclh_ObjClearPtr(&resultObj);

    CffiReturnCleanup(&callCtx);
    for (i = 0; i < callCtx.nArgs; ++i)
        CffiArgCleanup(&callCtx, i);

pop_and_go:
    /* Clean up the temporary type descriptors for varargs */
    if (varArgsInited && varArgTypesP) {
        for (i = 0; i < nVarArgs; ++i) {
            CffiTypeAndAttrsCleanup(&varArgTypesP[i]);
        }
    }

    Tclh_LifoPopMark(mark);
    return ret;

numargs_error:
    /* Do NOT jump here if args are still to be cleaned up */
    resultObj = Tcl_NewListObj(protoP->nParams + 2, NULL);
    Tcl_ListObjAppendElement(NULL, resultObj, Tcl_NewStringObj("Syntax:", -1));
    for (i = 0; i < objArgIndex; ++i)
        Tcl_ListObjAppendElement(NULL, resultObj, objv[i]);
    for (i = 0; i < protoP->nParams; ++i) {
        /* RETVAL params are "invisible" from caller's perspective */
        if (! (protoP->params[i].typeAttrs.flags & CFFI_F_ATTR_RETVAL))
            Tcl_ListObjAppendElement(
                NULL, resultObj, protoP->params[i].nameObj);
    }
    if (protoP->flags & CFFI_F_PROTO_VARARGS) {
        Tcl_ListObjAppendElement(NULL, resultObj, Tcl_NewStringObj("...", 3));
    }
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
 *    callMode - a calling convention
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
                      CffiABIProtocol abi)
{
    Tcl_Obj *fqnObj;
    CffiProto *protoP = NULL;
    CffiFunction *fnP = NULL;
    CffiResult ret;

    ret = CffiPrototypeParse(ipCtxP,
                             abi,
                             cmdNameObj,
                             returnTypeObj,
                             paramsObj,
                             &protoP);
    if (ret != TCL_OK) {
        Tcl_AppendResult(ip,
                         " Error defining function ",
                         Tcl_GetString(cmdNameObj),
                         ".",
                         NULL);
        return TCL_ERROR;
    }

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
    Tcl_Size nNames;         /* # elements in nameObjs */

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

