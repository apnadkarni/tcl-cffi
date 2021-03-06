/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

static CffiResult
CffiHelpFunctionCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_CmdInfo cmdInfo;
    CffiFunction *fnP;
    CffiProto *protoP;
    Tcl_Obj *resultObj;
    int i;

    CFFI_ASSERT(objc == 3);
    if (!Tcl_GetCommandInfo(ipCtxP->interp, Tcl_GetString(objv[2]), &cmdInfo)
        || !cmdInfo.isNativeObjectProc
        || cmdInfo.objProc != CffiFunctionInstanceCmd
	|| cmdInfo.objClientData == NULL) {
        return Tclh_ErrorNotFound(ipCtxP->interp, "Cffi command", objv[2], NULL);
    }
    fnP = (CffiFunction *)cmdInfo.objClientData;
    protoP = fnP->protoP;
    resultObj = Tcl_NewStringObj("Syntax: ", 8);
    Tcl_AppendObjToObj(resultObj, objv[2]);
    for (i = 0; i < protoP->nParams; ++i) {
	if (protoP->params[i].typeAttrs.parseModeSpecificObj)
            Tcl_AppendStringsToObj(resultObj,
                                   " ?",
                                   Tcl_GetString(protoP->params[i].nameObj),
                                   "?",
                                   NULL);
        else
            Tcl_AppendStringsToObj(
                resultObj, " ", Tcl_GetString(protoP->params[i].nameObj), NULL);
    }

    Tcl_SetObjResult(ipCtxP->interp, resultObj);
    return TCL_OK;
}

static CffiResult
CffiHelpFunctionsCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *resultObj;
    Tcl_Obj *commandsObj;
    Tcl_Obj *evalObjs[3];
    int nEvalObjs;
    Tcl_Obj **commandObjs;
    int nCommands;
    Tcl_Interp *ip = ipCtxP->interp;
    CffiResult ret;
    int i;

    CFFI_ASSERT(objc == 2 || objc == 3);

    evalObjs[0] = Tcl_NewStringObj("::info", 6);
    evalObjs[1] = Tcl_NewStringObj("commands", 8);
    if (objc == 3) {
        evalObjs[2] = objv[2];
        nEvalObjs = 3;
    }
    else {
        nEvalObjs = 2;
    }

    for (i = 0; i < nEvalObjs; ++i)
        Tcl_IncrRefCount(evalObjs[i]);
    ret = Tcl_EvalObjv(ip, nEvalObjs, evalObjs, TCL_EVAL_DIRECT);
    if (ret == TCL_OK) {
        /* Duping instead of incrref protects against list shimmering */
        commandsObj = Tcl_DuplicateObj(Tcl_GetObjResult(ip));
        Tcl_ResetResult(ip);

        ret = Tcl_ListObjGetElements(ip, commandsObj, &nCommands, &commandObjs);
	if (ret == TCL_OK) {
            resultObj = Tcl_NewListObj(0, NULL);
            for (i = 0; i < nCommands; ++i) {
                Tcl_CmdInfo cmdInfo;
                if (Tcl_GetCommandInfo(
                        ip, Tcl_GetString(commandObjs[i]), &cmdInfo)
                    && cmdInfo.isNativeObjectProc
                    && cmdInfo.objProc == CffiFunctionInstanceCmd
                    && cmdInfo.objClientData != NULL) {
                    Tcl_ListObjAppendElement(NULL, resultObj, commandObjs[i]);
                }
            }
            Tcl_SetObjResult(ipCtxP->interp, resultObj);
        }
        Tcl_DecrRefCount(commandsObj);
    }
    for (i = 0; i < nEvalObjs; ++i)
        Tcl_DecrRefCount(evalObjs[i]);
    return ret;
}


CffiResult
CffiHelpObjCmd(ClientData cdata,
               Tcl_Interp *ip,
               int objc,
               Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    int cmdIndex;
    static Tclh_SubCommand subCommands[] = {
        {"function", 1, 1, "FUNCTION", CffiHelpFunctionCmd},
        {"functions", 0, 1, "?PATTERN?", CffiHelpFunctionsCmd},
	{NULL}};

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ipCtxP, objc, objv);
}
