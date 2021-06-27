/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"
#include <sys/types.h>

/* Function: CffiEnumFind
 * Returns the value of an enum member.
 *
 * Parameters:
 * ipCtxP - context in which the enum is defined
 * enumObj - name of the enum
 * nameObj - name of the member
 * valueObjP - location to store the value of the member
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on failure.
 */
CffiResult
CffiEnumFind(CffiInterpCtx *ipCtxP,
             Tcl_Obj *enumObj,
             Tcl_Obj *nameObj,
             Tcl_Obj **valueObjP)
{
    Tcl_HashEntry *heP;
    Tcl_Obj *valueObj;

    heP = Tcl_FindHashEntry(&ipCtxP->enums, enumObj);
    if (heP == NULL)
        return Tclh_ErrorNotFound(ipCtxP->interp, "Enum", enumObj, NULL);
    else {
        Tcl_Obj *entries = Tcl_GetHashValue(heP);
        CHECK(Tcl_DictObjGet(ipCtxP->interp, entries, nameObj, &valueObj));
        if (valueObj == NULL)
            return Tclh_ErrorNotFound(
                ipCtxP->interp, "Enum value", nameObj, NULL);
        *valueObjP = valueObj;
        return TCL_OK;
    }
}

/* Function: CffiEnumBitmask
 * Calculates a bitmask by doing a bitwise-OR of elements in a list
 *
 * Parameters:
 * ipCtxP - context in which the enum is defined
 * enumObj - name of the enum
 * valueListObj - list whose elements are to be OR-ed
 * maskP - location to store the result of OR-ing the elements
 *
 * The elements may be integers or names of members of the enum.
 *
 * Returns:
 * *TCL_OK* on success, else *TCL_ERROR* on failure.
 */
CffiResult
CffiEnumBitmask(CffiInterpCtx *ipCtxP,
                Tcl_Obj *enumObj,
                Tcl_Obj *valueListObj,
                Tcl_WideInt *maskP)
{
    Tcl_Obj **objs;
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_WideInt mask;
    int nobjs;
    int i;
    CffiResult ret = TCL_OK;

    CHECK(Tcl_ListObjGetElements(ip, valueListObj, &nobjs, &objs));
    mask = 0;
    for (i = 0; i < nobjs; ++i) {
        Tcl_WideInt wide;
        Tcl_Obj *wideObj;
        ret = Tcl_GetWideIntFromObj(enumObj ? NULL : ip, objs[i], &wide);
        if (ret != TCL_OK) {
            if (enumObj == NULL)
                return ret;
            CHECK(CffiEnumFind(ipCtxP, enumObj, objs[i], &wideObj));
            CHECK(Tcl_GetWideIntFromObj(ip, wideObj, &wide));
        }
        mask |= wide;
    }
    *maskP = mask;
    return TCL_OK;
}

static CffiResult
CffiEnumDefineCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;
    Tcl_Interp *ip = ipCtxP->interp;
    int count;
    int newEntry;

    CFFI_ASSERT(objc == 4);

    /* Verify name syntax */
    CHECK(CffiNameSyntaxCheck(ip, objv[2]));

    /* Verify it is properly formatted */
    CHECK(Tcl_DictObjSize(ip, objv[3], &count));

    heP = Tcl_CreateHashEntry(&ipCtxP->enums, (char *)objv[2], &newEntry);
    if (! newEntry)
        return Tclh_ErrorExists(ip, "Enum", objv[2], NULL);
    Tcl_IncrRefCount(objv[3]);
    Tcl_SetHashValue(heP, objv[3]);
    return TCL_OK;
}

static CffiResult
CffiEnumSequenceCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Obj *enumObj;
    Tcl_Obj **names;
    int nNames;
    int start;
    int newEntry;
    int i;

    CFFI_ASSERT(objc == 4 || objc == 5);

    CHECK(CffiNameSyntaxCheck(ip, objv[2]));
    CHECK(Tcl_ListObjGetElements(ip, objv[3], &nNames, &names));

    if (objc == 5)
        CHECK(Tclh_ObjToInt(ip, objv[4], &start));
    else
        start = 0;

    heP = Tcl_CreateHashEntry(&ipCtxP->enums, (char *)objv[2], &newEntry);
    if (! newEntry)
        return Tclh_ErrorExists(ip, "Enum", objv[2], NULL);

    /* We will create as list and let it be shimmered to dictionary as needed */
    enumObj = Tcl_NewListObj(2 * nNames, NULL);
    for (i = 0; i < nNames; ++i) {
        Tcl_ListObjAppendElement(NULL, enumObj, names[i]);
        Tcl_ListObjAppendElement(NULL, enumObj, Tcl_NewIntObj(start++));
    }

    Tcl_IncrRefCount(enumObj);
    Tcl_SetHashValue(heP, enumObj);
    return TCL_OK;
}

static CffiResult
CffiEnumMembersCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;

    CFFI_ASSERT(objc == 3);

    heP = Tcl_FindHashEntry(&ipCtxP->enums, objv[2]);
    if (heP == NULL)
        return Tclh_ErrorNotFound(ipCtxP->interp, "Enum", objv[2], NULL);

    Tcl_SetObjResult(ipCtxP->interp, Tcl_GetHashValue(heP));
    return TCL_OK;
}

static CffiResult
CffiEnumListCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_SetObjResult(ipCtxP->interp,
                     Tclh_ObjHashEnumerateEntries(&ipCtxP->enums,
                                                  objc > 2 ? objv[2] : NULL));
    return TCL_OK;
}

static void CffiEnumEntryDelete(Tcl_HashEntry *heP)
{
    Tcl_Obj *objP = Tcl_GetHashValue(heP);
    Tcl_DecrRefCount(objP);
}

static CffiResult
CffiEnumDeleteCmd(CffiInterpCtx *ipCtxP,
                    int objc,
                    Tcl_Obj *const objv[])
{
    CFFI_ASSERT(objc == 3);
    Tclh_ObjHashDeleteEntries(
        &ipCtxP->enums, objv[2], CffiEnumEntryDelete);
    return TCL_OK;
}

/* Called on interp deletion */
void
CffiEnumsCleanup(Tcl_HashTable *enumsTableP)
{
    Tcl_HashEntry *heP;
    Tcl_HashSearch hSearch;
    for (heP = Tcl_FirstHashEntry(enumsTableP, &hSearch);
         heP != NULL; heP = Tcl_NextHashEntry(&hSearch)) {
        Tcl_Obj *enumsObj = Tcl_GetHashValue(heP);
        Tcl_DecrRefCount(enumsObj);
    }
    Tcl_DeleteHashTable(enumsTableP);
}

CffiResult
CffiEnumObjCmd(ClientData cdata,
                Tcl_Interp *ip,
                int objc,
                Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    int cmdIndex;
    static const Tclh_SubCommand subCommands[] = {
        {"define", 2, 2, "ENUM MEMBERS", CffiEnumDefineCmd},
        {"delete", 1, 1, "PATTERN", CffiEnumDeleteCmd},
        {"members", 1, 1, "ENUM", CffiEnumMembersCmd},
        {"list", 0, 1, "?PATTERN?", CffiEnumListCmd},
        {"sequence", 2, 3, "ENUM MEMBERNAMES ?START?", CffiEnumSequenceCmd},
        {NULL}};

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ipCtxP, objc, objv);
}
