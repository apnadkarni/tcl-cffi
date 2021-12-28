#ifndef TCLHPOINTER_H
#define TCLHPOINTER_H

#include "tclhBase.h"

/* Typedef: Tclh_PointerTypeTag
 *
 * See <Pointer Type Tags>.
 */
typedef Tcl_Obj *Tclh_PointerTypeTag;

/* Section: Registered Pointers
 *
 * Provides a facility for safely passing pointers, Windows handles etc. to the
 * Tcl script level. The intent of pointer registration is to make use of
 * pointers passed to script level more robust by preventing errors such as use
 * after free, the wrong pointer type etc. Each pointer is also optionally typed
 * with a tag and verification can check not only that the pointer is registered
 * but has the right type tag.
 *
 * The function <Tclh_PointerLibInit> must be called before any other functions
 * from this library. This must be done in the extension's initialization
 * function for every interpreter in which the extension is loaded.
 *
 * Pointers can be registered as valid with <Tclh_PointerRegister> before being
 * passed up to the script. When passed in from a script, their validity can be
 * checked with <Tclh_PointerVerify>. Pointers should be marked invalid as
 * appropriate by unregistering them with <Tclh_PointerUnregister> or
 * alternatively <Tclh_PointerObjUnregister>. For convenience, when a pointer
 * may match one of several types, the <Tclh_PointerVerifyAnyOf> and
 * <Tclh_PointerObjVerifyAnyOf> take a variable number of type tags.
 *
 * If pointer registration is not deemed necessary (dangerous), the functions
 * <Tclh_PointerWrap> and <Tclh_PointerUnwrap> can be used to convert pointers
 * to and from Tcl_Obj values.
 *
 * Section: Pointer Type Tags
 *
 * Pointers are optionally associated with a type using a type tag
 * so that when checking arguments, the pointer type tag can be checked as
 * well. The type tag is typedefed as Tcl's *Tcl_Obj* type and is
 * treated as opaque as far as this library is concerned. The application
 * must provide a related functions, <Tclh_PointerTagMatch>, for the purpose
 * of checking a pointer tag.
 *
 * As a special case, no type checking is done on pointers with a type tag of
 * NULL.
 */

/* Function: Tclh_PointerLibInit
 * Must be called to initialize the Pointer module before any of
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
int Tclh_PointerLibInit(Tcl_Interp *interp);

/* Function: Tclh_PointerTagMatch
 * An application-supplied function called to match a pointer tag.
 *
 * Parameters:
 * pointer_tag - The tag associated with a pointer
 * expected_tag - The tag that *pointer_tag* is expected to match
 *
 * This is an *application-supplied* function that is called by the library
 * to see if the tag *pointer_tag* for a pointer matches the tag
 * *expected_tag*. The function should return a non-zero value on
 * a match and *0* otherwise. It is up to the application to decide what
 * construes a match, e.g. taking any inheritance models into consideration.
 *
 * Note that this function is only called if *pointer_tag* and
 * *expected_tag* do not have the same value.
 *
 * Returns:
 * *Non-0* if the pointer matches and *0* if it does not.
 */
int Tclh_PointerTagMatch(Tclh_PointerTypeTag pointer_tag,
                         Tclh_PointerTypeTag expected_tag);

/* Function: Tclh_PointerRegister
 * Registers a pointer value as being "valid".
 *
 * Parameters:
 * interp  - Tcl interpreter in which the pointer is to be registered.
 * pointer - Pointer value to be registered.
 * tag     - Type tag for the pointer. Pass NULL or 0 for typeless pointers.
 * objPP   - if not NULL, a pointer to a new Tcl_Obj holding the pointer
 *           representation is stored here on success. The Tcl_Obj has
 *           a reference count of 0.
 *
 * The validity of a registered pointer can then be tested by with
 * <Tclh_PointerVerify> and reversed by calling <Tclh_PointerUnregister>.
 *
 * Returns:
 * TCL_OK    - pointer was successfully registered
 * TCL_ERROR - pointer registration failed. An error message is stored in
 *             the interpreter.
 */
int Tclh_PointerRegister(Tcl_Interp *interp, void *pointer,
                         Tclh_PointerTypeTag tag, Tcl_Obj **objPP);

/* Function: Tclh_PointerRegisterCounted
 * Registers a pointer value as being "valid" permitting multiple registrations
 * for the same pointer.
 *
 * Parameters:
 * interp  - Tcl interpreter in which the pointer is to be registered.
 * pointer - Pointer value to be registered.
 * tag     - Type tag for the pointer. Pass NULL or 0 for typeless pointers.
 * objPP   - if not NULL, a pointer to a new Tcl_Obj holding the pointer
 *           representation is stored here on success. The Tcl_Obj has
 *           a reference count of 0.
 *
 * The validity of a registered pointer can then be tested by with
 * <Tclh_PointerVerify> and reversed by calling <Tclh_PointerUnregister>.
 * Unlike <Tclh_PointerRegister>, this command allows a pointer to be
 * registered multiple times. It will be unregistered when
 * <Tclh_PointerUnregister> is called the same number of times.
 *
 * Returns:
 * TCL_OK    - pointer was successfully registered
 * TCL_ERROR - pointer registration failed. An error message is stored in
 *             the interpreter.
 */
int Tclh_PointerRegisterCounted(Tcl_Interp *interp, void *pointer,
                                Tclh_PointerTypeTag tag, Tcl_Obj **objPP);

/* Function: Tclh_PointerUnregister
 * Unregisters a previously registered pointer.
 *
 * Parameters:
 * interp   - Tcl interpreter in which the pointer is to be unregistered.
 * pointer  - Pointer value to be unregistered.
 * expected_tag  - Type tag for the pointer.
 *
 * The pointer may have been registered either via <Tclh_PointerRegister> or
 * <Tclh_PointerRegisterCounted>. In the former case, the pointer becomes
 * immediately inaccessible (as defined by Tclh_PointerVerify). In the latter
 * case, it will become inaccessible if it has been unregistered as many times
 * as it has been registered.
 *
 * Returns:
 * TCL_OK    - The pointer was successfully unregistered.
 * TCL_ERROR - The pointer was not registered or was registered with a
 *             different type. An error message is left in interp.
 */
int Tclh_PointerUnregister(Tcl_Interp *interp, const void *pointer, Tclh_PointerTypeTag expected_tag);


/* Function: Tclh_PointerVerify
 * Verifies that the passed pointer is registered as a valid pointer
 * of a given type.
 *
 * interp   - Tcl interpreter in which the pointer is to be verified.
 * pointer  - Pointer value to be verified.
 * expected_tag - Type tag for the pointer. If *NULL*, the pointer registration
 *                is verified but its tag is not checked.
 *
 * Returns:
 * TCL_OK    - The pointer is registered with the same type tag.
 * TCL_ERROR - Pointer is unregistered or a different type. An error message
 *             is stored in interp.
 */
int Tclh_PointerVerify(Tcl_Interp *interp, const void *voidP, Tclh_PointerTypeTag expected_tag);

/* Function: Tclh_PointerObjGetTag
 * Returns the pointer type tag for a Tcl_Obj pointer wrapper.
 *
 * Parameters:
 * interp - Interpreter to store errors if any. May be NULL.
 * objP   - Tcl_Obj holding the wrapped pointer.
 * tagPtr - Location to store the type tag. Note the reference count is *not*
 *          incremented. Caller should do that if it wants to preserve it.
 *
 * Returns:
 * TCL_OK    - objP holds a valid pointer. *tagPtr will hold the type tag.
 * TCL_ERROR - objP is not a wrapped pointer. interp, if not NULL, will hold
 *             error message.
 */
int Tclh_PointerObjGetTag(Tcl_Interp *interp, Tcl_Obj *objP, Tclh_PointerTypeTag *tagPtr);

/* Function: Tclh_PointerObjUnregister
 * Unregisters a previously registered pointer passed in as a Tcl_Obj.
 * Null pointers are silently ignored without an error being raised.
 *
 * Parameters:
 * interp   - Tcl interpreter in which the pointer is to be unregistered.
 * objP     - Tcl_Obj containing a pointer value to be unregistered.
 * pointerP - If not NULL, the pointer value from objP is stored here
 *            on success.
 * tagPtr   - Type tag for the pointer. May be 0 or NULL if pointer
 *            type is not to be checked.
 *
 * Returns:
 * TCL_OK    - The pointer was successfully unregistered.
 * TCL_ERROR - The pointer was not registered or was registered with a
 *             different type. An error message is left in interp.
 */
int Tclh_PointerObjUnregister(Tcl_Interp *interp, Tcl_Obj *objP,
                              void **pointerP, Tclh_PointerTypeTag tag);

/* Function: Tclh_PointerObjUnregisterAnyOf
 * Unregisters a previously registered pointer passed in as a Tcl_Obj
 * after checking it is one of the specified types.
 *
 * Parameters:
 * interp   - Tcl interpreter in which the pointer is to be unregistered.
 * objP     - Tcl_Obj containing a pointer value to be unregistered.
 * pointerP - If not NULL, the pointer value from objP is stored here
 *            on success.
 * tag      - Type tag for the pointer. May be 0 or NULL if pointer
 *            type is not to be checked.
 *
 * Returns:
 * TCL_OK    - The pointer was successfully unregistered.
 * TCL_ERROR - The pointer was not registered or was registered with a
 *             different type. An error message is left in interp.
 */
int Tclh_PointerObjUnregisterAnyOf(Tcl_Interp *interp, Tcl_Obj *objP,
                                   void **pointerP, ... /* tag, ... , NULL */);

/* Function: Tclh_PointerObjVerify
 * Verifies a Tcl_Obj contains a wrapped pointer that is registered
 * and, optionally, of a specified type.
 *
 * Parameters:
 * interp   - Tcl interpreter in which the pointer is to be verified.
 * objP     - Tcl_Obj containing a pointer value to be verified.
 * pointerP - If not NULL, the pointer value from objP is stored here
 *            on success.
 * expected_tag - Type tag for the pointer. May be *NULL* if type is not
 *                to be checked.
 *
 * Returns:
 * TCL_OK    - The pointer was successfully verified.
 * TCL_ERROR - The pointer was not registered or was registered with a
 *             different type. An error message is left in interp.
 */
int Tclh_PointerObjVerify(Tcl_Interp *interp, Tcl_Obj *objP,
                          void **pointerP, Tclh_PointerTypeTag expected_tag);

/* Function: Tclh_PointerObjVerifyAnyOf
 * Verifies a Tcl_Obj contains a wrapped pointer that is registered
 * and one of several allowed types.
 *
 * Parameters:
 * interp   - Tcl interpreter in which the pointer is to be verified.
 * objP     - Tcl_Obj containing a pointer value to be verified.
 * pointerP - If not NULL, the pointer value from objP is stored here
 *            on success.
 * ...      - Remaining arguments are type tags terminated by a NULL argument.
 *            The pointer must be one of these types.
 *
 * Returns:
 * TCL_OK    - The pointer was successfully verified.
 * TCL_ERROR - The pointer was not registered or was registered with a
 *             type that is not one of the passed ones.
 *             An error message is left in interp.
 */
int Tclh_PointerObjVerifyAnyOf(Tcl_Interp *interp, Tcl_Obj *objP,
                          void **pointerP, ... /* tag, tag, NULL */);

/* Function: Tclh_PointerWrap
 * Wraps a pointer into a Tcl_Obj.
 * The passed pointer is not registered as a valid pointer, nor is
 * any check made that it was previously registered.
 *
 * Parameters:
 * pointer  - Pointer value to be wrapped.
 * tag      - Type tag for the pointer. May be 0 or NULL if pointer
 *            is typeless.
 *
 * Returns:
 * Pointer to a Tcl_Obj with reference count 0.
 */
Tcl_Obj *Tclh_PointerWrap(void *pointer, Tclh_PointerTypeTag tag);

/* Function: Tclh_PointerUnwrap
 * Unwraps a Tcl_Obj representing a pointer checking it is of the
 * expected type. No checks are made with respect to its registration.
 *
 * Parameters:
 * interp - Interpreter in which to store error messages. May be NULL.
 * objP   - Tcl_Obj holding the wrapped pointer value.
 * pointerP - if not NULL, location to store unwrapped pointer.
 * expected_tag - Type tag for the pointer. May be *NULL* if type is not
 *                to be checked.
 *
 * The function does not check tags if the pointer is NULL and also does
 * not have a tag.
 *
 * Returns:
 * TCL_OK    - Success, with the unwrapped pointer stored in *pointerP.
 * TCL_ERROR - Failure, with interp containing error message.
 */
int Tclh_PointerUnwrap(Tcl_Interp *interp, Tcl_Obj *objP,
                        void **pointerP, Tclh_PointerTypeTag expected_tag);

/* Function: Tclh_PointerUnwrapAnyOf
 * Unwraps a Tcl_Obj representing a pointer checking it is of several
 * possible types. No checks are made with respect to its registration.
 *
 * Parameters:
 * interp   - Interpreter in which to store error messages. May be NULL.
 * objP     - Tcl_Obj holding the wrapped pointer value.
 * pointerP - if not NULL, location to store unwrapped pointer.
 * ...      - List of type tags to match against the pointer. Terminated
 *            by a NULL argument.
 *
 * Returns:
 * Pointer to a Tcl_Obj with reference count 0.
 */
int Tclh_PointerUnwrapAnyOf(Tcl_Interp *interp, Tcl_Obj *objP,
                           void **pointerP, ... /* tag, ..., NULL */);

/* Function: Tclh_PointerEnumerate
 * Returns a list of registered pointers.
 *
 * Parameters:
 * interp - interpreter
 * tag - Type tag to match
 *
 * Returns:
 *
 */
Tcl_Obj *Tclh_PointerEnumerate(Tcl_Interp *interp, Tclh_PointerTypeTag tag);

#ifdef TCLH_SHORTNAMES

#define PointerRegister           Tclh_PointerRegister
#define PointerUnregister         Tclh_PointerUnregister
#define PointerVerify             Tclh_PointerVerify
#define PointerObjUnregister      Tclh_PointerObjUnregister
#define PointerObjUnregisterAnyOf Tclh_PointerObjUnregisterAnyOf
#define PointerObjVerify          Tclh_PointerObjVerify
#define PointerObjVerifyAnyOf     Tclh_PointerObjVerifyAnyOf
#define PointerWrap               Tclh_PointerWrap
#define PointerUnwrap             Tclh_PointerUnwrap
#define PointerObjGetTag          Tclh_PointerGetTag
#define PointerUnwrapAnyOf        Tclh_PointerUnwrapAnyOf
#define PointerEnumerate          Tclh_PointerEnumerate

#endif

/*
 * IMPLEMENTATION
 */

#ifdef TCLH_IMPL
# define TCLH_POINTER_IMPL
#endif
#ifdef TCLH_POINTER_IMPL


/*
 * TclhPointerRecord keeps track of pointers and the count of references
 * to them. Pointers that are single reference have a nRefs of -1.
 */
typedef struct TclhPointerRecord {
    Tcl_Obj *tagObj;            /* Identifies the "type". May be NULL */
    int nRefs;                  /* Number of references to the pointer */
} TclhPointerRecord;

/*
 * Pointer is a Tcl "type" whose internal representation is stored
 * as the pointer value and an associated C pointer/handle type.
 * The Tcl_Obj.internalRep.twoPtrValue.ptr1 holds the C pointer value
 * and Tcl_Obj.internalRep.twoPtrValue.ptr2 holds a Tcl_Obj describing
 * the type. This may be NULL if no type info needs to be associated
 * with the value.
 */
static void DupPointerType(Tcl_Obj *srcP, Tcl_Obj *dstP);
static void FreePointerType(Tcl_Obj *objP);
static void UpdatePointerTypeString(Tcl_Obj *objP);
static int  SetPointerFromAny(Tcl_Interp *interp, Tcl_Obj *objP);

static struct Tcl_ObjType gPointerType = {
    "Pointer",
    FreePointerType,
    DupPointerType,
    UpdatePointerTypeString,
    NULL,
};
TCLH_INLINE void *PointerValueGet(Tcl_Obj *objP) {
    return objP->internalRep.twoPtrValue.ptr1;
}
TCLH_INLINE void PointerValueSet(Tcl_Obj *objP, void *valueP) {
    objP->internalRep.twoPtrValue.ptr1 = valueP;
}
/* May return NULL */
TCLH_INLINE Tclh_PointerTypeTag PointerTypeGet(Tcl_Obj *objP) {
    return objP->internalRep.twoPtrValue.ptr2;
}
TCLH_INLINE void PointerTypeSet(Tcl_Obj *objP, Tclh_PointerTypeTag tag) {
    objP->internalRep.twoPtrValue.ptr2 = (void*)tag;
}
TCLH_INLINE int
PointerTypeSame(Tclh_PointerTypeTag pointer_tag,
                Tclh_PointerTypeTag expected_tag) {
    return pointer_tag == expected_tag
               ? 1
               : Tclh_PointerTagMatch(pointer_tag, expected_tag);
}

int
Tclh_PointerLibInit(Tcl_Interp *interp)
{
    return Tclh_BaseLibInit(interp);
}

Tcl_Obj *
Tclh_PointerWrap(void *pointerValue, Tclh_PointerTypeTag tag)
{
    Tcl_Obj *objP;

    objP = Tcl_NewObj();
    Tcl_InvalidateStringRep(objP);
    PointerValueSet(objP, pointerValue);
    if (tag)
        Tcl_IncrRefCount(tag);
    PointerTypeSet(objP, tag);
    objP->typePtr = &gPointerType;
    return objP;
}

int
Tclh_PointerUnwrap(Tcl_Interp *interp,
                   Tcl_Obj *objP,
                   void **pvP,
                   Tclh_PointerTypeTag expected_tag)
{
    Tclh_PointerTypeTag tag;
    void *pv;

    /* Try converting Tcl_Obj internal rep */
    if (objP->typePtr != &gPointerType) {
        if (SetPointerFromAny(interp, objP) != TCL_OK)
            return TCL_ERROR;
    }
    tag = PointerTypeGet(objP);
    pv  = PointerValueGet(objP);

    /*
    * No tag check if
    * - expected_tag is NULL or
    * - pointer is NULL AND has no tag
    * expected_tag NULL means no type check
    * NULL pointers
    */
    if (expected_tag && (pv || tag) && !PointerTypeSame(tag, expected_tag)) {
        return Tclh_ErrorWrongType(interp, objP, "Pointer type mismatch.");
    }

    *pvP = PointerValueGet(objP);
    return TCL_OK;
}

int
Tclh_PointerObjGetTag(Tcl_Interp *interp,
                   Tcl_Obj *objP,
                   Tclh_PointerTypeTag *tagPtr)
{
    /* Try converting Tcl_Obj internal rep */
    if (objP->typePtr != &gPointerType) {
        if (SetPointerFromAny(interp, objP) != TCL_OK)
            return TCL_ERROR;
    }
    *tagPtr = PointerTypeGet(objP);
    return TCL_OK;
}

static int
TclhUnwrapAnyOfVA(Tcl_Interp *interp,
                  Tcl_Obj *objP,
                  void **pvP,
                  Tclh_PointerTypeTag *tagP,
                  va_list args)
{
    Tclh_PointerTypeTag tag;

    while ((tag = va_arg(args, Tclh_PointerTypeTag)) != NULL) {
        if (Tclh_PointerUnwrap(NULL, objP, pvP, tag) == TCL_OK) {
            if (tagP)
                *tagP = tag;
            return TCL_OK;
        }
    }

    return Tclh_ErrorWrongType(interp, objP, "Pointer type mismatch.");
}

int
Tclh_PointerUnwrapAnyOf(Tcl_Interp *interp, Tcl_Obj *objP, void **pvP, ...)
{
    int     tclResult;
    va_list args;

    va_start(args, pvP);
    tclResult = TclhUnwrapAnyOfVA(interp, objP, pvP, NULL, args);
    va_end(args);
    return tclResult;
}

static void
UpdatePointerTypeString(Tcl_Obj *objP)
{
    Tclh_PointerTypeTag tagObj;
    int len;
    int taglen;
    char *tagStr;
    char *bytes;

    TCLH_ASSERT(objP->bytes == NULL);
    TCLH_ASSERT(objP->typePtr == &gPointerType);

    tagObj = PointerTypeGet(objP);
    if (tagObj) {
        tagStr = Tcl_GetStringFromObj(tagObj, &taglen);
    }
    else {
        tagStr = "";
        taglen = 0;
    }
    /* Assume 40 bytes enough for address */
    bytes = ckalloc(40 + 1 + taglen + 1);
    (void) TclhPrintAddress(PointerValueGet(objP), bytes, 40);
    len = Tclh_strlen(bytes);
    bytes[len] = '^';
    memcpy(bytes + len + 1, tagStr, taglen+1);
    objP->bytes = bytes;
    objP->length = len + 1 + taglen;
}

static void
FreePointerType(Tcl_Obj *objP)
{
    Tclh_PointerTypeTag tag = PointerTypeGet(objP);
    if (tag)
        Tcl_DecrRefCount(tag);
    PointerTypeSet(objP, NULL);
    PointerValueSet(objP, NULL);
    objP->typePtr = NULL;
}

static void
DupPointerType(Tcl_Obj *srcP, Tcl_Obj *dstP)
{
    Tclh_PointerTypeTag tag;
    dstP->typePtr = &gPointerType;
    PointerValueSet(dstP, PointerValueGet(srcP));
    tag = PointerTypeGet(srcP);
    if (tag)
        Tcl_IncrRefCount(tag);
    PointerTypeSet(dstP, tag);
}

static int
SetPointerFromAny(Tcl_Interp *interp, Tcl_Obj *objP)
{
    void *pv;
    Tclh_PointerTypeTag tagObj;
    char *srep;
    char *s;

    if (objP->typePtr == &gPointerType)
        return TCL_OK;

    /* Pointers are address^tag, 0 or NULL*/
    srep = Tcl_GetString(objP);
    if (sscanf(srep, "%p^", &pv) == 1) {
        s = strchr(srep, '^');
        if (s == NULL)
            goto invalid_value;
        if (s[1] == '\0')
            tagObj = NULL;
        else {
            tagObj = Tcl_NewStringObj(s + 1, -1);
            Tcl_IncrRefCount(tagObj);
        }
    }
    else {
        if (strcmp(srep, "NULL"))
            goto invalid_value;
        pv = NULL;
        tagObj = NULL;
    }

    /* OK, valid opaque rep. Convert the passed object's internal rep */
    if (objP->typePtr && objP->typePtr->freeIntRepProc) {
        objP->typePtr->freeIntRepProc(objP);
    }
    objP->typePtr = &gPointerType;
    PointerValueSet(objP, pv);
    PointerTypeSet(objP, tagObj);

    return TCL_OK;

invalid_value: /* s must point to value */
    return Tclh_ErrorInvalidValue(interp, objP, "Invalid pointer format.");
}

/*
 * Pointer registry implementation.
 */

static int
PointerTypeError(Tcl_Interp *interp,
                 Tclh_PointerTypeTag registered,
                 Tclh_PointerTypeTag tag)
{
    return Tclh_ErrorWrongType(
        interp, NULL, "Pointer tag does not match registered tag.");
}

static int
PointerNotRegisteredError(Tcl_Interp *interp,
                          const void *p,
                          Tclh_PointerTypeTag tag)
{
    char addr[40];
    char buf[100];
    TclhPrintAddress(p, addr, sizeof(addr));
    snprintf(buf,
             sizeof(buf),
             "Pointer %s^%s is not registered.",
             addr,
             tag ? Tcl_GetString(tag) : "");

    return Tclh_ErrorGeneric(interp, "NOT_FOUND", buf);
}

static void
TclhPointerRecordFree(TclhPointerRecord *ptrRecP)
{
    if (ptrRecP->tagObj)
        Tcl_DecrRefCount(ptrRecP->tagObj);
    ckfree(ptrRecP);
}

static void
TclhCleanupPointerRegistry(ClientData clientData, Tcl_Interp *interp)
{
    Tcl_HashTable *hTblPtr = (Tcl_HashTable *)clientData;
    Tcl_HashEntry *he;
    Tcl_HashSearch hSearch;
    for (he = Tcl_FirstHashEntry(hTblPtr, &hSearch);
         he != NULL; he = Tcl_NextHashEntry(&hSearch)) {
        TclhPointerRecord *ptrRecP = Tcl_GetHashValue(he);
        TclhPointerRecordFree(ptrRecP);
    }

    Tcl_DeleteHashTable(hTblPtr);
    ckfree(hTblPtr);
}

static Tcl_HashTable *
TclhInitPointerRegistry(Tcl_Interp *interp)
{
    Tcl_HashTable *hTblPtr;
    static const char *const pointerTableKey = TCLH_EMBEDDER "PointerTable";
    hTblPtr = Tcl_GetAssocData(interp, pointerTableKey, NULL);
    if (hTblPtr == NULL) {
	hTblPtr = ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(hTblPtr, TCL_ONE_WORD_KEYS);
	Tcl_SetAssocData(interp, pointerTableKey, TclhCleanupPointerRegistry, hTblPtr);
    }
    return hTblPtr;
}

int
TclhPointerRegister(Tcl_Interp *interp,
                    void *pointer,
                    Tclh_PointerTypeTag tag,
                    Tcl_Obj **objPP,
                    int counted)
{
    Tcl_HashTable *hTblPtr;
    Tcl_HashEntry *he;
    int            newEntry;
    TclhPointerRecord *ptrRecP;

    if (pointer == NULL)
        return Tclh_ErrorInvalidValue(interp, NULL, "Attempt to register null pointer.");

    hTblPtr = TclhInitPointerRegistry(interp);
    he = Tcl_CreateHashEntry(hTblPtr, pointer, &newEntry);

    if (he) {
        if (newEntry) {
            ptrRecP = ckalloc(sizeof(*ptrRecP));
            if (tag) {
                Tcl_IncrRefCount(tag);
                ptrRecP->tagObj = tag;
            }
            else
                ptrRecP->tagObj = NULL;
            /* -1 => uncounted pointer (only single reference allowed) */
            ptrRecP->nRefs = counted ? 1 : -1;
            Tcl_SetHashValue(he, ptrRecP);
        } else {
            ptrRecP = Tcl_GetHashValue(he);
            if (!counted || ptrRecP->nRefs < 0) {
                /* Registered or passed - at least one is not a counted pointer */
                return Tclh_ErrorExists(interp, "Registered pointer", NULL, NULL);
            }
            if (!PointerTypeSame(ptrRecP->tagObj, tag))
                return PointerTypeError(interp, ptrRecP->tagObj, tag);
            ptrRecP->nRefs += 1;
        }
        if (objPP)
            *objPP = Tclh_PointerWrap(pointer, tag);
    } else {
        TCLH_PANIC("Failed to allocate hash table entry.");
        return TCL_ERROR; /* NOT REACHED but keep compiler happy */
    }
    return TCL_OK;
}

int
Tclh_PointerRegister(Tcl_Interp *interp,
                     void *pointer,
                     Tclh_PointerTypeTag tag,
                     Tcl_Obj **objPP)
{
    return TclhPointerRegister(interp, pointer, tag, objPP, 0);
}

int
Tclh_PointerRegisterCounted(Tcl_Interp *interp,
                            void *pointer,
                            Tclh_PointerTypeTag tag,
                            Tcl_Obj **objPP)
{
    return TclhPointerRegister(interp, pointer, tag, objPP, 1);
}

static int
PointerVerifyOrUnregister(Tcl_Interp *interp,
                          const void *pointer,
                          Tclh_PointerTypeTag tag,
                          int unregister)
{
    Tcl_HashTable *hTblPtr;
    Tcl_HashEntry *he;

    hTblPtr = TclhInitPointerRegistry(interp);
    he = Tcl_FindHashEntry(hTblPtr, pointer);
    if (he) {
        TclhPointerRecord *ptrRecP = Tcl_GetHashValue(he);
        if (!PointerTypeSame(ptrRecP->tagObj, tag))
            return PointerTypeError(interp, ptrRecP->tagObj, tag);
        if (unregister) {
            if (ptrRecP->nRefs <= 1) {
                /*  Either uncounted or ref count will reach 0 */
                TclhPointerRecordFree(ptrRecP);
                Tcl_DeleteHashEntry(he);
            }
            else
                ptrRecP->nRefs -= 1;
        }
        return TCL_OK;
    }
    return PointerNotRegisteredError(interp, pointer, tag);
}

int
Tclh_PointerUnregister(Tcl_Interp *interp,
                       const void *pointer,
                       Tclh_PointerTypeTag tag)
{
    return PointerVerifyOrUnregister(interp, pointer, tag, 1);
}

Tcl_Obj *
Tclh_PointerEnumerate(Tcl_Interp *interp,
                      Tclh_PointerTypeTag tag)
{
    Tcl_HashEntry *he;
    Tcl_HashSearch hSearch;
    Tcl_HashTable *hTblPtr;
    Tcl_Obj *resultObj = Tcl_NewListObj(0, NULL);

    hTblPtr = TclhInitPointerRegistry(interp);
    for (he = Tcl_FirstHashEntry(hTblPtr, &hSearch);
         he != NULL; he = Tcl_NextHashEntry(&hSearch)) {
        void *pv                   = Tcl_GetHashKey(hTblPtr, he);
        TclhPointerRecord *ptrRecP = Tcl_GetHashValue(he);
        if (PointerTypeSame(ptrRecP->tagObj, tag)) {
            Tcl_ListObjAppendElement(NULL, resultObj, Tclh_PointerWrap(pv, ptrRecP->tagObj));
        }
    }
    return resultObj;
}

int
Tclh_PointerVerify(Tcl_Interp *interp,
                   const void *pointer,
                   Tclh_PointerTypeTag tag)
{
    return PointerVerifyOrUnregister(interp, pointer, tag, 0);
}

int
Tclh_PointerObjUnregister(Tcl_Interp *interp,
                          Tcl_Obj *objP,
                          void **pointerP,
                          Tclh_PointerTypeTag tag)
{
    void *pv = NULL;            /* Init to keep gcc happy */
    int   tclResult;

    tclResult = Tclh_PointerUnwrap(interp, objP, &pv, tag);
    if (tclResult == TCL_OK) {
        if (pv != NULL)
            tclResult = Tclh_PointerUnregister(interp, pv, tag);
        if (tclResult == TCL_OK && pointerP != NULL)
            *pointerP = pv;
    }
    return tclResult;
}

static int
PointerObjVerifyOrUnregisterAnyOf(Tcl_Interp *interp,
                                  Tcl_Obj *objP,
                                  void **pointerP,
                                  int unregister,
                                  va_list args)
{
    int tclResult;
    void *pv = NULL;            /* Init to keep gcc happy */
    Tclh_PointerTypeTag tag = NULL;

    tclResult = TclhUnwrapAnyOfVA(interp, objP, &pv, &tag, args);
    if (tclResult == TCL_OK) {
        if (unregister)
            tclResult = Tclh_PointerUnregister(interp, pv, tag);
        else
            tclResult = Tclh_PointerVerify(interp, pv, tag);
        if (tclResult == TCL_OK && pointerP != NULL)
            *pointerP = pv;
    }
    return tclResult;
}

int
Tclh_PointerObjUnregisterAnyOf(Tcl_Interp *interp,
                               Tcl_Obj *objP,
                               void **pointerP,
                               ... /* tag, ... , NULL */)
{
    int tclResult;
    va_list args;

    va_start(args, pointerP);
    tclResult = PointerObjVerifyOrUnregisterAnyOf(interp, objP, pointerP, 1, args);
    va_end(args);
    return tclResult;
}

int
Tclh_PointerObjVerify(Tcl_Interp *interp,
                      Tcl_Obj *objP,
                      void **pointerP,
                      Tclh_PointerTypeTag tag)
{
    void *pv = NULL;            /* Init to keep gcc happy */
    int   tclResult;

    tclResult = Tclh_PointerUnwrap(interp, objP, &pv, tag);
    if (tclResult == TCL_OK) {
        tclResult = Tclh_PointerVerify(interp, pv, tag);
        if (tclResult == TCL_OK) {
            if (pointerP)
                *pointerP = pv;
        }
    }
    return tclResult;
}

int
Tclh_PointerObjVerifyAnyOf(Tcl_Interp *interp,
                           Tcl_Obj *objP,
                           void **pointerP,
                           ... /* tag, tag, NULL */)
{
    int tclResult;
    va_list args;

    va_start(args, pointerP);
    tclResult = PointerObjVerifyOrUnregisterAnyOf(interp, objP, pointerP, 0, args);
    va_end(args);
    return tclResult;
}

#endif /* TCLH_IMPL */

#endif /* TCLHPOINTER_H */
