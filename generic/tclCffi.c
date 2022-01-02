/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#define TCLH_IMPL
#define TCLH_EMBEDDER PACKAGE_NAME
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
    {NULL, NULL}
};


/* Function: Tclh_SubCommandNameToIndex
 * Looks up a subcommand table and returns index of a matching entry.
 *
 * The function looks up the subcommand table to find the entry with
 * name given by *nameObj*. Each entry in the table is a <Tclh_SubCommand>
 * structure and the name is matched against the *cmdName* field of that
 * structure. Unique prefixes of subcommands are accepted as a match.
 * The end of the table is indicated by an entry whose *cmdName* field is NULL.
 *
 * This function only matches against the name and does not concern itself
 * with any of the other fields in an entry.
 *
 * The function is a wrapper around the Tcl *Tcl_GetIndexFromObjStruct*
 * and subject to its constraints. In particular, *cmdTableP* must point
 * to static storage as a pointer to it is retained.
 *
 * Parameters:
 * ip - Interpreter
 * nameObj - name of method to lookup
 * cmdTableP - pointer to command table
 * indexP - location to store index of matched entry
 *
 * Returns:
 * *TCL_OK* on success with index of match stored in *indexP*; otherwise,
 * *TCL_ERROR* with error message in *ip*.
 */
CffiResult
Tclh_SubCommandNameToIndex(Tcl_Interp *ip,
                           Tcl_Obj *nameObj,
                           Tclh_SubCommand *cmdTableP,
                           int *indexP)
{
    return Tcl_GetIndexFromObjStruct(
        ip, nameObj, cmdTableP, sizeof(*cmdTableP), "subcommand", 0, indexP);
}

/* Function: Tclh_SubCommandLookup
 * Looks up a subcommand table and returns index of a matching entry after
 * verifying number of arguments.
 *
 * The function looks up the subcommand table to find the entry with
 * name given by *objv[1]*. Each entry in the table is a <Tclh_SubCommand>
 * structure and the name is matched against the *cmdName* field of that
 * structure. Unique prefixes of subcommands are accepted as a match.
 * The end of the table is indicated by an entry whose *cmdName* field is NULL.
 *
 * This function only matches against the name and does not concern itself
 * with any of the other fields in an entry.
 *
 * The function is a wrapper around the Tcl *Tcl_GetIndexFromObjStruct*
 * and subject to its constraints. In particular, *cmdTableP* must point
 * to static storage as a pointer to it is retained.
 *
 * On finding a match, the function verifies that the number of arguments
 * satisfy the number of arguments expected by the subcommand based on
 * the *minargs* and *maxargs* fields of the matching <Tclh_SubCommand>
 * entry. These fields should hold the argument count range for the
 * subcommand (meaning not counting subcommand itself)
 *
 * Parameters:
 * ip - Interpreter
 * cmdTableP - pointer to command table
 * objc - Number of elements in *objv*. Must be at least *1*.
 * objv - Array of *Tcl_Obj* pointers as passed to Tcl commands. *objv[0]*
 *        is expected to be the main command and *objv[1]* the subcommand.
 * indexP - location to store index of matched entry
 *
 * Returns:
 * *TCL_OK* on success with index of match stored in *indexP*; otherwise,
 * *TCL_ERROR* with error message in *ip*.
 */
CffiResult
Tclh_SubCommandLookup(Tcl_Interp *ip,
                      const Tclh_SubCommand *cmdTableP,
                      int objc,
                      Tcl_Obj *const objv[],
                      int *indexP)
{
    if (objc < 2) {
        return Tclh_ErrorNumArgs(ip, 1, objv, "subcommand ?arg ...?");
    }
    CHECK(Tcl_GetIndexFromObjStruct(
        ip, objv[1], cmdTableP, sizeof(*cmdTableP), "subcommand", 0, indexP));
    cmdTableP += *indexP;
    /*
     * Can't use CHECK_NARGS here because of slightly different semantics.
     */
    if ((objc-2) < cmdTableP->minargs || (objc-2) > cmdTableP->maxargs) {
        return Tclh_ErrorNumArgs(ip, 2, objv, cmdTableP->message);
    }
    return TCL_OK;
}

/* TBD - make into a TCLH function */
/* Function: CffiQualifyName
 * Fully qualifes a name that is relative.
 *
 * If the name is already fully qualified, it is returned as is. Otherwise
 * it is qualified with the name of the current namespace.
 *
 * Parameters:
 * ip - interpreter
 * nameObj - name
 *
 * Returns:
 * A Tcl_Obj with the qualified name. This may be *nameObj* or a new Tcl_Obj.
 * In either case, no manipulation of reference counts is done.
 */
Tcl_Obj *
CffiQualifyName(Tcl_Interp *ip, Tcl_Obj *nameObj)
{
    const char *name = Tcl_GetString(nameObj);

    if (name[0] == ':' && name[1] == ':')
        return nameObj; /* Already fully qualified */
    else {
        Tcl_Obj *fqnObj;
        Tcl_Namespace *nsP;
        nsP = Tcl_GetCurrentNamespace(ip);
        if (nsP) {
            fqnObj = Tcl_NewStringObj(nsP->fullName, -1);
            if (strcmp("::", nsP->fullName))
                Tcl_AppendToObj(fqnObj, "::", 2);
        }
        else
            fqnObj = Tcl_NewStringObj("::", 2); /* Should not happen? */
        Tcl_AppendObjToObj(fqnObj, nameObj);
        return fqnObj;
    }
}

/* TBD - move to TCLH */
/* Function: Tclh_ObjHashEnumerateEntries
 * Returns a list of keys of a hash table that uses *Tcl_Obj* keys.
 *
 * Parameters:
 * htP - hash table
 * patObj - pattern to match or NULL (all keys)
 *
 * Returns:
 * Returns a new *Tcl_Obj* with reference count 0 containing matching keys.
 */
Tcl_Obj *
Tclh_ObjHashEnumerateEntries(Tcl_HashTable *htP, Tcl_Obj *patObj)
{
    Tcl_HashEntry *heP;
    Tcl_HashSearch hSearch;
    const char *pattern;
    Tcl_Obj *resultObj = Tcl_NewListObj(0, NULL);

    pattern = patObj ? Tcl_GetString(patObj) : NULL;
    for (heP = Tcl_FirstHashEntry(htP, &hSearch);
         heP != NULL; heP = Tcl_NextHashEntry(&hSearch)) {
        /* If a pattern was specified, only return those */
        if (pattern) {
            Tcl_Obj *key = Tcl_GetHashKey(htP, heP);
            if (! Tcl_StringMatch(Tcl_GetString(key), pattern))
                continue;
        }

        Tcl_ListObjAppendElement(NULL,
                                 resultObj,
                                 (Tcl_Obj *)Tcl_GetHashKey(htP, heP));
    }
    return resultObj;
}

/* TBD - move to TCLH */
/* Function: Tclh_ObjHashDeleteEntries
 * Deletes entries with keys matching the specified pattern in a *Tcl_Obj*
 * hash table.
 *
 * Parameters:
 * htP - hash table
 * patObj - key or pattern to match. Must not be *NULL*.
 * deleteFn - passed the hash entry for clean up purposes
 *
 * Assumes the hash table keys cannot contain glob metacharacters.
 */
void Tclh_ObjHashDeleteEntries(Tcl_HashTable *htP,
                               Tcl_Obj *patObj,
                               void (*deleteFn)(Tcl_HashEntry *))
{
    Tcl_HashEntry *heP;
    Tcl_HashSearch hSearch;
    const char *pattern;

    CFFI_ASSERT(patObj);

    heP = Tcl_FindHashEntry(htP, patObj);
    if (heP) {
        (*deleteFn)(heP);
        Tcl_DeleteHashEntry(heP);
        return;
    }

    /* Check if glob pattern */
    pattern = Tcl_GetString(patObj);
    for (heP = Tcl_FirstHashEntry(htP, &hSearch);
         heP != NULL; heP = Tcl_NextHashEntry(&hSearch)) {
        Tcl_Obj *key = Tcl_GetHashKey(htP, heP);
        if (Tcl_StringMatch(Tcl_GetString(key), pattern)) {
            (*deleteFn)(heP);
            Tcl_DeleteHashEntry(heP);
        }
    }
}

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
    int len;
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

/* TBD - move to TCLH */
static Tcl_Obj *
Tclh_NamespaceTail(const char *nsPathP, Tcl_Obj **qualifiersObjP)
{
    const char *tailP;
    const char *endP;
    Tcl_Obj *tailObj;

    /* Go to end. TBD - +strlen() intrinsic faster? */
    for (tailP = nsPathP;  *tailP != '\0';  tailP++) {
        /* empty body */
    }
    endP = tailP;
    while (--tailP > nsPathP) {
        if ((*tailP == ':') && (*(tailP-1) == ':')) {
            tailP++; /* Just after the last "::" */
            break;
        }
    }

    /*
     * tail -> tail NULL
     * head::tail -> tail head
     * head:: -> NULL head
     * {} -> NULL NULL
     */
    CFFI_ASSERT(tailP >= nsPathP);
    tailObj =
        *tailP == '\0' ? NULL : Tcl_NewStringObj(tailP, (Tclh_SSizeT)(endP - tailP));
    if (qualifiersObjP) {
        /* Remember we do not want trailing :: */
        if (tailP < 2+nsPathP)
            *qualifiersObjP = NULL;
        else
            *qualifiersObjP = Tcl_NewStringObj(nsPathP, (Tclh_SSizeT) (tailP - nsPathP - 2));
    }
    return tailObj;
}

static CffiResult
CffiCallObjCmd(ClientData cdata,
               Tcl_Interp *ip,
               int objc,
               Tcl_Obj *const objv[])
{
    Tcl_Obj *protoNameObj;
    Tcl_Obj *scopeObj;
    CffiCallVmCtx *vmCtxP = (CffiCallVmCtx *)cdata;
    CffiProto *protoP;
    CffiFunction *fnP;
    CffiScope *scopeP;
    CffiResult ret;
    void *fnAddr;

    CHECK_NARGS(ip, 2, INT_MAX, "FNPTR ?ARG ...?");
    CHECK(Tclh_PointerObjGetTag(ip, objv[1], &protoNameObj));
    CHECK(Tclh_PointerUnwrap(ip, objv[1], &fnAddr, NULL));
    if (protoNameObj) {
        /* The tag is consists of the scope followed by prototype name. */
        protoNameObj =
            Tclh_NamespaceTail(Tcl_GetString(protoNameObj), &scopeObj);
    }
    if (protoNameObj == NULL)
        return Tclh_ErrorNotFound(
            ip, "Prototype", protoNameObj, "Function prototype not found.");

    CFFI_ASSERT(protoNameObj);
    Tcl_IncrRefCount(protoNameObj);
    if (scopeObj) {
        scopeP = CffiScopeGet(vmCtxP->ipCtxP, Tcl_GetString(scopeObj));
        Tcl_DecrRefCount(scopeObj);
        scopeObj = NULL;
    }
    else
        scopeP = CffiScopeGet(vmCtxP->ipCtxP, NULL);

    protoP = CffiProtoGet(scopeP, protoNameObj);
    Tcl_DecrRefCount(protoNameObj);
    protoNameObj = NULL;

    if (protoP == NULL) {
        return Tclh_ErrorNotFound(
            ip, "Prototype", objv[1], "Function prototype not found.");
    }

    fnP = ckalloc(sizeof(*fnP));
    fnP->fnAddr = fnAddr;
    fnP->vmCtxP = vmCtxP;
    fnP->libCtxP = NULL;
    fnP->cmdNameObj = NULL;
    CffiProtoRef(protoP);
    fnP->protoP = protoP;

    ret = CffiFunctionCall(fnP, ip, 2, objc, objv);

    CffiFunctionCleanup(fnP);
    ckfree(fnP);

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
    Tcl_Obj *head, *tail;
    CffiResult ret = TCL_OK;
    Tcl_Obj *result = Tcl_NewListObj(2, NULL);

    tail = Tclh_NamespaceTail(Tcl_GetString(objv[1]), &head);

    if (head)
        Tcl_ListObjAppendElement(ip, result, head);
    else
        Tcl_ListObjAppendElement(ip, result, Tcl_NewStringObj("NULL", -1));
    if (tail)
        Tcl_ListObjAppendElement(ip, result, tail);
    else
        Tcl_ListObjAppendElement(ip, result, Tcl_NewStringObj("NULL", -1));
    Tcl_SetObjResult(ip, result);
    return ret;
}

/* Function: CffiFinit
 * Cleans up interpreter-wide resources when extension is unloaded from
 * interpreter.
 *
 * Parameters:
 * ip - interpreter
 * cdata - CallVm context for the interpreter.
 */
void CffiFinit(ClientData cdata, Tcl_Interp *ip)
{
    CffiCallVmCtx *vmCtxP = (CffiCallVmCtx *)cdata;
    CffiInterpCtx *ipCtxP = vmCtxP->ipCtxP;
    if (ipCtxP) {
        CffiScopesCleanup(&ipCtxP->scopes);
        MemLifoClose(&ipCtxP->memlifo);
#ifdef CFFI_USE_DYNCALL
    if (ipCtxP->vmP)
        dcFree(ipCtxP->vmP);
#endif
        ckfree(ipCtxP);
    }
    ckfree(vmCtxP);
}

DLLEXPORT int
Cffi_Init(Tcl_Interp *ip)
{
    CffiInterpCtx *ipCtxP;
    CffiCallVmCtx *vmCtxP;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(ip, "8.6", 0) == NULL) {
        return TCL_ERROR;
    }
#else
    if (Tcl_PkgRequire(ip, "Tcl", "8.6", 0) == NULL) {
        return TCL_ERROR;
    }
#endif

    if (Tclh_BaseLibInit(ip) != TCL_OK)
        return TCL_ERROR;
    if (Tclh_ObjLibInit(ip) != TCL_OK)
        return TCL_ERROR;
    if (Tclh_PointerLibInit(ip) != TCL_OK)
        return TCL_ERROR;

    ipCtxP = ckalloc(sizeof(*ipCtxP));
    ipCtxP->interp = ip;
    Tcl_InitHashTable(&ipCtxP->scopes, TCL_STRING_KEYS);
    ipCtxP->globalScopeP = NULL;
#ifdef OBSOLETE
    Tcl_InitObjHashTable(&ipCtxP->aliases);
    Tcl_InitObjHashTable(&ipCtxP->enums);
#endif

#ifdef CFFI_USE_DYNCALL
    ipCtxP->vmP    = dcNewCallVM(4096); /* TBD - size? */
#endif

    /* Initialize the global scope */
    ipCtxP->globalScopeP = CffiScopeGet(ipCtxP, "::");
    CffiScopeRef(ipCtxP->globalScopeP);

    /* TBD - size 16000 too much? */
    if (MemLifoInit(
            &ipCtxP->memlifo, NULL, NULL, 16000, MEMLIFO_F_PANIC_ON_FAIL) !=
        MEMLIFO_E_SUCCESS) {
        return Tclh_ErrorAllocation(ip, "Memlifo", NULL);
    }

    vmCtxP = ckalloc(sizeof(*vmCtxP));
    vmCtxP->ipCtxP = ipCtxP;

    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::Wrapper", CffiWrapperObjCmd, vmCtxP, NULL);
#ifdef CFFI_USE_DYNCALL
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::dyncall::Symbols", CffiDyncallSymbolsObjCmd, NULL, NULL);
#endif
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::Struct", CffiStructObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::call", CffiCallObjCmd, vmCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::prototype", CffiPrototypeObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::memory", CffiMemoryObjCmd, NULL, NULL);
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
        ip, CFFI_NAMESPACE "::sandbox", CffiSandboxObjCmd, NULL, NULL);

    Tcl_CallWhenDeleted(ip, CffiFinit, vmCtxP);

    Tcl_PkgProvide(ip, PACKAGE_NAME, PACKAGE_VERSION);

    Tcl_RegisterConfig(ip, PACKAGE_NAME, cffiConfig, "utf-8");

    return TCL_OK;
}
