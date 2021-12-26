/*
 * Copyright (c) 2021 Ashok P. Nadkarni
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
                int nptrs = argsP[i].actualCount;
                /* Note no error checks because the CffiFunctionSetup calls
                   above would have already done validation */
                if (nptrs <= 1) {
                    if (argsP[i].savedValue.u.ptr != NULL)
                        Tclh_PointerUnregister(
                            ip, argsP[i].savedValue.u.ptr, NULL);
                }
                else {
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
 * arg_index - index into argument array of call context
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
    CffiInterpCtx *ipCtxP = callP->fnP->vmCtxP->ipCtxP;
    CffiArgument *argP    = &callP->argsP[arg_index];
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_CHAR_ARRAY);
    CFFI_ASSERT(argP->actualCount > 0);

    valueP->u.ptr =
        MemLifoAlloc(&ipCtxP->memlifo, argP->actualCount);

    /* If input, we need to encode appropriately */
    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT))
        return CffiCharsFromObj(ipCtxP->interp,
                                typeAttrsP->dataType.u.tagObj,
                                valueObj,
                                valueP->u.ptr,
                                argP->actualCount);
    else {
        /*
         * To protect against the called C function leaving the output
         * argument unmodified on error which would result in our
         * processing garbage in CffiArgPostProcess, set null terminator.
         */
        *(char *)valueP->u.ptr = '\0';
        /* In case encoding employs double nulls */
        if (argP->actualCount > 1)
            *(1 + (char *)valueP->u.ptr) = '\0';
        return TCL_OK;
    }
}

/* Function: CffiArgPrepareUniChars
 * Initializes a CffiValue to pass a unichars argument.
 *
 * Parameters:
 * callP - the call context
 * arg_index - index into argument array of call context
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
    CffiInterpCtx *ipCtxP = callP->fnP->vmCtxP->ipCtxP;
    CffiArgument *argP    = &callP->argsP[arg_index];
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;

    valueP->u.ptr =
        MemLifoAlloc(&ipCtxP->memlifo, argP->actualCount * sizeof(Tcl_UniChar));
    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_UNICHAR_ARRAY);

    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
        return CffiUniCharsFromObj(
            ipCtxP->interp, valueObj, valueP->u.ptr, argP->actualCount);
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

/* Function: CffiArgPrepareInString
 * Initializes a CffiValue to pass a string input argument.
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - parameter type descriptor
 * valueObj - the Tcl_Obj containing string to pass. May be NULL for pure
 *   output parameters
 * valueP - location to store the argument
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with string in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
static CffiResult
CffiArgPrepareInString(Tcl_Interp *ip,
                     const CffiTypeAndAttrs *typeAttrsP,
                     Tcl_Obj *valueObj,
                     CffiValue *valueP)
{
    int len;
    const char *s;
    Tcl_Encoding encoding;

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_ASTRING);
    CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_IN);

    Tcl_DStringInit(&valueP->ancillary.ds);
    s = Tcl_GetStringFromObj(valueObj, &len);
    /*
     * Note this encoding step is required even for UTF8 since Tcl's
     * internal UTF8 is not exactly UTF8
     */
    if (typeAttrsP->dataType.u.tagObj) {
        CHECK(Tcl_GetEncodingFromObj(
            ip, typeAttrsP->dataType.u.tagObj, &encoding));
    }
    else
        encoding = NULL;
    /*
     * NOTE: UtfToExternalDString will append more than one null byte
     * for multibyte encodings if necessary. These are NOT included
     * in the DString length.
     */
    Tcl_UtfToExternalDString(encoding, s, len, &valueP->ancillary.ds);
    if (encoding)
        Tcl_FreeEncoding(encoding);
    return TCL_OK;
}

/* Function: CffiArgPrepareInUniString
 * Initializes a CffiValue to pass a input unistring argument.
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - parameter type descriptor
 * valueObj - the Tcl_Obj containing string to pass.
 * valueP - location to store the argument
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with string in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
static CffiResult
CffiArgPrepareInUniString(Tcl_Interp *ip,
                        const CffiTypeAndAttrs *typeAttrsP,
                        Tcl_Obj *valueObj,
                        CffiValue *valueP)
{
    int len = 0;
    const Tcl_UniChar *s;

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_UNISTRING);
    CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_IN);

    /* TBD - why do we need valueP->ds at all? Copy straight into memlifo */
    Tcl_DStringInit(&valueP->ancillary.ds);
    s = Tcl_GetUnicodeFromObj(valueObj, &len);
    /* Note we copy the terminating two-byte end of string null as well */
    Tcl_DStringAppend(&valueP->ancillary.ds, (char *)s, (len + 1) * sizeof(*s));
    return TCL_OK;
}


/* Function: CffiArgPrepareInBinary
 * Initializes a CffiValue to pass a input byte array argument.
 *
 * Parameters:
 * ip - interpreter
 * paramP - parameter descriptor
 * valueObj - the Tcl_Obj containing the byte array to pass. May be NULL
 *   for pure output parameters
 * valueP - location to store the argument
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with string in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
static CffiResult
CffiArgPrepareInBinary(Tcl_Interp *ip,
                     const CffiTypeAndAttrs *typeAttrsP,
                     Tcl_Obj *valueObj,
                     CffiValue *valueP)
{
    Tcl_Obj *objP;

    CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_IN);
    /*
     * Pure input but could still shimmer so dup it. Potential
     * future optimizations TBD:
     *  - dup only if shared (ref count > 1)
     *  - use memlifo to copy bytes instead of duping
     */
    objP = Tcl_DuplicateObj(valueObj);
    Tcl_IncrRefCount(objP);
    valueP->ancillary.baObj = objP;

    return TCL_OK;
}

/* Function: CffiArgPrepareBytes
 * Initializes a CffiValue to pass a bytes argument.
 *
 * Parameters:
 * callP - the call context
 * arg_index - index into argument array of call context
 * valueObj - the value passed in from script. May be NULL for pure output.
 * valueP - location of native value to store
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with pointer to bytes in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
static CffiResult
CffiArgPrepareBytes(CffiCall *callP,
                       int arg_index,
                       Tcl_Obj *valueObj,
                       CffiValue *valueP)
{
    CffiInterpCtx *ipCtxP = callP->fnP->vmCtxP->ipCtxP;
    CffiArgument *argP    = &callP->argsP[arg_index];
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;

    valueP->u.ptr = MemLifoAlloc(&ipCtxP->memlifo, argP->actualCount);
    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_BYTE_ARRAY);

    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
        /* NOTE: because of shimmering possibility, we need to copy */
        return CffiBytesFromObj(
            ipCtxP->interp, valueObj, valueP->u.ptr, argP->actualCount);
    }
    else
        return TCL_OK;
}

/* Function: CffiArgCleanup
 * Releases any resources stored within a CffiValue
 *
 * Parameters:
 * valueP - pointer to a value
 */
void CffiArgCleanup(CffiCall *callP, int arg_index)
{
    const CffiTypeAndAttrs *typeAttrsP;
    CffiValue *valueP;

    if ((callP->argsP[arg_index].flags & CFFI_F_ARG_INITIALIZED) == 0)
        return;

    typeAttrsP = &callP->fnP->protoP->params[arg_index].typeAttrs;
    valueP = &callP->argsP[arg_index].value;

    /*
     * IMPORTANT: the logic here must be consistent with CffiArgPostProcess
     * and CffiArgPrepare. Any changes here should be reflected there too.
     */

    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
        Tcl_DStringFree(&valueP->ancillary.ds);
        break;
    case CFFI_K_TYPE_BINARY:
        Tclh_ObjClearPtr(&valueP->ancillary.baObj);
        break;
    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
        /* valueP->u.{charP,unicharP,bytesP} points to memlifo storage */
        /* FALLTHRU */
    default:
        /* Scalars have no storage to deallocate */
        break;
    }
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
 * callP->argsP[arg_index].flags - sets CFFI_F_ARG_INITIALIZED
 * callP->argsP[arg_index].value - the native value.
 * callP->argsP[arg_index].savedValue - copy of above for some types only
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
    CffiInterpCtx *ipCtxP = callP->fnP->vmCtxP->ipCtxP;
    Tcl_Interp *ip        = ipCtxP->interp;
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;
    CffiArgument *argP    = &callP->argsP[arg_index];
    Tcl_Obj **varNameObjP = &argP->varNameObj;
    enum CffiBaseType baseType;
    int flags;
    char *p;

    /* Expected initialization to virgin state */
    CFFI_ASSERT(callP->argsP[arg_index].flags == 0);

    /*
     * IMPORTANT: the logic here must be consistent with CffiArgPostProcess
     * and CffiArgCleanup. Any changes here should be reflected there too.
     */

    flags = typeAttrsP->flags;
    baseType = typeAttrsP->dataType.baseType;
    if (typeAttrsP->dataType.count != 0) {
        switch (baseType) {
        case CFFI_K_TYPE_ASTRING:
        case CFFI_K_TYPE_UNISTRING:
        case CFFI_K_TYPE_BINARY:
            return ErrorInvalidValue(ip,
                                     NULL,
                                     "Arrays not supported for "
                                     "string/unistring/binary types.");
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
                 || (typeAttrsP->dataType.count == 0
                     && baseType != CFFI_K_TYPE_CHAR_ARRAY
                     && baseType != CFFI_K_TYPE_UNICHAR_ARRAY
                     && baseType != CFFI_K_TYPE_BYTE_ARRAY
                     && baseType != CFFI_K_TYPE_STRUCT));

    /*
     * Modulo bugs, even dynamic array sizes are supposed to be initialized
     * before calling this function on an argument.
     */
    CFFI_ASSERT(argP->actualCount >= 0);
    if (argP->actualCount < 0) {
        /* Should not happen. Just a failsafe. */
        return ErrorInvalidValue(
            ip, NULL, "Variable size array parameters not implemented.");
    }

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
#define STOREARG(storefn_, fld_)                        \
    do {                                                \
        storefn_(callP, arg_index, argP->value.u.fld_); \
    } while (0)
#define STOREARGBYREF(fld_)                                         \
    do {                                                            \
        CffiStoreArgPointer(callP, arg_index, &argP->value.u.fld_); \
    } while (0)
#endif /*  CFFI_USE_DYNCALL */

#ifdef CFFI_USE_LIBFFI
#define STOREARG(storefn_, fld_)                             \
    do {                                                     \
        callP->argValuesPP[arg_index] = &argP->value.u.fld_; \
    } while (0)
#define STOREARGBYREF(fld_)                                  \
    do {                                                     \
        argP->valueP                  = &argP->value.u.fld_; \
        callP->argValuesPP[arg_index] = &argP->valueP;       \
    } while (0)

#endif /* CFFI_USE_LIBFFI */

#define STORENUM(objfn_, dcfn_, fld_, type_)                                   \
    do {                                                                       \
        CFFI_ASSERT(argP->actualCount >= 0);                                   \
        if (argP->actualCount == 0) {                                          \
            if (flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT)) {                \
                if (flags & (CFFI_F_ATTR_BITMASK | CFFI_F_ATTR_ENUM)) {        \
                    Tcl_WideInt wide;                                          \
                    CHECK(CffiIntValueFromObj(                                 \
                        ipCtxP, typeAttrsP, valueObj, &wide));                 \
                    argP->value.u.fld_ = (type_)wide;                          \
                }                                                              \
                else {                                                         \
                    CHECK(objfn_(ip, valueObj, &argP->value.u.fld_));          \
                }                                                              \
            }                                                                  \
            if (flags & CFFI_F_ATTR_BYREF)                                     \
                STOREARGBYREF(fld_);                                           \
            else                                                               \
                STOREARG(dcfn_, fld_);                                         \
        }                                                                      \
        else {                                                                 \
            /* Array - has to be byref */                                      \
            type_ *valueArray;                                                 \
            CFFI_ASSERT(flags &CFFI_F_ATTR_BYREF);                             \
            valueArray =                                                       \
                MemLifoAlloc(&ipCtxP->memlifo,                                 \
                             argP->actualCount * sizeof(argP->value.u.fld_));  \
            if (flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT)) {                \
                Tcl_Obj **valueObjList;                                        \
                int i, nvalues;                                                \
                if (Tcl_ListObjGetElements(                                    \
                        ip, valueObj, &nvalues, &valueObjList)                 \
                    != TCL_OK)                                                 \
                    return TCL_ERROR;                                          \
                /* Note - if caller has specified too few values, it's ok */   \
                /* because perhaps the actual count is specified in another */ \
                /* parameter. If too many, only up to array size */            \
                if (nvalues > argP->actualCount)                               \
                    nvalues = argP->actualCount;                               \
                if (flags & (CFFI_F_ATTR_BITMASK | CFFI_F_ATTR_ENUM)) {        \
                    for (i = 0; i < nvalues; ++i) {                            \
                        Tcl_WideInt wide;                                      \
                        CHECK(CffiIntValueFromObj(                             \
                            ipCtxP, typeAttrsP, valueObjList[i], &wide));      \
                        valueArray[i] = (type_)wide;                           \
                    }                                                          \
                }                                                              \
                else {                                                         \
                    for (i = 0; i < nvalues; ++i) {                            \
                        CHECK(objfn_(ip, valueObjList[i], &valueArray[i]));    \
                    }                                                          \
                }                                                              \
                /* Fill additional elements with 0 */                          \
                for (; i < argP->actualCount; ++i)                             \
                    valueArray[i] = 0;                                         \
            }                                                                  \
            argP->value.u.ptr = valueArray;                                    \
            /* BYREF but really a pointer so STOREARG, not STOREARGBYREF */    \
            STOREARG(CffiStoreArgPointer, ptr);                                \
        }                                                                      \
    } while (0)

    switch (baseType) {
    case CFFI_K_TYPE_SCHAR: STORENUM(ObjToChar, CffiStoreArgSChar, schar, signed char); break;
    case CFFI_K_TYPE_UCHAR: STORENUM(ObjToUChar, CffiStoreArgUChar, uchar, unsigned char); break;
    case CFFI_K_TYPE_SHORT: STORENUM(ObjToShort, CffiStoreArgShort, sshort, short); break;
    case CFFI_K_TYPE_USHORT: STORENUM(ObjToUShort, CffiStoreArgUShort, ushort, unsigned short); break;
    case CFFI_K_TYPE_INT: STORENUM(ObjToInt, CffiStoreArgInt, sint, int); break;
    case CFFI_K_TYPE_UINT: STORENUM(ObjToUInt, CffiStoreArgUInt, uint, unsigned int); break;
    case CFFI_K_TYPE_LONG: STORENUM(ObjToLong, CffiStoreArgLong, slong, long); break;
    case CFFI_K_TYPE_ULONG: STORENUM(ObjToULong, CffiStoreArgULong, ulong, unsigned long); break;
    case CFFI_K_TYPE_LONGLONG: STORENUM(ObjToLongLong, CffiStoreArgLongLong, slonglong, signed long long); break;
        /*  TBD - unsigned 64-bit is broken */
    case CFFI_K_TYPE_ULONGLONG: STORENUM(ObjToULongLong, CffiStoreArgULongLong, ulonglong, unsigned long long); break;
    case CFFI_K_TYPE_FLOAT: STORENUM(ObjToFloat, CffiStoreArgFloat, flt, float); break;
    case CFFI_K_TYPE_DOUBLE: STORENUM(ObjToDouble, CffiStoreArgDouble, dbl, double); break;
    case CFFI_K_TYPE_STRUCT:
        CFFI_ASSERT(argP->actualCount >= 0);
        if (argP->actualCount == 0) {
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
                    STOREARG(CffiStoreArgPointer, ptr);
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
                                        valueObj,
                                        structValueP, &ipCtxP->memlifo));
            }
            if (typeAttrsP->flags & CFFI_F_ATTR_BYREF) {
                argP->value.u.ptr = structValueP;
                /* BYREF but really a pointer so STOREARG, not STOREARGBYREF */
                STOREARG(CffiStoreArgPointer, ptr);
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
            valueArray =
                MemLifoAlloc(&ipCtxP->memlifo, argP->actualCount * struct_size);
            if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
                /* IN or INOUT */
                Tcl_Obj **valueObjList;
                int i, nvalues;
                CHECK(Tcl_ListObjGetElements(
                    ip, valueObj, &nvalues, &valueObjList));
                if (nvalues > argP->actualCount)
                    nvalues = argP->actualCount;
                for (toP = valueArray, i = 0; i < nvalues;
                     toP += struct_size, ++i) {
                    CHECK(CffiStructFromObj(ipCtxP,
                                            typeAttrsP->dataType.u.structP,
                                            valueObjList[i],
                                            toP, &ipCtxP->memlifo));
                }
                if (i < argP->actualCount) {
                    /* Fill uninitialized with 0 */
                    memset(toP, 0, (argP->actualCount - i) * struct_size);
                }
            }
            argP->value.u.ptr = valueArray;
            STOREARG(CffiStoreArgPointer, ptr);
        }
        break;

    case CFFI_K_TYPE_POINTER:
        /* TBD - can we just use STORENUM here ? */
        CFFI_ASSERT(argP->actualCount >= 0);
        if (argP->actualCount == 0) {
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
                STOREARG(CffiStoreArgPointer, ptr);
        } else {
            void **valueArray;
            CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
            valueArray = MemLifoAlloc(&ipCtxP->memlifo,
                                      argP->actualCount * sizeof(void *));
            if (flags & CFFI_F_ATTR_OUT)
                argP->value.u.ptr = valueArray;
            else {
                Tcl_Obj **valueObjList;
                int i, nvalues;
                CHECK(Tcl_ListObjGetElements(
                    ip, valueObj, &nvalues, &valueObjList));
                if (nvalues > argP->actualCount)
                    nvalues = argP->actualCount;
                for (i = 0; i < nvalues; ++i) {
                    CHECK(CffiPointerFromObj(
                        ip, typeAttrsP, valueObjList[i], &valueArray[i]));
                }
                CFFI_ASSERT(i == nvalues);
                for ( ; i < argP->actualCount; ++i)
                    valueArray[i] = NULL;/* Fill additional elements */
                argP->value.u.ptr = valueArray;
                if (flags & (CFFI_F_ATTR_DISPOSE | CFFI_F_ATTR_DISPOSEONSUCCESS)) {
                    /* Save pointers to dispose after call completion */
                    void **savedValueArray;
                    savedValueArray = MemLifoAlloc(
                        &ipCtxP->memlifo, argP->actualCount * sizeof(void *));
                    for (i = 0; i < argP->actualCount; ++i)
                        savedValueArray[i] = valueArray[i];
                    argP->savedValue.u.ptr = savedValueArray;
                }
            }
            /* BYREF but really a pointer so STOREARG, not STOREARGBYREF */
            STOREARG(CffiStoreArgPointer, ptr);
        }
        break;

    case CFFI_K_TYPE_CHAR_ARRAY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        CHECK(CffiArgPrepareChars(callP, arg_index, valueObj, &argP->value));
        /* BYREF but really a pointer so STOREARG, not STOREARGBYREF */
        STOREARG(CffiStoreArgPointer, ptr);
        break;

    case CFFI_K_TYPE_ASTRING:
        CFFI_ASSERT(!(flags & CFFI_F_ATTR_INOUT));
        if (flags & CFFI_F_ATTR_OUT) {
            CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
            argP->value.u.ptr = NULL;
            STOREARGBYREF(ptr);
        }
        else {
            CFFI_ASSERT(flags & CFFI_F_ATTR_IN);
            CHECK(CffiArgPrepareInString(ip, typeAttrsP, valueObj, &argP->value));
            if ((flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_NULLIFEMPTY))
                    == (CFFI_F_ATTR_IN | CFFI_F_ATTR_NULLIFEMPTY)
                && Tcl_DStringLength(&argP->value.ancillary.ds) == 0) {
                argP->value.u.ptr = NULL; /* Null if empty */
            }
            else
                argP->value.u.ptr = Tcl_DStringValue(&argP->value.ancillary.ds);
            if (flags & CFFI_F_ATTR_BYREF)
                STOREARGBYREF(ptr);
            else
                STOREARG(CffiStoreArgPointer, ptr);
        }
        break;

    case CFFI_K_TYPE_UNISTRING:
        CFFI_ASSERT(!(flags & CFFI_F_ATTR_INOUT));
        /*
         * Code below is written to support OUT and BYREF (not INOUT) despite
         * the asserts above which are maintained in the type declaration
         * parsing code.
         */
        if (flags & CFFI_F_ATTR_OUT) {
            CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
            argP->value.u.ptr = NULL;
            STOREARGBYREF(ptr);
        }
        else {
            CFFI_ASSERT(flags & CFFI_F_ATTR_IN);
            CHECK(CffiArgPrepareInUniString(ip, typeAttrsP, valueObj, &argP->value));
            p = Tcl_DStringValue(&argP->value.ancillary.ds);
            if ((flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_NULLIFEMPTY))
                    == (CFFI_F_ATTR_IN | CFFI_F_ATTR_NULLIFEMPTY)
                && p[0] == 0 && p[1] == 0) {
                p = NULL; /* Null if empty */
            }
            argP->value.u.ptr = p;
            if (flags & CFFI_F_ATTR_BYREF)
                STOREARGBYREF(ptr);
            else
                STOREARG(CffiStoreArgPointer, ptr);
        }
        break;

    case CFFI_K_TYPE_UNICHAR_ARRAY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        CHECK(CffiArgPrepareUniChars(callP, arg_index, valueObj, &argP->value));
        /* BYREF but really a pointer so STOREARG, not STOREARGBYREF */
        STOREARG(CffiStoreArgPointer, ptr);
        break;

    case CFFI_K_TYPE_BINARY:
        CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_IN);
        CHECK(CffiArgPrepareInBinary(ip, typeAttrsP, valueObj, &argP->value));
        argP->value.u.ptr = Tcl_GetByteArrayFromObj(argP->value.ancillary.baObj, NULL);
        if (flags & CFFI_F_ATTR_BYREF)
            STOREARGBYREF(ptr);
        else
            STOREARG(CffiStoreArgPointer, ptr);
        break;

    case CFFI_K_TYPE_BYTE_ARRAY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        CHECK(CffiArgPrepareBytes(callP, arg_index, valueObj, &argP->value));
        /* BYREF but really a pointer so STOREARG, not STOREARGBYREF */
        STOREARG(CffiStoreArgPointer, ptr);
        break;

    default:
        return ErrorInvalidValue( ip, NULL, "Unsupported type.");
    }

    callP->argsP[arg_index].flags |= CFFI_F_ARG_INITIALIZED;

    return TCL_OK;

#undef STORENUM
#undef STOREARG
#undef STOREARGBYREF
}

/* Function: CffiArgPostProcess
 * Does the post processing of an argument after a call
 *
 * Post processing of the argument consists of checking if the parameter
 * was an *out* or *inout* parameter and storing it in the output Tcl variable.
 * Note no cleanup of argument storage is done.
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
    Tcl_Interp *ip = callP->fnP->vmCtxP->ipCtxP->interp;
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;
    CffiArgument *argP = &callP->argsP[arg_index];
    CffiValue *valueP = &argP->value;
    Tcl_Obj *varObjP  = argP->varNameObj;
    int ret;
    Tcl_Obj *valueObj;

    CFFI_ASSERT(callP->argsP[arg_index].flags & CFFI_F_ARG_INITIALIZED);
    CFFI_ASSERT(argP->actualCount >= 0); /* Array size known */

    if (typeAttrsP->flags & CFFI_F_ATTR_IN)
        return TCL_OK;

    /*
     * There are three categories:
     *  - scalar values are directly stored in *valueP
     *  - structs and arrays of scalars are stored in at the location
     *    pointed to by valueP->u.ptr
     *  - strings/unistring/binary are stored in *valueP but not as
     *    native C values.
     */
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
        /* Scalars stored at valueP, arrays of scalars at valueP->u.ptr */
        if (argP->actualCount == 0)
            ret = CffiNativeValueToObj(
                ip, typeAttrsP, valueP, argP->actualCount, &valueObj);
        else
            ret = CffiNativeValueToObj(
                ip, typeAttrsP, valueP->u.ptr, argP->actualCount, &valueObj);
        break;

    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
        ret = CffiNativeValueToObj(
            ip, typeAttrsP, valueP->u.ptr, argP->actualCount, &valueObj);
        break;

    case CFFI_K_TYPE_STRUCT:
        /* Arrays not supported for struct currently */
        ret = CffiNativeValueToObj(
            ip, typeAttrsP, valueP->u.ptr, argP->actualCount, &valueObj);
        break;

    case CFFI_K_TYPE_ASTRING:
        ret = CffiCharsToObj(
            ip, typeAttrsP, valueP->u.ptr, &valueObj);
        break;

    case CFFI_K_TYPE_UNISTRING:
        if (valueP->u.ptr)
            valueObj = Tcl_NewUnicodeObj((Tcl_UniChar *)valueP->u.ptr, -1);
        else
            valueObj = Tcl_NewObj();
        ret = TCL_OK;
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
    if ((typeAttrsP->flags & CFFI_F_ATTR_ENUM)
        && typeAttrsP->dataType.u.tagObj) {
        Tcl_WideInt wide;
        if (typeAttrsP->dataType.count == 0) {
            /* Note on error, keep the value */
            if (Tcl_GetWideIntFromObj(NULL, valueObj, &wide) == TCL_OK) {
                Tcl_DecrRefCount(valueObj);
                CffiEnumFindReverse(callP->fnP->vmCtxP->ipCtxP,
                                    typeAttrsP->dataType.u.tagObj,
                                    wide,
                                    0,
                                    &valueObj);
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
                Tcl_Obj *enumValueObj;
                int i;
                enumValuesObj = Tcl_NewListObj(nelems, NULL);
                for (i = 0; i < nelems; ++i) {
                    if (Tcl_GetWideIntFromObj(NULL, elemObjs[i], &wide)
                        != TCL_OK) {
                        break;
                    }
                    CffiEnumFindReverse(callP->fnP->vmCtxP->ipCtxP,
                                        typeAttrsP->dataType.u.tagObj,
                                        wide,
                                        0,
                                        &enumValueObj);
                    Tcl_ListObjAppendElement(NULL, enumValuesObj, enumValueObj);
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
    switch (callP->fnP->protoP->returnType.typeAttrs.dataType.baseType) {
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
            &callP->fnP->libCtxP->vmCtxP->ipCtxP->memlifo,
            callP->fnP->protoP->returnType.typeAttrs.dataType.u.structP->size);
        break;
    default:
        Tcl_SetResult(callP->fnP->vmCtxP->ipCtxP->interp,
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
     * storage is not allowed for a return type.
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
    ipCtxP = callP->fnP->vmCtxP->ipCtxP;
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
            CffiReloadArg(callP, &argsP[i], &protoP->params[i].typeAttrs);
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
    case CFFI_K_TYPE_ULONG: CALLFN(Tclh_ObjFromULong, CffiCallULongFunc, ulong);
    case CFFI_K_TYPE_LONGLONG: CALLFN(Tcl_NewWideIntObj, CffiCallLongLongFunc, slonglong);
    case CFFI_K_TYPE_ULONGLONG: CALLFN(Tclh_ObjFromULongLong, CffiCallULongLongFunc, ulonglong);
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
#ifdef CFFI_USE_DYNCALL
        /* Should not happen. If asserts disabled fallthru to error */
        CFFI_ASSERT(0);
#endif
#ifdef CFFI_USE_LIBFFI
        CffiLibffiCall(&callCtx);
        ret = CffiStructToObj(ip,
                              protoP->returnType.typeAttrs.dataType.u.structP,
                              callCtx.retValueP,
                              &resultObj);
        break;
#endif
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
#ifdef CFFI_USE_LIBFFI
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
                                 libCtxP->vmCtxP,
                                 libCtxP,
                                 fn,
                                 cmdNameObj,
                                 returnTypeObj,
                                 paramsObj,
                                 callMode);
}

