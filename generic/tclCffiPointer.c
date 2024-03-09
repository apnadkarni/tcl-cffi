/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

CFFI_INLINE void CffiPointerNullifyTag(Tcl_Obj **tagObjP) {
    if (*tagObjP) {
        Tcl_Size len;
        /* Tcl_GetCharLength will shimmer so GetStringFromObj */
        (void)Tcl_GetStringFromObj(*tagObjP, &len);
        if (len == 0)
            *tagObjP = NULL;
    }
}


static CffiResult
CffiPointerCastableCmd(CffiInterpCtx *ipCtxP,
                       Tcl_Obj *subtypeObjList,
                       Tcl_Obj *supertypeObj)
{
    Tcl_Obj *superFqnObj;
    CffiResult ret;
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Obj **subtypeObjs;
    Tcl_Size nSubtypes;
    Tcl_Size i;

    CffiPointerNullifyTag(&supertypeObj);
    if (supertypeObj == NULL)
        return TCL_OK; /* void pointer - everything is already castable to it */

    if (Tcl_ListObjGetElements(
        ip, subtypeObjList, &nSubtypes, &subtypeObjs) != TCL_OK)
        return TCL_ERROR;

    /* Qualify tags if unqualified */
    superFqnObj = Tclh_NsQualifyNameObj(ip, supertypeObj, NULL);
    Tcl_IncrRefCount(superFqnObj);

    ret = TCL_OK;
    for (i = 0; i < nSubtypes; ++i) {
        Tcl_Obj *subFqnObj;
        CffiPointerNullifyTag(&subtypeObjs[i]);
        if (subtypeObjs[i] == NULL)
            continue; /* Ignore void* as a subtype. Requires explicit cast */
        subFqnObj = Tclh_NsQualifyNameObj(ip, subtypeObjs[i], NULL);
        Tcl_IncrRefCount(subFqnObj);
        ret = Tclh_PointerSubtagDefine(
            ip, ipCtxP->tclhCtxP, subFqnObj, superFqnObj);
        if (ret != TCL_OK)
            break;
        Tcl_DecrRefCount(subFqnObj);
    }

    Tcl_DecrRefCount(superFqnObj);
    return ret;
}

static CffiResult
CffiPointerUncastableCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *tagObj)
{
    Tcl_Interp *ip = ipCtxP->interp;
    CffiResult ret;

    CffiPointerNullifyTag(&tagObj);
    if (tagObj == NULL)
        return TCL_OK; /* void pointer - always castable - ignore */

    tagObj = Tclh_NsQualifyNameObj(ip, tagObj, NULL);
    Tcl_IncrRefCount(tagObj);

    ret = Tclh_PointerSubtagRemove(ip, ipCtxP->tclhCtxP, tagObj);
    Tcl_DecrRefCount(tagObj);
    return ret;
}

static CffiResult
CffiPointerCastCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *ptrObj, Tcl_Obj *newTagObj)
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Obj *fqnObj;
    int ret;
    CffiPointerNullifyTag(&newTagObj);
    if (newTagObj) {
        newTagObj = Tclh_NsQualifyNameObj(ip, newTagObj, NULL);
        Tcl_IncrRefCount(newTagObj);
    }
    ret = Tclh_PointerCast(
        ip, ipCtxP->tclhCtxP, ptrObj, newTagObj, &fqnObj);
    if (newTagObj)
        Tcl_DecrRefCount(newTagObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, fqnObj);
    return ret;
}

static CffiResult
CffiPointerCompareCmd(CffiInterpCtx *ipCtxP, Tcl_Obj *ptr1Obj, Tcl_Obj *ptr2Obj)
{
    int cmp;
    CHECK(Tclh_PointerObjCompare(ipCtxP->interp, ptr1Obj, ptr2Obj, &cmp));
    Tcl_SetObjResult(ipCtxP->interp, Tcl_NewIntObj(cmp));
    return TCL_OK;
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
    int validity;

    static const Tclh_SubCommand subCommands[] = {
        {"address", 1, 1, "POINTER", NULL},
        {"cast", 1, 2, "POINTER ?TAG?", NULL},
        {"castable", 2, 2, "SUBTAG SUPERTAG", NULL},
        {"castables", 0, 0, ""},
        {"check", 1, 1, "POINTER", NULL},
        {"compare", 2, 2, "POINTER POINTER", NULL},
        {"counted", 1, 1, "POINTER", NULL},
        {"dispose", 1, 1, "POINTER", NULL},
        {"info", 1, 1, "POINTER", NULL},
        {"invalidate", 1, 1, "POINTER", NULL},
        {"isnull", 1, 1, "POINTER", NULL},
        {"isvalid", 1, 1, "POINTER", NULL},
        {"list", 0, 1, "?TAG?", NULL},
        {"make", 1, 2, "ADDRESS ?TAG?", NULL},
        {"pin", 1, 1, "POINTER", NULL},
        {"safe", 1, 1, "POINTER", NULL},
        {"tag", 1, 1, "POINTER", NULL},
        {"uncastable", 1, 1, "TAG"},
        {NULL}
    };
    enum cmdIndex {
        ADDRESS,
        CAST,
        CASTABLE,
        CASTABLES,
        CHECK,
        COMPARE,
        COUNTED,
        DISPOSE,
        INFO,
        INVALIDATE,
        ISNULL,
        ISVALID,
        LIST,
        MAKE,
        PIN,
        SAFE,
        TAG,
        UNCASTABLE,
    };
    Tclh_PointerRegistrationStatus registration;

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));

    /* LIST and MAKE do not take a pointer arg like the others */
    switch (cmdIndex) {
    case LIST:
        if (objc > 2) {
            objP = objv[2];
        } else
            objP = NULL;
        Tcl_SetObjResult(ip,
                         Tclh_PointerEnumerate(ip,
                                               ipCtxP->tclhCtxP,
                                               objP));
        return TCL_OK;
    case MAKE:
        CHECK(Tclh_ObjToAddress(ip, objv[2], &pv));
        objP = NULL;
        if (objc >= 4 && pv != NULL) {
            objP = objv[3];
            CffiPointerNullifyTag(&objP);
            if (objP)
                objP = Tclh_NsQualifyNameObj(ip, objP, NULL);
        }
        Tcl_SetObjResult(ip, Tclh_PointerWrap(pv, objP));
        return TCL_OK;
    case CASTABLE:
        return CffiPointerCastableCmd(ipCtxP, objv[2], objv[3]);
    case CAST:
        return CffiPointerCastCmd(ipCtxP, objv[2], objc > 3 ? objv[3] : NULL);
    case COMPARE:
        return CffiPointerCompareCmd(ipCtxP, objv[2], objv[3]);
    case CASTABLES:
        objP = Tclh_PointerSubtags(ip, ipCtxP->tclhCtxP);
        if (objP) {
            Tcl_SetObjResult(ip, objP);
            return TCL_OK;
        }
        return TCL_ERROR;/* Tclh_PointerSubtags should have set error */
    case CHECK:
    case ISVALID:
        CHECK(Tclh_PointerObjDissect(ip,
                                     ipCtxP->tclhCtxP,
                                     objv[2],
                                     NULL,
                                     &pv,
                                     &objP,
                                     NULL,
                                     &registration));
        validity = 1;
        if (pv == NULL || registration == TCLH_POINTER_REGISTRATION_MISSING
            || (objP && registration == TCLH_POINTER_REGISTRATION_WRONGTAG)) {
            validity = 0;
        }
        if (cmdIndex == ISVALID) {
            Tcl_SetObjResult(ip, Tcl_NewIntObj(validity));
            return TCL_OK;
        } else {
            if (validity)
                return TCL_OK;
            return Tclh_ErrorInvalidValue(
                ip, objv[2], "Pointer is NULL or not registered as a valid pointer.");
        }
        break;
    case UNCASTABLE:
        return CffiPointerUncastableCmd(ipCtxP, objv[2]);
    case INFO:
        objP = Tclh_PointerObjInfo(ip, ipCtxP->tclhCtxP, objv[2]);
        if (objP) {
            Tcl_SetObjResult(ip, objP);
            return TCL_OK;
        } else
            return TCL_ERROR;

    default:
        break;
    }

    ret = Tclh_PointerUnwrap(ip, objv[2], &pv);
    if (ret != TCL_OK)
        return ret;

    if (cmdIndex == ISNULL) {
        Tcl_SetObjResult(ip, Tcl_NewBooleanObj(pv == NULL));
        return TCL_OK;
    } else if (cmdIndex == ADDRESS) {
        Tcl_SetObjResult(ip, Tclh_ObjFromAddress(pv));
        return TCL_OK;
    }

    if (pv == NULL) {
        switch (cmdIndex) {
        case DISPOSE:
        case INVALIDATE:
            return TCL_OK;
        case ISVALID:
        case TAG:
            break;
        default:
            return Tclh_ErrorPointerNull(ip);
        }
    }

    ret = Tclh_PointerObjGetTag(ip, objv[2], &objP);
    if (ret != TCL_OK)
        return ret;

    switch (cmdIndex) {
    case TAG:
        if (ret == TCL_OK && objP)
            Tcl_SetObjResult(ip, objP);
        return ret;
    case SAFE:
        ret = Tclh_PointerRegister(
            ip, ipCtxP->tclhCtxP, pv, objP, NULL);
        if (ret == TCL_OK) {
            Tcl_SetObjResult(ip, objv[2]);
        }
        return ret;
    case COUNTED:
        ret = Tclh_PointerRegisterCounted(
            ip, ipCtxP->tclhCtxP, pv, objP, NULL);
        if (ret == TCL_OK) {
            Tcl_SetObjResult(ip, objv[2]);
        }
        return ret;
    case PIN:
        /* Note: tag objP is ignored */
        ret = Tclh_PointerRegisterPinned(
            ip, ipCtxP->tclhCtxP, pv, objP, NULL);
        if (ret == TCL_OK) {
            Tcl_SetObjResult(ip, objv[2]);
        }
        return ret;
    case DISPOSE:
        if (pv)
            return Tclh_PointerUnregisterTagged(
                ip, ipCtxP->tclhCtxP, pv, objP);
        return TCL_OK;
    case INVALIDATE:
        if (pv)
            return Tclh_PointerInvalidateTagged(
                ip, ipCtxP->tclhCtxP, pv, objP);
        return TCL_OK;
    default: /* Just to keep compiler happy */
        Tcl_SetResult(
            ip, "Internal error: unexpected pointer subcommand", TCL_STATIC);
        return TCL_ERROR; /* Just to keep compiler happy */
    }
}

