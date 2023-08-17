/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

static CffiResult
CffiSymbolsDestroyCmd(Tcl_Interp *ip,
                    int objc,
                    Tcl_Obj *const objv[],
                    DLSyms *dlsP)
{
    /*
    * objv[0] is the command name for the loaded symbols file. Deleteing
    * the command will also release associated resources like dlsP
    */
    if (Tcl_DeleteCommand(ip, Tcl_GetString(objv[0])) == 0)
        return TCL_OK;
    else
        return Tclh_ErrorOperFailed(ip, "delete", objv[0], NULL);
}

static CffiResult
CffiSymbolsCountCmd(Tcl_Interp *ip, int objc, Tcl_Obj *const objv[], DLSyms *dlsP)
{
    Tcl_SetObjResult(ip, Tcl_NewIntObj(dlSymsCount(dlsP)));
    return TCL_OK;
}

static CffiResult
CffiSymbolsIndexCmd(Tcl_Interp *ip, int objc, Tcl_Obj *const objv[], DLSyms *dlsP)
{
    int ival;
    const char *symName;

    CHECK(Tcl_GetIntFromObj(ip, objv[2], &ival));
    /*
     * For at least one executable format (PE), dyncall 1.2 does not check
     * index range so do so ourselves.
     */
    if (ival < 0 || ival >= dlSymsCount(dlsP)) {
        return Tclh_ErrorNotFound(
            ip, "Symbol index", objv[2], "No symbol at specified index.");
    }
    symName = dlSymsName(dlsP, ival);
    if (symName != NULL) {
        Tcl_SetResult(ip, (char *)symName, TCL_VOLATILE);
    }
    return TCL_OK;
}

static CffiResult
CffiSymbolsAtAddressCmd(Tcl_Interp *ip, int objc, Tcl_Obj *const objv[], DLSyms *dlsP)
{
    Tcl_WideInt wide;
    const char *symName;

    /* TBD - use pointer type instead of WideInt? */
    CHECK(Tcl_GetWideIntFromObj(ip, objv[2], &wide));

    symName = dlSymsNameFromValue(dlsP, (void *) (intptr_t) wide);
    if (symName == NULL)
        return Tclh_ErrorNotFound(
            ip,
            "Address",
            objv[2],
            "No symbol at specified address or library not loaded.");
    Tcl_SetResult(ip, (char *)symName, TCL_VOLATILE);
    return TCL_OK;
}

static CffiResult
CffiSymbolsInstanceCmd(ClientData cdata,
                        Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[])
{
    DLSyms *dlsP = (DLSyms *)cdata;
    static const Tclh_SubCommand subCommands[] = {
        {"destroy", 0, 0, "", CffiSymbolsDestroyCmd},
        {"count", 0, 0, "", CffiSymbolsCountCmd},
        {"index", 1, 1, "INDEX", CffiSymbolsIndexCmd},
        {"ataddress", 1, 1, "ADDRESS", CffiSymbolsAtAddressCmd},
        {NULL},
    };
    int cmdIndex;

    /* TBD - check dlsP validity?*/

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ip, objc, objv, dlsP);
}

static void
CffiSymbolsInstanceDeleter(ClientData cdata)
{
    dlSymsCleanup((DLSyms*)cdata);
}

/* Function: CffiSymbolsObjCmd
 * Implements the script level *dll* command.
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
CffiDyncallSymbolsObjCmd(ClientData cdata,
                   Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[])
{
    DLSyms *dlsP;
    Tcl_Obj *nameObj;
    Tcl_Obj *pathObj;
    static const Tclh_SubCommand subCommands[] = {
        {"new", 1, 1, "?DLLPATH?", NULL},
        {"create", 2, 2, "OBJNAME ?DLLPATH?", NULL},
        {NULL}
    };
    int cmdIndex;
    int ret;
    static unsigned int  name_generator; /* No worries about thread safety as
                                            generated names are interp-local */

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));

    pathObj = NULL;
    if (cmdIndex == 0) {
        /* new */
        nameObj = Tcl_ObjPrintf("::" CFFI_NAMESPACE "::syms%u", ++name_generator);
        if (objc > 2)
            pathObj = objv[2];
    }
    else {
        /* create */
        nameObj = Tclh_NsQualifyNameObj(ip, objv[2], NULL);
        if (objc > 3)
            pathObj = objv[3];
    }
    Tcl_IncrRefCount(nameObj);

    dlsP = dlSymsInit(pathObj ? Tcl_GetString(pathObj) : NULL);
#if defined(_WIN32)
    /* dyncall 1.2 does not protect against missing exports table in PE files */
    /* TBD this is hardcoded to dyncall 1.2 internals! */
    struct MyDLSyms_ {
        DLLib *pLib;
        const char *pBase;
        const DWORD *pNames;
        const DWORD *pFuncs;
        const unsigned short *pOrds;
        size_t count;
    };

    if (dlsP) {
        const char *base = (const char *)((struct MyDLSyms_ *)dlsP)->pLib;
        if (base) {
            IMAGE_DOS_HEADER *pDOSHeader;
            IMAGE_NT_HEADERS *pNTHeader;
            IMAGE_DATA_DIRECTORY *pExportsDataDir;
            pDOSHeader      = (IMAGE_DOS_HEADER *)base;
            pNTHeader       = (IMAGE_NT_HEADERS *)(base + pDOSHeader->e_lfanew);
            if (pNTHeader->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) {
                /* This image doesn't have an export directory table. */
                base = NULL;
            }
            else {
                /* No offset to table */
                pExportsDataDir =
                    &pNTHeader->OptionalHeader
                         .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
                if (pExportsDataDir->VirtualAddress == 0)
                    base = NULL;
            }
        }
        if (base == NULL) {
            dlSymsCleanup(dlsP);
            dlsP = NULL;
        }
    }
#endif
    if (dlsP == NULL) {
        ret = Tclh_ErrorNotFound(
            ip, "Symbols container", pathObj, "Could not find file or export table in file.");
    }
    else {
        Tcl_CreateObjCommand(ip,
                             Tcl_GetString(nameObj),
                             CffiSymbolsInstanceCmd,
                             dlsP,
                             CffiSymbolsInstanceDeleter);
        Tcl_SetObjResult(ip, nameObj);
        ret = TCL_OK;
    }
    Tcl_DecrRefCount(nameObj);
    return ret;
}

/* Function: CffiDyncallResetCall
 * Resets the argument stack prior to a call
 *
 * Parameters:
 * ip - interpreter
 * callP - Call context
 *
 * Returns:
 * Tcl result code.
 */
CffiResult CffiDyncallResetCall(Tcl_Interp *ip, CffiCall *callP)
{
    DCCallVM *vmP = callP->fnP->ipCtxP->vmP;

    dcReset(vmP);
    dcMode(vmP, callP->fnP->protoP->abi);
    return TCL_OK;
}

/* Function: CffiDyncallReloadArg
 * Loads an argument value into the dyncall argument context.
 *
 * Parameters:
 *
 * Unlike the *CffiArgPrepare* function, this expects the argument value
 * to have already been computed.
 *
 * vmP - dyncall vm context
 * argP - argument value to load
 * typeAttrsP - argument type descriptor
 */
void CffiDyncallReloadArg(CffiCall *callP, CffiArgument *argP, CffiTypeAndAttrs *typeAttrsP)
{
    DCCallVM *vmP = callP->fnP->ipCtxP->vmP;

    CFFI_ASSERT(argP->flags & CFFI_F_ARG_INITIALIZED);

#define STORE_(dcfn_, fld_)                                             \
    do {                                                                \
        if (argP->arraySize < 0) {                                      \
            if (typeAttrsP->flags & CFFI_F_ATTR_BYREF)                  \
                dcArgPointer(vmP, (DCpointer)&argP->value.u.fld_);      \
            else                                                        \
                dcfn_(vmP, argP->value.u.fld_);                         \
        }                                                               \
        else {                                                          \
            CFFI_ASSERT(typeAttrsP->flags &CFFI_F_ATTR_BYREF);          \
            dcArgPointer(vmP, (DCpointer)argP->value.u.ptr);            \
        }                                                               \
    } while (0)
    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_SCHAR: STORE_(dcArgChar, schar); break;
    case CFFI_K_TYPE_UCHAR: STORE_(dcArgChar, uchar); break;
    case CFFI_K_TYPE_SHORT: STORE_(dcArgShort, sshort); break;
    case CFFI_K_TYPE_USHORT: STORE_(dcArgShort, ushort); break;
    case CFFI_K_TYPE_INT: STORE_(dcArgInt, sint); break;
    case CFFI_K_TYPE_UINT: STORE_(dcArgInt, uint); break;
    case CFFI_K_TYPE_LONG: STORE_(dcArgLong, slong); break;
    case CFFI_K_TYPE_ULONG: STORE_(dcArgLong, ulong); break;
    case CFFI_K_TYPE_LONGLONG: STORE_(dcArgLongLong, slonglong); break;
    case CFFI_K_TYPE_ULONGLONG: STORE_(dcArgLongLong, ulonglong); break;
    case CFFI_K_TYPE_FLOAT: STORE_(dcArgFloat, flt); break;
    case CFFI_K_TYPE_DOUBLE: STORE_(dcArgDouble, dbl); break;
    case CFFI_K_TYPE_POINTER: STORE_(dcArgDouble, dbl); break;
    case CFFI_K_TYPE_STRUCT: /* FALLTHRU */
    case CFFI_K_TYPE_CHAR_ARRAY: /* FALLTHRU */
    case CFFI_K_TYPE_BYTE_ARRAY: /* FALLTHRU */
    case CFFI_K_TYPE_UNICHAR_ARRAY:
        dcArgPointer(vmP, argP->value.u.ptr);
        break;
    case CFFI_K_TYPE_ASTRING:/* FALLTHRU */
    case CFFI_K_TYPE_UNISTRING: /* FALLTHRU */
    case CFFI_K_TYPE_WINSTRING:
    case CFFI_K_TYPE_BINARY:
        if (typeAttrsP->flags & CFFI_F_ATTR_BYREF)
            dcArgPointer(vmP, &argP->value.u.ptr);
        else
            dcArgPointer(vmP, argP->value.u.ptr);
        break;
    default:
        CFFI_PANIC("CffiLoadArg: unknown typei %d",
                   typeAttrsP->dataType.baseType);
        break;
    }
#undef STORE_
}

void CffiDyncallFinit(CffiInterpCtx *ipCtxP)
{
    if (ipCtxP->vmP) {
        dcFree(ipCtxP->vmP);
        ipCtxP->vmP = NULL;
    }
}

CffiResult CffiDyncallInit(CffiInterpCtx *ipCtxP)
{
    ipCtxP->vmP    = dcNewCallVM(4096); /* TBD - size? */
    return TCL_OK;
}
