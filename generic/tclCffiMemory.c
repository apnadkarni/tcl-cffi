/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"


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


CffiResult
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

