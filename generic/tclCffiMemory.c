/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"


/* Function: CffiMemoryAllocateCmd
 * Implements the *memory allocate* script level command.
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
CffiMemoryAllocateCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[], int flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_WideInt size;
    CffiResult ret;
    Tcl_Obj *ptrObj;
    Tcl_Obj *tagObj;
    void *p;

    CHECK(Tclh_ObjToRangedInt(ip, objv[2], 1, INT_MAX, &size));
    p = ckalloc((int)size);
    if (objc == 4) {
        tagObj = CffiMakePointerTagFromObj(CffiScopeGet(ipCtxP, NULL), objv[3]);
        Tcl_IncrRefCount(tagObj);
    }
    else
        tagObj = NULL;

    ret = Tclh_PointerRegister(ip, p, tagObj, &ptrObj);
    if (tagObj)
        Tcl_DecrRefCount(tagObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, ptrObj);
    else
        ckfree(p);
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
CffiMemoryFreeCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[], int flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
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
CffiMemoryFromBinaryCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[], int flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    CffiResult ret;
    Tcl_Obj *ptrObj;
    unsigned char *bytes;
    Tcl_Obj *tagObj;
    void *p;
    int len;

    bytes = Tcl_GetByteArrayFromObj(objv[2], &len);
    p = ckalloc(len);
    memmove(p, bytes, len);
    if (objc == 4) {
        tagObj = CffiMakePointerTagFromObj(CffiScopeGet(ipCtxP, NULL), objv[3]);
        Tcl_IncrRefCount(tagObj);
    }
    else
        tagObj = NULL;
    ret = Tclh_PointerRegister(ip, p, tagObj, &ptrObj);
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
 * *TCL_OK* on success with Tcl binary value as interpreter result,
 * *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemoryToBinaryCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[], int flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    void *pv;
    unsigned int len;

    if (flags & 1)
        CHECK(Tclh_PointerUnwrap(ip, objv[2], &pv, NULL));
    else
        CHECK(Tclh_PointerObjVerify(ip, objv[2], &pv, NULL));

    if (pv == NULL) {
        Tcl_SetResult(ip, "Pointer is NULL.", TCL_STATIC);
        return TCL_ERROR;
    }

    CHECK(Tclh_ObjToUInt(ip, objv[3], &len));
    Tcl_SetObjResult(ip, Tcl_NewByteArrayObj(pv, len));
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
CffiMemoryFromStringCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[], int flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Encoding encoding;
    Tcl_DString ds;
    CffiResult ret;
    Tcl_Obj *ptrObj;
    char *p;
    int len;

    if (objc < 4)
        encoding = NULL;
    else
        CHECK(CffiGetEncodingFromObj(ip, objv[3], &encoding));

    Tcl_UtfToExternalDString(encoding, Tcl_GetString(objv[2]), -1, &ds);
    if (encoding)
        Tcl_FreeEncoding(encoding);
    len = Tcl_DStringLength(&ds);

    /*
     * The encoded string in ds may be terminated by one or two nulls
     * depending on the encoding. We do not know which. Moreover,
     * Tcl_DStringLength does not tell us either. So we just tack on an
     * extra two null bytes;
     */
    p   = ckalloc(len + 2);
    memmove(p, Tcl_DStringValue(&ds), len);
    p[len]     = 0;
    p[len + 1] = 0;
    Tcl_DStringFree(&ds);

    ret = Tclh_PointerRegister(ip, p, NULL, &ptrObj);
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
 * flags - if the low bit is set, the pointer is treated as unsafe and not
 *        checked for validity.
 *
 * The memory pointed to by *objv[2]* is treated as a null-terminated string
 * in the encoding specified by *objv[3]*. If *objv[3]* is not specified,
 * the system encoding is used.
 *
 * The passed in pointer should be registered unless the low bit of *flags*
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
                      int flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    void *pv;
    Tcl_Encoding encoding;
    Tcl_DString ds;

    if (flags & 1)
        CHECK(Tclh_PointerUnwrap(ip, objv[2], &pv, NULL));
    else
        CHECK(Tclh_PointerObjVerify(ip, objv[2], &pv, NULL));

    if (pv == NULL) {
        Tcl_SetResult(ip, "Pointer is NULL.", TCL_STATIC);
        return TCL_ERROR;
    }

    if (objc < 4)
        encoding = NULL;
    else
        CHECK(CffiGetEncodingFromObj(ip, objv[3], &encoding));

    Tcl_ExternalToUtfDString(encoding, pv, -1, &ds);
    if (encoding)
        Tcl_FreeEncoding(encoding);

    /* Should optimize this by direct transfer of ds storage - See TclDStringToObj */
    Tcl_SetObjResult(
        ip,
        Tcl_NewStringObj(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)));
    Tcl_DStringFree(&ds);

    return TCL_OK;
}

/* Function: CffiMemorySetCmd
 * Implements the *memory set* script level command.
 *
 * Parameters:
 * ip - interpreter
 * objc - count of elements in objv[]. Should be 5 including command
 *        and subcommand.
 * objv - argument array.
 * flags - unused
 *
 * Returns:
 * *TCL_OK* on success with an empty string value as interpreter result,
 * *TCL_ERROR* on failure with error message in interpreter.
 */
static CffiResult
CffiMemorySetCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[], int flags)
{
    Tcl_Interp *ip = ipCtxP->interp;
    void *pv;
    Tcl_WideInt len;
    unsigned char val;

    CHECK(Tclh_PointerObjVerify(ip, objv[2], &pv, NULL));
    CHECK(Tclh_ObjToUChar(ip, objv[3], &val));
    CHECK(Tclh_ObjToRangedInt(ip, objv[4], 0, INT_MAX, &len));

    memset(pv, val, (int) len);
    return TCL_OK;
}

CffiResult
CffiMemoryObjCmd(ClientData cdata,
                 Tcl_Interp *ip,
                 int objc,
                 Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    /* The flags field low bit is set for unsafe pointer operation */
    static const Tclh_SubCommand subCommands[] = {
        {"allocate", 1, 2, "SIZE ?TYPETAG?", CffiMemoryAllocateCmd, 0},
        {"free", 1, 1, "POINTER", CffiMemoryFreeCmd, 0},
        {"frombinary", 1, 2, "BINARY ?TYPETAG?", CffiMemoryFromBinaryCmd, 0},
        {"fromstring", 1, 2, "STRING ?ENCODING?", CffiMemoryFromStringCmd, 0},
        {"set", 3, 3, "POINTER BYTEVALUE COUNT", CffiMemorySetCmd, 0},
        {"tobinary", 2, 2, "POINTER SIZE", CffiMemoryToBinaryCmd, 0},
        {"tobinary!", 2, 2, "POINTER SIZE", CffiMemoryToBinaryCmd, 1},
        {"tostring", 1, 2, "POINTER ?ENCODING?", CffiMemoryToStringCmd, 0},
        {"tostring!", 1, 2, "POINTER ?ENCODING?", CffiMemoryToStringCmd, 1},
        {NULL}
    };
    int cmdIndex;

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(
        ipCtxP, objc, objv, subCommands[cmdIndex].flags);
}

