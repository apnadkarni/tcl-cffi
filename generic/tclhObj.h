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
int Tclh_ObjToRangedInt(Tcl_Interp * interp,
                        Tcl_Obj *    obj,
                        Tcl_WideInt  low,
                        Tcl_WideInt  high,
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
int Tclh_ObjToLongLong(Tcl_Interp *interp, Tcl_Obj *obj, long long *ptr);

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

#ifdef TCLH_SHORTNAMES
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
#endif

/*
 * IMPLEMENTATION
 */

#ifdef TCLH_IMPL
# define TCLH_TCL_OBJ_IMPL
#endif
#ifdef TCLH_TCL_OBJ_IMPL

int
Tclh_ObjToRangedInt(Tcl_Interp * interp,
                    Tcl_Obj *    obj,
                    Tcl_WideInt  low,
                    Tcl_WideInt  high,
                    Tcl_WideInt *wideP)
{
    Tcl_WideInt wide;
    if (Tcl_GetWideIntFromObj(interp, obj, &wide) != TCL_OK)
        return TCL_ERROR;

    if (wide < low || wide > high)
        return Tclh_ErrorRange(interp, obj, low, high);

    if (wideP)
        *wideP = wide;

    return TCL_OK;
}

int Tclh_ObjToChar(Tcl_Interp *interp, Tcl_Obj *obj, signed char *cP)
{
    Tcl_WideInt wide;

    if (Tclh_ObjToRangedInt(interp, obj, CHAR_MIN, CHAR_MAX, &wide) != TCL_OK)
        return TCL_ERROR;
    *cP = (signed char) wide;
    return TCL_OK;
}

int Tclh_ObjToUChar(Tcl_Interp *interp, Tcl_Obj *obj, unsigned char *ucP)
{
    Tcl_WideInt wide;

    if (Tclh_ObjToRangedInt(interp, obj, 0, UCHAR_MAX, &wide) != TCL_OK)
        return TCL_ERROR;
    *ucP = (unsigned char) wide;
    return TCL_OK;
}

int Tclh_ObjToShort(Tcl_Interp *interp, Tcl_Obj *obj, short *shortP)
{
    Tcl_WideInt wide;

    if (Tclh_ObjToRangedInt(interp, obj, SHRT_MIN, SHRT_MAX, &wide) != TCL_OK)
        return TCL_ERROR;
    *shortP = (short) wide;
    return TCL_OK;
}

int Tclh_ObjToUShort(Tcl_Interp *interp, Tcl_Obj *obj, unsigned short *ushortP)
{
    Tcl_WideInt wide;

    if (Tclh_ObjToRangedInt(interp, obj, 0, USHRT_MAX, &wide) != TCL_OK)
        return TCL_ERROR;
    *ushortP = (unsigned short) wide;
    return TCL_OK;
}

int Tclh_ObjToInt(Tcl_Interp *interp, Tcl_Obj *objP, int *valP)
{
    return Tcl_GetIntFromObj(interp, objP, valP);
}

int Tclh_ObjToUInt(Tcl_Interp *interp, Tcl_Obj *obj, unsigned int *uiP)
{
    Tcl_WideInt wide;

    if (Tclh_ObjToRangedInt(interp, obj, 0, UINT_MAX, &wide) != TCL_OK)
        return TCL_ERROR;
    *uiP = (unsigned int) wide;
    return TCL_OK;
}

int Tclh_ObjToLong(Tcl_Interp *interp, Tcl_Obj *objP, long *valP)
{
    return Tcl_GetLongFromObj(interp, objP, valP);
}

int Tclh_ObjToULong(Tcl_Interp *interp, Tcl_Obj *objP, unsigned long *valP)
{

    if (sizeof(unsigned long) < sizeof(Tcl_WideInt)) {
        Tcl_WideInt wide;
        if (Tclh_ObjToRangedInt(interp, objP, 0, UINT_MAX, &wide) != TCL_OK)
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

int Tclh_ObjToBoolean(Tcl_Interp *interp, Tcl_Obj *objP, int *valP)
{
    return Tcl_GetBooleanFromObj(interp, objP, valP);
}

int Tclh_ObjToWideInt(Tcl_Interp *interp, Tcl_Obj *objP, Tcl_WideInt *wideP)
{
    /* TBD - does not check for positive overflow etc. */
    return Tcl_GetWideIntFromObj(interp, objP, wideP);
}

int Tclh_ObjToLongLong(Tcl_Interp *interp, Tcl_Obj *objP, signed long long *llP)
{
    /* TBD - does not check for positive overflow etc. */
    TCLH_ASSERT(sizeof(Tcl_WideInt) == sizeof(signed long long));
    return Tcl_GetWideIntFromObj(interp, objP, (Tcl_WideInt *)llP);
}

int
Tclh_ObjToULongLong(Tcl_Interp *interp, Tcl_Obj *objP, unsigned long long *ullP)
{
    /*
     * Tcl_GetWideIntFromObj has several issues:
     * - it ignores whitespace
     * - it will happily accept negative numbers in strings that will
     *   then show up as positive when retrieved. For example,
     *   Tcl_GWIFO(Tcl_NewStringObj(-18446744073709551615, -1)) will
     *   return a value of 1.
     * - It will also happily accept valid positive numbers in the range
     *   LLONG_MAX - ULLONG_MAX but return them as negative numbers which we
     *   cannot distinguish from genuine negative numbers
     * For these reasons, we parse the number from the string representation
     * if one is already present. If it is not present however, we use
     * Tcl_GWIFO directly because generating a string would not help - it
     * would be generated from the Tcl_WideInt value any way and we have
     * no way of knowing if it was negative or originally positive.
     */
    if (objP->bytes) {
        unsigned long long ull;
        char *end;
        /* Could have white space. Note unsigned char for isascii */
        const char *s = objP->bytes;
        while (isspace(*(unsigned char *)s))
            ++s; /* Ignores unicode spaces *shrug* */
        /* strtoull et all accept negative numbers. What a crock */
        if (*s != '-') {
            /*
             * At least VC++ does not reset errno so need to reset it
             * otherwise when return value is ULLONG_MAX successfully cannot
             * distinguish from error case.
             */
            errno = 0;
            ull   = strtoull(s, &end, 0);
            if (s != end && *end == '\0'
                && ((ull != ULLONG_MAX && ull != 0) || errno == 0)) {
                *ullP = ull;
                return TCL_OK;
            }
        }
    }
    else {
        Tcl_WideInt wide;
        if (Tcl_GetWideIntFromObj(interp, objP, &wide) != TCL_OK)
            return TCL_ERROR;
        if (wide >= 0) {
            *ullP = (unsigned long long)wide;
            return TCL_OK;
        }
        /* Assume it was originally negative and not a +ve reinterpretation */
    }
    return Tclh_ErrorInvalidValue(
        interp, objP, "Value is not a unsigned long long.");
}

#if 0
int TBDTclh_ObjToULongLong(Tcl_Interp *interp, Tcl_Obj *objP, unsigned long long *ullP)
{
    Tcl_WideInt wide;

    TCLH_ASSERT(sizeof(Tcl_WideInt) == sizeof(unsigned long long));

    /*
     * Below code depends on Tcl_GWIFO accepting values up to ULONGLONG_MAX
     * (though they will be treated as negative if > LONGLONG_MAX)
     */
    if (Tcl_GetWideIntFromObj(interp, objP, &wide) != TCL_OK)
        return TCL_ERROR;
    if (wide < 0) {
        /*
         * On a successful return with a negative result, we do not know
         * if it is an error or not. It could be
         * - a genuine error. A negative value is stored in the internal rep
         * - a false error. An overflowed positive value was stored that
         *   looks negative
         * In general, we cannot now distinguish between the two. The best
         * we can do is look for a string rep *if it exists* and see if
         * it has a minus sign. Note we do NOT generate a string rep if
         * it does not exist because that would be meaningless if generated
         * from the existing internal rep. Not foolproof but know of no
         * better way.
         */
        if (objP->bytes) {
            /* Could have white space. Note unsigned char for isascii */
            const unsigned char *s = (unsigned char *) objP->bytes;
            /* Ignores unicode spaces *shrug* */
            while (isspace(*s))
                ++s;
            /*
             * If not a digit or plus, assume error. It could be a minus
             * sign or \0 or something else (in which case one wonders why
             * Tcl_GWIFO did not return an error)
             */
            if (*s != '+' && ! isdigit(*s)) {
                return Tclh_ErrorInvalidValue(
                    interp, objP, "Value is not a unsigned long long.");
            }
        }
    }
    *ullP = (unsigned long long)wide;
    return TCL_OK;
}
#endif

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

#endif /* TCLH_TCL_OBJ_IMPL */

#endif /* TCLHOBJ_H */
