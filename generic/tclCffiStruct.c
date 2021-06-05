/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"
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

CffiResult
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

