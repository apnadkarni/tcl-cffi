/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"
#include <sys/types.h>

/* Function: CffiEnumGetMap
 * Gets the hash table containing mappings for an enum.
 *
 * Parameters:
 * ip - interpreter. May be NULL if error messages not to be reported.
 * scopeP - scope in which to look up enum
 * enumObj - name of the enum
 * mapObjP - location to store the dictionary holding the mapping. May be
 *   *NULL* if only existence is being checked
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on failure.
 */
CffiResult
CffiEnumGetMap(Tcl_Interp *ip,
               CffiScope *scopeP,
               Tcl_Obj *enumObj,
               Tcl_Obj **mapObjP)
{
    Tcl_HashEntry *heP;
    heP = Tcl_FindHashEntry(&scopeP->enums, enumObj);
    if (heP == NULL) {
        return Tclh_ErrorNotFound(ip, "Enum", enumObj, NULL);
    }
    else {
        if (mapObjP)
            *mapObjP = Tcl_GetHashValue(heP);
        return TCL_OK;
    }
}

/* Function: CffiEnumMemberFind
 * Returns the value of a member of a given enum.
 *
 * Parameters:
 * ip - interpreter. May be NULL if no error messages are to be reported.
 * mapObj - the enum map
 * nameObj - name of the member
 * valueObjP - location to store the value of the member. If *NULL*,
 *   only a check is made as to existence.
 *
 * The reference count on the Tcl_Obj returned in valueObjP is NOT incremented.
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on failure.
 */
CffiResult
CffiEnumMemberFind(Tcl_Interp *ip,
                   Tcl_Obj *mapObj,
                   Tcl_Obj *memberNameObj,
                   Tcl_Obj **valueObjP)
{
    Tcl_Obj *valueObj;

    if (Tcl_DictObjGet(NULL, mapObj, memberNameObj, &valueObj) != TCL_OK
        || valueObj == NULL) {
        Tclh_ErrorNotFound(ip, "Enum member name", memberNameObj, NULL);
        return TCL_ERROR;
    }
    if (valueObjP)
        *valueObjP = valueObj;
    return TCL_OK;
}



/* Function: CffiEnumFind
 * Returns the value of an enum member.
 *
 * Parameters:
 * ipCtxP - context in which the enum is defined
 * scopeP - the scope in which to look up the enum definition
 * enumObj - name of the enum
 * nameObj - name of the member
 * flags - If CFFI_F_ENUM_SKIP_ERROR_MESSAGE is set, error message is not
 *   stored in the interpreter. If CFFI_F_ENUM_INCLUDE_GLOBAL is set, the
 *   global scope is also looked up.
 * valueObjP - location to store the value of the member. If *NULL*,
 *   only a check is made as to existence.
 *
 * The reference count on the Tcl_Obj returned in valueObjP is NOT incremented.
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on failure.
 */
CffiResult
CffiEnumFind(CffiInterpCtx *ipCtxP,
             CffiScope *scopeP,
             Tcl_Obj *enumNameObj,
             Tcl_Obj *memberNameObj,
             int flags,
             Tcl_Obj **valueObjP)
{
    Tcl_Obj *entries;
    Tcl_Interp *ip;
    int ret;

    /*
     * If the global scope is also to be looked up we do not want error
     * messages on the first lookup irrespective of what caller said
     */
    if ((flags & CFFI_F_ENUM_SKIP_ERROR_MESSAGE)
        || ((flags & CFFI_F_ENUM_INCLUDE_GLOBAL)
            && scopeP != ipCtxP->globalScopeP))
        ip = NULL; /* Do not want error messages */
    else
        ip = ipCtxP->interp;

    ret = CffiEnumGetMap(ip, scopeP, enumNameObj, &entries);
    if (ret == TCL_OK) {
        if (flags & CFFI_F_ENUM_SKIP_ERROR_MESSAGE)
            ip = NULL;
        else
            ip = ipCtxP->interp;
        /*
         * If enum is defined locally, we do not look up global namespace
         * on error if member is not defined
         */
        return CffiEnumMemberFind(ip, entries, memberNameObj, valueObjP);
    }
    else if ((flags & CFFI_F_ENUM_INCLUDE_GLOBAL) == 0)
        return ret;

    /* No luck so check global scope */
    CFFI_ASSERT(ipCtxP->globalScopeP);

    ip = (flags & CFFI_F_ENUM_SKIP_ERROR_MESSAGE) ? NULL : ipCtxP->interp;
    ret = CffiEnumGetMap(ip, ipCtxP->globalScopeP, enumNameObj, &entries);
    if (ret == TCL_OK)
        ret = CffiEnumMemberFind(ip, entries, memberNameObj, valueObjP);
    return ret;
}

/* Function: CffiEnumMemberFindReverse
 * Returns the name of an member value in a given enum
 *
 * Parameters:
 * ip - interpreter. May be NULL if no error messages are required.
 * mapObj - Enum mapping table
 * needleObj - Value to map to a name
 * nameObjP - location to store the name of the member.
 *
 * The reference count on the Tcl_Obj returned in nameObjP is NOT incremented.
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on failure if member name is not found.
 */
CffiResult
CffiEnumMemberFindReverse(Tcl_Interp *ip,
                          Tcl_Obj *mapObj,
                          Tcl_WideInt needle,
                          Tcl_Obj **nameObjP)
{
    Tcl_Obj *nameObj;
    Tcl_Obj *valueObj;
    int done;
    Tcl_DictSearch search;
    CHECK(Tcl_DictObjFirst(ip, mapObj, &search, &nameObj, &valueObj, &done));
    while (!done) {
        Tcl_WideInt wide;
        if (Tcl_GetWideIntFromObj(NULL, valueObj, &wide) == TCL_OK
            && wide == needle) {
            *nameObjP = nameObj;
            Tcl_DictObjDone(&search);
            return TCL_OK;
        }
        Tcl_DictObjNext(&search, &nameObj, &valueObj, &done);
    }
    Tcl_DictObjDone(&search);
    return Tclh_ErrorNotFound(ip, "Enum member value", NULL, NULL);
}

/* Function: CffiEnumFindReverse
 * Returns the name of an enum member value.
 *
 * Parameters:
 * ipCtxP - Context in which the enum is defined
 * scopeP - the scope in which to look up the enum definition
 * enumNameObj - Name of the enum
 * needleObj - Value to map to a name
 * flags - If CFFI_F_ENUM_SKIP_ERROR_MESSAGE is set, error message is not
 *   stored in the interpreter if the needle is not founc and the needle
 *   is itself returned in *nameObjP*
 * nameObjP - location to store the name of the member
 *
 * The reference count on the Tcl_Obj returned in nameObjP is NOT incremented.
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on failure.
 */
CffiResult
CffiEnumFindReverse(CffiInterpCtx *ipCtxP,
                    CffiScope *scopeP,
                    Tcl_Obj *enumNameObj,
                    Tcl_WideInt needle,
                    int flags,
                    Tcl_Obj **nameObjP)
{
    Tcl_Obj *entries;
    Tcl_Interp *ip;
    int ret;

    /*
     * If the global scope is also to be looked up we do not want error
     * messages on the first lookup irrespective of what caller said
     */
    if ((flags & CFFI_F_ENUM_SKIP_ERROR_MESSAGE)
        || ((flags & CFFI_F_ENUM_INCLUDE_GLOBAL)
            && scopeP != ipCtxP->globalScopeP))
        ip = NULL; /* Do not want error messages */
    else
        ip = ipCtxP->interp;

    ret = CffiEnumGetMap(ip, scopeP, enumNameObj, &entries);
    if (ret == TCL_OK) {
        if (flags & CFFI_F_ENUM_SKIP_ERROR_MESSAGE) {
            if (CffiEnumMemberFindReverse(NULL, entries, needle, nameObjP)
                != TCL_OK)
                *nameObjP = Tcl_NewWideIntObj(needle);
        }
        else {
            ret = CffiEnumMemberFindReverse(
                ipCtxP->interp, entries, needle, nameObjP);
        }
        return ret;
    }
    else if ((flags & CFFI_F_ENUM_INCLUDE_GLOBAL) == 0)
        return ret;

    /* No luck so check global scope */
    CFFI_ASSERT(ipCtxP->globalScopeP);

    ip = (flags & CFFI_F_ENUM_SKIP_ERROR_MESSAGE) ? NULL : ipCtxP->interp;
    ret = CffiEnumGetMap(ip, ipCtxP->globalScopeP, enumNameObj, &entries);
    if (ret == TCL_OK) {
        ret = CffiEnumMemberFindReverse(ip, entries, needle, nameObjP);
    }
    if (ret != TCL_OK && ip == NULL) {
        *nameObjP = Tcl_NewWideIntObj(needle);
        ret       = TCL_OK;
    }

    return ret;
}


/* Function: CffiEnumMemberBitmask
 * Calculates a bitmask by doing a bitwise-OR of elements in a list
 *
 * Parameters:
 * ip - interpreter. Pass as NULL if error messages not of interest.
 * mapObj - enum mapping dictionary
 * valueListObj - list whose elements are to be OR-ed
 * maskP - location to store the result of OR-ing the elements
 *
 * The elements may be integers or names of members of the enum.
 *
 * Returns:
 * *TCL_OK* on success, else *TCL_ERROR* on failure.
 */
CffiResult
CffiEnumMemberBitmask(Tcl_Interp *ip,
                      Tcl_Obj *mapObj,
                      Tcl_Obj *valueListObj,
                      Tcl_WideInt *maskP)
{
    Tcl_Obj **objs;
    Tcl_WideInt mask;
    int nobjs;
    int i;
    CffiResult ret = TCL_OK;

    /* TBD - handles ulonglong correctly? */
    CHECK(Tcl_ListObjGetElements(ip, valueListObj, &nobjs, &objs));
    mask = 0;
    for (i = 0; i < nobjs; ++i) {
        Tcl_WideInt wide;
        Tcl_Obj *wideObj;
        ret = Tcl_GetWideIntFromObj(mapObj ? NULL : ip, objs[i], &wide);
        if (ret != TCL_OK) {
            if (mapObj == NULL)
                return ret;
            CHECK(CffiEnumMemberFind(ip, mapObj, objs[i], &wideObj));
            CHECK(Tcl_GetWideIntFromObj(ip, wideObj, &wide));
        }
        mask |= wide;
    }
    *maskP = mask;
    return TCL_OK;
}

/* Function: CffiEnumMemberBitunmask
 * Returns a list of enum member names corresponding to bits that are
 * set in an integer value
 *
 * Parameters:
 * ip - interpreter. Pass as NULL if error messages not of interest
 * mapObj - enum mapping dictionary. May be NULL if no associated enum.
 * bitmask - integer bit mask
 * listObjP - location to hold list of enum names corresponding to bits
 *   that are set in *bitmask*
 *
 * The bitmask of any bits that were not mapped to an enum member is
 * returned as the last element of *listObjP*.
 *
 * Returns:
 * *TCL_OK* on success, else *TCL_ERROR* on failure.
 */
CffiResult
CffiEnumMemberBitUnmask(Tcl_Interp *ip,
                        Tcl_Obj *mapObj,
                        Tcl_WideInt bitmask,
                        Tcl_Obj **listObjP)
{
    Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
    Tcl_WideInt remain = bitmask;

    if (mapObj) {
        Tcl_Obj *nameObj;
        Tcl_Obj *valueObj;
        Tcl_DictSearch search;
        int done;
        if (Tcl_DictObjFirst(ip, mapObj, &search, &nameObj, &valueObj, &done)
            != TCL_OK) {
            Tcl_DecrRefCount(listObj);
            return TCL_ERROR;
        }
        while (!done) {
            Tcl_WideInt wide;
            if (Tcl_GetWideIntFromObj(NULL, valueObj, &wide) == TCL_OK
                && ((wide & bitmask) == wide)) {
                Tcl_ListObjAppendElement(NULL, listObj, nameObj);
                remain &= ~wide;
            }
            Tcl_DictObjNext(&search, &nameObj, &valueObj, &done);
        }
        Tcl_DictObjDone(&search);
    }
    Tcl_ListObjAppendElement(NULL, listObj, Tcl_NewWideIntObj(remain));
    *listObjP = listObj;
    return TCL_OK;
}

static CffiResult
CffiEnumDefineCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;
    Tcl_Interp *ip = ipCtxP->interp;
    int newEntry;
    Tcl_DictSearch search;
    Tcl_Obj *valueObj;
    Tcl_Obj *nameObj;
    CffiScope *scopeP;
    int done;

    CFFI_ASSERT(objc == 4);

    /* Verify name syntax */
    CHECK(CffiNameSyntaxCheck(ip, objv[2]));

    /* Verify it is properly formatted */
    CHECK(Tcl_DictObjFirst(
        ip, objv[3], &search, &nameObj, &valueObj, &done));
    while (!done) {
        Tcl_WideInt wide;
        CHECK(CffiNameSyntaxCheck(ip, nameObj));
        /* Values must be integers */
        CHECK(Tcl_GetWideIntFromObj(ip, valueObj, &wide));
        Tcl_DictObjNext(&search, &nameObj, &valueObj, &done);
    }

    scopeP = CffiScopeGet(ipCtxP, NULL);

    heP = Tcl_CreateHashEntry(&scopeP->enums, (char *)objv[2], &newEntry);
    if (! newEntry)
        return Tclh_ErrorExists(ip, "Enum", objv[2], NULL);
    Tcl_IncrRefCount(objv[3]);
    Tcl_SetHashValue(heP, objv[3]);
    return TCL_OK;
}

static CffiResult
CffiEnumMaskCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_WideInt mask;
    Tcl_Obj *mapObj;

    CFFI_ASSERT(objc == 4);
    CHECK(CffiEnumGetMap(ip, CffiScopeGet(ipCtxP, NULL), objv[2], &mapObj));
    CHECK(CffiEnumMemberBitmask(ip, mapObj, objv[3], &mask));
    Tcl_SetObjResult(ip, Tcl_NewWideIntObj(mask));
    return TCL_OK;
}

static CffiResult
CffiEnumUnmaskCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_WideInt mask;
    Tcl_Obj *mapObj;
    Tcl_Obj *listObj;

    CFFI_ASSERT(objc == 4);
    CHECK(Tcl_GetWideIntFromObj(ip, objv[3], &mask));
    CHECK(CffiEnumGetMap(ip, CffiScopeGet(ipCtxP, NULL), objv[2], &mapObj));
    CHECK(CffiEnumMemberBitUnmask(ip, mapObj, mask, &listObj));
    Tcl_SetObjResult(ip, listObj);
    return TCL_OK;
}

static CffiResult
CffiEnumValueCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *valueObj;

    CFFI_ASSERT(objc == 4);
    CHECK(CffiEnumFind(ipCtxP,
                       CffiScopeGet(ipCtxP, NULL),
                       objv[2],
                       objv[3],
                       CFFI_F_ENUM_INCLUDE_GLOBAL,
                       &valueObj));
    Tcl_SetObjResult(ipCtxP->interp, valueObj);
    return TCL_OK;
}

static CffiResult
CffiEnumNameCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *nameObj;
    Tcl_WideInt wide;

    CFFI_ASSERT(objc == 4);
    CHECK(Tcl_GetWideIntFromObj(ipCtxP->interp, objv[3], &wide));
    CHECK(CffiEnumFindReverse(ipCtxP,
                              CffiScopeGet(ipCtxP, NULL),
                              objv[2],
                              wide,
                              CFFI_F_ENUM_INCLUDE_GLOBAL,
                              &nameObj));
    Tcl_SetObjResult(ipCtxP->interp, nameObj);
    return TCL_OK;
}

static CffiResult
CffiEnumFlagsCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Obj *enumObj;
    Tcl_Obj **names;
    CffiScope *scopeP;
    int nNames;
    int newEntry;
    int i;

    CFFI_ASSERT(objc == 4);

    CHECK(CffiNameSyntaxCheck(ip, objv[2]));
    CHECK(Tcl_ListObjGetElements(ip, objv[3], &nNames, &names));

    if (nNames > 64)
        return Tclh_ErrorInvalidValue(
            ip, NULL, "Enum specified with more than 64 flag bits.");

    /* We will create as list and let it be shimmered to dictionary as needed */
    enumObj = Tcl_NewListObj(2 * nNames, NULL);
    for (i = 0; i < nNames; ++i) {
        if (CffiNameSyntaxCheck(ip, names[i]) != TCL_OK) {
            Tcl_DecrRefCount(enumObj);
            return TCL_ERROR;
        }
        Tcl_ListObjAppendElement(NULL, enumObj, names[i]);
        Tcl_ListObjAppendElement(NULL, enumObj, Tcl_NewIntObj(1 << i));
    }

    scopeP = CffiScopeGet(ipCtxP, NULL);
    heP = Tcl_CreateHashEntry(&scopeP->enums, (char *)objv[2], &newEntry);
    if (! newEntry) {
        Tcl_DecrRefCount(enumObj);
        return Tclh_ErrorExists(ip, "Enum", objv[2], NULL);
    }

    Tcl_IncrRefCount(enumObj);
    Tcl_SetHashValue(heP, enumObj);
    return TCL_OK;
}


static CffiResult
CffiEnumSequenceCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Obj *enumObj;
    Tcl_Obj **names;
    CffiScope *scopeP;
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


    /* We will create as list and let it be shimmered to dictionary as needed */
    enumObj = Tcl_NewListObj(2 * nNames, NULL);
    for (i = 0; i < nNames; ++i) {
        if (CffiNameSyntaxCheck(ip, names[i]) != TCL_OK) {
            Tcl_DecrRefCount(enumObj);
            return TCL_ERROR;
        }
        Tcl_ListObjAppendElement(NULL, enumObj, names[i]);
        Tcl_ListObjAppendElement(NULL, enumObj, Tcl_NewIntObj(start++));
    }

    scopeP = CffiScopeGet(ipCtxP, NULL);
    heP = Tcl_CreateHashEntry(&scopeP->enums, (char *)objv[2], &newEntry);
    if (! newEntry) {
        Tcl_DecrRefCount(enumObj);
        return Tclh_ErrorExists(ip, "Enum", objv[2], NULL);
    }

    Tcl_IncrRefCount(enumObj);
    Tcl_SetHashValue(heP, enumObj);
    return TCL_OK;
}

static CffiResult
CffiEnumMembersCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *entries;

    CFFI_ASSERT(objc == 3);

    CHECK(CffiEnumGetMap(
        ipCtxP->interp, CffiScopeGet(ipCtxP, NULL), objv[2], &entries));
    Tcl_SetObjResult(ipCtxP->interp, entries);
    return TCL_OK;
}

static CffiResult
CffiEnumListCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    CffiScope *scopeP = CffiScopeGet(ipCtxP, NULL);
    Tcl_SetObjResult(ipCtxP->interp,
                     Tclh_ObjHashEnumerateEntries(&scopeP->enums,
                                                  objc > 2 ? objv[2] : NULL));
    return TCL_OK;
}

static void CffiEnumEntryDelete(Tcl_HashEntry *heP)
{
    Tcl_Obj *objP = Tcl_GetHashValue(heP);
    if (objP)
        Tcl_DecrRefCount(objP);
}

static CffiResult
CffiEnumDeleteCmd(CffiInterpCtx *ipCtxP,
                    int objc,
                    Tcl_Obj *const objv[])
{
    CffiScope *scopeP = CffiScopeGet(ipCtxP, NULL);

    CFFI_ASSERT(objc == 3);
    Tclh_ObjHashDeleteEntries(
        &scopeP->enums, objv[2], CffiEnumEntryDelete);
    return TCL_OK;
}

/* Called on interp deletion */
void
CffiEnumsCleanup(Tcl_HashTable *enumsTableP)
{
    Tclh_ObjHashDeleteEntries(enumsTableP, NULL, CffiEnumEntryDelete);
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
        {"flags", 2, 2, "ENUM FLAGNAMES", CffiEnumFlagsCmd},
        {"list", 0, 1, "?PATTERN?", CffiEnumListCmd},
        {"members", 1, 1, "ENUM", CffiEnumMembersCmd},
        {"name", 2, 2, "ENUM VALUE", CffiEnumNameCmd},
        {"sequence", 2, 3, "ENUM MEMBERNAMES ?START?", CffiEnumSequenceCmd},
        {"value", 2, 2, "ENUM MEMBERNAME", CffiEnumValueCmd},
        {"mask", 2, 2, "ENUM MEMBERLIST", CffiEnumMaskCmd},
        {"unmask", 2, 2, "ENUM INTEGER", CffiEnumUnmaskCmd},
        {NULL}};

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ipCtxP, objc, objv);
}
