#ifndef TCLHBASE_H
#define TCLHBASE_H

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "tcl.h"

#ifdef _WIN32
#include <windows.h>
#endif

/* TBD - constrain length of all arguments in error messages */

/* Common definitions included by all Tcl helper *implemenations* */

#ifndef TCLH_INLINE
#ifdef _MSC_VER
# define TCLH_INLINE __inline
#else
# define TCLH_INLINE static inline
#endif
#endif

#ifndef TCLH_PANIC
#define TCLH_PANIC Tcl_Panic
#endif

#ifndef TCLH_ASSERT_LEVEL
# ifdef NDEBUG
#  define TCLH_ASSERT_LEVEL 0
# else
#  define TCLH_ASSERT_LEVEL 2
# endif
#endif

#if TCLH_ASSERT_LEVEL == 0
# define TCLH_ASSERT(bool_) (void) 0
#elif TCLH_ASSERT_LEVEL == 1
# define TCLH_ASSERT(bool_) (void)( (bool_) || (__debugbreak(), 0))
#else
# define TCLH_ASSERT(bool_) (void)( (bool_) || (TCLH_PANIC("Assertion (%s) failed at line %d in file %s.", #bool_, __LINE__, __FILE__), 0) )
#endif

#ifdef TCLH_IMPL
#ifndef TCLH_EMBEDDER
#error TCLH_EMBEDDER not defined. Please #define this to name of your package.
#endif
#endif

/*
 * Typedef: Tclh_SSizeT
 * This typedef is used to store max lengths of Tcl strings.
 * Its use is primarily to avoid compiler warnings with downcasting from size_t.
 */
#if TCL_MAJOR_VERSION <= 8
typedef int Tclh_SSizeT;
#define Tclh_SSizeT_MAX INT_MAX
typedef unsigned int Tclh_USizeT;
#define Tclh_USizeT_MAX UINT_MAX
#else
#error Define Tclh_SSizeT et al for Tcl 9
#endif

TCLH_INLINE char *Tclh_memdup(void *from, int len) {
    void *to = ckalloc(len);
    memcpy(to, from, len);
    return to;
}

TCLH_INLINE Tclh_SSizeT Tclh_strlen(const char *s) {
    return (int) strlen(s);
}

TCLH_INLINE char *Tclh_strdup(const char *from) {
    int len = Tclh_strlen(from) + 1;
    char *to = ckalloc(len);
    memcpy(to, from, len);
    return to;
}

TCLH_INLINE char *Tclh_strdupn(const char *from, int len) {
    char *to = ckalloc(len+1);
    memcpy(to, from, len);
    to[len] = '\0';
    return to;
}

/*
 * Error handling functions are also here because they are used by all modules.
 */

/* Section: Error reporting utilities
 *
 * Provides convenient error reporting functions for common error types.
 * The <Tclh_ErrorLibInit> function must be called before any other functions in
 * this module. In addition to storing an appropriate error message in the
 * interpreter, all functions record a Tcl error code in a consistent format.
 */

/* Macro: TCLH_CHECK_RESULT
 * Checks if a value and returns from the calling function if it
 * is anything other than TCL_OK.
 *
 * Parameters:
 * value_or_fncall - Value to check. Commonly this may be a function call which
 *                   returns one of the standard Tcl result values.
 */
#define TCLH_CHECK_RESULT(value_or_fncall)      \
    do {                                        \
        int result = value_or_fncall;           \
        if (result != TCL_OK)                   \
            return result;                      \
    } while (0)

/* Macro: TCLH_CHECK_NARGS
 * Checks the number of arguments passed as the objv[] array to a function
 * implementing a Tcl command to be within a specified range.
 *
 * Parameters:
 * min_ - minimum number of arguments
 * max_ - maximum number of arguments
 * message_ - the additional message argument passed to Tcl_WrongNumArgs to
 *            generate the error message.
 *
 * If the number arguments does not fall in within the range, stores an
 * error message in the interpreter and returns from the caller with a
 * TCL_ERROR result code. The macros assumes that variable names *objv* and
 * *objc* in the caller's context refer to the argument array and number of
 * arguments respectively. The generated error message uses one argument
 * from the passed objv array. See Tcl_WrongNumArgs for more information
 * about the generated error message.
 */
#define TCLH_CHECK_NARGS(ip_, min_, max_, message_)   \
    do {                                                \
        if (objc < min_ || objc > max_) {               \
            return Tclh_ErrorNumArgs(ip_, 1, objv, message_);    \
        }                                               \
    } while(0)


/* Function: Tclh_ErrorExists
 * Reports an error that an object already exists.
 *
 * Parameters:
 * interp  - Tcl interpreter in which to report the error.
 * type    - String indicating type of object, e.g. *File*. Defaults to *Object*
 *           if passed as NULL.
 * searchObj - The object being searched for, e.g. the file name. This is
 *           included in the error message if not *NULL*.
 * message - Additional text to append to the standard error message. May be NULL.
 *
 *
 * The Tcl *errorCode* variable is set to a list of three elements: the
 * *TCLH_EMBEDDER* macro value set by the extension, the literal string
 * *EXISTS* and the error message.
 *
 * Returns:
 * TCL_ERROR - Always returns this value so caller can just pass on the return
 *             value from this function.
 */
int Tclh_ErrorExists(Tcl_Interp *interp, const char *type,
                     Tcl_Obj *searchObj, const char *message);

/* Function: Tclh_ErrorGeneric
 * Reports a generic error.
 *
 * Parameters:
 * interp - Tcl interpreter in which to report the error.
 * code   - String literal value to use for the error code. Defaults to
 *          *ERROR* if *NULL*.
 * message - Additional text to add to the error message. May be NULL.
 *
 * This is a catchall function to report any errors that do not fall into one
 * of the other categories of errors. If *message* is not *NULL*, it is stored
 * as the Tcl result. Otherwise, a generic *Unknown error* message is stored.
 *
 * The Tcl *errorCode* variable is set to a list of three elements: the
 * *TCLH_EMBEDDER* macro value set by the extension, the literal string *ERROR*
 * and the error message.
 *
 * Returns:
 * TCL_ERROR - Always returns this value so caller can just pass on the return
 *             value from this function.
 */
int Tclh_ErrorGeneric(Tcl_Interp *interp, const char *code, const char *message);

/* Function: Tclh_ErrorNotFound
 * Reports an error where an object is not found or is not accessible.
 *
 * Parameters:
 * interp  - Tcl interpreter in which to report the error.
 * type    - String indicating type of object, e.g. *File*. Defaults to *Object*
 *           if passed as NULL.
 * searchObj - The object being searched for, e.g. the file name. This is
 *           included in the error message if not *NULL*.
 * message - Additional text to append to the standard error message. May be NULL.
 *
 *
 * The Tcl *errorCode* variable is set to a list of three elements: the
 * *TCLH_EMBEDDER* macro value set by the extension, the literal string
 * *NOT_FOUND* and the error message.
 *
 * Returns:
 * TCL_ERROR - Always returns this value so caller can just pass on the return
 *             value from this function.
 */
int Tclh_ErrorNotFound(Tcl_Interp *interp, const char *type,
                       Tcl_Obj *searchObj, const char *message);

/* Function: Tclh_ErrorOperFailed
 * Reports the failure of an operation.
 *
 * Parameters:
 * interp  - Tcl interpreter in which to report the error.
 * oper    - String describing the operation, e.g. *delete*. May be passed
 *           as *NULL*.
 * operandObj - The object on which the operand was attempted, e.g. the file
 *           name. This is included in the error message if not *NULL*.
 * message - Additional text to append to the standard error message. May be NULL.
 *
 *
 * The Tcl *errorCode* variable is set to a list of three elements: the
 * *TCLH_EMBEDDER* macro value set by the extension, the literal string
 * *OPER_FAILED* and the error message.
 *
 * Returns:
 * TCL_ERROR - Always returns this value so caller can just pass on the return
 *             value from this function.
 */
int Tclh_ErrorOperFailed(Tcl_Interp *interp, const char *type,
                         Tcl_Obj *searchObj, const char *message);

/* Function: Tclh_ErrorInvalidValue
 * Reports an invalid argument passed in to a command function.
 *
 * Parameters:
 * interp    - Tcl interpreter in which to report the error.
 * badArgObj - The object argument that was found to be invalid. This is
 *           included in the error message if not *NULL*.
 * message - Additional text to append to the standard error message. May be NULL.
 *
 * The Tcl *errorCode* variable is set to a list of three elements: the
 * *TCLH_EMBEDDER* macro value set by the extension, the literal string
 * *INVALID_VALUE* and the error message.
 *
 * Returns:
 * TCL_ERROR - Always returns this value so caller can just pass on the return
 *             value from this function.
 */
int Tclh_ErrorInvalidValue(Tcl_Interp *interp, Tcl_Obj *badArgObj,
                         const char *message);


/* Function: Tclh_ErrorNumArgs
 * Reports an invalid number of arguments passed into a command function.
 *
 * Parameters:
 * interp - Tcl interpreter in which to report the error.
 * objc   - Number of elements in the objv[] array to include in the error message.
 * objv   - Array containing command arguments.
 * message - Additional text to add to the error message. May be NULL.
 *
 * The Tcl *errorCode* variable is set to a list of three elements: the
 * *TCLH_EMBEDDER* macro value set by the extension, the literal string *ERROR*
 * and the error message.
 *
 * This is a simple wrapper around the standard *Tcl_WrongNumArgs* function
 * except it returns the *TCL_ERROR* as a result. See that function description
 * for details.
 *
 * Returns:
 * TCL_ERROR - Always returns this value so caller can just pass on the return
 *             value from this function.
 */
int
Tclh_ErrorNumArgs(Tcl_Interp *interp,
                  int objc, Tcl_Obj *const objv[],
                  const char *message);

/* Function: Tclh_ErrorWrongType
 * Reports an error where a value is of the wrong type.
 *
 * Parameters:
 * interp  - Tcl interpreter in which to report the error.
 * argObj  - The value in question. This is
 *           included in the error message if not *NULL*.
 * message - Additional text to append to the standard error message. May be *NULL*.
 *
 *
 * The Tcl *errorCode* variable is set to a list of three elements: the
 * *TCLH_EMBEDDER* macro value set by the extension, the literal string
 * *WRONG_TYPE* and the error message.
 *
 * Returns:
 * TCL_ERROR - Always returns this value so caller can just pass on the return
 *             value from this function.
 */
int Tclh_ErrorWrongType(Tcl_Interp *interp, Tcl_Obj *argObj,
                        const char *message);

/* Function: Tclh_ErrorAllocation
 * Reports an error where an allocation failed.
 *
 * Parameters:
 * interp  - Tcl interpreter in which to report the error.
 * type    - String indicating type of object, e.g. *Memory*. Defaults to *Object*
 *           if passed as NULL.
 * message - Additional text to append to the standard error message. May be NULL.
 *
 * The Tcl *errorCode* variable is set to a list of three elements: the
 * *TCLH_EMBEDDER* macro value set by the extension, the literal string
 * *ALLOCATION* and the error message.
 *
 * Returns:
 * TCL_ERROR - Always returns this value so caller can just pass on the return
 *             value from this function.
 */
int Tclh_ErrorAllocation(Tcl_Interp *interp, const char *type, const char *message);

/* Function: Tclh_ErrorRange
 * Reports an out-of-range error for integers.
 *
 * Parameters:
 * interp  - Tcl interpreter in which to report the error.
 * objP    - Value that is out of range
 * low     - low end of permitted range (inclusive)
 * high    - high end of permitted range (inclusive)
 *
 * Returns:
 * TCL_ERROR - Always returns this value so caller can just pass on the return
 *             value from this function.
 */
int Tclh_ErrorRange(Tcl_Interp *interp,
                    Tcl_Obj *objP,
                    Tcl_WideInt low,
                    Tcl_WideInt high);

#ifdef _WIN32
/* Function: Tclh_ErrorWindowsError
 * Reports a Windows error code message.
 *
 * Parameters:
 * interp  - Tcl interpreter in which to report the error.
 * winerror - Windows error codnfe
 * message - Additional text to append to the standard error message. May be NULL.
 *
 * Returns:
 * TCL_ERROR - Always returns this value so caller can just pass on the return
 *             value from this function.
 */
int
Tclh_ErrorWindowsError(Tcl_Interp *interp, unsigned int winerror, const char *message);
#endif


#ifdef TCLH_SHORTNAMES
#define ErrorGeneric    Tclh_ErrorGeneric
#define ErrorInvalidValue Tclh_ErrorInvalidValue
#define ErrorNotFound   Tclh_ErrorNotFound
#define ErrorExists     Tclh_ErrorExists
#define ErrorWrongType  Tclh_ErrorWrongType
#define ErrorNumArgs    Tclh_ErrorNumArgs
#define ErrorOperFailed Tclh_ErrorOperFailed
#define ErrorAllocation Tclh_ErrorAllocation
#define ErrorRange      Tclh_ErrorRange
#ifdef _WIN32
#define ErrorWindowsError Tclh_ErrorWindowsError
#endif
#endif


/*
 * IMPLEMENTATION.
 */

#ifdef TCLH_IMPL

int Tclh_BaseLibInit(Tcl_Interp *interp)
{
    return TCL_OK;
}

void
TclhRecordErrorCode(Tcl_Interp *interp, const char *code, Tcl_Obj *msgObj)
{
    Tcl_Obj *objs[3];
    Tcl_Obj *errorCodeObj;

    TCLH_ASSERT(interp);
    TCLH_ASSERT(code);

    objs[0] = Tcl_NewStringObj(TCLH_EMBEDDER, -1);
    objs[1] = Tcl_NewStringObj(code, -1);
    objs[2] = msgObj;
    errorCodeObj = Tcl_NewListObj(msgObj == NULL ? 2 : 3, objs);
    Tcl_SetObjErrorCode(interp, errorCodeObj);
}

/*  NOTE: caller should hold a ref count to msgObj if it will be accessed
 *    when this function returns
 */
static int
TclhRecordError(Tcl_Interp *interp, const char *code, Tcl_Obj *msgObj)
{
    TCLH_ASSERT(code);
    TCLH_ASSERT(msgObj);
    if (interp) {
        TclhRecordErrorCode(interp, code, msgObj);
        Tcl_SetObjResult(interp, msgObj);
    }
    else {
        /*
         * If not passing to interp, we have to free msgObj. The Incr/Decr
         * dance is in case caller has a reference to it, we should not free
         * it.
         */
        Tcl_IncrRefCount(msgObj);
        Tcl_DecrRefCount(msgObj);
    }
    return TCL_ERROR;
}

int
Tclh_ErrorGeneric(Tcl_Interp *interp, const char *code, const char *message)
{
    Tcl_Obj *msgObj =
            Tcl_NewStringObj(message ? message : "Unknown error.", -1);
    if (code == NULL)
        code = "ERROR";
    return TclhRecordError(interp, "ERROR", msgObj);
}


int
Tclh_ErrorWrongType(Tcl_Interp *interp, Tcl_Obj *argObj, const char *message)
{
    Tcl_Obj *msgObj;
    if (message == NULL)
        message = "";
    if (argObj) {
        msgObj = Tcl_ObjPrintf("Value \"%s\" has the wrong type. %s",
                               Tcl_GetString(argObj), message);
    }
    else {
        msgObj = Tcl_ObjPrintf("Value has the wrong type. %s", message);
    }
    return TclhRecordError(interp, "WRONG_TYPE", msgObj);
}

int
Tclh_ErrorExists(Tcl_Interp *interp,
                 const char *type,
                 Tcl_Obj *   searchObj,
                 const char *message)
{
    Tcl_Obj *msgObj;
    if (type == NULL)
        type = "Object";
    if (message == NULL)
        message = "";
    if (searchObj) {
        msgObj = Tcl_ObjPrintf("%s \"%s\" already exists. %s",
                               type,
                               Tcl_GetString(searchObj),
                               message);
    }
    else {
        msgObj = Tcl_ObjPrintf("%s already exists. %s", type, message);
    }
    return TclhRecordError(interp, "EXISTS", msgObj);
}

int
Tclh_ErrorNotFound(Tcl_Interp *interp,
                   const char *type,
                   Tcl_Obj *   searchObj,
                   const char *message)
{
    Tcl_Obj *msgObj;
    if (type == NULL)
        type = "Object";
    if (message == NULL)
        message = "";
    if (searchObj) {
        msgObj = Tcl_ObjPrintf("%s \"%s\" not found or inaccessible. %s",
                               type,
                               Tcl_GetString(searchObj),
                               message);
    }
    else {
        msgObj = Tcl_ObjPrintf("%s not found. %s", type, message);
    }
    return TclhRecordError(interp, "NOT_FOUND", msgObj);
}

int Tclh_ErrorOperFailed(Tcl_Interp *interp, const char *oper,
                         Tcl_Obj *operandObj, const char *message)
{
    Tcl_Obj *msgObj;
    const char *operand;
    operand = operandObj == NULL ? "object" : Tcl_GetString(operandObj);
    if (message == NULL)
        message = "";
    if (oper)
        msgObj = Tcl_ObjPrintf("Operation %s failed on %s. %s", oper, operand, message);
    else
        msgObj = Tcl_ObjPrintf("Operation failed on %s. %s", operand, message);
    return TclhRecordError(interp, "OPER_FAILED", msgObj);
}

int
Tclh_ErrorInvalidValue(Tcl_Interp *interp, Tcl_Obj *badArgObj, const char *message)
{
    Tcl_Obj *msgObj;
    if (message == NULL)
        message = "";
    if (badArgObj) {
        msgObj = Tcl_ObjPrintf("Invalid value \"%s\". %s",
                               Tcl_GetString(badArgObj),
                               message);
    }
    else {
        msgObj = Tcl_ObjPrintf("Invalid value. %s", message);
    }
    return TclhRecordError(interp, "INVALID_VALUE", msgObj);
}

int
Tclh_ErrorNumArgs(Tcl_Interp *   interp,
                  int            objc,
                  Tcl_Obj *const objv[],
                  const char *   message)
{
    Tcl_WrongNumArgs(interp, objc, objv, message);
    return TCL_ERROR;
}

int
Tclh_ErrorAllocation(Tcl_Interp *interp, const char *type, const char *message)
{
    Tcl_Obj *msgObj;
    if (message == NULL)
        message = "";
    if (type == NULL)
        type = "Object";
    msgObj = Tcl_ObjPrintf("%s allocation failed. %s", type, message);
    return TclhRecordError(interp, "ALLOCATION", msgObj);
}

int
Tclh_ErrorRange(Tcl_Interp *interp,
                Tcl_Obj *objP,
                Tcl_WideInt low,
                Tcl_WideInt high)
{
    Tcl_Obj *msgObj;
    /* Resort to snprintf because ObjPrintf does not handle %lld correctly */
    char buf[200];
    snprintf(buf,
             sizeof(buf),
             "Value %.20s not in range. Must be within [%" TCL_LL_MODIFIER
             "d,%" TCL_LL_MODIFIER "d].",
             Tcl_GetString(objP),
             low,
             high);
    msgObj = Tcl_NewStringObj(buf, -1);
    return TclhRecordError(interp, "RANGE", msgObj);
}

/*
 * Print an address is cross-platform manner. Internal routine, minimal error checks
 */
char *TclhPrintAddress(const void *address, char *buf, int buflen)
{
#ifdef OBSOLETE
/* %p on gcc prints (nil) for null pointers - not what we want */
    static int fixup = -1; /* Thread safety? Meh... */

    TCLH_ASSERT(buflen > 2);

    /*
     *  MSC does not prefix with 0x, gcc and may be others, do. Remove
     *  any extraneous prefix if present. Take care about string lengths.
     */
    if (fixup == -1) {
        /* We have not determined the format. Do so */
        snprintf(buf, buflen, "%p", address);
        if (buf[1] == 'x') {
            /* Does what we want. */
            fixup = 0;
            return buf;
        }
        else
            fixup = 1;
    }
    if (fixup == 0) {
        snprintf(buf, buflen, "%p", address);
    } else {
        snprintf(buf, buflen, "0x%p", address);
    }
    return buf;
#else
    TCLH_ASSERT(buflen > 2);

    if (sizeof(void*) == sizeof(int)) {
        unsigned int i = (unsigned int) (intptr_t) address;
        snprintf(buf, buflen, "0x%.8x", i);
    }
    else {
        TCLH_ASSERT(sizeof(void *) == sizeof(unsigned long long));
        unsigned long long ull = (intptr_t)address;
        snprintf(buf, buflen, "0x%.16llx", ull);
    }
    return buf;
#endif
}

#ifdef _WIN32
Tcl_Obj *TclhMapWindowsError(
    DWORD winError,      /* Windows error code */
    HANDLE moduleHandle,        /* Handle to module containing error string.
                                 * If NULL, assumed to be system message. */
    const char *msgPtr)         /* Message prefix. May be NULL. */
{
    int   length;
    DWORD flags;
    WCHAR *winErrorMessagePtr = NULL;
    Tcl_Obj *objPtr;

    if (msgPtr) {
        int msgLen;
        msgLen = Tclh_strlen(msgPtr);
        objPtr = Tcl_NewStringObj(msgPtr, msgLen);
        if (msgLen && msgPtr[msgLen-1] == ' ')
            Tcl_AppendToObj(objPtr, " ", 1);
    } else {
        objPtr = Tcl_NewObj();
    }

    flags = moduleHandle ? FORMAT_MESSAGE_FROM_HMODULE : FORMAT_MESSAGE_FROM_SYSTEM;
    flags |=
        FORMAT_MESSAGE_ALLOCATE_BUFFER /* So we do not worry about length */
        | FORMAT_MESSAGE_IGNORE_INSERTS /* Ignore message arguments */
        | FORMAT_MESSAGE_MAX_WIDTH_MASK;/* Ignore soft line breaks */

    length = FormatMessageW(flags, moduleHandle, winError,
                            0, /* Lang id */
                            (WCHAR *) &winErrorMessagePtr,
                            0, NULL);
    if (length > 0) {
        /* Strip trailing CR LF if any */
        if (winErrorMessagePtr[length-1] == L'\n')
            --length;
        if (length > 0) {
            if (winErrorMessagePtr[length-1] == L'\r')
                --length;
        }
        Tcl_AppendUnicodeToObj(objPtr, winErrorMessagePtr, length);
        LocalFree(winErrorMessagePtr);
    } else {
        Tcl_AppendPrintfToObj(objPtr, "Windows error code %ld", winError);
    }
    return objPtr;
}

int
Tclh_ErrorWindowsError(Tcl_Interp *interp, unsigned int winerror, const char *message)
{
    Tcl_Obj *msgObj = TclhMapWindowsError(winerror, NULL, message);
    return TclhRecordError(interp, "WINERROR", msgObj);
}
#endif /* _WIN32 */

#endif /* TCLH_IMPL */

#endif /* TCLHBASE_H */
