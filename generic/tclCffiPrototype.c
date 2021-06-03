/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"
#include <errno.h>
#include <sys/types.h>

/* Function: CffiParamCleanup
 * Releases resources associated with a CffiParam
 *
 * Parameters:
 * paramP - pointer to datum to clean up
 */
static void CffiParamCleanup(CffiParam *paramP)
{
    CffiTypeAndAttrsCleanup(&paramP->typeAttrs);
    if (paramP->nameObj)
        Tcl_DecrRefCount(paramP->nameObj);
}

static CffiProto *CffiProtoAllocate(int nparams)
{
    int         sz;
    CffiProto *protoP;

    sz = offsetof(CffiProto, params) + (nparams * sizeof(protoP->params[0]));
    protoP = ckalloc(sz);
    memset(protoP, 0, sz);
    return protoP;
}

/* Function: CffiProtoUnref
 * Cleans up resources associated with a prototype representation.
 *
 * Parameters:
 * protoP - pointer to a prototype representation. This must be in a
 *          consistent state.
 *
 * Returns:
 * Nothing. Any associated resources are released. Note that *protoP*
 * itself is not freed.
 */
void
CffiProtoUnref(CffiProto *protoP)
{
    if (protoP->nRefs <= 1) {
        int i;
        CffiParamCleanup(&protoP->returnType);
        for (i = 0; i < protoP->nParams; ++i) {
            CffiParamCleanup(&protoP->params[i]);
        }
        ckfree(protoP);
    }
    else
        protoP->nRefs -= 1;
}

/* Function: CffiProtoParse
 * Parses a function prototype definition returning an internal representation.
 *
 * Parameters:
 * ipCtxP - pointer to the context in which prototype is defined
 * fnNameObj - name of the function or prototype definition
 * returnTypeObj - return type of the function in format expected by
 *            CffiTypeAndAttrsParse
 * paramsObj - parameter definition list consisting of alternating parameter
 *            names and type definitions in the format expected by
 *            <CffiTypeAndAttrsParse>.
 * protoPP   - location to hold a pointer to the allocated internal prototype
 *            representation with reference count set to 0.
 *
 * Returns:
 * Returns *TCL_OK* on success with a pointer to the internal prototype
 * representation in *protoPP* and *TCL_ERROR* on failure with error message
 * in the interpreter.
 *
 */
CffiResult
CffiPrototypeParse(CffiInterpCtx *ipCtxP,
                   Tcl_Obj *fnNameObj,
                   Tcl_Obj *returnTypeObj,
                   Tcl_Obj *paramsObj,
                   CffiProto **protoPP)
{
    Tcl_Interp *ip = ipCtxP->interp;
    CffiProto *protoP;
    Tcl_Obj **objs;
    int nobjs;
    int i, j;

    CHECK(Tcl_ListObjGetElements(ip, paramsObj, &nobjs, &objs));
    if (nobjs & 1)
        return Tclh_ErrorInvalidValue(ip, paramsObj, "Parameter type missing.");

    /*
     * Parameter list is alternating name, type elements. Thus number of
     * parameters is nobjs/2
     */
    protoP = CffiProtoAllocate(nobjs / 2);
    if (CffiTypeAndAttrsParse(ipCtxP,
                              returnTypeObj,
                              CFFI_F_TYPE_PARSE_RETURN,
                              &protoP->returnType.typeAttrs)
        != TCL_OK) {
        CffiProtoUnref(protoP);
        return TCL_ERROR;
    }
    Tcl_IncrRefCount(fnNameObj);
    protoP->returnType.nameObj = fnNameObj;

    protoP->nParams = 0; /* Update as we go along  */
    for (i = 0, j = 0; i < nobjs; i += 2, ++j) {
        if (CffiTypeAndAttrsParse(ipCtxP,
                                  objs[i + 1],
                                  CFFI_F_TYPE_PARSE_PARAM,
                                  &protoP->params[j].typeAttrs)
            != TCL_OK) {
            CffiProtoUnref(protoP);
            return TCL_ERROR;
        }
        Tcl_IncrRefCount(objs[i]);
        protoP->params[j].nameObj = objs[i];
        protoP->nParams += 1; /* Update incrementally for error cleanup */
    }

    *protoPP       = protoP;

    return TCL_OK;
}

/* Function: CffiProtoGet
 * Checks for the existence of a prototype definition and returns its internal
 * form.
 *
 * Parameters:
 *   ipCtxP - Interpreter context
 *   protoNameObj - prototype name
 *   protoPP - location to store pointer to the CffiProto
 *
 * Note that the reference count of the returned structure is not incremented
 * Caller should do that if it is going to be held on to.
 *
 * Returns:
 * If a prototype of the specified name is found, a pointer to it
 * is returned otherwise *NULL*.
 */
CffiProto *
CffiProtoGet(CffiInterpCtx *ipCtxP,
             Tcl_Obj *protoNameObj)
{
    Tcl_HashEntry *heP;
    CffiProto *protoP;

    heP = Tcl_FindHashEntry(&ipCtxP->prototypes, protoNameObj);
    if (heP == NULL)
        return NULL;
    protoP = Tcl_GetHashValue(heP);
    return protoP;
}

/* Function: CffiPrototypeDefineCmd
 * Implements the core of the *prototype define* command.
 *
 * Parameters:
 *  ipCtxP - context
 *  ip - interpreter
 *  objc - size of *objv*
 *  objv - *prototype* *function|stdcall* NAME RETURN PARAMS
 *  callMode - dyncall calling convention
 *
 * Returns:
 * TCL_OK on success, TCL_ERROR on failure.
 */
static CffiResult
CffiPrototypeDefineCmd(CffiInterpCtx *ipCtxP,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[],
                  DCint callMode)
{
    Tcl_HashEntry *heP;
    CffiProto *protoP;
    Tcl_Obj *nameObj = objv[2];
    int new_entry;

    CFFI_ASSERT(objc == 5);

    /* Restrict name syntax - TBD - pull into common routine */
    CHECK(CffiNameSyntaxCheck(ip, nameObj));

    heP = Tcl_FindHashEntry(&ipCtxP->prototypes, nameObj);
    if (heP)
        return Tclh_ErrorExists(ip, "Prototype", nameObj, NULL);

    CHECK(CffiPrototypeParse(ipCtxP, nameObj, objv[3], objv[4], &protoP));
    if (callMode != DC_CALL_C_DEFAULT)
        protoP->returnType.typeAttrs.callMode = callMode;

    heP = Tcl_CreateHashEntry(&ipCtxP->prototypes, (char *) nameObj, &new_entry);
    if (! new_entry) {
        /* Should not happen because of existence check above but future proofing */
        CffiProto *oldP = Tcl_GetHashValue(heP);
        CffiProtoUnref(oldP);
    }
    CffiProtoRef(protoP);
    Tcl_SetHashValue(heP, protoP);
    return TCL_OK;
}

void
CffiPrototypesCleanup(Tcl_HashTable *protoTableP)
{
    Tcl_HashEntry *heP;
    Tcl_HashSearch hSearch;
    for (heP = Tcl_FirstHashEntry(protoTableP, &hSearch);
         heP != NULL; heP = Tcl_NextHashEntry(&hSearch)) {
        CffiProto *protoP = Tcl_GetHashValue(heP);
        CffiProtoUnref(protoP);
    }
    Tcl_DeleteHashTable(protoTableP);
}

CffiResult
CffiPrototypeObjCmd(ClientData cdata,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    int cmdIndex;
    static const Tclh_SubCommand subCommands[] = {
        {"function", 3, 3, "NAME RETURN PARAMS", CffiPrototypeDefineCmd},
        {"stdcall", 3, 3, "NAME RETURN PARAMS", CffiPrototypeDefineCmd},
        #if 0
        {"body", 1, 1, "ALIAS", CffiAliasBodyCmd},
        {"delete", 1, 1, "PATTERN", CffiAliasDeleteCmd},
        {"list", 0, 1, "?PATTERN?", CffiAliasListCmd},
        {"load", 1, 1, "ALIASSET", CffiAliasLoadCmd},
        #endif
        {NULL}
    };

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    if (cmdIndex == 0 || cmdIndex == 1) {
        return CffiPrototypeDefineCmd(ipCtxP,
                                      ip,
                                      objc,
                                      objv,
#if defined(_WIN32) && !defined(_WIN64)
                                      cmdIndex == 0 ? DC_CALL_C_DEFAULT
                                                    : DC_CALL_C_X86_WIN32_STD
#else
                                      DC_CALL_C_DEFAULT
#endif
        );
    }
    else
        return subCommands[cmdIndex].cmdFn(ipCtxP, ip, objc, objv);
}
