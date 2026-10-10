/*
 * Copyright (c) 2021-2023, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

static CffiResult
CffiLookupInterfaceCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *nameObj)
{
    Tcl_CmdInfo cmdInfo;

    if (!Tcl_GetCommandInfo(ipCtxP->interp, Tcl_GetString(nameObj), &cmdInfo)
        || !cmdInfo.isNativeObjectProc
        || cmdInfo.objProc != CffiInterfaceInstanceCmd
        || cmdInfo.objClientData == NULL) {
        return Tclh_ErrorNotFound(ipCtxP->interp, "Cffi interface", nameObj, NULL);
    }

    CffiInterface *ifcP = (CffiInterface *)cmdInfo.objClientData;

    Tcl_Obj *resultObjs[8];
    Tcl_Obj *nameKeyObj = Tcl_NewStringObj("Name", 4);
    Tcl_Obj *docKeyObj  = Tcl_NewStringObj("Doc", 3);
    Tcl_IncrRefCount(docKeyObj); /* Because may not be used */

    resultObjs[0]       = Tcl_NewStringObj("Type", 4);
    resultObjs[1]       = Tcl_NewStringObj("interface", 9);
    resultObjs[2]       = nameKeyObj;
    resultObjs[3]       = ifcP->nameObj;
    resultObjs[4]       = Tcl_NewStringObj("Methods", 7);
    resultObjs[5]       = Tcl_NewListObj(0, NULL);
    while (ifcP) {
        Tcl_Obj *ifcObjs[2];
        ifcObjs[0] = ifcP->nameObj;
        ifcObjs[1] = Tcl_NewListObj(0, NULL);
        for (Tcl_Size i = ifcP->nInheritedMethods; i < ifcP->nMethods; ++i) {
            Tcl_Obj *methodObjs[4];
            int methodObjCount = 2;
            methodObjs[0] = nameKeyObj;
            methodObjs[1] = ifcP->vtable[i].methodNameObj;
	    if (ifcP->vtable->docObj) {
                methodObjs[2] = docKeyObj;
                methodObjs[3] = ifcP->vtable->docObj;
                methodObjCount = 4;
            }
            Tcl_ListObjAppendElement(
                NULL, ifcObjs[1], Tcl_NewListObj(methodObjCount, methodObjs));
        }
        Tcl_ListObjAppendElement(
            NULL, resultObjs[5], Tcl_NewListObj(2, ifcObjs));
        ifcP = ifcP->baseIfcP;
    }
    int resultObjCount = 6;
    if (ifcP->baseIfcP) {
        resultObjs[resultObjCount++] = Tcl_NewStringObj("Superclass", 10);
        resultObjs[resultObjCount++] = ifcP->baseIfcP->nameObj;
    }
    Tcl_DecrRefCount(docKeyObj);
    Tcl_SetObjResult(ipCtxP->interp,
                     Tcl_NewListObj(resultObjCount, resultObjs));
    return TCL_OK;
}

static CffiResult
CffiLookupFunctionCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *fnNameObj)
{
    Tcl_CmdInfo cmdInfo;
    CffiProto *protoP;
    int i;
    const char *typeP;
    Tcl_Obj *docObj = NULL;

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
        typeP  = "function";
        docObj = fnP->docObj;
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
        docObj = methodP->ifcP->vtable[methodP->vtableSlot].docObj;
        typeP  = "method";
    }

    /*
     * Actually is a dict but create as a list. More convenient than having to
     * dealing with reference counts
     */
    Tcl_Obj *resultObjs[10];
    int resultObjCount;
    Tcl_Obj *nameKeyObj    = Tcl_NewStringObj("Name", 4);
    Tcl_Obj *typeKeyObj    = Tcl_NewStringObj("Type", 4);
    Tcl_Obj *defaultKeyObj = Tcl_NewStringObj("Default", 7);
    /* This object may possibly be never added below */
    Tcl_IncrRefCount(defaultKeyObj);

    resultObjs[0] = typeKeyObj;
    resultObjs[1] = Tcl_NewStringObj(typeP, -1);
    resultObjs[2] = nameKeyObj;
    resultObjs[3] = fnNameObj;

    Tcl_Obj *paramsObj = Tcl_NewListObj(0, NULL);

    int retvalIndex = -1;
    for (i = 0; i < protoP->nParams; ++i) {
        if (protoP->params[i].typeAttrs.flags & CFFI_F_ATTR_RETVAL) {
            retvalIndex = i;
            continue;
        }
        Tcl_Obj *params[6];

        params[0] = nameKeyObj;
        params[1] = protoP->params[i].nameObj;
        params[2] = typeKeyObj;
        params[3] = CffiTypeAndAttrsUnparse(&protoP->params[i].typeAttrs);

        Tcl_Size elemCount = 4;
        if (protoP->params[i].typeAttrs.parseModeSpecificObj) {
            params[4] = defaultKeyObj;
            params[5] = protoP->params[i].typeAttrs.parseModeSpecificObj;
            elemCount = 6;
        }

        Tcl_ListObjAppendElement(
            NULL, paramsObj, Tcl_NewListObj(elemCount, params));
    }
    Tcl_DecrRefCount(defaultKeyObj);
    defaultKeyObj = NULL;

    if (protoP->flags & CFFI_F_PROTO_VARARGS) {
        Tcl_Obj *params[2];
        params[0] = nameKeyObj;
        params[1] = Tcl_NewStringObj("args", 4);
        Tcl_ListObjAppendElement(NULL, paramsObj, Tcl_NewListObj(2, params));
    }

    resultObjs[4] = Tcl_NewStringObj("Params", 6);
    resultObjs[5] = paramsObj;
    resultObjCount = 6;

    Tcl_Obj *typeObj = NULL;
    if (retvalIndex >= 0) {
        typeObj =
            CffiTypeUnparse(&protoP->params[retvalIndex].typeAttrs.dataType);
    }
    else if (protoP->returnType.typeAttrs.dataType.baseType != CFFI_K_TYPE_VOID
             && !(protoP->returnType.typeAttrs.flags & CFFI_F_ATTR_DISCARD)) {
        typeObj = CffiTypeAndAttrsUnparse(&protoP->returnType.typeAttrs);
    }
    if (typeObj) {
        resultObjs[resultObjCount++] = Tcl_NewStringObj("Return", 6);
        resultObjs[resultObjCount++]  = typeObj;
    }

    if (docObj) {
        resultObjs[resultObjCount++] = Tcl_NewStringObj("Doc", 3);
        resultObjs[resultObjCount++]  = docObj;
    }
    CFFI_ASSERT(resultObjCount <= sizeof(resultObjs) / sizeof(resultObjs[0]));
    Tcl_SetObjResult(ipCtxP->interp,
                     Tcl_NewListObj(resultObjCount, resultObjs));
    return TCL_OK;
}

static CffiResult
CffiLookupStructOrUnionCmd(CffiInterpCtx *ipCtxP,
                           Tcl_Obj *nameObj,
                           CffiBaseType baseType)
{
    CffiStruct *structP;

    CHECK(CffiStructResolve(
        ipCtxP->interp, Tcl_GetString(nameObj), baseType, &structP));

    Tcl_Obj *nameKeyObj    = Tcl_NewStringObj("Name", 4);
    Tcl_Obj *typeKeyObj    = Tcl_NewStringObj("Type", 4);
    Tcl_Obj *resultObjs[6];

    resultObjs[0] = nameKeyObj;
    resultObjs[1] = nameObj;
    resultObjs[2] = typeKeyObj;
    resultObjs[3] = Tcl_NewStringObj(
        baseType == CFFI_K_TYPE_STRUCT ? "struct " : "union ", -1);

    resultObjs[4] = Tcl_NewStringObj("Fields", 6);
    resultObjs[5] = Tcl_NewListObj(structP->nFields, NULL);
    for (int i = 0; i < structP->nFields; ++i) {
        CffiField *fieldP = &structP->fields[i];
        Tcl_Obj *fieldObjs[4];
        fieldObjs[0] = nameKeyObj;
        fieldObjs[1] = fieldP->nameObj;
        fieldObjs[2] = typeKeyObj;
        fieldObjs[3] = CffiTypeAndAttrsUnparse(&fieldP->fieldType);
        Tcl_ListObjAppendElement(
            NULL, resultObjs[5], Tcl_NewListObj(4, fieldObjs));
    }

    Tcl_SetObjResult(ipCtxP->interp, Tcl_NewListObj(6, resultObjs));
    return TCL_OK;
}

static CffiResult
CffiLookupUnionCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *nameObj)
{
    return CffiLookupStructOrUnionCmd(ipCtxP, nameObj, CFFI_K_TYPE_UNION);
}

static CffiResult
CffiLookupStructCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *nameObj)
{
    return CffiLookupStructOrUnionCmd(ipCtxP, nameObj, CFFI_K_TYPE_STRUCT);
}

static CffiResult
CffiLookupEnumCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *enumNameObj)
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Obj *mapObj;

    CHECK(CffiEnumGetMap(ipCtxP, enumNameObj, 0, &mapObj));

    Tcl_Obj *nameObj;
    Tcl_Obj *valueObj;
    int done;
    Tcl_DictSearch search;

    CHECK(Tcl_DictObjFirst(ip, mapObj, &search, &nameObj, &valueObj, &done));
    Tcl_Obj *resultObjs[6];
    resultObjs[0] = Tcl_NewStringObj("Name", 4);
    resultObjs[1] = enumNameObj;
    resultObjs[2] = Tcl_NewStringObj("Type", 4);
    resultObjs[3] = Tcl_NewStringObj("enum", 4);
    resultObjs[4] = Tcl_NewStringObj("Members", 7);
    resultObjs[5] = Tcl_NewListObj(0, NULL);
    while (!done) {
        Tcl_ListObjAppendElement(NULL, resultObjs[5], nameObj);
        Tcl_ListObjAppendElement(NULL, resultObjs[5], valueObj);
        Tcl_DictObjNext(&search, &nameObj, &valueObj, &done);
    }
    Tcl_DictObjDone(&search);
    Tcl_SetObjResult(ip, Tcl_NewListObj(6, resultObjs));
    return TCL_OK;
}

static CffiResult
CffiLookupAliasCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *nameObj)
{
    CffiTypeAndAttrs *typeAttrsP;

    CHECK(
        CffiAliasLookup(ipCtxP, Tcl_GetString(nameObj), 0, &typeAttrsP, NULL));

    Tcl_Obj *resultObjs[6];
    resultObjs[0] = Tcl_NewStringObj("Name", 4);
    resultObjs[1] = nameObj;
    resultObjs[2] = Tcl_NewStringObj("Type", 4);
    resultObjs[3] = Tcl_NewStringObj("alias", 5);
    resultObjs[4] = Tcl_NewStringObj("Alias", 7);
    resultObjs[5] = CffiTypeAndAttrsUnparse(typeAttrsP);
    Tcl_SetObjResult(ipCtxP->interp, Tcl_NewListObj(6, resultObjs));
    return TCL_OK;
}

static CffiResult
CffiLookupFunctionsCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *patObj)
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
CffiLookupObjCmd(ClientData cdata,
               Tcl_Interp *ip,
               int objc,
               Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    enum cmds { ALIAS, ENUM, FUNCTION, FUNCTIONS, STRUCT, UNION };
    int cmdIndex;
    static Tclh_SubCommand subCommands[] = {
        {"alias", 0, 1, "NAME", CffiLookupAliasCmd},
        {"enum", 0, 1, "NAME", CffiLookupEnumCmd},
        {"function", 0, 1, "NAME", CffiLookupFunctionCmd},
        {"functions", 0, 1, "?PATTERN?", CffiLookupFunctionsCmd},
        {"interface", 0, 1, "NAME", CffiLookupInterfaceCmd},
        {"struct", 0, 1, "NAME", CffiLookupStructCmd},
        {"union", 0, 1, "NAME", CffiLookupUnionCmd},
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
    if (CffiLookupFunctionCmd(ipCtxP, objv[1]) == TCL_OK
        || CffiLookupAliasCmd(ipCtxP, objv[1]) == TCL_OK
        || CffiLookupEnumCmd(ipCtxP, objv[1]) == TCL_OK
        || CffiLookupStructCmd(ipCtxP, objv[1]) == TCL_OK
        || CffiLookupUnionCmd(ipCtxP, objv[1]) == TCL_OK
        || CffiLookupInterfaceCmd(ipCtxP, objv[1]) == TCL_OK) {
        return TCL_OK;
    }

    return Tclh_ErrorNotFound(ip, "CFFI program element", objv[1], NULL);
}
