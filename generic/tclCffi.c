/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#define TCLH_IMPL
#define TCLH_EMBEDDER PACKAGE_NAME
#include "tclCffiInt.h"


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
    TCLH_ASSERT(objc > 1);
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


static CffiResult
CffiCallObjCmd(ClientData cdata,
               Tcl_Interp *ip,
               int objc,
               Tcl_Obj *const objv[])
{
    Tcl_Obj *protoNameObj;
    CffiCallVmCtx *vmCtxP = (CffiCallVmCtx *)cdata;
    CffiProto *protoP;
    CffiFunction *fnP;
    CffiResult ret;
    void *fnAddr;

    CHECK_NARGS(ip, 2, INT_MAX, "FNPTR ?ARG ...?");
    CHECK(Tclh_PointerObjGetTag(ip, objv[1], &protoNameObj));
    CHECK(Tclh_PointerUnwrap(ip, objv[1], &fnAddr, NULL));
    if (protoNameObj == NULL
        || (protoP = CffiProtoGet(vmCtxP->ipCtxP, protoNameObj)) == NULL) {
        return Tclh_ErrorNotFound(
            ip, "Prototype", protoNameObj, "Function prototype not found.");
    }

    fnP = ckalloc(sizeof(*fnP));
    fnP->fnAddr = fnAddr;
    fnP->vmCtxP = vmCtxP;
    CffiProtoRef(protoP);
    fnP->protoP = protoP;

    ret = CffiFunctionCall(fnP, ip, 2, objc, objv);

    CffiProtoUnref(protoP);
    ckfree(fnP);
    return ret;
}

static CffiResult
CffiSandboxObjCmd(ClientData cdata,
                  Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[])
{
    unsigned long long ull;

    if (Tclh_ObjToULongLong(ip, objv[1], &ull) != TCL_OK)
        return TCL_ERROR;
    else {
        char buf[40];
#ifdef _WIN32
        _snprintf(buf, 40, "%I64u", ull);
#else
        snprintf(buf, 40, "%llu", ull);
#endif
        Tcl_SetResult(ip, buf, TCL_VOLATILE);
        return TCL_OK;
    }
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
    if (vmCtxP->vmP)
        dcFree(vmCtxP->vmP);
    if (vmCtxP->ipCtxP) {
        CffiAliasesCleanup(&vmCtxP->ipCtxP->aliases);
        CffiPrototypesCleanup(&vmCtxP->ipCtxP->prototypes);
        MemLifoClose(&vmCtxP->ipCtxP->memlifo);
        ckfree(vmCtxP->ipCtxP);
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
    if (Tclh_PointerLibInit(ip) != TCL_OK)
        return TCL_ERROR;

    ipCtxP = ckalloc(sizeof(*ipCtxP));
    ipCtxP->interp = ip;
    Tcl_InitObjHashTable(&ipCtxP->aliases);
    Tcl_InitObjHashTable(&ipCtxP->prototypes);

    /* TBD - size 16000 too much? */
    if (MemLifoInit(
            &ipCtxP->memlifo, NULL, NULL, 16000, MEMLIFO_F_PANIC_ON_FAIL) !=
        MEMLIFO_E_SUCCESS) {
        return Tclh_ErrorAllocation(ip, "Memlifo", NULL);
    }

    vmCtxP = ckalloc(sizeof(*vmCtxP));
    vmCtxP->ipCtxP = ipCtxP;
    vmCtxP->vmP    = dcNewCallVM(4096); /* TBD - size? */

    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::dyncall::Library", CffiDyncallLibraryObjCmd, vmCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::dyncall::Symbols", CffiDyncallSymbolsObjCmd, NULL, NULL);
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
        ip, CFFI_NAMESPACE "::alias", CffiAliasObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::pointer", CffiPointerObjCmd, ipCtxP, NULL);
    Tcl_CreateObjCommand(
        ip, CFFI_NAMESPACE "::sandbox", CffiSandboxObjCmd, NULL, NULL);

    Tcl_CallWhenDeleted(ip, CffiFinit, vmCtxP);

    Tcl_PkgProvide(ip, PACKAGE_NAME, PACKAGE_VERSION);
    return TCL_OK;
}

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
            fqnObj = Tcl_NewStringObj("::", 2);
        Tcl_AppendObjToObj(fqnObj, nameObj);
        return fqnObj;
    }
}
