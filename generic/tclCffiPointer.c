/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

CffiResult
CffiPointerObjCmd(ClientData cdata,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[])
{
    int cmdIndex;
    void *pv;
    CffiResult ret;
    Tcl_Obj *tagObj;

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
        {NULL}
    };
    enum cmdIndex { ADDRESS, LIST, CHECK, ISVALID, TAG, SAFE, COUNTED, DISPOSE, ISNULL };

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));

    /* LIST does not take a pointer arg like the others */
    if (cmdIndex == LIST) {
        Tcl_Obj *resultObj;
        resultObj = Tclh_PointerEnumerate(ip, objc == 3 ? objv[2] : NULL);
        Tcl_SetObjResult(ip, resultObj);
        return TCL_OK;
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

    ret = Tclh_PointerObjGetTag(ip, objv[2], &tagObj);
    if (ret != TCL_OK)
        return ret;

    switch (cmdIndex) {
    case TAG:
        if (ret == TCL_OK && tagObj)
            Tcl_SetObjResult(ip, tagObj);
        return ret;
    case CHECK:
        return Tclh_PointerVerify(ip, pv, tagObj);
    case ISVALID:
        ret = Tclh_PointerVerify(ip, pv, tagObj);
        Tcl_SetObjResult(ip, Tcl_NewBooleanObj(ret == TCL_OK));
        return TCL_OK;
    case SAFE:
        return Tclh_PointerRegister(ip, pv, tagObj, NULL);
    case COUNTED:
        return Tclh_PointerRegisterCounted(ip, pv, tagObj, NULL);
    case DISPOSE:
        return Tclh_PointerUnregister(ip, pv, tagObj);
    default: /* Just to keep compiler happy */
        Tcl_SetResult(
            ip, "Internal error: unexpected pointer subcommand", TCL_STATIC);
        return TCL_ERROR; /* Just to keep compiler happy */
    }
}

