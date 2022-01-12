/*
 * Copyright (c) 2021 Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

#ifdef OBSOLETE
static void
CffiScopeCleanup(CffiScope *scopeP)
{
    CffiAliasesCleanup(&scopeP->aliases);
    CffiEnumsCleanup(&scopeP->enums);
    CffiPrototypesCleanup(&scopeP->prototypes);
}

/* Function: CffiScopeInit
 * Initializes a scope structure
 *
 * Parameters:
 * scopeP - the scope structure to initialize
 *
 * Returns:
 * *TCL_OK* or *TCL_ERROR*.
 */
static CffiResult
CffiScopeInit(CffiScope *scopeP)
{
    TBDTcl_InitObjHashTable(&scopeP->aliases);
    Tcl_InitHashTable(&scopeP->enums, TCL_STRING_KEYS);
    TBDTcl_InitObjHashTable(&scopeP->prototypes);
    return TCL_OK;
}

/* Called on interp deletion to release all scopes */
void
CffiScopesCleanup(CffiInterpCtx *ipCtxP)
{
    CffiScopeCleanup(&ipCtxP->scope);
}

CffiResult
CffiScopesInit(CffiInterpCtx *ipCtxP)
{
    return CffiScopeInit(&ipCtxP->scope);
}
#endif // OBSOLETE
