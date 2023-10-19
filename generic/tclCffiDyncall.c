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

    if (CffiProtoIsVarargs(callP->fnP->protoP)) {
        /* If not default ABI, should have been caught at definition time */
        CFFI_ASSERT(callP->fnP->protoP->abi == CffiDefaultABI());
        dcMode(vmP, DC_CALL_C_ELLIPSIS);
    }
    else
        dcMode(vmP, callP->fnP->protoP->abi);
    dcReset(vmP);
    return TCL_OK;
}

CffiResult
CffiDyncallVarargsInit(CffiInterpCtx *ipCtxP,
                       int numVarArgs,
                       Tcl_Obj * const *varArgObjs,
                       CffiTypeAndAttrs *varArgTypesP)
{
    CffiResult ret = TCL_OK;
    Tcl_Interp *ip = ipCtxP->interp;
    int i;

    /*
     * Finally, gather up the vararg types. Unlike the fixed parameters,
     * here the types are specified at call time so need to parse those
     */
    int numVarArgTypesInited;
    for (numVarArgTypesInited = 0, i = 0; i < numVarArgs; ++i) {
        /* The varargs arguments are pairs consisting of type and value. */
        Tcl_Obj **argObjs;
        Tcl_Size n;
        if (Tcl_ListObjGetElements(NULL, varArgObjs[i], &n, &argObjs) != TCL_OK
            || n != 2) {
            ret = Tclh_ErrorInvalidValue(
                ip, varArgObjs[i], "A vararg must be a type and value pair.");
            break;
        }
        ret = CffiTypeAndAttrsParse(
            ipCtxP, argObjs[0], CFFI_F_TYPE_PARSE_PARAM, &varArgTypesP[i]);
        if (ret != TCL_OK)
            break;
        ++numVarArgTypesInited; /* So correct number is cleaned on error */

        ret = CffiCheckVarargType(ip, &varArgTypesP[i], argObjs[0]);
        if (ret != TCL_OK)
            break;
    }

    /* Error in vararg type conversion or cif prep. Clean any varargs types */
    if (ret != TCL_OK) {
        for (i = 0; i < numVarArgTypesInited; ++i) {
            CffiTypeAndAttrsCleanup(&varArgTypesP[i]);
        }
    }
    return ret;
}

/*
 *------------------------------------------------------------------------
 *
 * CffiDyncallAggrInit --
 *
 *    Constructs a struct descriptor for dyncall. This is used to pass
 *    structs by value.
 *
 * Parameters:
 * ipCtxP - interp context
 * structP - struct descriptor
 *
 * Results:
 *    TCL_OK on success and TCL_ERROR on failure.
 *
 * Side effects:
 *    On success stores the dyncall struct descriptor in structP
 *
 *------------------------------------------------------------------------
 */
CffiResult CffiDyncallAggrInit(CffiInterpCtx *ipCtxP, CffiStruct *structP)
{

    if (structP->dcAggrP)
        return TCL_OK;

    if (CffiStructIsVariableSize(structP)) {
        Tclh_ErrorInvalidValue(
            ipCtxP->interp,
            NULL,
            "Variable size struct cannot be passed by value.");
        return TCL_ERROR;
    }
    if (structP->pack != 0) {
        Tclh_ErrorInvalidValue(
            ipCtxP->interp,
            NULL,
            "Packed struct cannot be passed by value.");
        return TCL_ERROR;
    }

    int fldIndex;
    DCaggr *dcAggrP;

    dcAggrP = dcNewAggr(structP->nFields, CffiStructFixedSize(structP));
    for (fldIndex = 0; fldIndex < structP->nFields; ++fldIndex) {
        int elemCount;
        CffiField *fldP = &structP->fields[fldIndex];
        elemCount       = fldP->fieldType.dataType.arraySize;
        CFFI_ASSERT(elemCount != 0); /* Else varsize check above would fail */
        if (elemCount < 0)
            elemCount = 1; /* Scalar */
        switch (fldP->fieldType.dataType.baseType) {
        case CFFI_K_TYPE_CHAR_ARRAY: /* FALLTHRU */
        case CFFI_K_TYPE_SCHAR:
            dcAggrField(dcAggrP, DC_SIGCHAR_CHAR, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_BYTE_ARRAY: /* FALLTHRU */
        case CFFI_K_TYPE_UCHAR:
            dcAggrField(dcAggrP, DC_SIGCHAR_UCHAR, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_SHORT:
            dcAggrField(dcAggrP, DC_SIGCHAR_SHORT, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_USHORT:
            dcAggrField(dcAggrP, DC_SIGCHAR_USHORT, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_INT:
            dcAggrField(dcAggrP, DC_SIGCHAR_INT, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_UINT:
            dcAggrField(dcAggrP, DC_SIGCHAR_UINT, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_LONG:
            dcAggrField(dcAggrP, DC_SIGCHAR_LONG, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_ULONG:
            dcAggrField(dcAggrP, DC_SIGCHAR_ULONG, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_LONGLONG:
            dcAggrField(dcAggrP, DC_SIGCHAR_LONGLONG, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_ULONGLONG:
            dcAggrField(dcAggrP, DC_SIGCHAR_ULONGLONG, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_FLOAT:
            dcAggrField(dcAggrP, DC_SIGCHAR_FLOAT, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_DOUBLE:
            dcAggrField(dcAggrP, DC_SIGCHAR_DOUBLE, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_ASTRING: /* FALLTHRU */
        case CFFI_K_TYPE_UNISTRING:
#ifdef _WIN32
        case CFFI_K_TYPE_WINSTRING:
#endif
        case CFFI_K_TYPE_POINTER:
            dcAggrField(dcAggrP, DC_SIGCHAR_POINTER, fldP->offset, elemCount);
            break;
        case CFFI_K_TYPE_UNICHAR_ARRAY:
            if (sizeof(Tcl_UniChar) == sizeof(unsigned short))
                dcAggrField(
                    dcAggrP, DC_SIGCHAR_USHORT, fldP->offset, elemCount);
            else {
                CFFI_ASSERT(sizeof(Tcl_UniChar) == sizeof(unsigned int));
                dcAggrField(dcAggrP, DC_SIGCHAR_UINT, fldP->offset, elemCount);
            }
            break;
#ifdef _WIN32
        case CFFI_K_TYPE_WINCHAR_ARRAY:
            dcAggrField(dcAggrP, DC_SIGCHAR_USHORT, fldP->offset, elemCount);
            break;
#endif
        case CFFI_K_TYPE_STRUCT:
            if (CffiDyncallAggrInit(ipCtxP, fldP->fieldType.dataType.u.structP)
                != TCL_OK) {
                dcCloseAggr(dcAggrP);
                dcFreeAggr(dcAggrP);
                return TCL_ERROR;
            }
            CFFI_ASSERT(fldP->fieldType.dataType.u.structP->dcAggrP);
            dcAggrField(dcAggrP,
                        DC_SIGCHAR_AGGREGATE,
                        fldP->offset,
                        elemCount,
                        fldP->fieldType.dataType.u.structP->dcAggrP);
            break;
        case CFFI_K_TYPE_BINARY: /* FALLTHRU */
        default:
            CffiErrorType(ipCtxP->interp,
                          fldP->fieldType.dataType.baseType,
                          __FILE__,
                          __LINE__);
            dcFreeAggr(dcAggrP);
            return TCL_ERROR;
        }
    }

    dcCloseAggr(dcAggrP);
    structP->dcAggrP = dcAggrP;
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
void
CffiDyncallReloadArg(CffiCall *callP,
                     CffiArgument *argP,
                     CffiTypeAndAttrs *typeAttrsP)
{
    DCCallVM *vmP = callP->fnP->ipCtxP->vmP;

    CFFI_ASSERT(argP->flags & CFFI_F_ARG_INITIALIZED);

#define STORE_(dcfn_, fld_)                                        \
    do {                                                           \
        if (argP->arraySize < 0) {                                 \
            if (typeAttrsP->flags & CFFI_F_ATTR_BYREF)             \
                dcArgPointer(vmP, (DCpointer)&argP->value.u.fld_); \
            else                                                   \
                dcfn_(vmP, argP->value.u.fld_);                    \
        }                                                          \
        else {                                                     \
            CFFI_ASSERT(typeAttrsP->flags &CFFI_F_ATTR_BYREF);     \
            dcArgPointer(vmP, (DCpointer)argP->value.u.ptr);       \
        }                                                          \
    } while (0)
    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_SCHAR:
        STORE_(dcArgChar, schar);
        break;
    case CFFI_K_TYPE_UCHAR:
        STORE_(dcArgChar, uchar);
        break;
    case CFFI_K_TYPE_SHORT:
        STORE_(dcArgShort, sshort);
        break;
    case CFFI_K_TYPE_USHORT:
        STORE_(dcArgShort, ushort);
        break;
    case CFFI_K_TYPE_INT:
        STORE_(dcArgInt, sint);
        break;
    case CFFI_K_TYPE_UINT:
        STORE_(dcArgInt, uint);
        break;
    case CFFI_K_TYPE_LONG:
        STORE_(dcArgLong, slong);
        break;
    case CFFI_K_TYPE_ULONG:
        STORE_(dcArgLong, ulong);
        break;
    case CFFI_K_TYPE_LONGLONG:
        STORE_(dcArgLongLong, slonglong);
        break;
    case CFFI_K_TYPE_ULONGLONG:
        STORE_(dcArgLongLong, ulonglong);
        break;
    case CFFI_K_TYPE_FLOAT:
        STORE_(dcArgFloat, flt);
        break;
    case CFFI_K_TYPE_DOUBLE:
        STORE_(dcArgDouble, dbl);
        break;
    case CFFI_K_TYPE_POINTER:
        STORE_(dcArgDouble, dbl);
        break;
    case CFFI_K_TYPE_CHAR_ARRAY: /* FALLTHRU */
    case CFFI_K_TYPE_BYTE_ARRAY: /* FALLTHRU */
    case CFFI_K_TYPE_UNICHAR_ARRAY:
#ifdef _WIN32
    case CFFI_K_TYPE_WINCHAR_ARRAY:
#endif
        dcArgPointer(vmP, argP->value.u.ptr);
        break;
    case CFFI_K_TYPE_ASTRING:   /* FALLTHRU */
    case CFFI_K_TYPE_UNISTRING: /* FALLTHRU */
#ifdef _WIN32
    case CFFI_K_TYPE_WINSTRING:
#endif
    case CFFI_K_TYPE_BINARY:
        if (typeAttrsP->flags & CFFI_F_ATTR_BYREF)
            dcArgPointer(vmP, &argP->value.u.ptr);
        else
            dcArgPointer(vmP, argP->value.u.ptr);
        break;
    case CFFI_K_TYPE_STRUCT:
        if (typeAttrsP->flags & CFFI_F_ATTR_BYREF)
            dcArgPointer(vmP, argP->value.u.ptr);
        else {
            /* Since reload, dcAggrP should already have been init'ed */
            CFFI_ASSERT(typeAttrsP->dataType.u.structP->dcAggrP);
            CFFI_ASSERT(argP->value.u.ptr);
            dcArgAggr(callP->fnP->ipCtxP->vmP,
                      typeAttrsP->dataType.u.structP->dcAggrP,
                      argP->value.u.ptr);
        }
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
