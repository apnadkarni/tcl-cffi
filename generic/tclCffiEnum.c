/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

/* Function: CffiEnumGetMap
 * Gets the hash table containing mappings for an enum.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * nameObj - name of the enum
 * flags - if CFFI_F_SKIP_ERROR_MESSAGES is set, no errors are
 *   recorded in the interpreter
 * mapObjP - location to store the dictionary holding the mapping. May be
 *   *NULL* if only existence is being checked
 *
 * If the name is not fully qualified, it is also looked up relative to the
 * current namespace and the global namespace in that order.
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on failure.
 */
CffiResult
CffiEnumGetMap(CffiInterpCtx *ipCtxP,
               Tcl_Obj *nameObj,
               CffiFlags flags,
               Tcl_Obj **mapObjP)
{
    return CffiNameLookup(ipCtxP->interp,
                          &ipCtxP->scope.enums,
                          Tcl_GetString(nameObj),
                          "Enum",
                          flags,
                          (ClientData *)mapObjP,
                          NULL);
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
    Tcl_Size nobjs;
    Tcl_Size i;
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
            /* The check wide != 0 is because some C enums include 0 "no bits set" */
            if (Tcl_GetWideIntFromObj(NULL, valueObj, &wide) == TCL_OK
                && wide != 0 && ((wide & bitmask) == wide)) {
                Tcl_ListObjAppendElement(NULL, listObj, nameObj);
            }
            Tcl_DictObjNext(&search, &nameObj, &valueObj, &done);
        }
        Tcl_DictObjDone(&search);
    }
    Tcl_ListObjAppendElement(NULL, listObj, Tcl_NewWideIntObj(bitmask));
    *listObjP = listObj;
    return TCL_OK;
}

static CffiResult
CffiEnumDefineCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_DictSearch search;
    Tcl_Obj *valueObj;
    Tcl_Obj *memberNameObj;
    Tcl_Obj *fqnObj;
    int done;

    CFFI_ASSERT(objc == 4);

    /* Verify it is properly formatted */
    CHECK(Tcl_DictObjFirst(
        ip, objv[3], &search, &memberNameObj, &valueObj, &done));
    while (!done) {
        Tcl_WideInt wide;
        CHECK(CffiNameSyntaxCheck(ip, memberNameObj));
        /* Values must be integers */
        CHECK(Tcl_GetWideIntFromObj(ip, valueObj, &wide));
        Tcl_DictObjNext(&search, &memberNameObj, &valueObj, &done);
    }

    if (CffiNameObjAdd(
            ip, &ipCtxP->scope.enums, objv[2], "Enum", objv[3], &fqnObj)
        != TCL_OK) {
        return TCL_ERROR;
    }
    Tcl_IncrRefCount(objv[3]);
    Tcl_SetObjResult(ip, fqnObj);
    return TCL_OK;
}

static CffiResult
CffiEnumMaskCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_WideInt mask;
    Tcl_Obj *mapObj;

    CFFI_ASSERT(objc == 4);
    CHECK(CffiEnumGetMap(ipCtxP, objv[2], 0, &mapObj));
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
    CHECK(CffiEnumGetMap(ipCtxP, objv[2], 0, &mapObj));
    CHECK(CffiEnumMemberBitUnmask(ip, mapObj, mask, &listObj));
    Tcl_SetObjResult(ip, listObj);
    return TCL_OK;
}

static CffiResult
CffiEnumValueCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *valueObj;
    Tcl_Obj *entries;
    CffiResult ret;

    CFFI_ASSERT(objc == 4 || objc == 5);

    CHECK(CffiEnumGetMap(ipCtxP, objv[2], 0, &entries));

    /* If a default has been supplied, we will return it on failure. */
    ret = CffiEnumMemberFind(
        objc == 4 ? ipCtxP->interp : NULL, entries, objv[3], &valueObj);
    if (ret != TCL_OK) {
        if (objc == 4)
            return ret;
        valueObj = objv[4];
    }
    Tcl_SetObjResult(ipCtxP->interp, valueObj);
    return TCL_OK;
}

static CffiResult
CffiEnumNameCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *nameObj;
    Tcl_Obj *entries;
    Tcl_WideInt wide;
    CffiResult ret;

    CFFI_ASSERT(objc == 4 || objc == 5);

    CHECK(Tcl_GetWideIntFromObj(ipCtxP->interp, objv[3], &wide));

    CHECK(CffiEnumGetMap(ipCtxP, objv[2], 0, &entries));

    /* If a default has been supplied, we will return it on failure. */
    ret = CffiEnumMemberFindReverse(
        objc == 4 ? ipCtxP->interp : NULL, entries, wide, &nameObj);
    if (ret != TCL_OK) {
        if (objc == 4)
            return ret;
        nameObj = objv[4];
    }
    Tcl_SetObjResult(ipCtxP->interp, nameObj);
    return TCL_OK;
}

static CffiResult
CffiEnumFlagsCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Obj *enumObj;
    Tcl_Obj *fqnObj;
    Tcl_Obj **names;
    Tcl_Size nNames;
    Tcl_Size i;
    int ret;

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
        Tcl_ListObjAppendElement(NULL, enumObj, Tcl_NewIntObj((Tcl_WideInt)1 << i));
    }

    Tcl_IncrRefCount(enumObj);
    ret = CffiNameObjAdd(
        ip, &ipCtxP->scope.enums, objv[2], "Enum", enumObj, &fqnObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, fqnObj);
    else
        Tcl_DecrRefCount(enumObj);
    return ret;
}


static CffiResult
CffiEnumSequenceCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Obj *enumObj;
    Tcl_Obj *fqnObj;
    Tcl_Obj **names;
    Tcl_Size nNames;
    Tcl_Size i;
    Tcl_WideInt start;
    Tcl_WideInt value;
    int ret;

    CFFI_ASSERT(objc == 4 || objc == 5);

    CHECK(CffiNameSyntaxCheck(ip, objv[2]));
    CHECK(Tcl_ListObjGetElements(ip, objv[3], &nNames, &names));

    if (objc == 5)
        CHECK(Tclh_ObjToWideInt(ip, objv[4], &start));
    else
        start = 0;


    /* We will create as list and let it be shimmered to dictionary as needed */
    enumObj = Tcl_NewListObj(2 * nNames, NULL);
    for (value = start, i = 0; i < nNames; ++value, ++i) {
        Tcl_Obj **objs;
        Tcl_Size nobjs;
        if (Tcl_ListObjGetElements(NULL, names[i], &nobjs, &objs) != TCL_OK
            || nobjs == 0 || nobjs > 2
            || CffiNameSyntaxCheck(ip, objs[0]) != TCL_OK
            || (nobjs == 2
                && Tcl_GetWideIntFromObj(ip, objs[1], &value) != TCL_OK)) {
            Tcl_DecrRefCount(enumObj);
            return Tclh_ErrorInvalidValue(ip, names[i], "Invalid enum sequence member definition.");
        }
        Tcl_ListObjAppendElement(NULL, enumObj, objs[0]);
        Tcl_ListObjAppendElement(NULL, enumObj, Tcl_NewWideIntObj(value));
    }

    Tcl_IncrRefCount(enumObj);
    ret = CffiNameObjAdd(
        ip, &ipCtxP->scope.enums, objv[2], "Enum", enumObj, &fqnObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, fqnObj);
    else
        Tcl_DecrRefCount(enumObj);
    return ret;
}

static CffiResult
CffiEnumMembersCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *entries;

    CFFI_ASSERT(objc == 3);

    CHECK(CffiEnumGetMap(ipCtxP, objv[2], 0, &entries));
    Tcl_SetObjResult(ipCtxP->interp, entries);
    return TCL_OK;
}

static CffiResult
CffiEnumNamesCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *entries;
    Tcl_Interp *ip = ipCtxP->interp;

    CFFI_ASSERT(objc == 3);

    CHECK(CffiEnumGetMap(ipCtxP, objv[2], 0, &entries));

    Tcl_DictSearch search;
    Tcl_Obj *keyObj;
    Tcl_Obj *namesObj;
    int done;
    CHECK(Tcl_DictObjFirst(ip, entries, &search, &keyObj, NULL, &done));

    namesObj = Tcl_NewListObj(0, NULL);
    while (!done) {
        Tcl_ListObjAppendElement(ip, namesObj, keyObj);
        Tcl_DictObjNext(&search, &keyObj, NULL, &done);
    }
    Tcl_DictObjDone(&search);
    Tcl_SetObjResult(ip, namesObj);
    return TCL_OK;
}


static CffiResult
CffiEnumListCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    const char *pattern;
    CffiResult ret;
    Tcl_Obj *namesObj;

    /*
     * Default pattern to "*", not NULL as the latter will list all scopes
     * we only want the ones in current namespace.
     */
    pattern = objc > 2 ? Tcl_GetString(objv[2]) : "*";
    ret = CffiNameListNames(
        ipCtxP->interp, &ipCtxP->scope.enums, pattern, &namesObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ipCtxP->interp, namesObj);
    return ret;
}

static void CffiEnumNameDeleteCallback(ClientData clientData)
{
    Tcl_Obj *objP = (Tcl_Obj *)clientData;
    if (objP)
        Tcl_DecrRefCount(objP);
}

static CffiResult
CffiEnumDeleteCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    CFFI_ASSERT(objc == 3);
    return CffiNameDeleteNames(ipCtxP->interp,
                               &ipCtxP->scope.enums,
                               Tcl_GetString(objv[2]),
                               CffiEnumNameDeleteCallback);
}

static CffiResult
CffiEnumClearCmd(CffiInterpCtx *ipCtxP, int objc, Tcl_Obj *const objv[])
{
    CFFI_ASSERT(objc == 2);
    return CffiNameDeleteNames(ipCtxP->interp,
                               &ipCtxP->scope.enums,
                               NULL,
                               CffiEnumNameDeleteCallback);
}

/* Called on interp deletion */
void
CffiEnumsCleanup(CffiInterpCtx *ipCtxP)
{
    CffiNameTableFinit(
        ipCtxP->interp, &ipCtxP->scope.enums, CffiEnumNameDeleteCallback);
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
        {"clear", 0, 0, "", CffiEnumClearCmd},
        {"define", 2, 2, "ENUM MEMBERS", CffiEnumDefineCmd},
        {"delete", 1, 1, "PATTERN", CffiEnumDeleteCmd},
        {"flags", 2, 2, "ENUM FLAGNAMES", CffiEnumFlagsCmd},
        {"list", 0, 1, "?PATTERN?", CffiEnumListCmd},
        {"members", 1, 1, "ENUM", CffiEnumMembersCmd},
        {"name", 2, 3, "ENUM VALUE ?DEFAULT?", CffiEnumNameCmd},
        {"names", 1, 1, "ENUM", CffiEnumNamesCmd},
        {"sequence", 2, 3, "ENUM MEMBERNAMES ?START?", CffiEnumSequenceCmd},
        {"value", 2, 3, "ENUM MEMBERNAME ?DEFAULT?", CffiEnumValueCmd},
        {"mask", 2, 2, "ENUM MEMBERLIST", CffiEnumMaskCmd},
        {"unmask", 2, 2, "ENUM INTEGER", CffiEnumUnmaskCmd},
        {NULL}};

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ipCtxP, objc, objv);
}
