/*
 * Copyright (c) 2022, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

/* Function: CffiNameSyntaxCheck
 * Verifies that a name matches permitted syntax.`
 *
 * Parameters:
 * ip - interpreter
 * nameObj - name to be checked
 *
 * Returns:
 * *TCL_OK* if syntax is permitted, other *TCL_ERROR* with error message
 * in interpreter.
 */
CffiResult CffiNameSyntaxCheck(Tcl_Interp *ip, Tcl_Obj *nameObj)
{
    unsigned char ch;
    const unsigned char *nameP; /* NOTE *unsigned* char for isalpha etc.*/

    nameP = (unsigned char*) Tcl_GetString(nameObj);
    ch    = *nameP++;
    /* First letter must be alpha */
    if (isalpha(ch) || ch == '_' || ch == ':') {
        /* Subsequent letter alphanumeric, _ or : */
        while ((ch = *nameP++) != '\0') {
            if (!isalnum(ch) && ch != '_' && ch != ':')
                goto invalid_alias_syntax; /* Horrors, a goto */
        }
        return TCL_OK;
    }
invalid_alias_syntax:
    return Tclh_ErrorInvalidValue(ip, nameObj, "Invalid name syntax.");
}


/* Function: CffiNameLookup
 * Looks up a name in a specified table, returning the associated
 * value.
 *
 * Parameters:
 * ip - interpreter. If NULL, names are not resolved using the
 *    current namespace and error messages are not recorded.
 * htP - hash table to look up
 * nameP - name to use as the key
 * nameTypeP - the type of name being looked up. Only used for error
 *    messages. May be NULL.
 * flags - if the CFFI_F_SKIP_ERROR_MESSAGES bit is set,
 *    errors are not record even if ip is not NULL.
 * valueP - location to store the retrieved value if found.
 * fqnObjP - location to store the fully qualified name of the found entry.
 *    May be NULL if not of interest.
 *
 * If *nameP* is fully qualified, it is used directly for the lookup.
 * Otherwise, an attempt is made to lookup by qualifying with the current
 * namespace if *ip* is not NULL, and as a last resort, the global
 * namespace.
 *
 * Returns:
 * *TCL_OK* if successful, else *TCL_ERROR*.
 */
CffiResult
CffiNameLookup(Tcl_Interp *ip,
               Tcl_HashTable *htP,
               const char *nameP,
               const char *nameTypeP,
               CffiFlags flags,
               ClientData *valueP,
               Tcl_Obj **fqnObjP)
{
    Tcl_DString ds;
    CffiResult ret;
    const char *fqnP;
    Tcl_Namespace *nsP;

    if (Tclh_NsIsFQN(nameP)) {
        if (Tclh_HashLookup(htP, nameP, valueP) != TCL_OK)
            goto notfound;
        if (fqnObjP)
            *fqnObjP = Tcl_NewStringObj(nameP, -1);
        return TCL_OK;
    }

    /* If interpreter provided, try with current namespace, else global */
    if (ip) {
        nsP  = Tcl_GetCurrentNamespace(ip);
        fqnP = Tclh_NsQualifyName(NULL, nameP, -1, &ds, nsP->fullName);
        ret  = Tclh_HashLookup(htP, fqnP, valueP);
        if (ret == TCL_OK && fqnObjP)
            *fqnObjP = Tcl_NewStringObj(fqnP, -1); /* BEFORE freeing ds! */
        Tcl_DStringFree(&ds);
        if (ret == TCL_OK)
            return TCL_OK;
        /* If current namespace was global, no point trying with global below */
        if (Tclh_NsIsGlobalNs(nsP->fullName))
            goto notfound;
    }

    /* Final resort - global namespace */
    fqnP = Tclh_NsQualifyName(NULL, nameP, -1, &ds, "::");
    ret  = Tclh_HashLookup(htP, fqnP, valueP);
    if (ret == TCL_OK && fqnObjP)
        *fqnObjP = Tcl_NewStringObj(fqnP, -1); /* BEFORE freeing ds! */
    Tcl_DStringFree(&ds);

    if (ret == TCL_OK)
        return TCL_OK;

notfound:
    if (ip && (flags & CFFI_F_SKIP_ERROR_MESSAGES) == 0)
        Tclh_ErrorNotFoundStr(ip, nameTypeP, nameP, NULL);
    return TCL_ERROR;
}

/* Function: CffiNameAdd
 * Adds an entry to a name lookup table
 *
 * Parameters:
 * ip - interpreter. Must not be NULL if nameP is not fully qualified.
 * htP - name table
 * nameP - name to add
 * nameTypeP - type of the object the name references. Only used in error
 *   messages and may be *NULL*.
 * value - value to add
 * fqnObjP - location to store the fully qualified name. May be NULL.
 *   The *Tcl_Obj* reference count should not be decremented without
 *   an explicit increment.
 *
 * If *name* is not fully qualified, it is qualified with the name of the
 * current namespace.
 *
 * Returns:
 * *TCL_OK* if successfully added or *TCL_ERROR* if the name already exists.
 */
CffiResult
CffiNameAdd(Tcl_Interp *ip,
            Tcl_HashTable *htP,
            const char *nameP,
            const char *nameTypeP,
            ClientData value,
            Tcl_Obj **fqnObjP)
{
    Tcl_DString ds;
    CffiResult ret;
    Tcl_DStringInit(&ds);
    if (!Tclh_NsIsFQN(nameP)) {
        if (ip == NULL) {
            ret = Tclh_ErrorInvalidValueStr(
                ip,
                nameP,
                "Internal error: relative name cannot be resolved if "
                "interpreter is not specified");
            goto vamoose;
        }
        nameP = Tclh_NsQualifyName(ip, nameP, -1, &ds, NULL);
    }
    ret = Tclh_HashAdd(ip, htP, nameP, value);
    if (ret == TCL_OK)  {
        if (fqnObjP)
            *fqnObjP = Tcl_NewStringObj(nameP, -1);
    }
    else {
        Tcl_AppendResult(ip,
                         nameTypeP ? nameTypeP : "Entry",
                         " with name \"",
                         nameP,
                         "\" already exists.",
                         NULL);
    }
vamoose:
    Tcl_DStringFree(&ds);
    return ret;
}

/* Function: CffiNameObjAdd
 * Adds an entry to a name lookup table
 *
 * Parameters:
 * ip - interpreter. Must not be NULL if nameP is not fully qualified.
 * htP - name table
 * nameObj - name to add
 * nameTypeP - type of the object the name references. Only used in error
 *   messages and may be *NULL*.
 * value - value to add
 * fqnObjP - location to store the fully qualified name. May be NULL.
 *   The *Tcl_Obj* reference count should not be decremented without
 *   an explicit increment.
 *
 * If *nameObj* is not fully qualified, it is qualified with the name of the
 * current namespace.
 *
 * Returns:
 * *TCL_OK* if successfully added or *TCL_ERROR* if the name already exists.
 */
CffiResult
CffiNameObjAdd(Tcl_Interp *ip,
               Tcl_HashTable *htP,
               Tcl_Obj *nameObj,
               const char *nameTypeP,
               ClientData value,
               Tcl_Obj **fqnObjP)
{
    return CffiNameAdd(ip, htP, Tcl_GetString(nameObj), nameTypeP, value, fqnObjP);
}

struct CffiNameListNamesState {
    Tcl_Obj *resultObj;
    const char *pattern;
    int pattern_tail_pos;
    };

static int
CffiNameListNamesCallback(Tcl_HashTable *htP,
                          Tcl_HashEntry *heP,
                          ClientData clientData)
{
    struct CffiNameListNamesState *stateP =
        (struct CffiNameListNamesState *)clientData;
    const char *key = Tcl_GetHashKey(htP, heP);

    if (stateP->pattern) {
        int key_tail_pos = Tclh_NsTailPos(key);
        /* Split the tail to match that as a wildcard pattern */
        if (key_tail_pos != stateP->pattern_tail_pos ||
        strncmp(key, stateP->pattern, key_tail_pos) ||
        !Tcl_StringMatch(key+key_tail_pos, stateP->pattern+key_tail_pos))
            return 1;

    }
    /* Include in the list of matches */
    Tcl_ListObjAppendElement(
        NULL, stateP->resultObj, Tcl_NewStringObj(key, -1));

    return 1; /* Keep iterating */
}

/* Function: CffiNameListNames
 * Retrieves the list of names matching a pattern.
 *
 * Parameters:
 * ip - interpreter. May be NULL. Only used for error messages.
 * htP - hash table to be enumerated
 * pattern - pattern to match. May be NULL to match all. If not
 *   not fully qualified, it is qualified with the current namespace.
 *   Only the tail of the pattern is treated as a glob pattern
 *   and matched against the tail of the hash table key. The rest is treated
 *   as a normal string compare.
 * namesObjP - location to store the list of names
 *
 * Returns:
 * *TCL_OK* on success with list of matching names stored in *namesObjP* and
 * *TCL_ERROR* on error.
 */
CffiResult
CffiNameListNames(Tcl_Interp *ip,
                  Tcl_HashTable *htP,
                  const char *pattern,
                  Tcl_Obj **namesObjP)
{
    struct CffiNameListNamesState state;
    Tcl_DString ds;

    state.resultObj = Tcl_NewListObj(0, NULL);
    if (pattern) {
        state.pattern = Tclh_NsQualifyName(ip, pattern, -1, &ds, NULL);
        state.pattern_tail_pos = Tclh_NsTailPos(state.pattern);
    }
    else {
        state.pattern = NULL;
        state.pattern_tail_pos = 0;
    }
    Tclh_HashIterate(htP, CffiNameListNamesCallback, &state);
    if (pattern)
        Tcl_DStringFree(&ds);
    *namesObjP = state.resultObj;
    return TCL_OK;
}

struct CffiNameDeleteNamesState {
    const char *pattern;
    int pattern_tail_pos;
    void (*deleteFn)(ClientData clientData);
};

static int
CffiNameDeleteNamesCallback(Tcl_HashTable *htP,
                            Tcl_HashEntry *heP,
                            ClientData clientData)
{
    struct CffiNameDeleteNamesState *stateP =
        (struct CffiNameDeleteNamesState *)clientData;

    if (stateP->pattern) {
        const char *key  = Tcl_GetHashKey(htP, heP);
        int key_tail_pos = Tclh_NsTailPos(key);
        /* Split the tail to match that as a wildcard pattern */
        if (key_tail_pos != stateP->pattern_tail_pos ||
        strncmp(key, stateP->pattern, key_tail_pos) ||
        !Tcl_StringMatch(key+key_tail_pos, stateP->pattern+key_tail_pos))
            return 1;
    }
    stateP->deleteFn(Tcl_GetHashValue(heP));
    Tcl_DeleteHashEntry(heP);

    return 1;
}

/* Function: CffiNameDeleteNames
 * Deletes the list of names matching a pattern.
 *
 * Parameters:
 * ip - interpreter. May be NULL. Only used for error messages.
 * htP - hash table to be enumerated
 * pattern - pattern to match. May be NULL to delete all. If not
 *   not fully qualified, it is qualified with the current namespace.
 *   Only the tail of the pattern is treated as a glob pattern
 *   and matched against the tail of the hash table key. The rest is treated
 *   as a normal string compare.
 * deleteFn - function to call with value for the name.
 * Returns:
 * *TCL_OK* on success and *TCL_ERROR* on failure.
 */
CffiResult
CffiNameDeleteNames(Tcl_Interp *ip,
                    Tcl_HashTable *htP,
                    const char *pattern,
                    void (*deleteFn)(ClientData))
{
    struct CffiNameDeleteNamesState state;
    Tcl_DString ds;
    if (pattern) {
        state.pattern = Tclh_NsQualifyName(ip, pattern, -1, &ds, NULL);
        state.pattern_tail_pos = Tclh_NsTailPos(state.pattern);
    }
    else {
        state.pattern = NULL;
        state.pattern_tail_pos = 0;
    }

    state.deleteFn = deleteFn;
    Tclh_HashIterate(htP, CffiNameDeleteNamesCallback, &state);
    if (pattern)
        Tcl_DStringFree(&ds);
    return TCL_OK;
}

/* Function: CffiNameTableFinit
 * Cleans up a name table releasing resources
 *
 * Parameters:
 * ip - interpreter. May be NULL. Only used for error messages.
 * htP - name table to clean up
 * deleteFn - function to call with value for the name.
 */
void
CffiNameTableFinit(Tcl_Interp *ip,
                   Tcl_HashTable *htP,
                   void (*deleteFn)(ClientData))
{
    CffiNameDeleteNames(ip, htP, NULL, deleteFn);
    Tcl_DeleteHashTable(htP);
}

/* Function: CffiNameTableInit
 * Initializes a table of names
 *
 * Parameters:
 * htP - table to initialize
 */
void
CffiNameTableInit(Tcl_HashTable *htP)
{
    Tcl_InitHashTable(htP, TCL_STRING_KEYS);
}