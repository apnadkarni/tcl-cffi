/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

static void
CffiScopeCleanup(CffiScope *scopeP)
{
    CffiEnumsCleanup(&scopeP->enums);
    CffiAliasesCleanup(&scopeP->aliases);
}

static void
CffiScopeUnref(CffiScope *scopeP)
{
    if (scopeP->nRefs > 1)
        scopeP->nRefs -= 1;
    else {
        CffiScopeCleanup(scopeP);
        ckfree(scopeP);
    }
}

/* Called on interp deletion to release all scopes */
void
CffiScopesCleanup(Tcl_HashTable *scopesP)
{
    Tcl_HashEntry *heP;
    Tcl_HashSearch hSearch;
    for (heP = Tcl_FirstHashEntry(scopesP, &hSearch);
         heP != NULL; heP = Tcl_NextHashEntry(&hSearch)) {
        CffiScope *scopeP = Tcl_GetHashValue(heP);
        CffiScopeUnref(scopeP);
    }
    Tcl_DeleteHashTable(scopesP);
}

/* Function: CffiScopeGet
 * Returns the scope descriptor for the specified scope
 *
 * Parameters:
 * ipCtxP - the interpreter context holding the scopes
 * nameP - the name of the scope. If NULL, the current namespace is used.
 *
 * The scope is created if it does not exist.
 *
 * Returns:
 * Pointer to the scope descriptor.
 */
CffiScope *
CffiScopeGet(CffiInterpCtx *ipCtxP, const char *nameP)
{
    CffiScope *scopeP;
    Tcl_HashEntry *heP;
    Tcl_Namespace *nsP;
    int new_entry;

    if (nameP == NULL) {
        nsP = Tcl_GetCurrentNamespace(ipCtxP->interp);
        if (nsP == NULL)
            nameP = "::";
        else
            nameP = nsP->fullName;
    }

    heP = Tcl_CreateHashEntry(&ipCtxP->scopes, nameP, &new_entry);
    if (new_entry) {
        scopeP = ckalloc(sizeof(*scopeP));
        Tcl_InitObjHashTable(&scopeP->aliases);
        Tcl_InitObjHashTable(&scopeP->enums);
        scopeP->nRefs = 1;
        Tcl_SetHashValue(heP, scopeP);
    }
    else {
        scopeP = Tcl_GetHashValue(heP);
    }
    return scopeP;
}

