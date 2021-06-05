/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"
#include <sys/types.h>

/* Function: CffiAliasGet
 * Checks for the existence of a type definition and returns its internal form.
 *
 * Parameters:
 *   ipCtxP - Interpreter context
 *   aliasNameObj - typedef name
 *   typeAttrP - pointer to structure to hold parsed information. Note this
 *               structure is *overwritten*, not merged.
 *
 * Returns:
 * If a type definition of the specified name is found, it is stored
 * in *typeAttrP* and the function returns 1. Otherwise, the function
 * returns 0.
 */
int
CffiAliasGet(CffiInterpCtx *ipCtxP,
             Tcl_Obj *aliasNameObj,
             CffiTypeAndAttrs *typeAttrP)
{
    Tcl_HashEntry *heP;
    CffiTypeAndAttrs *heValueP;

    heP = Tcl_FindHashEntry(&ipCtxP->aliases, aliasNameObj);
    if (heP == NULL)
        return 0;
    heValueP = Tcl_GetHashValue(heP);
    CffiTypeAndAttrsInit(typeAttrP, heValueP);
    return 1;
}

/* Function: CffiAliasAdd
 * Adds a new alias, overwriting any existing ones.
 *
 * The alias must not match a base type name.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * nameObj - typedef to add
 * typedefObj - type definition
 *
 * Returns:
 *
 */
CffiResult
CffiAliasAdd(CffiInterpCtx *ipCtxP, Tcl_Obj *nameObj, Tcl_Obj *typedefObj)
{
    Tcl_HashEntry *heP;
    CffiTypeAndAttrs *typeAttrsP;
    int new_entry;
    const CffiBaseTypeInfo *baseTypeInfoP;

    CHECK(CffiNameSyntaxCheck(ipCtxP->interp, nameObj));

    baseTypeInfoP = CffiBaseTypeInfoGet(NULL, nameObj);
    if (baseTypeInfoP) {
        return Tclh_ErrorExists(
            ipCtxP->interp, "Type or alias", nameObj, NULL);
    }


    typeAttrsP = ckalloc(sizeof(*typeAttrsP));
    if (CffiTypeAndAttrsParse(ipCtxP,
                              typedefObj,
                              CFFI_F_TYPE_PARSE_PARAM
                              | CFFI_F_TYPE_PARSE_RETURN
                              | CFFI_F_TYPE_PARSE_FIELD,
                              typeAttrsP)
        != TCL_OK) {
        ckfree(typeAttrsP);
        return TCL_ERROR;
    }

    heP = Tcl_CreateHashEntry(&ipCtxP->aliases, (char *) nameObj, &new_entry);
    if (! new_entry) {
        CffiTypeAndAttrs *oldP = Tcl_GetHashValue(heP);
        CffiTypeAndAttrsCleanup(oldP);
        ckfree(oldP);
    }
    Tcl_SetHashValue(heP, typeAttrsP);
    return TCL_OK;
}

/* Like CffiAliasAdd but with char* arguments */
CffiResult
CffiAliasAddStr(CffiInterpCtx *ipCtxP, const char *nameStr, const char *typedefStr)
{
    CffiResult ret;
    Tcl_Obj *nameObj;
    Tcl_Obj *typedefObj;
    nameObj = Tcl_NewStringObj(nameStr, -1);
    Tcl_IncrRefCount(nameObj);
    typedefObj = Tcl_NewStringObj(typedefStr, -1);
    Tcl_IncrRefCount(typedefObj);
    ret = CffiAliasAdd(ipCtxP, nameObj, typedefObj);
    Tcl_DecrRefCount(nameObj);
    Tcl_DecrRefCount(typedefObj);
    return ret;
}

static CffiResult
CffiAliasDefineCmd(CffiInterpCtx *ipCtxP,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;

    CFFI_ASSERT(objc == 4);

    heP = Tcl_FindHashEntry(&ipCtxP->aliases, objv[2]);
    if (heP)
        return Tclh_ErrorExists(ip, "Type or alias", objv[2], NULL);
    return CffiAliasAdd(ipCtxP, objv[2], objv[3]);
}

static CffiResult
CffiAliasBodyCmd(CffiInterpCtx *ipCtxP,
                 Tcl_Interp *ip,
                 int objc,
                 Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;
    CffiTypeAndAttrs *typeAttrsP;

    CFFI_ASSERT(objc == 3);

    heP = Tcl_FindHashEntry(&ipCtxP->aliases, objv[2]);
    if (heP == NULL)
        return Tclh_ErrorNotFound(ip, "Type or alias", objv[2], NULL);
    typeAttrsP = Tcl_GetHashValue(heP);
    Tcl_SetObjResult(ip, CffiTypeAndAttrsUnparse(typeAttrsP));
    return TCL_OK;
}

static CffiResult
CffiAliasListCmd(CffiInterpCtx *ipCtxP,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;
    Tcl_HashSearch hSearch;
    const char *pattern;
    Tcl_HashTable *typedefsP = &ipCtxP->aliases;
    Tcl_Obj *resultObj = Tcl_NewListObj(0, NULL);

    pattern = objc > 2 ? Tcl_GetString(objv[2]) : NULL;
    for (heP = Tcl_FirstHashEntry(typedefsP, &hSearch);
         heP != NULL; heP = Tcl_NextHashEntry(&hSearch)) {
        /* If a pattern was specified, only return those */
        if (pattern) {
            Tcl_Obj *key = Tcl_GetHashKey(typedefsP, heP);
            if (! Tcl_StringMatch(Tcl_GetString(key), pattern))
                continue;
        }

        Tcl_ListObjAppendElement(ipCtxP->interp,
                                 resultObj,
                                 (Tcl_Obj *)Tcl_GetHashKey(typedefsP, heP));
    }
    Tcl_SetObjResult(ipCtxP->interp, resultObj);
    return TCL_OK;
}

static CffiResult
CffiAliasLoadCmd(CffiInterpCtx *ipCtxP,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    CFFI_ASSERT(objc == 3);
    return CffiAddBuiltinAliases(ipCtxP, objv[2]);
}

static CffiResult
CffiAliasDeleteCmd(CffiInterpCtx *ipCtxP,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    Tcl_HashEntry *heP;
    Tcl_HashSearch hSearch;
    const char *pattern;
    Tcl_HashTable *tableP = &ipCtxP->aliases;

    CFFI_ASSERT(objc == 3);
    heP = Tcl_FindHashEntry(tableP, objv[2]);
    if (heP) {
        CffiTypeAndAttrs *typeAttrsP;
        typeAttrsP = Tcl_GetHashValue(heP);
        CffiTypeAndAttrsCleanup(typeAttrsP);
        ckfree(typeAttrsP);
        Tcl_DeleteHashEntry(heP);
        return TCL_OK;
    }

    /* Check if glob pattern */
    pattern = Tcl_GetString(objv[2]);
    for (heP = Tcl_FirstHashEntry(tableP, &hSearch);
         heP != NULL; heP = Tcl_NextHashEntry(&hSearch)) {
        Tcl_Obj *key = Tcl_GetHashKey(tableP, heP);
        if (Tcl_StringMatch(Tcl_GetString(key), pattern)) {
            CffiTypeAndAttrs *typeAndAttrsP = Tcl_GetHashValue(heP);
            CffiTypeAndAttrsCleanup(typeAndAttrsP);
            ckfree(typeAndAttrsP);
            Tcl_DeleteHashEntry(heP);
        }
    }

    return TCL_OK;
}


/* Function: CffiAddBuiltinAliases
 * Adds predefined type definitions.
 *
 * Parameters:
 * ipCtxP - interpreter context containing typeAliases
 */
int
CffiAddBuiltinAliases(CffiInterpCtx *ipCtxP, Tcl_Obj *objP)
{
    const char *s;

#define SIGNEDTYPEINDEX(type_)                                  \
    do {                                                        \
        if (sizeof(type_) == sizeof(signed char))               \
            typeIndex = CFFI_K_TYPE_SCHAR;                     \
        else if (sizeof(type_) == sizeof(signed short))         \
            typeIndex = CFFI_K_TYPE_SHORT;                    \
        else if (sizeof(type_) == sizeof(signed int))           \
            typeIndex = CFFI_K_TYPE_INT;                      \
        else if (sizeof(type_) == sizeof(signed long))          \
            typeIndex = CFFI_K_TYPE_LONG;                     \
        else if (sizeof(type_) == sizeof(signed long long))     \
            typeIndex = CFFI_K_TYPE_LONGLONG;                 \
        else                                                    \
            CFFI_ASSERT(0);                                    \
    } while (0)

#define UNSIGNEDTYPEINDEX(type_)                                \
    do {                                                        \
        if (sizeof(type_) == sizeof(unsigned char))             \
            typeIndex = CFFI_K_TYPE_UCHAR;                     \
        else if (sizeof(type_) == sizeof(unsigned short))       \
            typeIndex = CFFI_K_TYPE_USHORT;                    \
        else if (sizeof(type_) == sizeof(unsigned int))         \
            typeIndex = CFFI_K_TYPE_UINT;                      \
        else if (sizeof(type_) == sizeof(unsigned long))        \
            typeIndex = CFFI_K_TYPE_ULONG;                     \
        else if (sizeof(type_) == sizeof(unsigned long long))   \
            typeIndex = CFFI_K_TYPE_ULONGLONG;                 \
        else                                                    \
            CFFI_ASSERT(0);                                    \
    } while (0)

#define ADDTYPEINDEX(newtype_, existingIndex_)                            \
    do {                                                                  \
        if (CffiAliasAddStr(                                           \
                ipCtxP, #newtype_, cffiBaseTypes[existingIndex_].token) \
            != TCL_OK)                                                    \
            return TCL_ERROR;                                             \
    } while (0)

#define ADDINTTYPE(type_)                                                    \
    do {                                                                     \
        enum CffiBaseType typeIndex;                                        \
        type_ val = (type_)-1;                                               \
        if (val < 0)                                                         \
            SIGNEDTYPEINDEX(type_);                                          \
        else                                                                 \
            UNSIGNEDTYPEINDEX(type_);                                        \
        ADDTYPEINDEX(type_, typeIndex);                                          \
    } while (0)

    s = Tcl_GetString(objP);

    if (!strcmp(s, "C")) {
        ADDINTTYPE(size_t);
        ADDINTTYPE(int8_t);
        ADDINTTYPE(uint8_t);
        ADDINTTYPE(int16_t);
        ADDINTTYPE(uint16_t);
        ADDINTTYPE(int32_t);
        ADDINTTYPE(uint32_t);
        ADDINTTYPE(int64_t);
        ADDINTTYPE(uint64_t);
    }
#ifdef _WIN32
    else if (!strcmp(s, "win32")) {
        ADDINTTYPE(BOOL);
        ADDINTTYPE(BOOLEAN);
        ADDINTTYPE(CHAR);
        ADDINTTYPE(BYTE);
        ADDINTTYPE(WORD);
        ADDINTTYPE(DWORD);
        ADDINTTYPE(DWORD_PTR);
        ADDINTTYPE(DWORDLONG);
        ADDINTTYPE(HALF_PTR);
        ADDINTTYPE(INT);
        ADDINTTYPE(INT_PTR);
        ADDINTTYPE(LONG);
        ADDINTTYPE(LONGLONG);
        ADDINTTYPE(LONG_PTR);
        ADDINTTYPE(LPARAM);
        ADDINTTYPE(LRESULT);
        ADDINTTYPE(SHORT);
        ADDINTTYPE(SIZE_T);
        ADDINTTYPE(SSIZE_T);
        ADDINTTYPE(UCHAR);
        ADDINTTYPE(UINT);
        ADDINTTYPE(UINT_PTR);
        ADDINTTYPE(ULONG);
        ADDINTTYPE(ULONGLONG);
        ADDINTTYPE(ULONG_PTR);
        ADDINTTYPE(USHORT);
        ADDINTTYPE(WPARAM);

        if (CffiAliasAddStr(ipCtxP, "LPVOID", "pointer") != TCL_OK)
            return TCL_ERROR;
        if (CffiAliasAddStr(ipCtxP, "HANDLE", "pointer.HANDLE") != TCL_OK)
            return TCL_ERROR;
        if (CffiAliasAddStr(ipCtxP, "LPSTR", "string") != TCL_OK)
            return TCL_ERROR;
        if (CffiAliasAddStr(ipCtxP, "LPWSTR", "unistring") != TCL_OK)
            return TCL_ERROR;
    }
#endif
    else if (!strcmp(s, "posix")) {
        /* sys/types.h */
#ifdef _WIN32
        /* POSIX defines on Windows */
        ADDINTTYPE(time_t);
        ADDINTTYPE(ino_t);
        ADDINTTYPE(off_t);
        ADDINTTYPE(dev_t);
#else
        ADDINTTYPE(blkcnt_t);
        ADDINTTYPE(blksize_t);
        ADDINTTYPE(clock_t);
#ifdef NOTYET
        /* MacOS Catalina does not define this. Should we keep it in? */
        ADDINTTYPE(clockid_t);
#endif
        ADDINTTYPE(dev_t);
        ADDINTTYPE(fsblkcnt_t);
        ADDINTTYPE(fsfilcnt_t);
        ADDINTTYPE(gid_t);
        ADDINTTYPE(id_t);
        ADDINTTYPE(ino_t);
        ADDINTTYPE(key_t);
        ADDINTTYPE(mode_t);
        ADDINTTYPE(nlink_t);
        ADDINTTYPE(off_t);
        ADDINTTYPE(pid_t);
        ADDINTTYPE(size_t);
        ADDINTTYPE(ssize_t);
        ADDINTTYPE(suseconds_t);
        ADDINTTYPE(time_t);
        ADDINTTYPE(uid_t);
#endif
    }
    else {
        return Tclh_ErrorInvalidValue(
            ipCtxP->interp, objP, "Unknown predefined alias set.");
    }

    return TCL_OK;
}

/* Called on interp deletion */
void
CffiAliasesCleanup(Tcl_HashTable *typeAliasTableP)
{
    Tcl_HashEntry *heP;
    Tcl_HashSearch hSearch;
    for (heP = Tcl_FirstHashEntry(typeAliasTableP, &hSearch);
         heP != NULL; heP = Tcl_NextHashEntry(&hSearch)) {
        CffiTypeAndAttrs *typeAndAttrsP = Tcl_GetHashValue(heP);
        CffiTypeAndAttrsCleanup(typeAndAttrsP);
        ckfree(typeAndAttrsP);
    }
    Tcl_DeleteHashTable(typeAliasTableP);
}
CffiResult
CffiAliasObjCmd(ClientData cdata,
                Tcl_Interp *ip,
                int objc,
                Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    int cmdIndex;
    static const Tclh_SubCommand subCommands[] = {
        {"body", 1, 1, "ALIAS", CffiAliasBodyCmd},
        {"define", 2, 2, "ALIAS DEFINITION", CffiAliasDefineCmd},
        {"delete", 1, 1, "PATTERN", CffiAliasDeleteCmd},
        {"list", 0, 1, "?PATTERN?", CffiAliasListCmd},
        {"load", 1, 1, "ALIASSET", CffiAliasLoadCmd},
        {NULL}
    };

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ipCtxP, ip, objc, objv);
}
