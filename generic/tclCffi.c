/*
 * Copyright (c) 2021-2023 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

static Tcl_Config cffiConfig[] = {
#if defined(CFFI_USE_DYNCALL)
    {"backend", "dyncall"},
#elif defined(CFFI_USE_LIBFFI)
    {"backend", "libffi"},
#else
# error Back end library not defined.
#endif

    {"version", PACKAGE_VERSION},

#if defined(_MSC_VER)
    {"compiler", "vc++"},
#elif defined(__GNUC__)
    {"compiler", "gcc"},
#elif defined(__clang__)
    {"compiler", "clang"},
#else
    {"compiler", "unknown"},
#endif

#ifdef CFFI_HAVE_STRUCT_BYVAL
    {"structbyval", "1"},
#else
    {"structbyval", "0"},
#endif

    {NULL, NULL}
};


/* Function: CffiGetEncodingFromObj
 * Gets the Tcl_Encoding named by the passed object
 *
 * Parameters:
 * ip - interpreter
 * encObj - name of encoding
 * encP - pointer where to store the encoding
 *
 * This function is a wrapper around Tcl_GetEncodingFromObj to treat an
 * empty string same as the default encoding.
 *
 * Returns:
 * *TCL_OK* on success with encoding stored in Tcl_Encoding, otherwise
 * *TCL_ERROR* with error message in interpreter.
 */
CffiResult
CffiGetEncodingFromObj(Tcl_Interp *ip, Tcl_Obj *encObj, Tcl_Encoding *encP)
{
    Tcl_Size len;
    /*
     * Use Tcl_GetStringFromObj because Tcl_GetCharLength will shimmer
     */
    (void)Tcl_GetStringFromObj(encObj, &len);
    if (len)
        CHECK(Tcl_GetEncodingFromObj(ip, encObj, encP));
    else
        *encP = NULL; /* Use default */
    return TCL_OK;
}

static CffiResult
CffiCallObjCmd(ClientData cdata,
               Tcl_Interp *ip,
               int objc,
               Tcl_Obj *const objv[])
{
    Tcl_Obj *protoNameObj;
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    CffiProto *protoP;
    CffiFunction *fnP;
    CffiResult ret;
    void *fnAddr;

    CHECK_NARGS(ip, 2, INT_MAX, "FNPTR ?ARG ...?");
    CHECK(Tclh_PointerObjGetTag(ip, objv[1], &protoNameObj));
    CHECK(Tclh_PointerUnwrap(ip, objv[1], &fnAddr));

    protoNameObj = Tclh_NsQualifyNameObj(ip, protoNameObj, NULL);
    Tcl_IncrRefCount(protoNameObj);
    protoP = CffiProtoGet(ipCtxP, protoNameObj);
    Tcl_DecrRefCount(protoNameObj);
    protoNameObj = NULL;

    if (protoP == NULL) {
        return Tclh_ErrorNotFound(
            ip, "Prototype", objv[1], "Function prototype not found.");
    }

    fnP = CffiFunctionNew(ipCtxP, protoP, NULL, NULL, fnAddr);
    CffiFunctionRef(fnP);
    ret = CffiFunctionCall(fnP, ip, 2, objc, objv);
    CffiFunctionUnref(fnP);

    return ret;
}

static CffiResult
CffiLimitsObjCmd(ClientData cdata,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[])
{
    const CffiBaseTypeInfo *typeP;
    int width;
    const char *minStr = NULL;
    const char *maxStr = NULL;

    CHECK_NARGS(ip, 2, 2, "TYPE");

    typeP = CffiBaseTypeInfoGet(ip, objv[1]);
    if (typeP == NULL)
        return TCL_ERROR;

    /*
     * Note we do not use LONG_MAX etc. for a couple of reasons. We use
     * this routine for testing purposes where we specifically want the string
     * representation. In addition, Tcl_NewWideIntObj (and Tcl_NewLongObj on
     * some platforms) do not support unsigned values at the upper limit.
     */
    width = typeP->size;
    switch (typeP->baseType) {
        case CFFI_K_TYPE_SCHAR:
        case CFFI_K_TYPE_SHORT:
        case CFFI_K_TYPE_INT:
        case CFFI_K_TYPE_LONG:
        case CFFI_K_TYPE_LONGLONG:
            switch (width) {
                case 1:
                    minStr = "-128";
                    maxStr = "127";
                    break;
                case 2:
                    minStr = "-32768";
                    maxStr = "32767";
                    break;
                case 4:
                    minStr = "-2147483648";
                    maxStr = "2147483647";
                    break;
                case 8:
                    minStr = "-9223372036854775808";
                    maxStr = "9223372036854775807";
                    break;
            }
            break;
        case CFFI_K_TYPE_UCHAR:
        case CFFI_K_TYPE_USHORT:
        case CFFI_K_TYPE_UINT:
        case CFFI_K_TYPE_ULONG:
        case CFFI_K_TYPE_ULONGLONG:
            minStr = "0";
            switch (width) {
                case 1:
                    maxStr = "255";
                    break;
                case 2:
                    maxStr = "65535";
                    break;
                case 4:
                    maxStr = "4294967295";
                    break;
                case 8:
                    maxStr = "18446744073709551615";
                    break;
            }
            break;
        default:
            Tcl_SetResult(
                ip, "Invalid or non-integral type specified.", TCL_STATIC);
            return TCL_ERROR;
    }

    /* Protect in case some platforms have larger widths */
    if (minStr && maxStr) {
        Tcl_Obj *objs[2];
        objs[0] = Tcl_NewStringObj(minStr, -1);
        objs[1] = Tcl_NewStringObj(maxStr, -1);
        Tcl_SetObjResult(ip, Tcl_NewListObj(2, objs));
        return TCL_OK;
    }
    else {
        Tcl_SetResult(ip,
                      "Internal error: integer type width not supported for "
                      "this platform.",
                      TCL_STATIC);
        return TCL_ERROR;
    }
}

static CffiResult
CffiSandboxObjCmd(ClientData cdata,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[])
{
    return TCL_OK;
}

static int
CffiClosureDeleteEntry(Tcl_HashTable *htP, Tcl_HashEntry *heP, ClientData unused)
{
    CffiCallback *cbP = Tcl_GetHashValue(heP);
    if (cbP) {
        if (cbP->depth != 0)
            return 0; /* Cannot delete while active */
        CffiCallbackCleanupAndFree(cbP);
    }
    return 1;
}

static void
CffiInterpCtxCleanupAndFree(CffiInterpCtx *ipCtxP)
{
#ifdef CFFI_USE_LIBFFI
        CffiLibffiFinit(ipCtxP);
#endif
#ifdef CFFI_USE_DYNCALL
        CffiDyncallFinit(ipCtxP);
#endif
        CffiAliasesCleanup(ipCtxP);
        CffiEnumsCleanup(ipCtxP);
        CffiPrototypesCleanup(ipCtxP);

        Tclh_HashIterate(
            &ipCtxP->callbackClosures, CffiClosureDeleteEntry, NULL);
        Tcl_DeleteHashTable(&ipCtxP->callbackClosures);

        CffiArenaFinit(ipCtxP);

        Tclh_LifoClose(&ipCtxP->memlifo);

        ckfree(ipCtxP);
}

static CffiResult
CffiInterpCtxAllocAndInit(Tcl_Interp *ip, CffiInterpCtx **ipCtxPP)
{
    CffiInterpCtx *ipCtxP;
    CffiResult ret;

    ipCtxP = ckalloc(sizeof(*ipCtxP));
    memset(ipCtxP, 0, sizeof(*ipCtxP)); /* So we can call cleanup any time */

    ipCtxP->interp = ip;

    /* Set up memlifo before anything else */
    /* TBD - size 16000 too much? */
    if (Tclh_LifoInit(
            &ipCtxP->memlifo, NULL, NULL, 16000, TCLH_LIFO_PANIC_ON_FAIL)
        != 0) {
        /* Don't call CleanupAndFree since that assumes memlifo init'ed */
        ckfree(ipCtxP);
        return Tclh_ErrorAllocation(ip, "Memlifo", NULL);
    }
    if (CffiArenaInit(ipCtxP) != TCL_OK) {
        Tclh_LifoClose(&ipCtxP->memlifo);
        /* Don't call CleanupAndFree on since that assumes all memlifo init'ed */
        ckfree(ipCtxP);
        return Tclh_ErrorAllocation(ip, "Arena", NULL);
    }

    CffiNameTableInit(&ipCtxP->scope.enums);
    CffiNameTableInit(&ipCtxP->scope.aliases);
    CffiNameTableInit(&ipCtxP->scope.prototypes);

    /* Table mapping callback closure function addresses to CffiCallback */
    Tcl_InitHashTable(&ipCtxP->callbackClosures, TCL_ONE_WORD_KEYS);

#ifdef CFFI_USE_DYNCALL
    ret = CffiDyncallInit(ipCtxP);
#endif
#ifdef CFFI_USE_LIBFFI
    ret = CffiLibffiInit(ipCtxP);
#endif
    if (ret == TCL_OK) {
        *ipCtxPP = ipCtxP;
        return TCL_OK;
    }

    CffiInterpCtxCleanupAndFree(ipCtxP);
    return TCL_ERROR;
}

/* Function: CffiFinit
 * Cleans up interpreter-wide resources when extension is unloaded from
 * interpreter.
 *
 * Parameters:
 * ip - interpreter
 * cdata - CallVm context for the interpreter.
 */
static void CffiFinit(ClientData cdata, Tcl_Interp *ip)
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    if (ipCtxP) {
        CffiInterpCtxCleanupAndFree(ipCtxP);
    }
}

DLLEXPORT int
Cffi_Init(Tcl_Interp *ip)
{
    CffiInterpCtx *ipCtxP = NULL;
    Tclh_LibContext *tclhCtxP = NULL;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(ip, "8.6", 0) == NULL) {
        return TCL_ERROR;
    }
#else
    if (Tcl_PkgRequire(ip, "Tcl", "8.6", 0) == NULL) {
        return TCL_ERROR;
    }
#endif

    CHECK(Tclh_LibInit(ip, &tclhCtxP));
    CHECK(Tclh_ObjLibInit(ip, tclhCtxP));
    CHECK(Tclh_PointerLibInit(ip, tclhCtxP));
    CHECK(Tclh_NsLibInit(ip, tclhCtxP));
    CHECK(Tclh_HashLibInit(ip, tclhCtxP));
    CHECK(Tclh_AtomLibInit(ip, tclhCtxP));
    CHECK(Tclh_CmdLibInit(ip, tclhCtxP));
    CHECK(CffiInterpCtxAllocAndInit(ip, &ipCtxP));
    ipCtxP->tclhCtxP = tclhCtxP;

    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::Wrapper", CffiWrapperObjCmd, ipCtxP, NULL);
#ifdef CFFI_USE_DYNCALL
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::dyncall::Symbols", CffiDyncallSymbolsObjCmd, NULL, NULL);
#endif
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::Struct", CffiStructObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::Union", CffiUnionObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::call", CffiCallObjCmd, ipCtxP, NULL);
#ifdef CFFI_HAVE_CALLBACKS
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::callback", CffiCallbackObjCmd, ipCtxP, NULL);
#endif
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::prototype", CffiPrototypeObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::memory", CffiMemoryObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::type", CffiTypeObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::enum", CffiEnumObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::alias", CffiAliasObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::pointer", CffiPointerObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::help", CffiHelpObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::limits", CffiLimitsObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::arena", CffiArenaObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::sandbox", CffiSandboxObjCmd, NULL, NULL);

    Tcl_CallWhenDeleted(ip, CffiFinit, ipCtxP);

    Tcl_PkgProvide(ip, PACKAGE_NAME, PACKAGE_VERSION);

    Tcl_RegisterConfig(ip, PACKAGE_NAME, cffiConfig, "utf-8");

    return TCL_OK;
}
