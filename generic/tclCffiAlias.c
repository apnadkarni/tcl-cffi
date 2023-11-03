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
    const char *aliasNameP;
    CffiResult ret;
    const char *lbStr; /* Left bracket */
    char temp[CFFI_K_MAX_NAME_LENGTH+1];
    const char *message = NULL;

    aliasNameP = Tcl_GetString(aliasNameObj);
    lbStr     = strchr(aliasNameP, '[');
    if (lbStr) {
        /* array size component */
        Tcl_Size nameLen = (Tcl_Size)(lbStr - aliasNameP);
        if (nameLen >= sizeof(temp)) {
            message = "Invalid alias name.";
            goto invalid;
        }
        memmove(temp, aliasNameP, nameLen+1);
        temp[nameLen] = 0;
        aliasNameP    = temp;
    }
    ret = CffiNameLookup(ipCtxP->interp,
                         &ipCtxP->scope.aliases,
                         aliasNameP,
                         "Alias",
                         flags & CFFI_F_SKIP_ERROR_MESSAGES,
                         (ClientData *)&aliasTypeAttrP,
                         NULL);

    if (ret == TCL_ERROR)
        return 0;


    CffiTypeAndAttrsInit(typeAttrP, aliasTypeAttrP);
    /* If an array size is specified here, override one in alias def */
    if (lbStr) {
        if (CffiTypeParseArraySize(lbStr, &typeAttrP->dataType) != TCL_OK) {
            message = "Invalid array size.";
            goto invalid;
        }
    }
    return 1;

invalid:
    (void)Tclh_ErrorInvalidValue(
        ipCtxP->interp, aliasNameObj, message);
    return 0;
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
        Tcl_Size nobjs;
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

    /*
     * Various macros to detect size and signedness at compile and runtime
     */
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

#define ADDTYPEINDEX(alias_, existingIndex_)                               \
    do {                                                                   \
        if (CffiAliasAddStr(                                               \
                ipCtxP, alias_, cffiBaseTypes[existingIndex_].token, NULL) \
            != TCL_OK)                                                     \
            return TCL_ERROR;                                              \
    } while (0)

#define ADDINTTYPE(type_, alias_)          \
    do {                                \
        enum CffiBaseType typeIndex;    \
        type_ val = (type_)-1;          \
        if (val < 0)                    \
            SIGNEDTYPEINDEX(type_);     \
        else                            \
            UNSIGNEDTYPEINDEX(type_);   \
        ADDTYPEINDEX(alias_, typeIndex); \
    } while (0)

#define NS "::cffi::c::"

    s = Tcl_GetString(objP);

    if (!strcmp(s, "C")) {
        {
            enum CffiBaseType typeIndex;
            UNSIGNEDTYPEINDEX(_Bool);
            ADDTYPEINDEX(NS "_Bool", typeIndex);
        }
        CffiAliasAddStr(ipCtxP, NS "bool", NS "_Bool", NULL);
        ADDINTTYPE(size_t, NS "size_t");
#ifdef _WIN32
        if (sizeof(SSIZE_T) == sizeof(int)) {
            CffiAliasAddStr(ipCtxP, NS "ssize_t", "int", NULL);
        } else if (sizeof(SSIZE_T) == sizeof(long)) {
            CffiAliasAddStr(ipCtxP, NS "ssize_t", "long", NULL);
        } else if (sizeof(SSIZE_T) == sizeof(long long)) {
            CffiAliasAddStr(ipCtxP, NS "ssize_t", "longlong", NULL);
        }
#else
        ADDINTTYPE(ssize_t, NS "ssize_t");
#endif
        ADDINTTYPE(int8_t, NS "int8_t");
        ADDINTTYPE(uint8_t, NS "uint8_t");
        ADDINTTYPE(int16_t, NS "int16_t");
        ADDINTTYPE(uint16_t, NS "uint16_t");
        ADDINTTYPE(int32_t, NS "int32_t");
        ADDINTTYPE(uint32_t, NS "uint32_t");
        ADDINTTYPE(int64_t, NS "int64_t");
        ADDINTTYPE(uint64_t, NS "uint64_t");
    }
#ifdef _WIN32
    else if (!strcmp(s, "win32")) {
        ADDINTTYPE(BOOL, NS "BOOL");
        ADDINTTYPE(BOOLEAN, NS "BOOLEAN");
        ADDINTTYPE(CHAR, NS "CHAR");
        ADDINTTYPE(BYTE, NS "BYTE");
        ADDINTTYPE(WORD, NS "WORD");
        ADDINTTYPE(DWORD, NS "DWORD");
        ADDINTTYPE(DWORD_PTR, NS "DWORD_PTR");
        ADDINTTYPE(DWORDLONG, NS "DWORDLONG");
        ADDINTTYPE(HALF_PTR, NS "HALF_PTR");
        ADDINTTYPE(INT, NS "INT");
        ADDINTTYPE(INT_PTR, NS "INT_PTR");
        ADDINTTYPE(LONG, NS "LONG");
        ADDINTTYPE(LONGLONG, NS "LONGLONG");
        ADDINTTYPE(LONG_PTR, NS "LONG_PTR");
        ADDINTTYPE(LPARAM, NS "LPARAM");
        ADDINTTYPE(LRESULT, NS "LRESULT");
        ADDINTTYPE(SHORT, NS "SHORT");
        ADDINTTYPE(SIZE_T, NS "SIZE_T");
        ADDINTTYPE(SSIZE_T, NS "SSIZE_T");
        ADDINTTYPE(UCHAR, NS "UCHAR");
        ADDINTTYPE(UINT, NS "UINT");
        ADDINTTYPE(UINT_PTR, NS "UINT_PTR");
        ADDINTTYPE(ULONG, NS "ULONG");
        ADDINTTYPE(ULONGLONG, NS "ULONGLONG");
        ADDINTTYPE(ULONG_PTR, NS "ULONG_PTR");
        ADDINTTYPE(USHORT, NS "USHORT");
        ADDINTTYPE(WPARAM, NS "WPARAM");

        if (CffiAliasAddStr(ipCtxP, NS "LPVOID", "pointer unsafe", NULL) != TCL_OK)
            return TCL_ERROR;
        if (CffiAliasAddStr(ipCtxP, NS "HANDLE", "pointer." NS "HANDLE", NULL) != TCL_OK)
            return TCL_ERROR;
    }
#endif
    else if (!strcmp(s, "posix")) {
        /* sys/types.h */
        ADDINTTYPE(dev_t, NS "dev_t");
        ADDINTTYPE(ino_t, NS "ino_t");
        ADDINTTYPE(time_t, NS "time_t");
        ADDINTTYPE(off_t, NS "off_t");
#ifndef _WIN32
        ADDINTTYPE(blkcnt_t, NS "blkcnt_t");
        ADDINTTYPE(blksize_t, NS "blksize_t");
        ADDINTTYPE(clock_t, NS "clock_t");
#ifdef NOTYET
        /* MacOS Catalina does not define this. Should we keep it in? */
        ADDINTTYPE(clockid_t, NS "clockid_t");
#endif
        ADDINTTYPE(fsblkcnt_t, NS "fsblkcnt_t");
        ADDINTTYPE(fsfilcnt_t, NS "fsfilcnt_t");
        ADDINTTYPE(gid_t, NS "gid_t");
        ADDINTTYPE(id_t, NS "id_t");
        ADDINTTYPE(key_t, NS "key_t");
        ADDINTTYPE(mode_t, NS "mode_t");
        ADDINTTYPE(nlink_t, NS "nlink_t");
        ADDINTTYPE(pid_t, NS "pid_t");
        ADDINTTYPE(size_t, NS "size_t");
        ADDINTTYPE(ssize_t, NS "ssize_t");
        ADDINTTYPE(suseconds_t, NS "suseconds_t");
        ADDINTTYPE(uid_t, NS "uid_t");
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
