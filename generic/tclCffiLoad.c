/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

void CffiLibCtxUnref(CffiLibCtx *ctxP)
{
    if (ctxP->nRefs <= 1) {
        /* Note cdlP->vmCtxP is interp-specific and not to be deleted here. */
#ifdef CFFI_USE_TCLLOAD
        Tcl_FSUnloadFile(NULL, ctxP->libH);
#else
        dlFreeLibrary(ctxP->libH);
#endif
        if (ctxP->pathObj)
            Tcl_DecrRefCount(ctxP->pathObj);
        ckfree(ctxP);
    }
    else
        ctxP->nRefs -= 1;
}

/* Function: CffiLibFindSymbol
 * Returns the value of a symbol in a loaded shared library.
 *
 * Parameters:
 * ip - interpreter for error messages. May be NULL.
 * libH - handle to the loaded library
 * symbolObj - symbol name
 * flags - if the CFFI_F_LOOKUP_STDCALL is set, an alternate
 *     name for the symbol is also looked up. Other bits are ignored.
 *
 * Note there is no way to distinguish between a symbol being
 * present with a 0 value and a missing symbol. In both cases, the
 * function will return *NULL*.
 *
 * Currently the alternate name for a symbol only applies to 32-bit Windows
 * build where if the symbol is not found, the lookup is retried with
 * a _ prefix. This is to allow for the stdcall convention.
 *
 *
 * Returns:
 * Returns the symbol value or *NULL* is symbol is not found.
 */
void *
CffiLibFindSymbol(Tcl_Interp *ip,
                  CffiLoadHandle libH,
                  Tcl_Obj *symbolObj,
                  CffiFlags flags)
{
    void *addr;

#ifdef CFFI_USE_TCLLOAD
    addr = Tcl_FindSymbol(NULL, libH, Tcl_GetString(symbolObj));
#else
    addr = dlFindSymbol(libH, Tcl_GetString(symbolObj));
#endif

#if defined(_WIN32) && !defined(_WIN64)
    if (addr == NULL && (flags & CFFI_F_LOOKUP_STDCALL)) {
        Tcl_Obj *altNameObj = Tcl_ObjPrintf("_%s", Tcl_GetString(symbolObj));
        Tcl_IncrRefCount(altNameObj);
        Tcl_DecrRefCount(altNameObj);
#ifdef CFFI_USE_TCLLOAD
        addr = Tcl_FindSymbol(NULL, libH, Tcl_GetString(altNameObj));
#else
        addr = dlFindSymbol(libH, Tcl_GetString(altNameObj));
#endif
    }
#endif
    if (addr == NULL && ip != NULL) {
        Tclh_ErrorNotFound(ip, "Symbol", symbolObj, NULL);
    }
    return addr;
}

/* Function: CffiLibLoad
 * Loads shared library and returns an allocated context for it.
 *
 * Parameters:
 * ip - interpreter
 * pathObj - path to the shared library
 * ctxPP - location to store pointer to allocated context.
 *
 * If *pathObj* is NULL or contains an empty string, it is interpreted
 * as the path to the executable as returned by the Tcl
 * *info nameofexecutable* command.
 *
 * Returns:
 * A Tcl return code.
 */
CffiResult CffiLibLoad(Tcl_Interp *ip, Tcl_Obj *pathObj, CffiLibCtx **ctxPP)
{
    CffiLibCtx *ctxP;
    CffiLoadHandle dlH;
    const char *pathP = NULL;

    if (pathObj) {
        pathP = Tcl_GetString(pathObj);
        if (*pathP == '\0') {
            pathObj = NULL;
            pathP   = NULL;
        }
    }
    if (pathP == NULL) {
        pathP = Tcl_GetNameOfExecutable();
        if (pathP == NULL) {
            /* Should not happen unless invoked at Tcl initialization time */
            return Tclh_ErrorNotFound(
                ip, "Shared library", pathObj, "Empty library file path and could not retrieve executable name.");
        }
        pathObj = Tcl_NewStringObj(pathP, -1);
    }

    CFFI_ASSERT(pathObj);
    CFFI_ASSERT(pathP);

    /* Tcl_LoadFile does not like objects with refcount > 0 */
    Tcl_IncrRefCount(pathObj);

#ifdef CFFI_USE_TCLLOAD
    if (Tcl_LoadFile(ip, pathObj, NULL, 0, NULL, &dlH) != TCL_OK)
        dlH = NULL;
#else
    dlH = dlLoadLibrary(pathP);
#endif

    if (dlH == NULL) {
        Tclh_ErrorNotFound(ip, "Shared library", pathObj, "Could not load shared library.");
        Tcl_DecrRefCount(pathObj);
        return TCL_ERROR;
    }

    ctxP          = ckalloc(sizeof(*ctxP));
    ctxP->ipCtxP  = NULL;
    ctxP->libH     = dlH;
    ctxP->pathObj = pathObj; /* Note ref count was already incr'ed above */
    ctxP->nRefs   = 1;
    *ctxPP        = ctxP;
    return TCL_OK;
}

/* Function: CffiLibPath
 * Returns the file path corresponding to a shared library context.
 *
 * Parameters:
 * ip - interpreter
 * ctxP - the shared library context
 *
 * The reference count on the returned *Tcl_Obj* must not be
 * decremented without a preceding increment. That is the caller's
 * responsibility.
 *
 * Returns:
 * A *Tcl_Obj* containing the path. The path may be an empty string
 * if it cannot be obtained.
 */
Tcl_Obj *CffiLibPath(Tcl_Interp *ip, CffiLibCtx *ctxP)
{
#ifndef CFFI_USE_TCLLOAD
    int len;
    char buf[1024 + 1]; /* TBD - what size ? */

    len = dlGetLibraryPath(ctxP->libH, buf, sizeof(buf) / sizeof(buf[0]));
    /*
     * Workarounds for bugs in dyncall 1.2 on some platforms when dcLoad was
     * called with null argument.
     */
    if (len > 0) {
        if (buf[len-1] == '\0')
            --len;
        return Tcl_NewStringObj(buf, len);
    }
#endif

    if (ctxP->pathObj)
        return ctxP->pathObj;
    else
        return Tcl_NewObj();
}
