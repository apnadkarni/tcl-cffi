#ifndef TCLHNAMESPACE_H
#define TCLHNAMESPACE_H

/*
 * Copyright (c) 2022, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclhBase.h"

/* Section: Namespace utilities
 *
 * Provides utility routines related to Tcl namespaces.
 */

/* Function: Tclh_NamespaceLibInit
 * Must be called to initialize the Namespace module before any of
 * the other functions in the module.
 *
 * Parameters:
 * interp - Tcl interpreter in which to initialize.
 *
 * Returns:
 * TCL_OK    - Library was successfully initialized.
 * TCL_ERROR - Initialization failed. Library functions must not be called.
 *             An error message is left in the interpreter result.
 */
int Tclh_NsLibInit(Tcl_Interp *interp);

/* Function: Tclh_NsIsGlobalNs
 * Returns true if passed namespace is the global namespace.
 *
 * Parameters:
 * nsP - namespace name
 *
 * Any name consisting only of two or more ':' characters is considered
 * the global namespace.
 *
 * Returns:
 * 1 if the passed name is the name of the global namespace
 * and 0 otherwise.
 */
TCLH_INLINE int Tclh_NsIsGlobalNs(const char *nsP)
{
    const char *p = nsP;
    while (*p == ':')
        ++p;
    return (*p == '\0') && ((p - nsP) >= 2);
}

/* Function: Tclh_NsIsFQN
 * Returns true if the given name is fully qualified.
 *
 * Parameters:
 * nameP - name to check
 *
 * Returns:
 * 1 if the passed name is fully qualified and 0 otherwise.
 */
TCLH_INLINE int Tclh_NsIsFQN(const char *nsP)
{
    return nsP[0] == ':' && nsP[1] == ':';
}

/* Function: Tclh_NsQualifyNameObj
 * Returns a fully qualified name
 *
 * Parameters:
 * ip - interpreter. This may be NULL iff defaultNsP is not NULL.
 * nameObj - Name to be qualified
 * defaultNsP - namespace to use to qualify if necessary.
 *   This must be fully qualified. If NULL, the current namespace will be used.
 *
 * The passed *Tcl_Obj* is fully qualified with the current namespace if
 * not already a FQN.
 *
 * The returned *Tcl_Obj* may be the same as *nameObj* if it is already
 * fully qualified or a newly allocated one. In either case, no changes in
 * reference counts is done and left to the caller. In particular, when
 * releasing the Tcl_Obj, caller must increment the reference count before
 * decrementing it to correctly handle the case where the existing object
 * is returned.
 *
 * Returns:
 * A *Tcl_Obj* containing the fully qualified name.
 */
Tcl_Obj *
Tclh_NsQualifyNameObj(Tcl_Interp *ip, Tcl_Obj *nameObj, const char *defaultNsP);

/* Function: Tclh_NsQualifyName
 * Fully qualifies a name.
 *
 * Parameters:
 * ip - interpreter. This may be NULL iff defaultNsP is not NULL.
 * nameP - name to be qualified
 * dsP - storage to use if necessary. This is initialized by the function
 *   and must be freed with Tcl_DStringFree on return in all cases.
 * defaultNsP - namespace to use to qualify if necessary.
 *   This must be fully qualified. If NULL, the current namespace will be used.
 *
 * The passed name is fully qualified with the current namespace if
 * not already a FQN.
 *
 * Returns:
 * A pointer to a fully qualified name. This may be either nameP or a
 * pointer into dsP. Caller should not assume eiter.
 */
const char *Tclh_NsQualifyName(Tcl_Interp *ip,
                         const char *nameP,
                         Tcl_DString *dsP,
                         const char *defaultNsP);

/* Function: Tclh_NsTailPos
 * Returns the index of the tail component in a name.
 *
 * Parameters:
 * nameP - the name to parse
 *
 * If there are no qualifiers, the returned index will be 0 corresponding to
 * the start of the name. If the name ends in a namespace separator, the
 * index will be that of the terminating nul.
 *
 * Returns:
 * Index of the tail component.
 */
int Tclh_NsTailPos(const char *nameP);

#ifdef TCLH_SHORTNAMES

#define NsIsGlobalNs  Tclh_NsIsGlobalNs
#define NsIsFQN       Tclh_NsIsFQN
#define NsQualifyName Tclh_NsQualifyName
#endif

/*
 * IMPLEMENTATION
 */

#ifdef TCLH_IMPL
# define TCLH_NAMESPACE_IMPL
#endif

#ifdef TCLH_NAMESPACE_IMPL

int Tclh_NsLibInit(Tcl_Interp *ip) {
    return Tclh_BaseLibInit(ip);
}

Tcl_Obj *
Tclh_NsQualifyNameObj(Tcl_Interp *ip, Tcl_Obj *nameObj, const char *defaultNsP)
{
    const char *nameP;
    Tcl_Namespace *nsP;
    Tcl_Obj *fqnObj;
    Tcl_Size nameLen;

    nameP = Tcl_GetStringFromObj(nameObj, &nameLen);
    if (Tclh_NsIsFQN(nameP))
        return nameObj;

    if (defaultNsP == NULL) {
        nsP = Tcl_GetCurrentNamespace(ip);
        TCLH_ASSERT(nsP);
        defaultNsP = nsP->fullName;
    }

    fqnObj = Tcl_NewStringObj(defaultNsP, -1);
    /* Append '::' only if not global namespace else we'll get :::: */
    if (!Tclh_NsIsGlobalNs(defaultNsP))
        Tcl_AppendToObj(fqnObj, "::", 2);
    Tcl_AppendToObj(fqnObj, nameP, nameLen);
    return fqnObj;
}

const char *
Tclh_NsQualifyName(Tcl_Interp *ip, const char *nameP, Tcl_DString *dsP, const char *defaultNsP)
{
    Tcl_Namespace *nsP;

    Tcl_DStringInit(dsP); /* Init BEFORE return below since caller will Reset it */

    if (Tclh_NsIsFQN(nameP))
        return nameP;

    /* Qualify with current namespace */
    if (defaultNsP == NULL) {
        nsP = Tcl_GetCurrentNamespace(ip);
        TCLH_ASSERT(nsP);
        defaultNsP = nsP->fullName;
    }

    Tcl_DStringAppend(dsP, defaultNsP, -1);
    /* Append '::' only if not global namespace else we'll get :::: */
    if (!Tclh_NsIsGlobalNs(defaultNsP))
        Tcl_DStringAppend(dsP, "::", 2);
    Tcl_DStringAppend(dsP, nameP, -1);
    return Tcl_DStringValue(dsP);
}



/* TBD - test with sandbox */
int
Tclh_NsTailPos(const char *nameP)
{
    const char *tailP;

    /* Go to end. TBD - +strlen() intrinsic faster? */
    for (tailP = nameP;  *tailP != '\0';  tailP++) {
        /* empty body */
    }

    while (tailP > (nameP+1)) {
        if (tailP[-1] == ':' && tailP[-2] == ':')
            return (int) (tailP - nameP);
        --tailP;
    }
    /* Could not find a :: */
    return 0;
}



#endif /* TCL_NAMESPACE_IMPL */

#endif /* TCLHNAMESPACE_H */