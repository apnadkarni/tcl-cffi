/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#define TCLH_SHORTNAMES
#include "tclCffiInt.h"


/* Function: CffiWrapperFunctionCmd
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
CffiWrapperFunctionCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP)
{
    CFFI_ASSERT(objc == 5);

    return CffiDefineOneFunctionFromLib(
        ip, ctxP, objv[2], objv[3], objv[4], CffiDefaultABI());
}

/* Function: CffiWrapperStdcallCmd
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
CffiWrapperStdcallCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP)
{
    CFFI_ASSERT(objc == 5);

    return CffiDefineOneFunctionFromLib(
        ip, ctxP, objv[2], objv[3], objv[4], CffiStdcallABI());
}


/* Function: CffiWrapperManyFunctionsCmd
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
CffiWrapperManyFunctionsCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP,
                    CffiABIProtocol callMode)
{
    Tcl_Obj **objs;
    Tcl_Size i, nobjs;
    int ret;
    Tcl_Obj *errorMessages = NULL;

    CFFI_ASSERT(objc == 3);

    CHECK(Tcl_ListObjGetElements(ip, objv[2], &nobjs, &objs));
    if (nobjs % 3) {
        return Tclh_ErrorInvalidValue(
            ip, objv[2], "Incomplete function definition list.");
    }

    for (i = 0; i < nobjs; i += 3) {
        ret = CffiDefineOneFunctionFromLib(
            ip, ctxP, objs[i], objs[i + 1], objs[i + 2], callMode);
        if (ret != TCL_OK) {
            if (errorMessages == NULL) {
                errorMessages = Tcl_NewStringObj("Errors:", -1);
            }
            Tcl_AppendStringsToObj(
                errorMessages, "\n", Tcl_GetString(Tcl_GetObjResult(ip)), NULL);
            Tcl_ResetResult(ip);
        }
    }
    if (errorMessages) {
        Tcl_SetObjResult(ip, errorMessages);
        return TCL_ERROR;
    }
    return TCL_OK;
}

/* Function: CffiWrapperFunctionsCmd
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
CffiWrapperFunctionsCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP)
{
    return CffiWrapperManyFunctionsCmd(ip, objc, objv, ctxP, CffiDefaultABI());
}


/* Function: CffiWrapperStdcallsCmd
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
CffiWrapperStdcallsCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    CffiLibCtx *ctxP)
{
    return CffiWrapperManyFunctionsCmd(
        ip, objc, objv, ctxP, CffiStdcallABI());
}

static CffiResult
CffiWrapperDestroyCmd(Tcl_Interp *ip,
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
CffiWrapperPathCmd(Tcl_Interp *ip,
                int objc,
                Tcl_Obj *const objv[],
                CffiLibCtx *ctxP)
{
    Tcl_SetObjResult(ip, CffiLibPath(ip, ctxP));
    return TCL_OK;
}

static CffiResult
CffiWrapperAddressOfCmd(Tcl_Interp *ip,
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
CffiWrapperInstanceCmd(ClientData cdata,
                   Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[])
{
    CffiLibCtx *ctxP = (CffiLibCtx *)cdata;
    static const Tclh_SubCommand subCommands[] = {
        {"addressof", 1, 1, "SYMBOL", CffiWrapperAddressOfCmd},
        {"destroy", 0, 0, "", CffiWrapperDestroyCmd},
        {"function", 3, 3, "NAME RETURNTYPE PARAMDEFS", CffiWrapperFunctionCmd},
        {"functions", 1, 1, "FUNCTIONLIST", CffiWrapperFunctionsCmd},
        {"path", 0, 0, "", CffiWrapperPathCmd},
        {"stdcall", 3, 3, "NAME RETURNTYPE PARAMDEFS", CffiWrapperStdcallCmd},
        {"stdcalls", 1, 1, "FUNCTIONLIST", CffiWrapperStdcallsCmd},
        {NULL}};
    int cmdIndex;

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ip, objc, objv, ctxP);
}

static void CffiWrapperInstanceDeleter(ClientData cdata)
{
    CffiLibCtxUnref((CffiLibCtx *)cdata);
}


/* Function: CffiWrapperObjCmd
 * Implements the script level *Wrapper* command.
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
CffiWrapperObjCmd(ClientData cdata,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[])
{
    CffiLibCtx *ctxP;
    Tcl_Obj *nameObj;
    Tcl_Obj *pathObj;
    static const Tclh_SubCommand subCommands[] = {
        {"new", 1, 1, "DLLPATH", NULL},
        {"create", 2, 2, "OBJNAME DLLPATH", NULL},
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
        nameObj = Tclh_NsQualifyNameObj(ip, objv[2], NULL);
        pathObj  = objc > 3 ? objv[3] : NULL;
    }
    Tcl_IncrRefCount(nameObj);

    ret = CffiLibLoad(ip, pathObj, &ctxP);
    if (ret == TCL_OK) {
        ctxP->ipCtxP = (CffiInterpCtx *)cdata;
        Tcl_CreateObjCommand(ip,
                             Tcl_GetString(nameObj),
                             CffiWrapperInstanceCmd,
                             ctxP,
                             CffiWrapperInstanceDeleter);
        Tcl_SetObjResult(ip, nameObj);
    }

    Tcl_DecrRefCount(nameObj);
    return ret;
}
