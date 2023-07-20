#ifndef TCLHOBJ_H
#define TCLHOBJ_H

#include "tclhBase.h"
#include <limits.h>
#include <errno.h>
#include <ctype.h>

/* Section: Tcl_Obj convenience functions
 *
 * Provides some simple convenience functions to deal with Tcl_Obj values. These
 * may be for converting Tcl_Obj between additional native types as well as
 * for existing types like 32-bit integers where Tcl already provides functions.
 * In the latter case, the purpose is simply to reduce code bloat arising from
 * indirection through the stubs table.
 */

/* Function: Tclh_ObjLibInit
 * Must be called to initialize the Obj module before any of
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
int Tclh_ObjLibInit(Tcl_Interp *interp);

/* Function: Tclh_ObjClearPtr
 * Releases a pointer to a *Tcl_Obj* and clears it.
 *
 * Parameters:
 * objPP - pointer to the pointer to a *Tcl_Obj*
 *
 * Decrements the reference count on the Tcl_Obj possibly freeing
 * it. The pointer itself is set to NULL.
 *
 * The pointer *objPP (but not objPP) may be NULL.
 */
TCLH_INLINE void Tclh_ObjClearPtr(Tcl_Obj **objPP) {
    if (*objPP) {
        Tcl_DecrRefCount(*objPP);
        *objPP = NULL;
    }
}

/* Function: Tclh_ObjToRangedInt
 * Unwraps a Tcl_Obj into a Tcl_WideInt if it is within a specified range.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * low - low end of the range
 * high - high end of the range
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains a number in the specified range. Otherwise returns
 * TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToRangedInt(Tcl_Interp *interp,
                        Tcl_Obj *obj,
                        Tcl_WideInt low,
                        Tcl_WideInt high,
                        Tcl_WideInt *ptr);

/* Function: Tclh_ObjToChar
 * Unwraps a Tcl_Obj into a C *char* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains an integer that fits in a C *char* type. Otherwise
 * returns TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToChar(Tcl_Interp *interp, Tcl_Obj *obj, signed char *ptr);

/* Function: Tclh_ObjToUChar
 * Unwraps a Tcl_Obj into a C *unsigned char* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains an integer that fits in a C *unsigned char* type.
 * Otherwise, returns TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToUChar(Tcl_Interp *interp, Tcl_Obj *obj, unsigned char *ptr);

/* Function: Tclh_ObjToShort
 * Unwraps a Tcl_Obj into a C *short* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains an integer that fits in a C *short* type. Otherwise
 * returns TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToShort(Tcl_Interp *interp, Tcl_Obj *obj, short *ptr);

/* Function: Tclh_ObjToUShort
 * Unwraps a Tcl_Obj into a C *unsigned short* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr*
 * if the passed Tcl_Obj contains an integer that fits in a C *unsigned short*
 * type. Otherwise returns TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToUShort(Tcl_Interp *interp, Tcl_Obj *obj, unsigned short *ptr);

/* Function: Tclh_ObjToInt
 * Unwraps a Tcl_Obj into a C *int* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains an integer that fits in a C *int* type. Otherwise
 * returns TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToInt(Tcl_Interp *interp, Tcl_Obj *obj, int *ptr);

/* Function: Tclh_ObjToUInt
 * Unwraps a Tcl_Obj into a C *unsigned int* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains an integer that fits in a C *unsigned int* type. Otherwise
 * returns TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToUInt(Tcl_Interp *interp, Tcl_Obj *obj, unsigned int *ptr);

/* Function: Tclh_ObjToLong
 * Unwraps a Tcl_Obj into a C *long* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains an integer that fits in a C *long* type. Otherwise
 * returns TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToLong(Tcl_Interp *interp, Tcl_Obj *obj, long *ptr);

/* Function: Tclh_ObjToULong
 * Unwraps a Tcl_Obj into a C *unsigned long* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains an integer that fits in a C *long* type. Otherwise
 * returns TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToULong(Tcl_Interp *interp, Tcl_Obj *obj, unsigned long *ptr);

/* Function: Tclh_ObjFromULong
 * Returns a Tcl_Obj wrapping a *unsigned long*
 *
 * Parameters:
 *  ull - unsigned long value to be wrapped
 *
 * Returns:
 * A pointer to a Tcl_Obj with a zero reference count.
 */
Tcl_Obj *Tclh_ObjFromULong(unsigned long ull);

/* Function: Tclh_ObjToWideInt
 * Unwraps a Tcl_Obj into a C *Tcl_WideInt* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains an integer that fits in a C *Tcl_WideInt* type. Otherwise
 * returns TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToWideInt(Tcl_Interp *interp, Tcl_Obj *obj, Tcl_WideInt *ptr);

/* Function: Tclh_ObjToLongLong
 * Unwraps a Tcl_Obj into a C *long long* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains an integer that fits in a C *long* type. Otherwise
 * returns TCL_ERROR with an error message in the interpreter.
 */
TCLH_INLINE int
Tclh_ObjToLongLong(Tcl_Interp *interp, Tcl_Obj *objP, signed long long *llP)
{
    /* TBD - does not check for positive overflow etc. */
    TCLH_ASSERT(sizeof(Tcl_WideInt) == sizeof(signed long long));
    return Tclh_ObjToWideInt(interp, objP, (Tcl_WideInt *)llP);
}


/* Function: Tclh_ObjToULongLong
 * Unwraps a Tcl_Obj into a C *unsigned long long* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains an integer that fits in a C *long* type. Otherwise
 * returns TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToULongLong(Tcl_Interp *interp, Tcl_Obj *obj, unsigned long long *ptr);

/* Function: Tclh_ObjFromULongLong
 * Returns a Tcl_Obj wrapping a *unsigned long long*
 *
 * Parameters:
 *  ull - unsigned long long value to be wrapped
 *
 * Returns:
 * A pointer to a Tcl_Obj with a zero reference count.
 */
Tcl_Obj *Tclh_ObjFromULongLong(unsigned long long ull);


/* Function: Tclh_ObjToFloat
 * Unwraps a Tcl_Obj into a C *float* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains an integer that fits in a C *float* type. Otherwise
 * returns TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToFloat(Tcl_Interp *interp, Tcl_Obj *obj, float *ptr);

/* Function: Tclh_ObjToDouble
 * Unwraps a Tcl_Obj into a C *double* value type.
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the number
 * ptr - location to store extracted number
 *
 * Returns:
 * Returns TCL_OK and stores the value in location pointed to by *ptr* if the
 * passed Tcl_Obj contains an integer that fits in a C *double* type. Otherwise
 * returns TCL_ERROR with an error message in the interpreter.
 */
int Tclh_ObjToDouble(Tcl_Interp *interp, Tcl_Obj *obj, double *ptr);

/* Function: Tclh_ObjGetBytesByRef
 * Retrieves a reference to the byte array in a Tcl_Obj.
 *
 * Parameters:
 * interp - Interpreter for error messages. May be NULL.
 * obj - Tcl_Obj containing the bytes
 * lenPtr - location to store number of bytes in the array. May be NULL.
 *
 * Returns:
 * On success, returns a pointer to internal byte array and stores the
 * length of the array in lenPtr if not NULL. On error, returns
 * a NULL pointer with an error message in the interpreter.
 */
TCLH_INLINE char *
Tclh_ObjGetBytesByRef(Tcl_Interp *interp, Tcl_Obj *obj, Tclh_SSizeT *lenPtr)
{
#if TCLH_TCLAPI_VERSION < 87
    return (char *) Tcl_GetByteArrayFromObj(obj, lenPtr);
#else
    return (char *)Tcl_GetBytesFromObj(interp, obj, lenPtr);
#endif
}

/* Function: Tclh_ObjFromAddress
 * Wraps a memory address into a Tcl_Obj.
 *
 * Parameters:
 * address - a memory address
 *
 * The *Tcl_Obj* is created as a formatted string with address in
 * hexadecimal format. The reference count on the returned *Tcl_Obj* is
 * *0* just like the standard *Tcl_Obj* creation functions.
 *
 * Returns:
 * Pointer to the created Tcl_Obj.
 */
Tcl_Obj *Tclh_ObjFromAddress (void *address);

/* Function: Tclh_ObjToAddress
 * Unwraps a Tcl_Obj into a memory address
 *
 * Parameters:
 * interp - Interpreter
 * obj - Tcl_Obj from which to extract the adddress
 * ptr - location to store extracted address
 *
 * Returns:
 * Returns TCL_OK on success and stores the value in location
 * pointed to by *ptr*. Otherwise returns TCL_ERROR with an error message in
 * the interpreter.
 */
int Tclh_ObjToAddress(Tcl_Interp *interp, Tcl_Obj *obj, void **ptr);

/* Function: Tclh_ObjArrayIncrRefs
 * Increments reference counts of all elements of a *Tcl_Obj** array
 *
 * Parameters:
 * objc - Number of elements in objv
 * objv - array of Tcl_Obj pointers
 */
TCLH_INLINE void Tclh_ObjArrayIncrRefs(int objc, Tcl_Obj * const *objv) {
    int i;
    for (i = 0; i < objc; ++i)
        Tcl_IncrRefCount(objv[i]);
}

/* Function: Tclh_ObjArrayDecrRefs
 * Decrements reference counts of all elements of a *Tcl_Obj** array
 *
 * Parameters:
 * objc - Number of elements in objv
 * objv - array of Tcl_Obj pointers
 */
TCLH_INLINE void Tclh_ObjArrayDecrRefs(int objc, Tcl_Obj * const *objv) {
    int i;
    for (i = 0; i < objc; ++i)
        Tcl_DecrRefCount(objv[i]);
}

#ifdef TCLH_SHORTNAMES
#define ObjClearPrt Tclh_ObjClearPtr
#define ObjToRangedInt Tclh_ObjToRangedInt
#define ObjToChar Tclh_ObjToChar
#define ObjToUChar Tclh_ObjToUChar
#define ObjToShort Tclh_ObjToShort
#define ObjToUShort Tclh_ObjToUShort
#define ObjToInt Tclh_ObjToInt
#define ObjToUInt Tclh_ObjToUInt
#define ObjToLong Tclh_ObjToLong
#define ObjToULong Tclh_ObjToULong
#define ObjToWideInt Tclh_ObjToWideInt
#define ObjToUWideInt Tclh_ObjToUWideInt
#define ObjToLongLong Tclh_ObjToLongLong
#define ObjToULongLong Tclh_ObjToULongLong
#define ObjToFloat Tclh_ObjToFloat
#define ObjToDouble Tclh_ObjToDouble
#define ObjArrayIncrRef Tclh_ObjArrayIncrRef
#define ObjArrayDecrRef Tclh_ObjArrayDecrRef
#define ObjFromAddress Tclh_ObjFromAddress
#define ObjToAddress Tclh_ObjToAddress
#define ObjGetBytesByRef Tclh_ObjGetBytesByRef
#endif

/*
 * IMPLEMENTATION
 */

#ifdef TCLH_IMPL
# define TCLH_TCL_OBJ_IMPL
#endif
#ifdef TCLH_TCL_OBJ_IMPL

static const Tcl_ObjType *gTclIntType;
static const Tcl_ObjType *gTclWideIntType;
static const Tcl_ObjType *gTclBooleanType;
static const Tcl_ObjType *gTclDoubleType;
static const Tcl_ObjType *gTclBignumType;

int Tclh_ObjLibInit(Tcl_Interp *interp)
{
    Tcl_Obj *objP;

    gTclIntType = Tcl_GetObjType("int");
    if (gTclIntType == NULL) {
        objP        = Tcl_NewIntObj(0);
        gTclIntType = objP->typePtr;
        Tcl_DecrRefCount(objP);
    }

    gTclWideIntType = Tcl_GetObjType("wideInt");
    if (gTclWideIntType == NULL) {
        objP            = Tcl_NewWideIntObj(0);
        gTclWideIntType = objP->typePtr;
        Tcl_DecrRefCount(objP);
    }
    gTclBooleanType = Tcl_GetObjType("boolean");
    if (gTclBooleanType == NULL) {
        objP = Tcl_NewBooleanObj(1);
#if TCLH_TCLAPI_VERSION >= 87
        char b;
        if (Tcl_GetBoolFromObj(NULL, objP, 0, &b) == TCL_OK) {
            gTclBooleanType = objP->typePtr;
        }
#endif
        Tcl_DecrRefCount(objP);
    }
    gTclDoubleType = Tcl_GetObjType("double");
    if (gTclDoubleType == NULL) {
        objP           = Tcl_NewDoubleObj(0.1);
        gTclDoubleType = objP->typePtr;
        Tcl_DecrRefCount(objP);
    }
    gTclBignumType = Tcl_GetObjType("bignum"); /* Likely NULL as it is not registered */
    if (gTclBignumType == NULL) {
        mp_int temp;
        objP           = Tcl_NewStringObj("0xffffffffffffffff", -1);
        int ret = Tcl_GetBignumFromObj(interp, objP, &temp);
        if (ret == TCL_OK) {
            gTclBignumType = objP->typePtr;
            mp_clear(&temp);
        }
        Tcl_DecrRefCount(objP);
    }

    return TCL_OK;
}

int
Tclh_ObjToRangedInt(Tcl_Interp * interp,
                    Tcl_Obj *    obj,
                    Tcl_WideInt  low,
                    Tcl_WideInt  high,
                    Tcl_WideInt *wideP)
{
    Tcl_WideInt wide;

    if (Tclh_ObjToWideInt(interp, obj, &wide) != TCL_OK)
        return TCL_ERROR;

    if (wide < low || wide > high)
        return Tclh_ErrorRange(interp, obj, low, high);

    if (wideP)
        *wideP = wide;

    return TCL_OK;
}

int Tclh_ObjToChar(Tcl_Interp *interp, Tcl_Obj *obj, signed char *cP)
{
    Tcl_WideInt wide = 0; /* Init to keep gcc happy */

    if (Tclh_ObjToRangedInt(interp, obj, SCHAR_MIN, SCHAR_MAX, &wide) != TCL_OK)
        return TCL_ERROR;
    *cP = (signed char) wide;
    return TCL_OK;
}

int Tclh_ObjToUChar(Tcl_Interp *interp, Tcl_Obj *obj, unsigned char *ucP)
{
    Tcl_WideInt wide = 0; /* Init to keep gcc happy */

    if (Tclh_ObjToRangedInt(interp, obj, 0, UCHAR_MAX, &wide) != TCL_OK)
        return TCL_ERROR;
    *ucP = (unsigned char) wide;
    return TCL_OK;
}

int Tclh_ObjToShort(Tcl_Interp *interp, Tcl_Obj *obj, short *shortP)
{
    Tcl_WideInt wide = 0; /* Init to keep gcc happy */

    if (Tclh_ObjToRangedInt(interp, obj, SHRT_MIN, SHRT_MAX, &wide) != TCL_OK)
        return TCL_ERROR;
    *shortP = (short) wide;
    return TCL_OK;
}

int Tclh_ObjToUShort(Tcl_Interp *interp, Tcl_Obj *obj, unsigned short *ushortP)
{
    Tcl_WideInt wide = 0; /* Init to keep gcc happy */

    if (Tclh_ObjToRangedInt(interp, obj, 0, USHRT_MAX, &wide) != TCL_OK)
        return TCL_ERROR;
    *ushortP = (unsigned short) wide;
    return TCL_OK;
}

int Tclh_ObjToInt(Tcl_Interp *interp, Tcl_Obj *objP, int *valP)
{
#if 0
    /* BAD - returns 2147483648 as -2147483648 and
       -2147483649 as 2147483647 instead of error */
       /* TODO - check if Tcl9 still does this */
    return Tcl_GetIntFromObj(interp, objP, valP);
#else
    Tcl_WideInt wide = 0; /* Init to keep gcc happy */

    if (Tclh_ObjToRangedInt(interp, objP, INT_MIN, INT_MAX, &wide) != TCL_OK)
        return TCL_ERROR;
    *valP = (int) wide;
    return TCL_OK;
#endif
}

int Tclh_ObjToUInt(Tcl_Interp *interp, Tcl_Obj *obj, unsigned int *uiP)
{
    Tcl_WideInt wide = 0; /* Init to keep gcc happy */

    if (Tclh_ObjToRangedInt(interp, obj, 0, UINT_MAX, &wide) != TCL_OK)
        return TCL_ERROR;
    *uiP = (unsigned int) wide;
    return TCL_OK;
}

int Tclh_ObjToLong(Tcl_Interp *interp, Tcl_Obj *objP, long *valP)
{
    if (sizeof(long) < sizeof(Tcl_WideInt)) {
        Tcl_WideInt wide = 0; /* Init to keep gcc happy */
        if (Tclh_ObjToRangedInt(interp, objP, LONG_MIN, LONG_MAX, &wide) != TCL_OK)
            return TCL_ERROR;
        *valP = (long)wide;
        return TCL_OK;
    }
    else {
        TCLH_ASSERT(sizeof(long long) == sizeof(long));
        return Tclh_ObjToLongLong(interp, objP, (long long *) valP);
    }
}

int Tclh_ObjToULong(Tcl_Interp *interp, Tcl_Obj *objP, unsigned long *valP)
{

    if (sizeof(unsigned long) < sizeof(Tcl_WideInt)) {
        Tcl_WideInt wide = 0; /* Init to keep gcc happy */
        if (Tclh_ObjToRangedInt(interp, objP, 0, ULONG_MAX, &wide) != TCL_OK)
            return TCL_ERROR;
        *valP = (unsigned long)wide;
        return TCL_OK;
    }
    else {
        /* TBD - unsigned not handled correctly */
        TCLH_ASSERT(sizeof(unsigned long long) == sizeof(unsigned long));
        return Tclh_ObjToULongLong(interp, objP, (unsigned long long *) valP);
    }
}

Tcl_Obj *Tclh_ObjFromULong(unsigned long ul)
{
    if (sizeof(unsigned long) < sizeof(Tcl_WideInt))
        return Tcl_NewWideIntObj(ul);
    else
        return Tclh_ObjFromULongLong(ul);
}

int Tclh_ObjToBoolean(Tcl_Interp *interp, Tcl_Obj *objP, int *valP)
{
    return Tcl_GetBooleanFromObj(interp, objP, valP);
}

int Tclh_ObjToWideInt(Tcl_Interp *interp, Tcl_Obj *objP, Tcl_WideInt *wideP)
{
    /* TODO - can we just use Tcl_GetWideIntFromObj in Tcl9? Does it error if > WIDE_MAX? */
    int ret;
    Tcl_WideInt wide;
    ret = Tcl_GetWideIntFromObj(interp, objP, &wide);
    if (ret != TCL_OK)
        return ret;
    /*
     * Tcl_GetWideIntFromObj has several issues:
     * - it will happily accept negative numbers in strings that will
     *   then show up as positive when retrieved. For example,
     *   Tcl_GWIFO(Tcl_NewStringObj(-18446744073709551615, -1)) will
     *   return a value of 1.
     *   Similarly -9223372036854775809 -> large positive number
     * - It will also happily accept valid positive numbers in the range
     *   LLONG_MAX - ULLONG_MAX but return them as negative numbers which we
     *   cannot distinguish from genuine negative numbers
     * So we check the internal rep. If it is an integer type other than
     * bignum, fine. Otherwise, we check for possibility of overflow by
     * comparing sign of retrieved wide int with the sign stored in the
     * bignum representation.
     */

    if (objP->typePtr != gTclIntType && objP->typePtr != gTclWideIntType
        && objP->typePtr != gTclBooleanType
        && objP->typePtr != gTclDoubleType) {
        /* Was it an integer overflow */
        mp_int temp;
        ret = Tcl_GetBignumFromObj(interp, objP, &temp);
        if (ret == TCL_OK) {
            if ((wide >= 0 && temp.sign == MP_NEG)
                || (wide < 0 && temp.sign != MP_NEG)) {
                Tcl_SetResult(interp,
                              "Integer magnitude too large to represent.",
                              TCL_STATIC);
                ret = TCL_ERROR;
            }
            mp_clear(&temp);
        }
    }

    if (ret == TCL_OK)
        *wideP = wide;
    return ret;
}

int
Tclh_ObjToULongLong(Tcl_Interp *interp,
                    Tcl_Obj *objP,
                    unsigned long long *ullP)
{
    int ret;

#if TCLH_TCLAPI_VERSION >= 87
    /* TODO - currently disabled so as to get same error for 8.6 and 9.0 builds */
    Tcl_WideUInt uwide;
    ret = Tcl_GetWideUIntFromObj(interp, objP, &uwide);
    if (ret == TCL_OK)
        *ullP = uwide;
    return ret;
#else
    Tcl_WideInt wide;

    TCLH_ASSERT(sizeof(unsigned long long) == sizeof(Tcl_WideInt));

    /* Tcl_GetWideInt will happily return overflows as negative numbers */
    ret = Tcl_GetWideIntFromObj(interp, objP, &wide);
    if (ret != TCL_OK)
        return ret;

    /*
     * We have to check for two things.
     *   1. an overflow condition in Tcl_GWIFO where
     *     (a) a large positive number that fits in the width is returned
     *         as negative e.g. 18446744073709551615 is returned as -1
     *     (b) an negative overflow is returned as a positive number,
     *         e.g. -18446744073709551615 is returned as 1.
     *   2. Once we have retrieved a valid number, reject it if negative.
     *
     * So we check the internal rep. If it is an integer type other than
     * bignum, fine (no overflow). Otherwise, we check for possibility of
     * overflow by comparing sign of retrieved wide int with the sign stored
     * in the bignum representation.
     */

    if (objP->typePtr == gTclIntType || objP->typePtr == gTclWideIntType
        || objP->typePtr == gTclBooleanType
        || objP->typePtr == gTclDoubleType) {
        /* No need for an overflow check (1) but still need to check (2) */
        if (wide < 0)
            goto negative_error;
        *ullP = (unsigned long long)wide;
    }
    else {
        /* Was it an integer overflow */
        mp_int temp;
        ret = Tcl_GetBignumFromObj(interp, objP, &temp);
        if (ret == TCL_OK) {
            int sign = temp.sign;
            mp_clear(&temp);
            if (sign == MP_NEG)
                goto negative_error;
            /*
             * Note Tcl_Tcl_GWIFO already takes care of overflows that do not
             * fit in Tcl_WideInt width. So we need not worry about that.
             * The overflow case is where a positive value is returned as
             * negative by Tcl_GWIFO; that is also taken care of by the
             * assignment below.
             */
            *ullP = (unsigned long long)wide;
        }
    }

    return ret;

negative_error:
    return TclhRecordError(
        interp,
        "RANGE",
        Tcl_NewStringObj("Negative values are not in range for unsigned types.", -1));
#endif
}

Tcl_Obj *Tclh_ObjFromULongLong(unsigned long long ull)
{
    TCLH_ASSERT(sizeof(Tcl_WideInt) == sizeof(unsigned long long));
    if (ull <= LLONG_MAX)
        return Tcl_NewWideIntObj((Tcl_WideInt) ull);
    else {
        /* Cannot use WideInt because that will treat as negative  */
        char buf[40]; /* Think 21 enough, but not bothered to count */
#ifdef _WIN32
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%I64u", ull);
#else
        snprintf(buf, sizeof(buf), "%llu", ull);
#endif
        return Tcl_NewStringObj(buf, -1);
    }
}

int Tclh_ObjToDouble(Tcl_Interp *interp, Tcl_Obj *objP, double *dblP)
{
    return Tcl_GetDoubleFromObj(interp, objP, dblP);
}

int Tclh_ObjToFloat(Tcl_Interp *interp, Tcl_Obj *objP, float *fltP)
{
    double dval;
    if (Tcl_GetDoubleFromObj(interp, objP, &dval) != TCL_OK)
        return TCL_ERROR;
    *fltP = (float) dval;
    return TCL_OK;
}

Tcl_Obj *Tclh_ObjFromAddress (void *address)
{
    char buf[40];
    char *start;

    start = TclhPrintAddress(address, buf, sizeof(buf) / sizeof(buf[0]));
    return Tcl_NewStringObj(start, -1);
}

int Tclh_ObjToAddress(Tcl_Interp *interp, Tcl_Obj *objP, void **pvP)
{
    int ret;

    /* Yeah, assumes pointers are 4 or 8 bytes only */
    if (sizeof(unsigned int) == sizeof(*pvP)) {
        unsigned int ui;
        ret = Tclh_ObjToUInt(interp, objP, &ui);
        if (ret == TCL_OK)
            *pvP = (void *)(uintptr_t)ui;
    }
    else {
        Tcl_WideInt wide;
        ret = Tcl_GetWideIntFromObj(interp, objP, &wide);
        if (ret == TCL_OK)
            *pvP = (void *)(uintptr_t)wide;
    }
    return ret;
}

#endif /* TCLH_TCL_OBJ_IMPL */

#endif /* TCLHOBJ_H */
