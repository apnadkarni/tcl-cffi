/*
 * Copyright (c) 2021-2023, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

static CffiResult
CffiHelpInterfaceCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *nameObj)
{
    Tcl_CmdInfo cmdInfo;
    Tcl_Obj *resultObj;
    Tcl_Size i;

    if (!Tcl_GetCommandInfo(ipCtxP->interp, Tcl_GetString(nameObj), &cmdInfo)
        || !cmdInfo.isNativeObjectProc
        || cmdInfo.objProc != CffiInterfaceInstanceCmd
        || cmdInfo.objClientData == NULL) {
        return Tclh_ErrorNotFound(ipCtxP->interp, "Cffi interface", nameObj, NULL);
    }

    CffiInterface *ifcP = (CffiInterface *)cmdInfo.objClientData;

    resultObj = Tcl_NewStringObj("Interface ", -1);
    Tcl_AppendObjToObj(resultObj, ifcP->nameObj);
    if (ifcP->baseIfcP) {
        Tcl_AppendStringsToObj(resultObj,
                               "\n  Inherits: ",
                               Tcl_GetString(ifcP->baseIfcP->nameObj),
                               NULL);
    }
    Tcl_AppendToObj(resultObj, "\n  Methods: ", -1);

    while (ifcP) {
        const char *sep = "";
        Tcl_AppendStringsToObj(
            resultObj, "\n    ", Tcl_GetString(ifcP->nameObj), ": ", NULL);
        for (i = ifcP->nInheritedMethods; i < ifcP->nMethods; ++i) {
            Tcl_AppendStringsToObj(resultObj,
                                   sep,
                                   Tcl_GetString(ifcP->vtable[i].methodNameObj),
                                   NULL);
            sep = " ";
        }
        ifcP = ifcP->baseIfcP;
    }
    Tcl_SetObjResult(ipCtxP->interp, resultObj);
    return TCL_OK;
}

static CffiResult
CffiHelpFunctionCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *fnNameObj)
{
    Tcl_CmdInfo cmdInfo;
    CffiProto *protoP;
    Tcl_Obj *resultObj;
    int i;

    if (!Tcl_GetCommandInfo(ipCtxP->interp, Tcl_GetString(fnNameObj), &cmdInfo)
        || !cmdInfo.isNativeObjectProc
        || (cmdInfo.objProc != CffiFunctionInstanceCmd
            && cmdInfo.objProc != CffiMethodInstanceCmd)
        || cmdInfo.objClientData == NULL) {
        return Tclh_ErrorNotFound(ipCtxP->interp, "Cffi command", fnNameObj, NULL);
    }
    if (cmdInfo.objProc == CffiFunctionInstanceCmd) {
        CffiFunction *fnP = (CffiFunction *)cmdInfo.objClientData;
        protoP = fnP->protoP;
    } else {
        CffiMethod *methodP = (CffiMethod *)cmdInfo.objClientData;
        if (methodP == NULL || methodP->ifcP == NULL
            || methodP->vtableSlot >= methodP->ifcP->nMethods) {
            Tcl_SetResult(ipCtxP->interp,
                          "Internal error: invalid method slot.",
                          TCL_STATIC);
            return TCL_ERROR;
        }
        protoP = methodP->ifcP->vtable[methodP->vtableSlot].protoP;
    }
    resultObj = Tcl_NewStringObj("Syntax: ", 8);
    Tcl_AppendObjToObj(resultObj, fnNameObj);

    int retvalIndex = -1;
    for (i = 0; i < protoP->nParams; ++i) {
        if (protoP->params[i].typeAttrs.flags & CFFI_F_ATTR_RETVAL) {
            retvalIndex = i;
            continue;
        }
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
    if (protoP->flags & CFFI_F_PROTO_VARARGS)
        Tcl_AppendStringsToObj(resultObj, " ?...?", NULL);

    /* The return type */
    if (retvalIndex >= 0) {
        Tcl_Obj *retvalObj =
            CffiTypeUnparse(&protoP->params[retvalIndex].typeAttrs.dataType);
        Tcl_AppendStringsToObj(
            resultObj, " -> ", Tcl_GetString(retvalObj), NULL);
        Tcl_DecrRefCount(retvalObj);
    } else {
        if (protoP->returnType.typeAttrs.dataType.baseType != CFFI_K_TYPE_VOID
            && !(protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_DISCARD)) {
            Tcl_Obj *rettypeObj =
                CffiTypeAndAttrsUnparse(&protoP->returnType.typeAttrs);
            Tcl_AppendStringsToObj(
                resultObj, " -> ", Tcl_GetString(rettypeObj), NULL);
            Tcl_DecrRefCount(rettypeObj);
        }
    }

    for (i = 0; i < protoP->nParams; ++i) {
        if (i == retvalIndex)
            continue;
        Tcl_Obj *typeObj = CffiTypeAndAttrsUnparse(&protoP->params[i].typeAttrs);
        Tcl_AppendStringsToObj(resultObj,
                               "\n  ",
                               Tcl_GetString(protoP->params[i].nameObj),
                               ": ",
                               Tcl_GetString(typeObj),
                               NULL);
        Tcl_DecrRefCount(typeObj);
    }

    Tcl_SetObjResult(ipCtxP->interp, resultObj);
    return TCL_OK;
}

static CffiResult
CffiHelpStructOrUnionCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *nameObj, CffiBaseType baseType)
{
    CffiStruct *structP;
    Tcl_Obj *resultObj;
    int i;

    CHECK(CffiStructResolve(
        ipCtxP->interp, Tcl_GetString(nameObj), baseType, &structP));
    resultObj = Tcl_NewStringObj(
        baseType == CFFI_K_TYPE_STRUCT ? "struct " : "union ", -1);
    Tcl_AppendObjToObj(resultObj, nameObj);
    for (i = 0; i < structP->nFields; ++i) {
        CffiField *fieldP = &structP->fields[i];
        Tcl_Obj *typeObj = CffiTypeAndAttrsUnparse(&fieldP->fieldType);
        Tcl_AppendStringsToObj(resultObj,
                               "\n  ",
                               Tcl_GetString(fieldP->nameObj),
                               ": ",
                               Tcl_GetString(typeObj),
                               NULL);
        Tcl_DecrRefCount(typeObj);
    }

    Tcl_SetObjResult(ipCtxP->interp, resultObj);
    return TCL_OK;
}

static CffiResult
CffiHelpUnionCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *nameObj)
{
    return CffiHelpStructOrUnionCmd(ipCtxP, nameObj, CFFI_K_TYPE_UNION);
}

static CffiResult
CffiHelpStructCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *nameObj)
{
    return CffiHelpStructOrUnionCmd(ipCtxP, nameObj, CFFI_K_TYPE_STRUCT);
}

static CffiResult
CffiHelpEnumCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *enumNameObj)
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Obj *mapObj;

    CHECK(CffiEnumGetMap(ipCtxP, enumNameObj, 0, &mapObj));

    Tcl_Obj *nameObj;
    Tcl_Obj *valueObj;
    int done;
    Tcl_DictSearch search;

    CHECK(Tcl_DictObjFirst(ip, mapObj, &search, &nameObj, &valueObj, &done));
    Tcl_Obj *resultObj;
    resultObj = Tcl_ObjPrintf("enum %s\n", Tcl_GetString(enumNameObj));
    while (!done) {
        Tcl_AppendStringsToObj(resultObj,
                               "  ",
                               Tcl_GetString(nameObj),
                               "\t",
                               Tcl_GetString(valueObj),
                               "\n",
                               NULL);
        Tcl_DictObjNext(&search, &nameObj, &valueObj, &done);
    }
    Tcl_DictObjDone(&search);
    Tcl_SetObjResult(ip, resultObj);
    return TCL_OK;
}

static CffiResult
CffiHelpAliasCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *nameObj)
{
    CffiTypeAndAttrs *typeAttrsP;

    CHECK(
        CffiAliasLookup(ipCtxP, Tcl_GetString(nameObj), 0, &typeAttrsP, NULL));

    Tcl_Obj *bodyObj = CffiTypeAndAttrsUnparse(typeAttrsP);
    Tcl_IncrRefCount(bodyObj);

    Tcl_Obj *resultObj;
    resultObj = Tcl_ObjPrintf(
        "alias %s\n  %s", Tcl_GetString(nameObj), Tcl_GetString(bodyObj));
    Tcl_DecrRefCount(bodyObj);
    Tcl_SetObjResult(ipCtxP->interp, resultObj);
    return TCL_OK;
}

static CffiResult
CffiHelpFunctionsCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *patObj)
{
    Tcl_Obj *resultObj;
    Tcl_Obj *commandsObj;
    Tcl_Obj *evalObjs[3];
    int nEvalObjs;
    Tcl_Obj **commandObjs;
    Tcl_Size nCommands;
    Tcl_Interp *ip = ipCtxP->interp;
    CffiResult ret;
    Tcl_Size i;

    evalObjs[0] = Tcl_NewStringObj("::info", 6);
    evalObjs[1] = Tcl_NewStringObj("commands", 8);
    if (patObj) {
        evalObjs[2] = patObj;
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
    enum cmds { ALIAS, ENUM, FUNCTION, FUNCTIONS, STRUCT, UNION };
    int cmdIndex;
    static Tclh_SubCommand subCommands[] = {
        {"alias", 0, 1, "NAME", CffiHelpAliasCmd},
        {"enum", 0, 1, "NAME", CffiHelpEnumCmd},
        {"function", 0, 1, "NAME", CffiHelpFunctionCmd},
        {"functions", 0, 1, "?PATTERN?", CffiHelpFunctionsCmd},
        {"interface", 0, 1, "NAME", CffiHelpInterfaceCmd},
        {"struct", 0, 1, "NAME", CffiHelpStructCmd},
	    {NULL}
    };

    /* Slightly convoluted logic since we want "help foo" to check all types */
    if (Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex) == TCL_OK) {
        switch (cmdIndex) {
        case FUNCTIONS:
            return subCommands[cmdIndex].cmdFn(ipCtxP,
                                               objc > 2 ? objv[2] : NULL);
        default:
            if (objc == 2) {
                return Tclh_ErrorNumArgs(
                    ip, 2, objv, subCommands[cmdIndex].message);
            }
            return subCommands[cmdIndex].cmdFn(ipCtxP, objv[2]);
        }
    }
    if (objc !=2)
        return TCL_ERROR;

    /* Try each kind in turn */
    if (CffiHelpFunctionCmd(ipCtxP, objv[1]) == TCL_OK
        || CffiHelpAliasCmd(ipCtxP, objv[1]) == TCL_OK
        || CffiHelpEnumCmd(ipCtxP, objv[1]) == TCL_OK
        || CffiHelpStructCmd(ipCtxP, objv[1]) == TCL_OK
        || CffiHelpUnionCmd(ipCtxP, objv[1]) == TCL_OK
        || CffiHelpInterfaceCmd(ipCtxP, objv[1]) == TCL_OK) {
        return TCL_OK;
    }

    return Tclh_ErrorNotFound(ip, "CFFI program element", objv[1], NULL);
}
