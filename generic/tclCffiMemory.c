/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

/* Function: CffiMemoryAddressFromObj
 * Calculates the memory address for an object in memory
 *
 * Parameters:
 * ip - interpreter. Must not be NULL if allowUnsafe is false
 * ptrObj - *Tcl_Obj* holding a pointer
 * allowUnsafe - if true, the pointer is not verified
 * addressP - location where address is to be stored
 *
 * In addition to errors from passed Tcl_Obj values not being the right type
 * etc.  the function will also return an error if the calculated address is NULL
 * or verification fails when requested.
 *
 * Returns:
 * *TCL_OK* or *TCL_ERROR*.
 */
static CffiResult
CffiMemoryAddressFromObj(CffiInterpCtx *ipCtxP,
                         Tcl_Obj *ptrObj,
                         int allowUnsafe,
                         void **addressP)
{
    void *pv;
    if (allowUnsafe)
        CHECK(Tclh_PointerUnwrap(ipCtxP->interp, ptrObj, &pv));
    else
        CHECK(Tclh_PointerObjVerify(
            ipCtxP->interp, ipCtxP->tclhCtxP, ptrObj, &pv, NULL));

    if (pv == NULL) {
        Tcl_SetResult(ipCtxP->interp, "Pointer is NULL.", TCL_STATIC);
        return TCL_ERROR;
    }
    *addressP = pv;
    return TCL_OK;
}

/* Function: CffiMemoryAllocateCmd
 * Implements the *memory allocate* script level command.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * objc - count of elements in objv[]. Should be 3 or 4 including command
 *        and subcommand.
 * objv - argument array.
 * flags - unused
 *
 * Allocated memory using *ckalloc* and returns a wrapped pointer to it. The
 * *objv[2]* argument contains the allocation size or a type specification.
 * Optionally, the *objv[3]* argument may be passed as the pointer type tag.
 *
 * Returns:
 * *TCL_OK* on success with wrapped pointer as interpreter result,
 * *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemoryAllocateCmd(CffiInterpCtx *ipCtxP,
                      int objc,
                      Tcl_Obj *const objv[],
                      CffiFlags flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Size size;
    CffiResult ret;
    Tcl_Obj *ptrObj;
    void *p;

    ret = Tclh_ObjToSizeInt(NULL, objv[2], &size);
    if (ret != TCL_OK) {
        ret = CffiTypeSizeForValue(ipCtxP, objv[2], NULL, NULL, &size);
    }
    if (ret != TCL_OK || size <= 0) {
        return Tclh_ErrorInvalidValue(
            ip,
            objv[2],
            "Allocation size argument must be a positive 32-bit integer or "
            "a fixed size type specification.");
    }

    p = Tcl_Alloc(size);

    ret = CffiMakePointerObj(ipCtxP, p, objc == 4 ? objv[3] : NULL, 0, &ptrObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, ptrObj);
    else
        ckfree(p);
    return ret;
}

/* Function: CffiMemoryNewCmd
 * Implements the *memory new* script level command.
 *
 * Parameters:
 * ip - interpreter
 * objc - count of elements in objv[].
 * objv - argument array.
 * flags - unused
 *
 * The command arguments given in objv[] are
 *
 * objv[2] - type declaration
 * objv[3] - initialization value
 * objv[4] - optional tag for returned pointer
 *
 * Allocated memory using *ckalloc*, initializes it and returns a wrapped
 * pointer to it.
 *
 * Returns:
 * *TCL_OK* on success with wrapped safe pointer as interpreter result,
 * *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemoryNewCmd(CffiInterpCtx *ipCtxP,
                 int objc,
                 Tcl_Obj *const objv[],
                 CffiFlags flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    CffiTypeAndAttrs typeAttrs;
    CffiResult ret;
    Tcl_Size size;
    void *pv;

    CFFI_ASSERT(objc >= 4);

    CHECK(CffiTypeSizeForValue(
        ipCtxP, objv[2], objv[3], &typeAttrs, &size));
    /* Note typeAttrs needs to be cleaned up beyond this point */

    pv = ckalloc(size);

    ret = CffiNativeValueFromObj(ipCtxP, &typeAttrs, 0, objv[3], 0, pv, 0, NULL);
    if (ret == TCL_OK) {
        Tcl_Obj *ptrObj;
        ret =
            CffiMakePointerObj(ipCtxP, pv, objc == 5 ? objv[4] : NULL, 0, &ptrObj);
        if (ret == TCL_OK)
            Tcl_SetObjResult(ip, ptrObj);
    }

    if (ret != TCL_OK)
        ckfree(pv);
    CffiTypeAndAttrsCleanup(&typeAttrs);
    return ret;
}

/* Function: CffiMemoryFreeCmd
 * Implements the *memory free* script level command.
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
CffiMemoryFreeCmd(CffiInterpCtx *ipCtxP,
                  int objc,
                  Tcl_Obj *const objv[],
                  CffiFlags flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    void *pv;
    CffiResult ret;

    CHECK(Tclh_PointerUnwrap(ip, objv[2], &pv));
    if (pv == NULL)
        return TCL_OK;
    ret = Tclh_PointerUnregister(ip, ipCtxP->tclhCtxP, pv, NULL);
    if (ret == TCL_OK)
        ckfree(pv);
    return ret;
}

/* Function: CffiMemoryFromBinaryCmd
 * Implements the *memory frombinary* script level command.
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
CffiMemoryFromBinaryCmd(CffiInterpCtx *ipCtxP,
                        int objc,
                        Tcl_Obj *const objv[],
                        CffiFlags flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    CffiResult ret;
    Tcl_Obj *ptrObj;
    unsigned char *bytes;
    Tcl_Obj *tagObj;
    void *p;
    Tcl_Size len;

    bytes = Tcl_GetByteArrayFromObj(objv[2], &len);
    p = ckalloc(len);
    memmove(p, bytes, len);
    if (objc == 4) {
        tagObj = CffiMakePointerTagFromObj(ipCtxP, objv[3]);
        Tcl_IncrRefCount(tagObj);
    }
    else
        tagObj = NULL;
    ret = Tclh_PointerRegister(ip, ipCtxP->tclhCtxP, p, tagObj, &ptrObj);
    if (tagObj)
        Tcl_DecrRefCount(tagObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, ptrObj);
    else
        ckfree(p);
    return ret;
}

/* Function: CffiMemoryToBinaryCmd
 * Implements the *memory tobinary* script level command.
 *
 * Parameters:
 * ip - interpreter
 * objc - count of elements in objv[]. Should be 4 or 5 including command
 *        and subcommand.
 * objv - argument array.
 * flags - if the CFFI_F_ALLOW_UNSAFE is set, the pointer is treated
 *        as unsafe and not checked for validity.
 *
 * Returns the *objv[3]* bytes of memory referenced by the wrapped pointer
 * in *objv[2]* as a *Tcl_Obj* byte array. The passed in pointer should be
 * registered unless the CFFI_F_ALLOW_UNSAFE of *flags* is set.
 * If the pointer is *NULL*, the returned byte array is empty.
 *
 * Returns:
 * *TCL_OK* on success with Tcl binary value as interpreter result,
 * *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemoryToBinaryCmd(CffiInterpCtx *ipCtxP,
                      int objc,
                      Tcl_Obj *const objv[],
                      CffiFlags flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    void *pv;
    unsigned int len;
    Tcl_WideInt off;

    if (flags & CFFI_F_ALLOW_UNSAFE)
        CHECK(Tclh_PointerUnwrap(ip, objv[2], &pv));
    else
        CHECK(Tclh_PointerObjVerify(
            ip, ipCtxP->tclhCtxP, objv[2], &pv, NULL));

    if (pv == NULL) {
        Tcl_SetResult(ip, "Pointer is NULL.", TCL_STATIC);
        return TCL_ERROR;
    }

    CHECK(Tclh_ObjToUInt(ip, objv[3], &len));

    if (objc < 5)
        off = 0;
    else {
        CHECK(Tclh_ObjToRangedInt(ip, objv[4], INT_MIN, INT_MAX, &off));
        if (off < 0 && !(flags & CFFI_F_ALLOW_UNSAFE)) {
            return Tclh_ErrorInvalidValue(
                ip,
                objv[4],
                "Negative offsets are not allowed for safe pointers.");
        }
    }

    Tcl_SetObjResult(ip, Tcl_NewByteArrayObj(off + (unsigned char *)pv, len));
    return TCL_OK;
}

/* Function: CffiMemoryFromStringCmd
 * Implements the *memory fromstring* script level command.
 *
 * Parameters:
 * ip - interpreter
 * objc - count of elements in objv[]. Should be 3 or 4 including command
 *        and subcommand.
 * objv - argument array.
 * flags - unused
 *
 * Allocates memory and copies the string in *objv[2]* in the encoding
 * specified by *objv[3]*. If *objv[3]* is not specified, the system
 * encoding is used.
 *
 * Returns:
 * *TCL_OK* on success with wrapped pointer to the allocated memory as
 * interpreter result, *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemoryFromStringCmd(CffiInterpCtx *ipCtxP,
                        int objc,
                        Tcl_Obj *const objv[],
                        CffiFlags flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Encoding encoding;
    Tcl_DString ds;
    CffiResult ret;
    Tcl_Obj *ptrObj;
    char *p;
    Tcl_Size len;

    if (objc < 4)
        encoding = NULL;
    else
        CHECK(CffiGetEncodingFromObj(ip, objv[3], &encoding));

    /* TODO - use new UtfDString API and check error */
    (void) Tcl_UtfToExternalDString(encoding, Tcl_GetString(objv[2]), -1, &ds);
    if (encoding)
        Tcl_FreeEncoding(encoding);
    len = Tcl_DStringLength(&ds);

    /*
     * The encoded string in ds may be terminated by 1-4 null bytes
     * depending on the encoding. We do not know which. Moreover,
     * Tcl_DStringLength does not tell us either. So we just tack on an
     * extra 4 null bytes (utf-32)
     */
    p   = ckalloc(len + 4);
    memmove(p, Tcl_DStringValue(&ds), len);
    for (int i = 0; i < 4; ++i) {
        p[len + i] = 0;
    }
    Tcl_DStringFree(&ds);

    ret = Tclh_PointerRegister(ip, ipCtxP->tclhCtxP, p, NULL, &ptrObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, ptrObj);
    else
        ckfree(p);
    return ret;
}


/* Function: CffiMemoryToStringCmd
 * Implements the *memory tostring* script level command.
 *
 * Parameters:
 * ip - interpreter
 * objc - count of elements in objv[]. Should be 3-5 including command
 *        and subcommand.
 * objv - argument array.
 * flags - if the CFFI_F_ALLOW_UNSAFE is set, the pointer is treated as unsafe and not
 *        checked for validity.
 *
 * The memory pointed to by *objv[2]* is treated as a null-terminated string
 * in the system encoding unless an encoding is specified in objv[3].
 * The returned string is the string at offset 0 or at the offset specified
 * by objv[3] or objv[4] depending on whether an encoding is present.
 *
 * The passed in pointer should be registered unless the CFFI_F_ALLOW_UNSAFE of *flags*
 * is set. A *NULL* pointer will raise an error.
 *
 * Returns:
 * *TCL_OK* on success with Tcl string value as interpreter result,
 * *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemoryToStringCmd(CffiInterpCtx *ipCtxP,
                      int objc,
                      Tcl_Obj *const objv[],
                      CffiFlags flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    void *pv;
    Tcl_Encoding encoding;
    Tcl_DString ds;
    Tcl_WideInt off;

    if (flags & CFFI_F_ALLOW_UNSAFE)
        CHECK(Tclh_PointerUnwrap(ip, objv[2], &pv));
    else
        CHECK(Tclh_PointerObjVerify(
            ip, ipCtxP->tclhCtxP, objv[2], &pv, NULL));

    if (pv == NULL) {
        Tcl_SetResult(ip, "Pointer is NULL.", TCL_STATIC);
        return TCL_ERROR;
    }

    if (objc == 3) {
        encoding = NULL;
        off      = 0;
    }
    else if (objc == 4) {
        /* Could be offset or an encoding */
        if (Tclh_ObjToRangedInt(NULL, objv[3], INT_MIN, INT_MAX, &off) == TCL_OK) {
            encoding = NULL;
        }
        else {
            /* Not numeric, check if an encoding */
            CHECK(CffiGetEncodingFromObj(ip, objv[3], &encoding));
            off = 0;
        }
    } else {
        /* objc == 5 */
        CHECK(Tclh_ObjToRangedInt(ip, objv[4], INT_MIN, INT_MAX, &off));
        CHECK(CffiGetEncodingFromObj(ip, objv[3], &encoding));
    }
    if (off < 0 && !(flags & CFFI_F_ALLOW_UNSAFE)) {
        if (encoding)
            Tcl_FreeEncoding(encoding);
        return Tclh_ErrorInvalidValue(
            ip, NULL, "Negative offsets are not allowed for safe pointers.");
    }

    /* TODO - use new UtfDString API and check error */
    (void) Tcl_ExternalToUtfDString(encoding, off + (char*)pv, -1, &ds);
    if (encoding)
        Tcl_FreeEncoding(encoding);

    /* Should optimize this by direct transfer of ds storage - See TclDStringToObj */
    Tcl_SetObjResult(
        ip,
        Tcl_NewStringObj(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)));
    Tcl_DStringFree(&ds);

    return TCL_OK;
}

/* Function: CffiMemoryGetCmd
 * Implements the *memory get* script level command.
 *
 * Parameters:
 * ip - interpreter
 * objc - count of elements in objv[]. Should be 4-5 including command
 *        and subcommand.
 * objv - argument array.
 * flags - if the CFFI_F_ALLOW_UNSAFE is set, the pointer is treated as unsafe and not
 *        checked for validity.
 *
 * The objv[2] element is the pointer to the memory address from where
 * a value of type objv[3] is to be retrieved. If objv[4]
 * is specified, it is the index into an array starting at objv[2] and
 * the value is retrieved from that corresponding location.
 *
 * The passed in pointer should be registered unless the CFFI_F_ALLOW_UNSAFE of *flags*
 * is set. A *NULL* pointer will raise an error.
 *
 * Returns:
 * *TCL_OK* on success with Tcl string value as interpreter result,
 * *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemoryGetCmd(CffiInterpCtx *ipCtxP,
                 int objc,
                 Tcl_Obj *const objv[],
                 CffiFlags flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    void *pv;
    unsigned int indx;
    CffiTypeAndAttrs typeAttrs;
    Tcl_Obj *resultObj;
    CffiResult ret;

    if (objc > 4) {
        CHECK(Tclh_ObjToUInt(ip, objv[4], &indx));
    }
    else
        indx = 0;

    CHECK(CffiMemoryAddressFromObj(ipCtxP, objv[2], flags & CFFI_F_ALLOW_UNSAFE, &pv));

    CHECK(CffiTypeAndAttrsParse(
        ipCtxP, objv[3], CFFI_F_TYPE_PARSE_FIELD, &typeAttrs));
    /* Note typeAttrs needs to be cleaned up beyond this point */

    ret = CffiNativeValueToObj(ipCtxP,
                               &typeAttrs,
                               pv,
                               indx,
                               typeAttrs.dataType.arraySize,
                               &resultObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ipCtxP->interp, resultObj);
    CffiTypeAndAttrsCleanup(&typeAttrs);
    return ret;
}

/* Function: CffiMemorySetCmd
 * Implements the *memory set* script level command.
 *
 * Parameters:
 * ip - interpreter
 * objc - count of elements in objv[]. Should be 5-6 including command
 *        and subcommand.
 * objv - argument array.
 * flags - if the CFFI_F_ALLOW_UNSAFE is set, the pointer is treated as unsafe and not
 *        checked for validity.
 *
 * The objv[2] element is the pointer to the location where the native
 * value of type objv[3] contained in objv[4] is to be stored. If objv[5]
 * is specified, it is the index into an array starting at objv[2] and
 * the value is stored in that corresponding location.
 *
 * The passed in pointer should be registered unless the CFFI_F_ALLOW_UNSAFE of *flags*
 * is set. A *NULL* pointer will raise an error.
 *
 * Returns:
 * *TCL_OK* on success with Tcl string value as interpreter result,
 * *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemorySetCmd(CffiInterpCtx *ipCtxP,
                 int objc,
                 Tcl_Obj *const objv[],
                 CffiFlags flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    void *pv;
    unsigned int indx;
    CffiTypeAndAttrs typeAttrs;
    CffiResult ret;

    if (objc > 5) {
        CHECK(Tclh_ObjToUInt(ip, objv[5], &indx));
    }
    else
        indx = 0;

    CHECK(CffiMemoryAddressFromObj(
        ipCtxP, objv[2], flags & CFFI_F_ALLOW_UNSAFE, &pv));

    CHECK(CffiTypeAndAttrsParse(
        ipCtxP, objv[3], CFFI_F_TYPE_PARSE_FIELD, &typeAttrs));
    /* Note typeAttrs needs to be cleaned up beyond this point */

    ret = CffiNativeValueFromObj(
        ipCtxP, &typeAttrs, 0, objv[4], CFFI_F_PRESERVE_ON_ERROR, pv, indx, NULL);

    CffiTypeAndAttrsCleanup(&typeAttrs);
    return ret;
}

/* Function: CffiMemoryFillCmd
 * Implements the *memory fill* script level command.
 *
 * Parameters:
 * ip - interpreter
 * objc - count of elements in objv[]. Should be 5 or 6 including command
 *        and subcommand.
 * objv - argument array.
 * flags - unused
 *
 * Returns:
 * *TCL_OK* on success with an empty string value as interpreter result,
 * *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemoryFillCmd(CffiInterpCtx *ipCtxP,
                  int objc,
                  Tcl_Obj *const objv[],
                  CffiFlags flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    void *pv;
    Tcl_WideInt len;
    Tcl_WideInt off;
    unsigned char val;

    if (flags & CFFI_F_ALLOW_UNSAFE) {
        CHECK(Tclh_PointerUnwrap(ip, objv[2], &pv));
    }
    else
        CHECK(Tclh_PointerObjVerify(
            ip, ipCtxP->tclhCtxP, objv[2], &pv, NULL));

    if (pv == NULL) {
        Tcl_SetResult(ip, "Pointer is NULL.", TCL_STATIC);
        return TCL_ERROR;
    }

    CHECK(Tclh_ObjToUChar(ip, objv[3], &val));
    CHECK(Tclh_ObjToRangedInt(ip, objv[4], 0, INT_MAX, &len));

    if (objc < 6)
        off = 0;
    else {
        CHECK(Tclh_ObjToRangedInt(ip, objv[5], INT_MIN, INT_MAX, &off));
        if (off < 0 && !(flags & CFFI_F_ALLOW_UNSAFE)) {
            return Tclh_ErrorInvalidValue(
                ip,
                objv[5],
                "Negative offsets are not allowed for safe pointers.");
        }
    }

    memset(off + (char *)pv, val, (int) len);
    return TCL_OK;
}

CffiResult
CffiMemoryObjCmd(ClientData cdata,
                 Tcl_Interp *ip,
                 int objc,
                 Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    /* The flags field CFFI_F_ALLOW_UNSAFE is set for unsafe pointer operation */
    static const Tclh_SubCommand subCommands[] = {
        {"allocate", 1, 2, "SIZE ?TAG?", CffiMemoryAllocateCmd, 0},
        {"free", 1, 1, "POINTER", CffiMemoryFreeCmd, 0},
        {"frombinary", 1, 2, "BINARY ?TAG?", CffiMemoryFromBinaryCmd, 0},
        {"fromstring", 1, 2, "STRING ?ENCODING?", CffiMemoryFromStringCmd, 0},
        {"new", 2, 3, "TYPE INITIALIZER ?TAG?", CffiMemoryNewCmd, 0},
        {"set", 3, 4, "POINTER TYPE VALUE ?INDEX?", CffiMemorySetCmd, 0},
        {"set!", 3, 4, "POINTER TYPE VALUE ?INDEX?", CffiMemorySetCmd, CFFI_F_ALLOW_UNSAFE},
        {"get", 2, 3, "POINTER TYPE ?INDEX?", CffiMemoryGetCmd, 0},
        {"get!", 2, 3, "POINTER TYPE ?INDEX?", CffiMemoryGetCmd, CFFI_F_ALLOW_UNSAFE},
        {"fill", 3, 4, "POINTER BYTEVALUE COUNT ?OFFSET?", CffiMemoryFillCmd, 0},
        {"fill!", 3, 4, "POINTER BYTEVALUE COUNT ?OFFSET?", CffiMemoryFillCmd, CFFI_F_ALLOW_UNSAFE},
        {"tobinary", 2, 3, "POINTER SIZE ?OFFSET?", CffiMemoryToBinaryCmd, 0},
        {"tobinary!", 2, 3, "POINTER SIZE ?OFFSET?", CffiMemoryToBinaryCmd, CFFI_F_ALLOW_UNSAFE},
        {"tostring", 1, 3, "POINTER ?ENCODING? ?OFFSET?", CffiMemoryToStringCmd, 0},
        {"tostring!", 1, 3, "POINTER ?ENCODING? ?OFFSET?", CffiMemoryToStringCmd, CFFI_F_ALLOW_UNSAFE},
        {NULL}
    };
    int cmdIndex;

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(
        ipCtxP, objc, objv, subCommands[cmdIndex].flags);
}
