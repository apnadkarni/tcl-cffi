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

#ifdef CFFI_HAVE_STRUCT_BYVAL
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
#endif /* CFFI_HAVE_STRUCT_BYVAL */

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
#ifdef CFFI_HAVE_STRUCT_BYVAL
            /* Since reload, dcAggrP should already have been init'ed */
            CFFI_ASSERT(typeAttrsP->dataType.u.structP->dcAggrP);
            CFFI_ASSERT(argP->value.u.ptr);
            dcArgAggr(callP->fnP->ipCtxP->vmP,
                      typeAttrsP->dataType.u.structP->dcAggrP,
                      argP->value.u.ptr);
#else
            CFFI_ASSERT(0); /* Should not have reached here */
#endif
        }
        break;
    default:
        CFFI_PANIC("CffiLoadArg: unknown typei %d",
                   typeAttrsP->dataType.baseType);
        break;
    }
#undef STORE_
}

#ifdef CFFI_HAVE_CALLBACKS
char
CffiDyncallMapCallbackType(CffiTypeAndAttrs *typeAttrsP)
{
    if (typeAttrsP->flags & CFFI_F_ATTR_BYREF) {
        return DC_SIGCHAR_POINTER;
    }
    CffiBaseType baseType = typeAttrsP->dataType.baseType;
    switch (baseType) {
    case CFFI_K_TYPE_SCHAR:
    case CFFI_K_TYPE_UCHAR:
    case CFFI_K_TYPE_SHORT:
    case CFFI_K_TYPE_USHORT:
    case CFFI_K_TYPE_INT:
    case CFFI_K_TYPE_UINT:
    case CFFI_K_TYPE_LONG:
    case CFFI_K_TYPE_ULONG:
    case CFFI_K_TYPE_LONGLONG:
    case CFFI_K_TYPE_ULONGLONG:
    case CFFI_K_TYPE_FLOAT:
    case CFFI_K_TYPE_DOUBLE:
    case CFFI_K_TYPE_POINTER:
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
#ifdef _WIN32
    case CFFI_K_TYPE_WINSTRING:
#endif
        return cffiBaseTypes[baseType].dcSigChar;
    default:
        return 0;
    }
}

char *CffiDyncallCallbackSig(CffiInterpCtx *ipCtxP, CffiProto *protoP)
{
    int paramIndex;
    int nParams = protoP->nParams;
    char *sigP  = ckalloc(2 + nParams + 3);/* Params, ")", return, \0 */
    char *p     = sigP;

#if defined(_WIN32) && !defined(_WIN64)
    if (protoP->abi == CffiStdcallABI()) {
        *p++ = '_';
        *p++ = 's';
    }
#endif

    for (paramIndex = 0; paramIndex < nParams; ++p, ++paramIndex) {
        CffiTypeAndAttrs *typeAttrsP = &protoP->params[paramIndex].typeAttrs;
        if (typeAttrsP->flags & CFFI_F_ATTR_BYREF) {
            *p = DC_SIGCHAR_POINTER;
            continue;
        }
        *p = CffiDyncallMapCallbackType(typeAttrsP);
        if (*p == 0) {
            goto unsupported_type;
        }
    }

    /* Return type */
    *p++ = ')';
    /* Special case void return since MapCallback will not like it */
    if (protoP->returnType.typeAttrs.dataType.baseType == CFFI_K_TYPE_VOID)
        *p = 'v';
    else {
        *p = CffiDyncallMapCallbackType(&protoP->returnType.typeAttrs);
        if (*p == 0)
            goto unsupported_type;
    }
    ++p;
    *p = '\0';
    return sigP;

unsupported_type:
    Tclh_ErrorInvalidValue(
        ipCtxP->interp, NULL, "Type is not supported for callbacks.");
    return NULL;
}

void CffiDyncallCallbackCleanup(CffiCallback *cbP)
{
    if (cbP->dcCallbackP)
        dcbFreeCallback(cbP->dcCallbackP);
    if (cbP->dcCallbackSig)
        ckfree(cbP->dcCallbackSig);
}

static CffiResult
CffiDyncallCallbackArgToObj(CffiCallback *cbP,
                           Tcl_Size argIndex,
                           DCArgs *dcArgsP,
                           Tcl_Obj **argObjP)
{
    CffiTypeAndAttrs *typeAttrsP = &cbP->protoP->params[argIndex].typeAttrs;
    Tcl_WideInt wide;
    unsigned long long ull;
    float flt;
    double dbl;
    void *ptr;
    void *valueP = NULL;

    CFFI_ASSERT(CffiTypeIsNotArray(&typeAttrsP->dataType));

    /*
     * Note even for integer values, we call CffiNativeScalarToObj since
     * integers may need to be mapped to enum names etc.
     */

#define EXTRACT_(var_, fn_)                                           \
    do {                                                              \
        if (typeAttrsP->flags & CFFI_F_ATTR_BYREF) {                  \
            /* arg is a pointer to the value, not the value itself */ \
            valueP = dcbArgPointer(dcArgsP);                          \
        }                                                             \
        else {                                                        \
            var_   = fn_(dcArgsP);                                    \
            valueP = &var_;                                           \
        }                                                             \
    } while (0)

    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_SCHAR    : EXTRACT_(wide, dcbArgChar); break;
    case CFFI_K_TYPE_UCHAR    : EXTRACT_(wide, dcbArgUChar); break;
    case CFFI_K_TYPE_SHORT    : EXTRACT_(wide, dcbArgShort); break;
    case CFFI_K_TYPE_USHORT   : EXTRACT_(wide, dcbArgUShort); break;
    case CFFI_K_TYPE_INT      : EXTRACT_(wide, dcbArgInt); break;
    case CFFI_K_TYPE_UINT     : EXTRACT_(wide, dcbArgUInt); break;
    case CFFI_K_TYPE_LONG     : EXTRACT_(wide, dcbArgLong); break;
    case CFFI_K_TYPE_ULONG    : EXTRACT_(wide, dcbArgULong); break;
    case CFFI_K_TYPE_LONGLONG : EXTRACT_(wide, dcbArgLongLong); break;
    case CFFI_K_TYPE_ULONGLONG: EXTRACT_(ull, dcbArgULongLong); break;
    case CFFI_K_TYPE_FLOAT    : EXTRACT_(flt, dcbArgFloat); break;
    case CFFI_K_TYPE_DOUBLE   : EXTRACT_(dbl, dcbArgDouble); break;
#ifdef _WIN32
    case CFFI_K_TYPE_WINSTRING:
#endif
    case CFFI_K_TYPE_ASTRING  :
    case CFFI_K_TYPE_UNISTRING:
    case CFFI_K_TYPE_POINTER  : EXTRACT_(ptr, dcbArgPointer); break;
    case CFFI_K_TYPE_STRUCT:
        CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_BYREF);
        /* args[argIndex] is the location of the pointer to the struct */
        valueP = dcbArgPointer(dcArgsP);
        if (valueP == NULL) {
            CffiStruct *structP;
            structP = typeAttrsP->dataType.u.structP;
            if (!(typeAttrsP->flags & CFFI_F_ATTR_NULLOK) || CffiStructIsVariableSize(structP)) {
                return Tclh_ErrorInvalidValue(
                    cbP->ipCtxP->interp,
                    NULL,
                    "Pointer passed to callback is NULL.");
            }
            valueP = Tclh_LifoAlloc(&cbP->ipCtxP->memlifo, structP->size);
            CHECK(CffiStructObjDefault(cbP->ipCtxP, structP, valueP));
        }
        /* TBD - Why not call CffiStructToObj directly here? */
        return CffiNativeValueToObj(
            cbP->ipCtxP, typeAttrsP, valueP, 0, -1, argObjP);

    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
#ifdef _WIN32
    case CFFI_K_TYPE_WINCHAR_ARRAY:
#endif
    case CFFI_K_TYPE_BYTE_ARRAY:
    case CFFI_K_TYPE_BINARY:
    case CFFI_K_TYPE_VOID:
    default:
        return Tclh_ErrorInvalidValue(
            cbP->ipCtxP->interp, NULL, "Invalid type for use in callbacks.");
    }

    /* TODO check for nullok attribute here? */
    if (valueP == NULL)
        return Tclh_ErrorInvalidValue(
            cbP->ipCtxP->interp, NULL, "Pointer passed to callback is NULL.");

    return CffiNativeScalarToObj(
        cbP->ipCtxP, typeAttrsP, valueP, 0, argObjP);

#undef EXTRACT_
}

static CffiResult
CffiDyncallCallbackStoreResult(CffiInterpCtx *ipCtxP,
                              CffiTypeAndAttrs *typeAttrsP,
                              Tcl_Obj *valueObj,
                              DCValue *dcResultP,
                              DCsigchar *sigCharP)
{
    char sigChar;
    double dbl;
    void *ptr;

#define RETURNINT_(type_, fld_, sig_)                                          \
    do {                                                                       \
        Tcl_WideInt wide;                                                      \
        CHECK(                                                                 \
            CffiIntValueFromObj(ipCtxP->interp, typeAttrsP, valueObj, &wide)); \
        dcResultP->fld_ = (type_)wide;                                         \
        sigChar         = sig_;                                                \
    } while (0)

    CFFI_ASSERT(CffiTypeIsNotArray(&typeAttrsP->dataType));
    CFFI_ASSERT((typeAttrsP->flags & CFFI_F_ATTR_BYREF) == 0);

    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_VOID: sigChar = DC_SIGCHAR_VOID; break;
    case CFFI_K_TYPE_SCHAR    : RETURNINT_(signed char, c, 'c'); break;
    case CFFI_K_TYPE_UCHAR    : RETURNINT_(unsigned char, C, 'C'); break;
    case CFFI_K_TYPE_SHORT    : RETURNINT_(short, s, 's'); break;
    case CFFI_K_TYPE_USHORT   : RETURNINT_(unsigned short, S, 'S'); break;
    case CFFI_K_TYPE_INT      : RETURNINT_(int, i, 'i'); break;
    case CFFI_K_TYPE_UINT     : RETURNINT_(unsigned int, I, 'I'); break;
    case CFFI_K_TYPE_LONG     : RETURNINT_(long, j, 'j'); break;
    case CFFI_K_TYPE_ULONG    : RETURNINT_(unsigned long, J, 'J'); break;
    case CFFI_K_TYPE_LONGLONG : RETURNINT_(long long, l, 'l'); break;
    case CFFI_K_TYPE_ULONGLONG: RETURNINT_(unsigned long long, L, 'L'); break;
    case CFFI_K_TYPE_FLOAT:
        CHECK(Tcl_GetDoubleFromObj(ipCtxP->interp, valueObj, &dbl));
        dcResultP->f = (float)dbl;
        sigChar = 'f';
        break;
    case CFFI_K_TYPE_DOUBLE:
        CHECK(Tcl_GetDoubleFromObj(ipCtxP->interp, valueObj, &dbl));
        dcResultP->d = dbl;
        sigChar = 'd';
        break;
    case CFFI_K_TYPE_POINTER:
        CHECK(Tclh_PointerUnwrap(ipCtxP->interp, valueObj, &ptr));
        dcResultP->p = ptr;
        sigChar = 'p';
        break;

    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
#ifdef _WIN32
    case CFFI_K_TYPE_WINSTRING:
#endif
    case CFFI_K_TYPE_STRUCT:
    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
#ifdef _WIN32
    case CFFI_K_TYPE_WINCHAR_ARRAY:
#endif
    case CFFI_K_TYPE_BYTE_ARRAY:
    case CFFI_K_TYPE_BINARY:
    default:
        return Tclh_ErrorInvalidValue(
            ipCtxP->interp, NULL, "Invalid type for use as callback return.");
    }
    *sigCharP = sigChar;
    return TCL_OK;
#undef RETURNINT_
}

/*
 *------------------------------------------------------------------------
 *
 * CffiDyncallCallback --
 *
 *    Wrapper called from dyncall to invoke callback scripts
 * 
 * Parameters:
 * dcbP - dyncall callback context
 * dcArgsP - arguments
 * dcResultP - result to be returned
 * userdata - CFFI callback context
 *
 * Results:
 *    A Dyncall signature character indicating type of the result
 *
 * Side effects:
 *    Runs a callback script, storing its result in dcResultP
 *
 *------------------------------------------------------------------------
 */
DCsigchar
CffiDyncallCallback(DCCallback *dcbP,
                    DCArgs *dcArgsP,
                    DCValue *dcResultP,
                    void *userdata)
{
    Tcl_Obj **evalObjs;
    Tcl_Obj **cmdObjs;
    Tcl_Size i, nEvalObjs, nCmdObjs;
    CffiResult ret;
    Tcl_Obj *resultObj;
    CffiCallback *cbP = (CffiCallback *)userdata;
    CffiInterpCtx *ipCtxP = cbP->ipCtxP;
    Tclh_LifoMark mark = NULL;
    DCsigchar dcSigChar;

    if (Tcl_ListObjGetElements(NULL, cbP->cmdObj, &nCmdObjs, &cmdObjs)
                != TCL_OK) {
        ret = TCL_ERROR;
        goto vamoose;
    }

    nEvalObjs = nCmdObjs + cbP->protoP->nParams;

    /*
     * Note: CffiFunctionCall would have already set a memlifo mark but
     * we do so anyways since in the future callbacks may be outside of
     * CffiFunctionCall context.
     */
    mark = Tclh_LifoPushMark(&ipCtxP->memlifo);
    /* TODO - overflow on multiply */
    evalObjs =
        Tclh_LifoAlloc(&ipCtxP->memlifo, nEvalObjs * sizeof(Tcl_Obj *));

    /* Do NOT return beyond this point without popping memlifo */

    /* 
     * Translate arguments passed by dyncall to Tcl_Objs. Note that the
     * dcArgsP is implicitly incremented by dyncall for each argument
     * retrieved.
     */
    for (i = 0; i < cbP->protoP->nParams; ++i) {
        ret = CffiDyncallCallbackArgToObj(
            cbP, i, dcArgsP, &evalObjs[nCmdObjs + i]);
        if (ret != TCL_OK) {
            int j;
            for (j = 0; j < i; ++j)
                Tcl_DecrRefCount(evalObjs[nCmdObjs + j]);
            goto vamoose;
        }
        Tcl_IncrRefCount(evalObjs[nCmdObjs + i]);
    }

    /*
    * Only incr refs for cmdObjs. argument objs at index nCmdObjs+i
    * already done above
    */
    for (i = 0; i < nCmdObjs; ++i) {
        evalObjs[i] = cmdObjs[i];
        Tcl_IncrRefCount(evalObjs[i]);
    }
    /* Ensure callback is not deleted by script */
    cbP->depth += 1;
    /* Note: evaluating in current context, not global context */
    ret = Tcl_EvalObjv(ipCtxP->interp, nEvalObjs, evalObjs, 0);
    for (i = 0; i < nEvalObjs; ++i) {
        Tcl_DecrRefCount(evalObjs[i]);
    }
    cbP->depth -= 1;

vamoose:
    /* May come here on an error or success */
    if (ret == TCL_OK) {
        /* Try converting result to a native value */
        resultObj = Tcl_GetObjResult(ipCtxP->interp);
        ret = CffiDyncallCallbackStoreResult(ipCtxP,
                                             &cbP->protoP->returnType.typeAttrs,
                                             resultObj,
                                             dcResultP,
                                             &dcSigChar);
        /* Do not want callback result percolating up the stack. */
        Tcl_ResetResult(ipCtxP->interp);
    }
    if (ret != TCL_OK) {
        /*
         * Either original eval raised error or could not convert above.
         * Store the designated error value. Should not fail since object
         * value should have been checked at callback definition time
         * but if void error cannot be reported this way.
         */
        if (CffiDyncallCallbackStoreResult(ipCtxP,
                                           &cbP->protoP->returnType.typeAttrs,
                                           cbP->errorResultObj,
                                           dcResultP,
                                           &dcSigChar)
                != TCL_OK
            || cbP->protoP->returnType.typeAttrs.dataType.baseType
                   == CFFI_K_TYPE_VOID) {
            /* Could not store error or void type. Background error */
            if (ipCtxP->interp) {
                if (cbP->errorResultObj) {
                    Tcl_SetObjResult(ipCtxP->interp, cbP->errorResultObj);
                } else {
                    /* May be NULL for void */
                    Tcl_AppendResult(
                        ipCtxP->interp, "Error in callback.", NULL);
                }

                Tcl_BackgroundError(ipCtxP->interp);
            }
        }
    }
    if (mark)
        Tclh_LifoPopMark(mark);

    return dcSigChar;
}

/*
 *------------------------------------------------------------------------
 *
 * CffiDyncallCallbackInit --
 *
 *    Initializes the dyncall-specific components of the callback structure
 *
 * Results:
 *    TCL_OK on success, TCL_ERROR on failure.
 *
 * Side effects:
 *    Store dyncall data in the cbP location
 *
 *------------------------------------------------------------------------
 */
CffiResult
CffiDyncallCallbackInit(CffiInterpCtx *ipCtxP,
                        CffiProto *protoP,
                        CffiCallback *cbP)
{
    char *cbSigP = CffiDyncallCallbackSig(ipCtxP, protoP);
    if (cbSigP == NULL)
        return TCL_ERROR;/* Error already stored in ipCtxP->interp */

    cbP->dcCallbackP = dcbNewCallback(cbSigP, CffiDyncallCallback, cbP);
    if (cbP->dcCallbackP == NULL) {
        ckfree(cbSigP);
        return Tclh_ErrorAllocation(ipCtxP->interp, "dcCallback", NULL);
    }
    cbP->dcCallbackSig = cbSigP;
    return TCL_OK;
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
#endif