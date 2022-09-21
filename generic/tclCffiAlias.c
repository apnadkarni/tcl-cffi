/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"
#include <sys/types.h>
#include <stdbool.h>

/* Function: CffiAliasGet
 * Checks for the existence of a type definition and returns its internal form.
 *
 * Parameters:
 *   ipCtxP - Interpreter context
 *   aliasNameObj - alias name
 *   typeAttrP - pointer to structure to hold parsed information. Note this
 *               structure is *overwritten*, not merged.
 *   flags - if CFFI_F_SKIP_ERROR_MESSAGES is set, error messages
 *           are not stored in the interpreter.
 * Returns:
 * If a type definition of the specified name is found, it is stored
 * in *typeAttrP* and the function returns 1. Otherwise, the function
 * returns 0.
 */
int
CffiAliasGet(CffiInterpCtx *ipCtxP,
             Tcl_Obj *aliasNameObj,
             CffiTypeAndAttrs *typeAttrP,
             CffiFlags flags)
{
    CffiTypeAndAttrs *aliasTypeAttrP;
    CffiResult ret;
    ret = CffiNameLookup(ipCtxP->interp,
                         &ipCtxP->scope.aliases,
                         Tcl_GetString(aliasNameObj),
                         "Alias",
                         flags & CFFI_F_SKIP_ERROR_MESSAGES,
                         (ClientData *)&aliasTypeAttrP,
                         NULL);
    if (ret == TCL_ERROR)
        return 0;
    CffiTypeAndAttrsInit(typeAttrP, aliasTypeAttrP);
    return 1;
}

/* Function: CffiAliasAdd
 * Adds a new alias, overwriting any existing ones.
 *
 * The alias is added in the current default scope.
 *
 * The alias must not match a base type name.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * nameObj - alias to add
 * typedefObj - target type definition
 * fqnObjP - location to store the final name (qualified if necessary). May
 *    be NULL. Note its reference count must NOT be decremented by caller without
 *    incrementing.
 * Returns:
 * *TCL_OK* or *TCL_ERROR*
 */
CffiResult
CffiAliasAdd(CffiInterpCtx *ipCtxP,
             Tcl_Obj *nameObj,
             Tcl_Obj *typedefObj,
             Tcl_Obj **fqnObjP)
{
    CffiTypeAndAttrs *typeAttrsP;
    const CffiBaseTypeInfo *baseTypeInfoP;
    CffiResult ret;
    Tcl_Obj *fqnObj = NULL;

    CHECK(CffiNameSyntaxCheck(ipCtxP->interp, nameObj));
    baseTypeInfoP = CffiBaseTypeInfoGet(NULL, nameObj);
    if (baseTypeInfoP) {
        return Tclh_ErrorExists(
            ipCtxP->interp, "Type or alias", nameObj, NULL);
    }

    typeAttrsP = ckalloc(sizeof(*typeAttrsP));
    if (CffiTypeAndAttrsParse(ipCtxP,
                              typedefObj,
                              CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_RETURN
                                  | CFFI_F_TYPE_PARSE_FIELD,
                              typeAttrsP)
        != TCL_OK) {
        ckfree(typeAttrsP);
        return TCL_ERROR;
    }

    ret = CffiNameObjAdd(ipCtxP->interp,
                         &ipCtxP->scope.aliases,
                         nameObj,
                         "Alias",
                         typeAttrsP,
                         &fqnObj);

    if (ret != TCL_OK) {
        /* Only permit if definition is the same */
        CffiTypeAndAttrs *oldP;
        Tcl_Obj *oldObj;
        Tcl_Obj *newObj;
        int different;

        /* Find existing definition */
        ret = CffiNameLookup(ipCtxP->interp,
                             &ipCtxP->scope.aliases,
                             Tcl_GetString(nameObj),
                             "Alias",
                             CFFI_F_SKIP_ERROR_MESSAGES,
                             (ClientData *)&oldP,
                             &fqnObj);
        if (ret == TCL_OK) {
            /* TBD - maybe write a typeAttrs comparison function */
            oldObj    = CffiTypeAndAttrsUnparse(oldP);
            newObj    = CffiTypeAndAttrsUnparse(typeAttrsP);
            different = strcmp(Tcl_GetString(oldObj), Tcl_GetString(newObj));
            Tcl_DecrRefCount(oldObj);
            Tcl_DecrRefCount(newObj);
            /* If same do not generate error */
            if (different)
                ret = Tclh_ErrorExists(
                    ipCtxP->interp,
                    "Alias",
                    nameObj,
                    "Alias exists with a different definition.");
            else {
                ret = TCL_OK;
                Tcl_ResetResult(ipCtxP->interp); /* Erase recorded error */
            }
            /* NOTE - fqnObj NOT to be freed even if error returned as hash table is holding it */
        }
        else {
            /* Should not really happen that we could not add but could not find either */
            /* Stay with the reported erro */
        }
        CffiTypeAndAttrsCleanup(typeAttrsP);
        ckfree(typeAttrsP);
    }

    if (ret == TCL_OK && fqnObjP) {
        CFFI_ASSERT(fqnObj);
        *fqnObjP = fqnObj;
    }

    return ret;
}

/* Like CffiAliasAdd but with char* arguments */
CffiResult
CffiAliasAddStr(CffiInterpCtx *ipCtxP,
                const char *nameStr,
                const char *typedefStr,
                Tcl_Obj **fqnObjP)
{
    CffiResult ret;
    Tcl_Obj *nameObj;
    Tcl_Obj *typedefObj;
    nameObj = Tcl_NewStringObj(nameStr, -1);
    Tcl_IncrRefCount(nameObj);
    typedefObj = Tcl_NewStringObj(typedefStr, -1);
    Tcl_IncrRefCount(typedefObj);
    ret = CffiAliasAdd(ipCtxP, nameObj, typedefObj, fqnObjP);
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
    Tcl_Obj *fqnObj;
    CffiResult ret;
    CFFI_ASSERT(objc == 3 || objc == 4);

    if (objc == 4) {
        ret = CffiAliasAdd(ipCtxP, objv[2], objv[3], &fqnObj);
        if (ret == TCL_OK)
            Tcl_SetObjResult(ip, fqnObj);
    }
    else {
        /* Dup to protect list from shimmering away */
        Tcl_Obj *defsObj = Tcl_DuplicateObj(objv[2]);
        Tcl_Obj **objs;
        int nobjs;
        Tcl_IncrRefCount(defsObj);
        ret = Tcl_ListObjGetElements(ip, defsObj, &nobjs, &objs);
        if (ret == TCL_OK) {
            if (nobjs & 1) {
                ret = Tclh_ErrorInvalidValue(
                    ip, defsObj, "Invalid alias dictionary, missing definition for alias.");
            }
            else {
                int i;
                Tcl_Obj *resultObj = Tcl_NewListObj(nobjs / 2, NULL);
                for (i = 0; i < nobjs; i += 2) {
                    ret = CffiAliasAdd(ipCtxP, objs[i], objs[i + 1], &fqnObj);
                    if (ret != TCL_OK)
                        break;
                    Tcl_ListObjAppendElement(ip, resultObj, fqnObj);
                }
                if (ret == TCL_OK)
                    Tcl_SetObjResult(ip, resultObj);
                else
                    Tcl_DecrRefCount(resultObj);
            }
        }
        Tcl_DecrRefCount(defsObj);
    }
    return ret;
}

static CffiResult
CffiAliasBodyCmd(CffiInterpCtx *ipCtxP,
                 Tcl_Interp *ip,
                 int objc,
                 Tcl_Obj *const objv[])
{
    CffiTypeAndAttrs *typeAttrsP;

    CFFI_ASSERT(objc == 3);

    CHECK(CffiNameLookup(ip,
                         &ipCtxP->scope.aliases,
                         Tcl_GetString(objv[2]),
                         "Alias",
                         0,
                         (ClientData *)&typeAttrsP,
                         NULL));
    Tcl_SetObjResult(ip, CffiTypeAndAttrsUnparse(typeAttrsP));
    return TCL_OK;
}

static CffiResult
CffiAliasListCmd(CffiInterpCtx *ipCtxP,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
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
        ipCtxP->interp, &ipCtxP->scope.aliases, pattern, &namesObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ipCtxP->interp, namesObj);
    return ret;
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

static void CffiAliasNameDeleteCallback(ClientData clientData)
{
    CffiTypeAndAttrs *typeAttrsP = (CffiTypeAndAttrs *)clientData;
    CffiTypeAndAttrsCleanup(typeAttrsP);
    ckfree(typeAttrsP);
}

static CffiResult
CffiAliasDeleteCmd(CffiInterpCtx *ipCtxP,
                    Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[])
{
    CFFI_ASSERT(objc == 3);
    return CffiNameDeleteNames(ipCtxP->interp,
                               &ipCtxP->scope.aliases,
                               Tcl_GetString(objv[2]),
                               CffiAliasNameDeleteCallback);
}

static CffiResult
CffiAliasClearCmd(CffiInterpCtx *ipCtxP,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[])
{
    CFFI_ASSERT(objc == 2);
    return CffiNameDeleteNames(ipCtxP->interp,
                               &ipCtxP->scope.aliases,
                               NULL,
                               CffiAliasNameDeleteCallback);
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

#define SIGNEDTYPEINDEX(type_)                              \
    do {                                                    \
        if (sizeof(type_) == sizeof(signed char))           \
            typeIndex = CFFI_K_TYPE_SCHAR;                  \
        else if (sizeof(type_) == sizeof(signed short))     \
            typeIndex = CFFI_K_TYPE_SHORT;                  \
        else if (sizeof(type_) == sizeof(signed int))       \
            typeIndex = CFFI_K_TYPE_INT;                    \
        else if (sizeof(type_) == sizeof(signed long))      \
            typeIndex = CFFI_K_TYPE_LONG;                   \
        else if (sizeof(type_) == sizeof(signed long long)) \
            typeIndex = CFFI_K_TYPE_LONGLONG;               \
        else                                                \
            CFFI_ASSERT(0);                                 \
    } while (0)

#define UNSIGNEDTYPEINDEX(type_)                              \
    do {                                                      \
        if (sizeof(type_) == sizeof(unsigned char))           \
            typeIndex = CFFI_K_TYPE_UCHAR;                    \
        else if (sizeof(type_) == sizeof(unsigned short))     \
            typeIndex = CFFI_K_TYPE_USHORT;                   \
        else if (sizeof(type_) == sizeof(unsigned int))       \
            typeIndex = CFFI_K_TYPE_UINT;                     \
        else if (sizeof(type_) == sizeof(unsigned long))      \
            typeIndex = CFFI_K_TYPE_ULONG;                    \
        else if (sizeof(type_) == sizeof(unsigned long long)) \
            typeIndex = CFFI_K_TYPE_ULONGLONG;                \
        else                                                  \
            CFFI_ASSERT(0);                                   \
    } while (0)

#define ADDTYPEINDEX(newtype_, existingIndex_)                                \
    do {                                                                      \
        if (CffiAliasAddStr(                                                  \
                ipCtxP, #newtype_, cffiBaseTypes[existingIndex_].token, NULL) \
            != TCL_OK)                                                        \
            return TCL_ERROR;                                                 \
    } while (0)

#define ADDINTTYPE(type_)               \
    do {                                \
        enum CffiBaseType typeIndex;    \
        type_ val = (type_)-1;          \
        if (val < 0)                    \
            SIGNEDTYPEINDEX(type_);     \
        else                            \
            UNSIGNEDTYPEINDEX(type_);   \
        ADDTYPEINDEX(type_, typeIndex); \
    } while (0)

    s = Tcl_GetString(objP);

    if (!strcmp(s, "C")) {
        {
            enum CffiBaseType typeIndex;
            UNSIGNEDTYPEINDEX(_Bool);
            ADDTYPEINDEX(_Bool, typeIndex);
        }
        CffiAliasAddStr(ipCtxP, "bool", "_Bool", NULL);
        ADDINTTYPE(size_t);
#ifdef _WIN32
        ADDINTTYPE(SSIZE_T);
        CffiAliasAddStr(ipCtxP, "ssize_t", "SSIZE_T", NULL);
#else
        ADDINTTYPE(ssize_t);
        CffiAliasAddStr(ipCtxP, "SSIZE_T", "ssize_t", NULL);
#endif
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

        if (CffiAliasAddStr(ipCtxP, "LPVOID", "pointer", NULL) != TCL_OK)
            return TCL_ERROR;
        if (CffiAliasAddStr(ipCtxP, "HANDLE", "pointer.HANDLE", NULL) != TCL_OK)
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
CffiAliasesCleanup(CffiInterpCtx *ipCtxP)
{
    CffiNameTableFinit(
        ipCtxP->interp, &ipCtxP->scope.aliases, CffiAliasNameDeleteCallback);
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
        {"clear", 0, 0, "", CffiAliasClearCmd},
        {"body", 1, 1, "ALIAS", CffiAliasBodyCmd},
        {"define", 1, 2, "(ALIASDEFS | ALIAS DEFINITION)", CffiAliasDefineCmd},
        {"delete", 1, 1, "PATTERN", CffiAliasDeleteCmd},
        {"list", 0, 1, "?PATTERN?", CffiAliasListCmd},
        {"load", 1, 1, "ALIASSET", CffiAliasLoadCmd},
        {NULL}
    };

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ipCtxP, ip, objc, objv);
}
