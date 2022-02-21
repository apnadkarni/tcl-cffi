/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

static CffiResult
CffiPointerCastableCmd(Tcl_Interp *ip,
                         Tcl_Obj *subtypeObj,
                         Tcl_Obj *supertypeObj)
{
    Tcl_Obj *subFqnObj;
    Tcl_Obj *superFqnObj;
    CffiResult ret;

    /* QUalify both tags if unqualified */
    subFqnObj = Tclh_NsQualifyNameObj(ip, subtypeObj, NULL);
    Tcl_IncrRefCount(subFqnObj);
    superFqnObj = Tclh_NsQualifyNameObj(ip, supertypeObj, NULL);
    Tcl_IncrRefCount(superFqnObj);

    ret = Tclh_PointerSubtagDefine(ip, subFqnObj, superFqnObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, subFqnObj);
    Tcl_DecrRefCount(subFqnObj);
    Tcl_DecrRefCount(superFqnObj);
    return ret;
}

static CffiResult
CffiPointerUncastableCmd(Tcl_Interp *ip, Tcl_Obj *tagObj)
{
    CffiResult ret;

    tagObj = Tclh_NsQualifyNameObj(ip, tagObj, NULL);
    Tcl_IncrRefCount(tagObj);

    ret = Tclh_PointerSubtagRemove(ip, tagObj);
    Tcl_DecrRefCount(tagObj);
    return ret;
}

static CffiResult
CffiPointerCastCmd(Tcl_Interp *ip, Tcl_Obj *ptrObj, Tcl_Obj *newTagObj)
{
    Tcl_Obj *fqnObj;
    int ret;
    if (newTagObj) {
        newTagObj = Tclh_NsQualifyNameObj(ip, newTagObj, NULL);
        Tcl_IncrRefCount(newTagObj);
    }
    ret = Tclh_PointerCast(ip, ptrObj, newTagObj, &fqnObj);
    if (newTagObj)
        Tcl_DecrRefCount(newTagObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, fqnObj);
    return ret;
}

CffiResult
CffiPointerObjCmd(ClientData cdata,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    int cmdIndex;
    void *pv;
    CffiResult ret;
    Tcl_Obj *objP;

    static const Tclh_SubCommand subCommands[] = {
        {"address", 1, 1, "POINTER", NULL},
        {"list", 0, 1, "?TAG?", NULL},
        {"check", 1, 1, "POINTER", NULL},
        {"isvalid", 1, 1, "POINTER", NULL},
        {"tag", 1, 1, "POINTER", NULL},
        {"safe", 1, 1, "POINTER", NULL},
        {"counted", 1, 1, "POINTER", NULL},
        {"dispose", 1, 1, "POINTER", NULL},
        {"isnull", 1, 1, "POINTER", NULL},
        {"make", 1, 2, "ADDRESS ?TAG?", NULL},
        {"cast", 1, 2, "POINTER ?TAG?", NULL},
        {"castable", 2, 2, "SUBTAG SUPERTAG", NULL},
        {"uncastable", 1, 1, "TAG"},
        {"castables", 0, 0, ""},
        {NULL}
    };
    enum cmdIndex {
        ADDRESS,
        LIST,
        CHECK,
        ISVALID,
        TAG,
        SAFE,
        COUNTED,
        DISPOSE,
        ISNULL,
        MAKE,
        CAST,
        CASTABLE,
        UNCASTABLE,
        CASTABLES
    };

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));

    /* LIST and MAKE do not take a pointer arg like the others */
    switch (cmdIndex) {
    case LIST:
        Tcl_SetObjResult(ip,
                         Tclh_PointerEnumerate(ip, objc == 3 ? objv[2] : NULL));
        return TCL_OK;
    case MAKE:
        CHECK(Tclh_ObjToAddress(ip, objv[2], &pv));
        objP = NULL;
        if (objc >= 4 && pv != NULL) {
            int len;
            /* Tcl_GetCharLength will shimmer so GetStringFromObj */
            (void) Tcl_GetStringFromObj(objv[3], &len);
            if (len != 0)
                objP = Tclh_NsQualifyNameObj(ip, objv[3], NULL);
        }
        Tcl_SetObjResult(ip, Tclh_PointerWrap(pv, objP));
        return TCL_OK;
    case CASTABLE:
        return CffiPointerCastableCmd(ip, objv[2], objv[3]);
    case CAST:
        return CffiPointerCastCmd(ip, objv[2], objc > 3 ? objv[3] : NULL);
    case UNCASTABLE:
        return CffiPointerUncastableCmd(ip, objv[2]);
    case CASTABLES:
        objP = Tclh_PointerSubtags(ip);
        if (objP) {
            Tcl_SetObjResult(ip, objP);
            return TCL_OK;
        }
        return TCL_ERROR;/* Tclh_PointerSubtags should have set error */
    default:
        break;
    }

    ret = Tclh_PointerUnwrap(ip, objv[2], &pv, NULL);
    if (ret != TCL_OK)
        return ret;

    if (cmdIndex == ISNULL) {
        Tcl_SetObjResult(ip, Tcl_NewBooleanObj(pv == NULL));
        return TCL_OK;
    } else if (cmdIndex == ADDRESS) {
        Tcl_SetObjResult(ip, Tclh_ObjFromAddress(pv));
        return TCL_OK;
    }

    if (pv == NULL && cmdIndex != ISVALID && cmdIndex != TAG)
        return Tclh_ErrorInvalidValue(ip, objv[2], "Pointer is NULL.");

    ret = Tclh_PointerObjGetTag(ip, objv[2], &objP);
    if (ret != TCL_OK)
        return ret;

    switch (cmdIndex) {
    case TAG:
        if (ret == TCL_OK && objP)
            Tcl_SetObjResult(ip, objP);
        return ret;
    case CHECK:
        return Tclh_PointerVerify(ip, pv, objP);
    case ISVALID:
        ret = Tclh_PointerVerify(ip, pv, objP);
        Tcl_SetObjResult(ip, Tcl_NewBooleanObj(ret == TCL_OK));
        return TCL_OK;
    case SAFE:
        return Tclh_PointerRegister(ip, pv, objP, NULL);
    case COUNTED:
        return Tclh_PointerRegisterCounted(ip, pv, objP, NULL);
    case DISPOSE:
        return Tclh_PointerUnregister(ip, pv, objP);
    default: /* Just to keep compiler happy */
        Tcl_SetResult(
            ip, "Internal error: unexpected pointer subcommand", TCL_STATIC);
        return TCL_ERROR; /* Just to keep compiler happy */
    }
}

