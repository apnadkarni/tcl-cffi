/*
 * Copyright (c) 2024 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#define TCLH_SHORTNAMES
#include "tclCffiInt.h"

void CffiInterfaceUnref(CffiInterface *ifcP)
{
    if (ifcP->nRefs > 1)
        ifcP->nRefs -= 1;
    else {
        int i;
        if (ifcP->nameObj)
            Tcl_DecrRefCount(ifcP->nameObj);
        if (ifcP->idObj)
            Tcl_DecrRefCount(ifcP->idObj);
        if (ifcP->baseIfcP)
            CffiInterfaceUnref(ifcP->baseIfcP);
        if (ifcP->vtable) {
            for (i = 0; i < ifcP->nMethods; ++i) {
                CffiProtoUnref(ifcP->vtable[i].protoP);
                Tcl_DecrRefCount(ifcP->vtable[i].methodNameObj);
            }
            Tcl_Free((char *) ifcP->vtable);
        }
        Tcl_Free((char *) ifcP);
    }
}

/* Function: CffiInterfaceResolve
 * Returns the internal representation of an interface
 *
 * Parameters:
 * ip - interpreter
 * nameP - name of the Interface
 * ifcPP - location to hold pointer to resolved representation
 *
 * *NOTE:* The reference count on the returned structure is *not* incremented.
 * The caller should do that if it wishes to hold on to it.
 *
 * Returns:
 * *TCL_OK* on success with pointer to the <CffiInterface> stored in
 * *ifcPP*, or *TCL_ERROR* on error with message in interpreter.
 */
CffiResult
CffiInterfaceResolve(Tcl_Interp *ip, const char *nameP, CffiInterface **ifcPP)
{
    Tcl_CmdInfo tci;
    Tcl_Obj *nameObj;
    int found;

    found = Tcl_GetCommandInfo(ip, nameP, &tci);
    if (!found)
        goto notfound;
    if (tci.objProc != CffiInterfaceInstanceCmd)
        goto notfound;

    CFFI_ASSERT(tci.clientData);
    *ifcPP = (CffiInterface *)tci.objClientData;
    return TCL_OK;

notfound:
    nameObj = Tcl_NewStringObj(nameP, -1);
    Tcl_IncrRefCount(nameObj);
    (void)Tclh_ErrorNotFound(
        ip, "interface", nameObj, NULL);
    Tcl_DecrRefCount(nameObj);
    return TCL_ERROR;
}

CffiResult
CffiMethodInstanceCmd(ClientData cdata,
                      Tcl_Interp *ip,
                      int objc,
                      Tcl_Obj *const objv[])
{
    CffiMethod *methodP = (CffiMethod *)cdata;
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)methodP->ifcP->ipCtxP;
    CffiFunction *fnP;
    CffiResult ret;
    void *instanceP;
    Tclh_PointerTagRelation tagRelation;
    Tclh_PointerRegistrationStatus registration;

    CHECK_NARGS(ip, 2, INT_MAX, "ifcPtr ?ARG ...?");

    /* Sanity check */
    if (methodP->vtableSlot >= methodP->ifcP->nMethods) {
        Tcl_SetResult(ip, "Internal error: invalid vtable slot", TCL_STATIC);
        return TCL_ERROR;
    }

    ret = Tclh_PointerObjDissect(ip,
                           ipCtxP->tclhCtxP,
                           objv[1],
                           methodP->ifcP->nameObj,
                           &instanceP,
                           NULL,
                           &tagRelation,
                           &registration);
    if (ret != TCL_OK)
        return ret;
    if (instanceP == NULL)
        return Tclh_ErrorPointerNull(ip);

    switch (tagRelation) {
    case TCLH_TAG_RELATION_EQUAL:
    case TCLH_TAG_RELATION_IMPLICITLY_CASTABLE:
        break;
    case TCLH_TAG_RELATION_UNRELATED:
    case TCLH_TAG_RELATION_EXPLICITLY_CASTABLE:
    default:
        return Tclh_ErrorPointerObjType(ip, objv[1], methodP->ifcP->nameObj);
    }

    switch (registration) {
    case TCLH_POINTER_REGISTRATION_OK:
    case TCLH_POINTER_REGISTRATION_DERIVED:
        break;
    case TCLH_POINTER_REGISTRATION_MISSING:
    case TCLH_POINTER_REGISTRATION_WRONGTAG:
    default:
        return Tclh_ErrorPointerObjRegistration(
            ip, objv[1], registration);
    }

    /*
     * The instance pointer points to a area of memory which starts with a
     * pointer to the method table for the instance.
     */
    typedef int (*fnptr)();
    typedef fnptr *vtableptr;
    vtableptr instanceVtablePtr = *(vtableptr *)instanceP;
    fnP = CffiFunctionNew(ipCtxP,
                          methodP->ifcP->vtable[methodP->vtableSlot].protoP,
                          NULL,
                          NULL,
                          instanceVtablePtr[methodP->vtableSlot]);
    CffiFunctionRef(fnP);
    ret = CffiFunctionCall(fnP, ip, 1, objc, objv);
    CffiFunctionUnref(fnP);

    return ret;
}

static CffiResult
CffiInterfaceDestroyCmd(Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[],
                        CffiInterface *ifcP)
{
    /*
    * objv[0] is the command name for the DLL. Deleteing
    * the command will also release associated resources
    */
    if (Tcl_DeleteCommand(ip, Tcl_GetString(objv[0])) == 0)
        return TCL_OK;
    else
        return Tclh_ErrorOperFailed(ip, "destroy", objv[0], NULL);
}

static void
CffiMethodInstanceDeleter(ClientData cdata)
{
    CffiMethod *methodP = (CffiMethod *)cdata;
    CffiInterfaceUnref(methodP->ifcP);
    Tcl_DecrRefCount(methodP->cmdNameObj);
}

static CffiResult
CffiInterfaceMethodsHelper(Tcl_Interp *ip,
                           int objc,
                           Tcl_Obj *const objv[],
                           CffiInterface *ifcP,
                           CffiABIProtocol callMode
                           )
{
    Tcl_Obj **objs;
    Tcl_Size i, nobjs;
    CffiResult ret = TCL_OK;
    static const char *options[] = {"-disposemethod", NULL};
    enum options_e { DISPOSE };
    int opt;
    CffiInterpCtx *ipCtxP = ifcP->ipCtxP;

    if (ifcP->vtable) {
        return Tclh_ErrorExists(ip, "Interface method table", objv[0], NULL);
    }

    /* INTERFACECMD methods METHODLIST */
    assert(objc >= 3);

    CHECK(Tcl_ListObjGetElements(ip, objv[2], &nobjs, &objs));
    if (nobjs % 3) {
        return Tclh_ErrorInvalidValue(
            ip, objv[2], "Incomplete method definition list.");
    }

    Tcl_Size baseSlots = ifcP->baseIfcP ? ifcP->baseIfcP->nMethods : 0;
    Tcl_Size totalSlots = baseSlots + (nobjs / 3);
    if (totalSlots == 0) {
        return Tclh_ErrorInvalidValue(ip, ifcP->nameObj, "Method list is empty.");
    }

    Tcl_Obj *disposeMethodName = NULL;
    int disposeMethodMatched   = 0;

    for (i = 3; i < objc; ++i) {
        if (Tcl_GetIndexFromObj(ip, objv[i], options, "option", 0, &opt) != TCL_OK)
            return TCL_ERROR;
        switch (opt) {
        case DISPOSE:
            ++i;
            if (i == objc) {
                Tcl_SetResult(
                    ip, "No value specified for \"-disposemethod\".", TCL_STATIC);
                return TCL_ERROR;
            }
            disposeMethodName = objv[i];
            break;
        }
    }

    Tcl_Size methodSlot;
    CffiInterfaceMember *ifcMembers;
    ifcMembers =
        (CffiInterfaceMember *)Tcl_Alloc(sizeof(*ifcMembers) * totalSlots);

    Tcl_Obj *params[256]; /* Assume max number of fixed params is 254 */

    /* First param is always pointer to self */
    Tcl_Obj *selfTypeObj;
    params[0] = Tcl_NewStringObj("pSelf", 5);
    Tcl_IncrRefCount(params[0]);
    selfTypeObj = Tcl_NewStringObj("pointer.", 8);
    Tcl_AppendObjToObj(selfTypeObj, ifcP->nameObj);
    Tcl_IncrRefCount(selfTypeObj);

    /* Start initializing leaving space for base (inherited) slots */
    for (i = 0, methodSlot = baseSlots; i < nobjs; i += 3, methodSlot += 1) {
        if (!strcmp("#", Tcl_GetString(objs[i])))
            continue; /* Comment */

        Tcl_Obj **explicit;
        Tcl_Size numExplicit;
        ret = Tcl_ListObjGetElements(
            ip, objs[i + 2], &numExplicit, &explicit);
        if (ret != TCL_OK)
            break;

        if (numExplicit > ((sizeof(params)/sizeof(params[0]))-2)) {
            ret = Tclh_ErrorGeneric(
                ip,
                NULL,
                "Number of method parameters exceeds maximum allowed.");
            break;
        }
        for (int j = 0; j < numExplicit; ++j) {
            params[j + 2] = explicit[j];
        }

        Tcl_Obj *methodNameObj;
        methodNameObj = Tcl_DuplicateObj(ifcP->nameObj);
        Tcl_AppendStringsToObj(
            methodNameObj, ".", Tcl_GetString(objs[i]), NULL);
        Tcl_IncrRefCount(methodNameObj);
        if (disposeMethodName
            && !strcmp(Tcl_GetString(disposeMethodName),
                       Tcl_GetString(objs[i]))) {
            /* Add a dispose attribute */
            Tcl_Obj *temp[2];
            temp[0] = selfTypeObj;
            temp[1]   = Tcl_NewStringObj("dispose", 7);
            params[1] = Tcl_NewListObj(2, temp);
            disposeMethodMatched = 1;
        } else {
            /* Not the dispose method. First arg is plain pointer */
            params[1] = selfTypeObj;
        }
        Tcl_IncrRefCount(params[1]);
        CffiProto *protoP;
        ret = CffiPrototypeParse(ipCtxP,
                                 callMode,
                                 methodNameObj,
                                 objs[i + 1],
                                 numExplicit + 2,
                                 params,
                                 &protoP);
        Tcl_DecrRefCount(params[1]);
        if (ret != TCL_OK) {
            Tcl_DecrRefCount(methodNameObj);
            break;
        }
        ifcMembers[methodSlot].protoP = protoP;
        CffiProtoRef(protoP);
        ifcMembers[methodSlot].methodNameObj = objs[i];
        Tcl_IncrRefCount(objs[i]);

        CffiMethod *methodP = (CffiMethod *)Tcl_Alloc(sizeof(*methodP));
        methodP->cmdNameObj = methodNameObj; /* Already incr ref-ed */
        methodP->ifcP       = ifcP;
        methodP->vtableSlot = (int) methodSlot;
        CffiInterfaceRef(ifcP);

        Tcl_CreateObjCommand(ip,
                             Tcl_GetString(methodNameObj),
                             CffiMethodInstanceCmd,
                             methodP,
                             CffiMethodInstanceDeleter);
    }
    /* At this point slots [baseSlots:methodSlot-1] have been initialized */

    if (disposeMethodName && !disposeMethodMatched) {
        ret = Tclh_ErrorNotFound(ip,
                                 "method name",
                                 disposeMethodName,
                                 "No such method found in method list.");
    }

    if (ret == TCL_OK) {
        /* Now that there are no errors, copy the inherited slots */
        if (baseSlots != 0) {
            CffiInterface *baseIfcP = ifcP->baseIfcP;
            CFFI_ASSERT(baseIfcP);
            for (i = 0; i < baseSlots; ++i) {
                ifcMembers[i].methodNameObj = baseIfcP->vtable[i].methodNameObj;
                Tcl_IncrRefCount(ifcMembers[i].methodNameObj);
                ifcMembers[i].protoP = baseIfcP->vtable[i].protoP;
                CffiProtoRef(ifcMembers[i].protoP);
            }
        }

        ifcP->nMethods = (int) methodSlot;
        ifcP->nInheritedMethods = (int) baseSlots;
        ifcP->vtable   = ifcMembers;
    }
    else {
        /* Free up the slots we initialized */
        CffiInterfaceRef(ifcP); /* Make sure pointer valid while commands deleted*/
        for (i = baseSlots; i < methodSlot; ++i) {
            /* First delete the created commands */
            Tcl_Obj *methodFqn;
            methodFqn = Tcl_DuplicateObj(ifcP->nameObj);
            Tcl_AppendStringsToObj(methodFqn,
                                   ".",
                                   Tcl_GetString(ifcMembers[i].methodNameObj),
                                   NULL);
            Tcl_IncrRefCount(methodFqn);
            Tcl_DeleteCommand(ip, Tcl_GetString(methodFqn));
            Tcl_DecrRefCount(methodFqn);

            CffiProtoUnref(ifcMembers[i].protoP);
            Tcl_DecrRefCount(ifcMembers[i].methodNameObj);
        }
        CffiInterfaceUnref(ifcP);
        Tcl_Free((char *) ifcMembers);
    }

    Tcl_DecrRefCount(params[0]);
    Tcl_DecrRefCount(selfTypeObj);

    return ret;
}

static CffiResult
CffiInterfaceMethodsCmd(Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[],
                        CffiInterface *ifcP)
{
    return CffiInterfaceMethodsHelper(ip, objc, objv, ifcP, CffiDefaultABI());
}

static CffiResult
CffiInterfaceStdMethodsCmd(Tcl_Interp *ip,
                           int objc,
                           Tcl_Obj *const objv[],
                           CffiInterface *ifcP)
{
    return CffiInterfaceMethodsHelper(ip, objc, objv, ifcP, CffiStdcallABI());
}

CffiResult
CffiInterfaceInstanceCmd(ClientData cdata,
                         Tcl_Interp *ip,
                         int objc,
                         Tcl_Obj *const objv[])
{
    CffiInterface *ifcP = (CffiInterface *)cdata;
    static const Tclh_SubCommand subCommands[] = {
        {"destroy", 0, 0, "", CffiInterfaceDestroyCmd},
        {"id", 0, 0, "", NULL},
        {"methods", 1, 3, "", CffiInterfaceMethodsCmd},
        {"stdmethods", 1, 3, "", CffiInterfaceStdMethodsCmd},
        {NULL}};
    enum cmdOpt { DESTROY, ID, METHODS, STDMETHODS };
    int cmdIndex;

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    switch (cmdIndex) {
    case DESTROY:
        return CffiInterfaceDestroyCmd(ip, objc, objv, ifcP);
    case ID:
        Tcl_SetObjResult(ip, ifcP->idObj ? ifcP->idObj : Tcl_NewObj());
        break;
    case METHODS:
    case STDMETHODS:
        return CffiInterfaceMethodsHelper(
            ip,
            objc,
            objv,
            ifcP,
            cmdIndex == METHODS ? CffiDefaultABI() : CffiStdcallABI());
    }
    return TCL_OK;
}

static void CffiInterfaceInstanceDeleter(ClientData cdata)
{
    CffiInterfaceUnref((CffiInterface *)cdata);
}

static CffiResult
CffiInterfaceCreateCmd(ClientData cdata,
                       Tcl_Interp *ip,
                       int objc,
                       Tcl_Obj *const objv[])
{
    CffiInterface *ifcP;
    static const char *options[] = {"-inherit", "-id", NULL};
    enum options_e { INHERIT, ID };
    int opt;
    CffiInterface *baseIfcP = NULL;
    Tcl_Obj       *idObj    = NULL;
    int i;
    const char *s;

    CFFI_ASSERT(objc >= 3);

    for (i = 3; i < objc; ++i) {
        if (Tcl_GetIndexFromObj(ip, objv[i], options, "option", 0, &opt) != TCL_OK)
            return TCL_ERROR;
        if (i == (objc-1)) {
            Tcl_SetObjResult(
                ip,
                Tcl_ObjPrintf("No value specified for option \"%s\".",
                              Tcl_GetString(objv[i])));
            return TCL_ERROR;
        }
        ++i;
        switch (opt) {
        case INHERIT:
            /* Empty interface names same as no inheritance */
            s = Tcl_GetString(objv[i]);
            if (*s && CffiInterfaceResolve(ip, s, &baseIfcP) != TCL_OK) {
                return TCL_ERROR;
            }
            break;
        case ID:
            idObj = objv[i];
            break;
        }
    }

    ifcP = (CffiInterface *) Tcl_Alloc(sizeof(*ifcP));
    ifcP->ipCtxP            = (CffiInterpCtx *)cdata;
    ifcP->nRefs             = 1;
    ifcP->nMethods          = 0;
    ifcP->nInheritedMethods = 0;
    ifcP->vtable            = NULL;

    ifcP->nameObj = Tclh_NsQualifyNameObj(ip, objv[2], NULL);
    Tcl_IncrRefCount(ifcP->nameObj);

    ifcP->idObj    = idObj;
    if (idObj)
        Tcl_IncrRefCount(idObj);

    ifcP->baseIfcP = baseIfcP;
    if (baseIfcP) {
        Tclh_PointerSubtagDefine(
            ip, ifcP->ipCtxP->tclhCtxP, ifcP->nameObj, baseIfcP->nameObj);
        CffiInterfaceRef(baseIfcP);
    }
    Tcl_CreateObjCommand(ip,
                         Tcl_GetString(ifcP->nameObj),
                         CffiInterfaceInstanceCmd,
                         ifcP,
                         CffiInterfaceInstanceDeleter);
    Tcl_SetObjResult(ip, ifcP->nameObj);
    return TCL_OK;
}

/* Function: CffiInterfaceObjCmd
 * Implements the script level *Interface* command.
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
CffiInterfaceObjCmd(ClientData cdata,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[])
{
    static const Tclh_SubCommand commands[] = {
        {"create", 1, 5, "IFCNAME ?-id ID? ?-inherit IFCBASE?", CffiInterfaceCreateCmd, 0},
        {NULL}
    };
    int cmdIndex;

    CHECK(Tclh_SubCommandLookup(ip, commands, objc, objv, &cmdIndex));
    return commands[cmdIndex].cmdFn((CffiInterpCtx *)cdata, ip, objc, objv);
}

/* TBD - should delete dispose the pointer? */