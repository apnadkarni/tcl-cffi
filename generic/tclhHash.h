#ifndef TCLHHASH_H
#define TCLHHASH_H

/*
 * Copyright (c) 2022, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclhBase.h"


/* Function: Tclh_HashAdd
 * Adds an entry to a table of names
 *
 * Parameters:
 * ip - interpreter. May be NULL if error messages not desired.
 * htP - table of names
 * key - key to add
 * value - value to associate with the name
 *
 * An error is returned if the entry already exists.
 *
 * Returns:
 * TCL_OK on success, TCL_ERROR on failure.
 */
int Tclh_HashAdd(Tcl_Interp *ip,
                 Tcl_HashTable *htP,
                 const void *key,
                 ClientData value);

/* Function: Tclh_HashAddOrReplace
 * Adds or replaces an entry to a table of names
 *
 * Parameters:
 * htP - table of names
 * key - key to add
 * value - value to associate with the name
 * oldValueP - location to store old value if an existing entry was replaced
 *
 * If the function returns 0, an existing entry was replaced, and
 * the value previous associated with the name is returned in *oldValueP*.
 *
 * Returns:
 * 0 if the name already existed, and non-0 if a new entry was added.
 */
int Tclh_HashAddOrReplace(Tcl_HashTable *htP,
                          const void *key,
                          ClientData value,
                          ClientData *oldValueP);

/* Function: Tclh_HashIterate
 * Invokes a specified function on all entries in a hash table
 *
 * Parameters:
 * htP - hash table
 * fnP - function to call for each entry
 * fnData - any value to pass to fnP
 *
 * The callback function *fnP* is passed a pointer to the hash entry. It
 * should follow the rules for Tcl hash tables and not modify the table
 * itself except for optionally deleting the passed entry. The iteration
 * will terminate if the callback function returns 0.
 *
 * Returns:
 * Returns *1* if the iteration terminated normally with all entries processed,
 * and *0* if it was terminated by the callback returning *0*.
 */
int Tclh_HashIterate(Tcl_HashTable *htP,
                     int (*fnP)(Tcl_HashEntry *, ClientData),
                     ClientData fnData);

#ifdef TCLH_SHORTNAMES

#define HashAdd          Tclh_HashAdd
#define HashAddOrReplace Tclh_HashAddOrReplace
#define HashIterate      Tclh_HashIterate

#endif

#ifdef TCLH_IMPL
# define TCLH_HASH_IMPL
#endif

#ifdef TCLH_HASH_IMPL

/* Section: Hash table utilities
 *
 * Contains utilities dealing with Tcl hash tables.
 */

int
Tclh_HashAdd(Tcl_Interp *ip,
             Tcl_HashTable *htP,
             const void *key,
             ClientData value)
{
    Tcl_HashEntry *heP;
    int isNew;

    heP = Tcl_CreateHashEntry(htP, key, &isNew);
    if (!isNew && ip)
        return Tclh_ErrorExists(ip, "Name", NULL, NULL);
    Tcl_SetHashValue(heP, value);
    return TCL_OK;
}

int
Tclh_HashAddOrReplace(Tcl_HashTable *htP,
                      const void *key,
                      ClientData value,
                      ClientData *oldValueP)
{
    Tcl_HashEntry *heP;
    int isNew;

    heP = Tcl_CreateHashEntry(htP, key, &isNew);
    if (!isNew)
        *oldValueP = Tclh_GetHashValue(heP);
    Tcl_SetHashValue(heP, value);
    return isNew;
}

int
Tclh_HashIterate(Tcl_HashTable *htP,
                 int (*fnP)(Tcl_HashEntry *, ClientData),
                 ClientData fnData)
{
    Tcl_HashEntry *heP;
    Tcl_HashSearch hSearch;

    for (heP = Tcl_FirstHashEntry(htP, &hSearch); heP != NULL;
         heP = Tcl_NextHashEntry(&hSearch)) {
        if (fnP(heP, fnData) == 0)
            return 0;
    }
    return 1;
}

#endif /* TCLH_HASH_IMPL */

#endif /* TCLHHASH_H */