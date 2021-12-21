#ifndef TCLCFFIINT_H
#define TCLCFFIINT_H

#ifdef _WIN32
#include <windows.h> /* Needed for the dyncall workaround referencing IMAGE_EXPORT_DIRECTORY etc  - TBD */
#endif

#include "tcl.h"

#include "tclhBase.h"
#include "tclhObj.h"
#include "tclhPointer.h"
#include "memlifo.h"

#ifdef CFFI_USE_DYNCALL

#include "dynload.h"
#include "dyncall.h"

#else

/* Libffi does not have its own load */
#define CFFI_USE_TCLLOAD

/*
 * When statically linking libffi, we need to define FFI_BUILDING before
 * including ffi.h. This is done on the command
 * line depending on the build option chosen. However, the vcpkg distribution
 * of libffi hard codes FFI_BUILDING in the ffi.h header file as a patch
 * leading to "C4005" macro redefinition warnings. Thus we need to disable
 * the warning. (Note the standard libffi does NOT hardcode it so we do
 * need to define it ourselves)
 */

#ifdef _MSC_VER
#pragma warning(push )
#pragma warning(disable:4005 )
#endif
#include "ffi.h"
#ifdef _MSC_VER
#pragma warning(default:4005 )
#pragma warning(pop)
#endif

#endif /* CFFI_USE_DYNCALL */

#include <stdint.h>

#define CFFI_NAMESPACE PACKAGE_NAME

#define CFFI_ASSERT TCLH_ASSERT
#define CHECK TCLH_CHECK_RESULT
#define CHECK_NARGS TCLH_CHECK_NARGS

typedef int CffiResult; /* TCL_OK etc. */

#ifdef _MSC_VER
# define CFFI_INLINE __inline
#else
# define CFFI_INLINE static inline
#endif

#define CFFI_PANIC TCLH_PANIC

/*
 * Base types - IMPORTANT!!! order must match cffiBaseTypes array
 */
typedef enum CffiBaseType {
    CFFI_K_TYPE_VOID,
    CFFI_K_TYPE_SCHAR,
    CFFI_K_TYPE_UCHAR,
    CFFI_K_TYPE_SHORT,
    CFFI_K_TYPE_USHORT,
    CFFI_K_TYPE_INT,
    CFFI_K_TYPE_UINT,
    CFFI_K_TYPE_LONG,
    CFFI_K_TYPE_ULONG,
    CFFI_K_TYPE_LONGLONG,
    CFFI_K_TYPE_ULONGLONG,
    CFFI_K_TYPE_FLOAT,
    CFFI_K_TYPE_DOUBLE,
    CFFI_K_TYPE_STRUCT,
    CFFI_K_TYPE_POINTER,
    CFFI_K_TYPE_ASTRING,
    CFFI_K_TYPE_UNISTRING,
    CFFI_K_TYPE_BINARY,
    CFFI_K_TYPE_CHAR_ARRAY,
    CFFI_K_TYPE_UNICHAR_ARRAY,
    CFFI_K_TYPE_BYTE_ARRAY,
    CFFI_K_NUM_TYPES
} CffiBaseType;

/*
 * Context types when parsing type definitions.
 */
typedef enum CffiTypeParseMode {
    CFFI_F_TYPE_PARSE_PARAM  = 1, /* Function parameter*/
    CFFI_F_TYPE_PARSE_RETURN = 2, /* Function return type */
    CFFI_F_TYPE_PARSE_FIELD  = 4, /* Structure field */
} CffiTypeParseMode;

/*
 * Forward declarations.
 */
typedef struct CffiStruct CffiStruct;

/*
 * Data type representation
 */
typedef struct CffiBaseTypeInfo {
    const char *token;/* Script level type identifier string token */
    int token_len;    /* Length of token */
    CffiBaseType baseType;/* C level type identifier */
    int validAttrFlags;    /* Mask type attribute flags valid for this type */
    int size;              /* Size of type - only for scalars */
} CffiBaseTypeInfo;
extern const CffiBaseTypeInfo cffiBaseTypes[];

typedef struct CffiType {
    enum CffiBaseType baseType;
    int               count;    /* 0->scalar
                                    <0 -> array of unknown size specified
                                          through countHolderObj
                                    >0 -> array of count base type elements */
    union {
        /* tagObj -
         * POINTER - pointer tag (may be NULL)
         * ASTRING, CHAR_ARRAY - encoding (may be NULL)
         * Numerical types - Enum name (may be NULL)
         */
        Tcl_Obj *tagObj;
        CffiStruct *structP; /* TCL_F_TYPE_STRUCT */
    } u;
    Tcl_Obj *countHolderObj; /* Holds the name of the slot (e.g. parameter name)
                                that contains the actual count at call time */
} CffiType;

/*
 * Function parameter descriptor
 */
typedef struct CffiTypeAndAttrs {
    Tcl_Obj *parseModeSpecificObj; /* Parameter parse - Default for parameter,
                                      Return parse - error handler */
    CffiType dataType;         /* Data type */
    int flags;
#define CFFI_F_ATTR_IN    0x0001 /* In parameter */
#define CFFI_F_ATTR_OUT   0x0002 /* Out parameter */
#define CFFI_F_ATTR_INOUT 0x0004 /* Out parameter */
#define CFFI_F_ATTR_BYREF 0x0008 /* Parameter is a reference */

#define CFFI_F_ATTR_DISPOSE     0x0010 /* Unregister the pointer */
#define CFFI_F_ATTR_COUNTED     0x0020 /* Counted safe pointer */
#define CFFI_F_ATTR_UNSAFE      0x0040 /* Pointers need not be checked */
#define CFFI_F_ATTR_DISPOSEONSUCCESS 0x0080 /* Unregister on successful call */

#define CFFI_F_ATTR_ZERO        0x0100 /* Must be zero/null */
#define CFFI_F_ATTR_NONZERO     0x0200 /* Must be nonzero/nonnull */
#define CFFI_F_ATTR_NONNEGATIVE 0x0400 /* Must be >= 0 */
#define CFFI_F_ATTR_POSITIVE    0x0800 /* Must be >= 0 */
#define CFFI_F_ATTR_LASTERROR 0x10000 /* Windows GetLastError for error message */
#define CFFI_F_ATTR_ERRNO     0x20000 /* Error in errno */
#define CFFI_F_ATTR_WINERROR  0x40000 /* Windows error code */
#define CFFI_F_ATTR_ONERROR   0x80000 /* Error handler */

#define CFFI_F_ATTR_STOREONERROR 0x100000 /* Store only on error */
#define CFFI_F_ATTR_STOREALWAYS  0x200000 /* Store on success and error */

#define CFFI_F_ATTR_ENUM        0x1000000 /* Use enum names */
#define CFFI_F_ATTR_BITMASK     0x2000000 /* Treat as a bitmask */
#define CFFI_F_ATTR_NULLIFEMPTY 0x4000000 /* Empty value -> null pointer */
#define CFFI_F_ATTR_NULLOK      0x8000000 /* Null pointers permissible */
} CffiTypeAndAttrs;

/* Attributes allowed on a parameter declaration */
#define CFFI_F_ATTR_PARAM_MASK                                                \
    (CFFI_F_ATTR_IN | CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT | CFFI_F_ATTR_BYREF \
     | CFFI_F_ATTR_STOREONERROR | CFFI_F_ATTR_STOREALWAYS)
/* Attributes related to pointer safety */
#define CFFI_F_ATTR_SAFETY_MASK                                              \
    (CFFI_F_ATTR_UNSAFE | CFFI_F_ATTR_DISPOSE | CFFI_F_ATTR_DISPOSEONSUCCESS \
     | CFFI_F_ATTR_COUNTED)
/* Error checking attributes */
#define CFFI_F_ATTR_REQUIREMENT_MASK                                  \
    (CFFI_F_ATTR_ZERO | CFFI_F_ATTR_NONZERO | CFFI_F_ATTR_NONNEGATIVE \
     | CFFI_F_ATTR_POSITIVE)
/* Error retrieval attributes */
#define CFFI_F_ATTR_ERROR_MASK                                        \
    (CFFI_F_ATTR_LASTERROR | CFFI_F_ATTR_ERRNO | CFFI_F_ATTR_WINERROR \
     | CFFI_F_ATTR_ONERROR)


/*
 * Calling protocol type
 */
#ifdef CFFI_USE_DYNCALL
typedef DCint CffiABIProtocol;
#else
typedef ffi_abi CffiABIProtocol;
#endif

/*
 * Union of value types supported by dyncall
 */
typedef struct CffiValue {
    union {
    signed char schar;
    unsigned char uchar;
    signed short sshort;
    unsigned short ushort;
    signed int sint;
    unsigned int uint;
    signed long slong;
    unsigned long ulong;
    signed long long slonglong;
    unsigned long long ulonglong;
    float flt;
    double dbl;
    void *ptr;
#ifndef CFFI_USE_DYNCALL
    ffi_arg ffi_val;
#endif
    } u;
    union {
        /* TBD - Tcl_DString has 200 bytes static storage. Memory wasteful? */
        /* Maybe make it a pointer onto memlifo? */
        Tcl_DString ds;  /* CFFI_K_TYPE_ASTRING */
        Tcl_Obj *baObj;  /* CFFI_K_TYPE_BINARY */
        Tcl_Obj *uniObj; /* CFFI_K_TYPE_UNISTRING */
    } ancillary ; /* Ancillary data needed for some types */
} CffiValue;

/*
 * C struct and struct field descriptors
 */
#ifndef CFFI_USE_DYNCALL
/*
 * Libffi needs a type descriptor for structures - one per ABI.
 */
typedef struct CffiLibffiStruct {
    CffiABIProtocol abi;             /* ABI this layout pertains to */
    struct CffiLibffiStruct *nextP; /* Link to struct for next ABI */
    ffi_type ffiType;
    ffi_type *ffiFieldTypes[1]; /* Actually variable size */
} CffiLibffiStruct;
#endif

typedef struct CffiStruct CffiStruct;
typedef struct CffiField {
    Tcl_Obj *nameObj;           /* Field name */
    CffiTypeAndAttrs fieldType; /* base type, cardinality, tag etc. */
    unsigned int offset;        /* Field offset from beginning of struct */
    unsigned int size;          /* Size of the field */
} CffiField;

struct CffiStruct {
    Tcl_Obj *name;              /* Struct type name */
#ifndef CFFI_USE_DYNCALL
    CffiLibffiStruct *libffiTypes; /* Corresponding libffi type descriptors */
#endif
    int nRefs;                  /* Shared, so need ref count */
    unsigned int size;          /* Size of struct */
    unsigned int alignment;     /* Alignment required for struct */
    int nFields;                /* Cardinality of fields[] */
    CffiField fields[1];       /* Actual size given by nFields */
    /* !!!DO NOT ADD FIELDS HERE!!! */
};
CFFI_INLINE void CffiStructRef(CffiStruct *structP) {
    structP->nRefs += 1;
}

/*
 * Interpreter along with common structures specific to the interpreter.
 */
typedef struct CffiInterpCtx {
    Tcl_Interp *interp;     /* The interpreter in which the DL is registered.
                               Note this does not need to be protected against
                               deletion as contexts are unregistered before
                               interp deletion */
    Tcl_HashTable aliases;  /* typedef name -> CffiTypeAndAttrs */
    Tcl_HashTable prototypes; /* prototype name -> CffiProto */
    Tcl_HashTable enums;      /* Enum -> (name->value table) */
    MemLifo memlifo;        /* Software stack */
} CffiInterpCtx;

/* Context required for making calls to a function in a DLL. */
typedef struct CffiCallVmCtx {
    CffiInterpCtx *ipCtxP;
#ifdef CFFI_USE_DYNCALL
    DCCallVM *vmP; /* The dyncall call context to use */
#endif
} CffiCallVmCtx;

/* Context for dll commands. */
#ifdef CFFI_USE_TCLLOAD
typedef Tcl_LoadHandle CffiLoadHandle;
#else
typedef DLLib *CffiLoadHandle;
#endif

typedef struct CffiLibCtx {
    CffiCallVmCtx *vmCtxP;
    CffiLoadHandle libH; /* The dyncall library context */
    Tcl_Obj *pathObj;    /* Path to the library. May be NULL */
    int nRefs; /* To ensure library not released with bound functions */
} CffiLibCtx;
CFFI_INLINE void CffiLibCtxRef(CffiLibCtx *libCtxP) {
    libCtxP->nRefs += 1;
}

/* Context for struct command functions */
typedef struct CffiStructCtx {
    CffiInterpCtx *ipCtxP;
    CffiStruct *structP;
} CffiStructCtx;

/* Function parameter definition */
typedef struct CffiParam {
    Tcl_Obj *nameObj;           /* Parameter name */
    CffiTypeAndAttrs typeAttrs;
} CffiParam;

/* Function prototype definition */
typedef struct CffiProto {
    int nRefs;             /* Reference count */
    int nParams;           /* Number of params, sizeof params array */
    CffiABIProtocol abi;   /* cdecl, stdcall etc. */
    CffiParam returnType;  /* Name and return type of function */
#ifndef CFFI_USE_DYNCALL
    ffi_cif *cifP; /* Descriptor used by cffi */
#endif
    CffiParam params[1];   /* Real size depends on nparams which
                               may even be 0!*/
    /* !!!DO NOT ADD FIELDS HERE AT END OF STRUCT!!! */
} CffiProto;
CFFI_INLINE void CffiProtoRef(CffiProto *protoP) {
    protoP->nRefs += 1;
}

/* Function definition */
typedef struct CffiFunction {
    CffiCallVmCtx *vmCtxP; /* Context for the call */
    void *fnAddr;          /* Pointer to the function to call */
    CffiProto *protoP;     /* Prototype for the call */
    CffiLibCtx *libCtxP; /* Containing library for bound functions or
                            NULL for free standing functions */
    Tcl_Obj *cmdNameObj; /* Name of Tcl command. May be NULL */
} CffiFunction;

/* Used to store argument context when preparing to call a function */
typedef struct CffiArgument {
    CffiValue value;      /* Native value being constructed. */
    CffiValue savedValue; /* Copy of above - needed after call in some cases
                             like disposable pointers. NOT used in all cases */
    Tcl_Obj *varNameObj;  /* Name of output variable or NULL */
#ifndef CFFI_USE_DYNCALL
    void *valueP; /* Always points to the value field. Used for libcffi
                     which needs an additional level of indirection for
                     byref parameters. Only set as needed in CffiPrepareArg */
#endif
    int actualCount;      /* For dynamic arrays, stores the actual size.
                             Always >= 0 (0 being scalar) */
    int flags;
#define CFFI_F_ARG_INITIALIZED 0x1
} CffiArgument;

/* Complete context for a call invocation */
typedef struct CffiCall {
    CffiFunction *fnP;         /* Function being called */
    CffiArgument *argsP;   /* Argument contexts */
#ifndef CFFI_USE_DYNCALL
    void **argValuesPP; /* Array of pointers into the actual value fields within
                           argsP[] elements */
    void *retValueP;    /* Points to storage to use for return value */
    CffiValue retValue; /* Holds return value */
#endif
    int nArgs;             /* Size of argsP. */
} CffiCall;

/*
 * Prototypes
 */
CffiResult CffiNameSyntaxCheck(Tcl_Interp *ip, Tcl_Obj *nameObj);

const CffiBaseTypeInfo *CffiBaseTypeInfoGet(Tcl_Interp *ip,
                                            Tcl_Obj *baseTypeObj);
CffiResult CffiTypeParse(Tcl_Interp *ip, Tcl_Obj *typeObj, CffiType *typeP);
void CffiTypeCleanup(CffiType *);
void CffiTypeLayoutInfo(const CffiType *typeP,
                        int *baseSizeP,
                        int *sizeP,
                        int *alignP);
void CffiTypeAndAttrsInit(CffiTypeAndAttrs *toP, CffiTypeAndAttrs *fromP);
CffiResult CffiTypeAndAttrsParse(CffiInterpCtx *ipCtxP,
                                 Tcl_Obj *typeAttrObj,
                                 CffiTypeParseMode parseMode,
                                 CffiTypeAndAttrs *typeAttrsP);
void CffiTypeAndAttrsCleanup(CffiTypeAndAttrs *typeAttrsP);
Tcl_Obj *CffiTypeAndAttrsUnparse(const CffiTypeAndAttrs *typeAttrsP);
CffiResult CffiStructParse(CffiInterpCtx *ipCtxP,
                           Tcl_Obj *nameObj,
                           Tcl_Obj *structObj,
                           CffiStruct **structPP);
void CffiStructUnref(CffiStruct *structP);
CffiResult
CffiStructResolve(Tcl_Interp *ip, const char *nameP, CffiStruct **structPP);
CffiResult
CffiBytesFromObj(Tcl_Interp *ip, Tcl_Obj *fromObj, char *toP, int toSize);
CffiResult
CffiUniCharsFromObj(Tcl_Interp *ip, Tcl_Obj *fromObj, char *toP, int toSize);
CffiResult CffiCharsFromObj(
    Tcl_Interp *ip, Tcl_Obj *encObj, Tcl_Obj *fromObj, char *toP, int toSize);
CffiResult CffiCharsToObj(Tcl_Interp *ip,
                          const CffiTypeAndAttrs *typeAttrsP,
                          char *srcP,
                          Tcl_Obj **resultObjP);
CffiResult CffiUniStringToObj(Tcl_Interp *ip,
                              const CffiTypeAndAttrs *typeAttrsP,
                              Tcl_DString *dsP,
                              Tcl_Obj **resultObjP);
CffiResult CffiStructFromObj(Tcl_Interp *ip,
                             const CffiStruct *structP,
                             Tcl_Obj *structValueObj,
                             void *resultP);
CffiResult CffiStructToObj(Tcl_Interp *ip,
                           const CffiStruct *structP,
                           void *valueP,
                           Tcl_Obj **valueObjP);
CffiResult CffiNativeValueToObj(Tcl_Interp *ip,
                                const CffiTypeAndAttrs *typeAttrsP,
                                void *valueP,
                                int count,
                                Tcl_Obj **valueObjP);
CffiResult CffiCheckPointer(Tcl_Interp *ip,
                            const CffiTypeAndAttrs *typeAttrsP,
                            void *pointer, Tcl_WideInt *sysErrorP);
CffiResult CffiPointerToObj(Tcl_Interp *ip,
                            const CffiTypeAndAttrs *typeAttrsP,
                            void *pointer,
                            Tcl_Obj **resultObjP);
CffiResult CffiPointerFromObj(Tcl_Interp *ip,
                              const CffiTypeAndAttrs *typeAttrsP,
                              Tcl_Obj *pointerObj,
                              void **pointerP);
CffiResult
CffiGetEncodingFromObj(Tcl_Interp *ip, Tcl_Obj *encObj, Tcl_Encoding *encP);
CffiResult CffiExternalCharsToObj(Tcl_Interp *ip,
                                  const CffiTypeAndAttrs *typeAttrsP,
                                  const char *srcP,
                                  Tcl_Obj **resultObjP);
CffiResult CffiExternalDStringToObj(Tcl_Interp *ip,
                                   const CffiTypeAndAttrs *typeAttrsP,
                                   Tcl_DString *dsP,
                                   Tcl_Obj **resultObjP);
CffiResult CffiCheckNumeric(Tcl_Interp *ip,
                            const CffiTypeAndAttrs *typeAttrsP,

                            CffiValue *valueP, Tcl_WideInt *sysErrorP);
Tcl_WideInt CffiGrabSystemError(const CffiTypeAndAttrs *typeAttrsP,
                                Tcl_WideInt winError);
Tcl_Obj *CffiQualifyName(Tcl_Interp *ip, Tcl_Obj *nameObj);

int CffiAliasGet(CffiInterpCtx *ipCtxP,
                 Tcl_Obj *aliasNameObj,
                 CffiTypeAndAttrs *typeAttrP);
CffiResult
CffiAliasAdd(CffiInterpCtx *ipCtxP, Tcl_Obj *nameObj, Tcl_Obj *typedefObj);
CffiResult CffiAliasAddStr(CffiInterpCtx *ipCtxP,
                           const char *nameStr,
                           const char *typedefStr);
int CffiAddBuiltinAliases(CffiInterpCtx *ipCtxP, Tcl_Obj *objP);
void CffiAliasesCleanup(Tcl_HashTable *typeAliasTableP);

CffiResult CffiPrototypeParse(CffiInterpCtx *ipCtxP,
                              Tcl_Obj *fnNameObj,
                              Tcl_Obj *returnTypeObj,
                              Tcl_Obj *paramsObj,
                              CffiProto **protoPP);
void CffiProtoUnref(CffiProto *protoP);
void CffiPrototypesCleanup(Tcl_HashTable *protoTableP);
CffiProto *CffiProtoGet(CffiInterpCtx *ipCtxP, Tcl_Obj *protoNameObj);

void CffiLibCtxUnref(CffiLibCtx *ctxP);
void *CffiLibFindSymbol(Tcl_Interp *ip, CffiLoadHandle libH, Tcl_Obj *symbolObj);
CffiResult CffiLibLoad(Tcl_Interp *ip, Tcl_Obj *pathObj, CffiLibCtx **ctxPP);
Tcl_Obj *CffiLibPath(Tcl_Interp *ip, CffiLibCtx *ctxP);


void CffiEnumsCleanup(Tcl_HashTable *enumsTableP);
CffiResult CffiEnumFind(CffiInterpCtx *ipCtxP,
                        Tcl_Obj *enumObj,
                        Tcl_Obj *nameObj,
                        Tcl_Obj **valueObjP);
CffiResult CffiEnumFindReverse(CffiInterpCtx *ipCtxP,
                               Tcl_Obj *enumObj,
                               Tcl_WideInt needle,
                               int strict,
                               Tcl_Obj **nameObjP);
CffiResult CffiEnumBitmask(CffiInterpCtx *ipCtxP,
                           Tcl_Obj *enumObj,
                           Tcl_Obj *valueListObj,
                           Tcl_WideInt *maskP);

#ifdef CFFI_USE_DYNCALL

CFFI_INLINE CffiABIProtocol CffiDefaultABI() {
    return DC_CALL_C_DEFAULT;
}
CFFI_INLINE CffiABIProtocol CffiStdcallABI() {
#if defined(_WIN32) && !defined(_WIN64)
    return DC_CALL_C_X86_WIN32_STD;
#else
    return DC_CALL_C_DEFAULT;
#endif
}

#define CffiReloadArg CffiDyncallReloadArg
void CffiDyncallReloadArg(CffiCall *callP,
                        CffiArgument *argP,
                        CffiTypeAndAttrs *typeAttrsP);
CffiResult CffiDyncallResetCall(Tcl_Interp *ip, CffiCall *callP);
#define CffiResetCall CffiDyncallResetCall

#define DEFINEFN_(type_, name_, fn_) \
CFFI_INLINE type_ name_ (CffiCall *callP) { \
    return fn_ (callP->fnP->vmCtxP->vmP, callP->fnP->fnAddr); \
}
CFFI_INLINE void CffiCallVoidFunc (CffiCall *callP) {
    dcCallVoid(callP->fnP->vmCtxP->vmP, callP->fnP->fnAddr);
}

DEFINEFN_(signed char, CffiCallSCharFunc, dcCallInt)
DEFINEFN_(unsigned char, CffiCallUCharFunc, dcCallInt)
DEFINEFN_(short, CffiCallShortFunc, dcCallInt)
DEFINEFN_(unsigned short, CffiCallUShortFunc, dcCallInt)
DEFINEFN_(int, CffiCallIntFunc, dcCallInt)
DEFINEFN_(unsigned int, CffiCallUIntFunc, dcCallInt)
DEFINEFN_(long, CffiCallLongFunc, dcCallLong)
DEFINEFN_(unsigned long, CffiCallULongFunc, dcCallLong)
DEFINEFN_(long long, CffiCallLongLongFunc, dcCallLongLong)
DEFINEFN_(unsigned long long, CffiCallULongLongFunc, dcCallLongLong)
DEFINEFN_(float, CffiCallFloatFunc, dcCallFloat)
DEFINEFN_(double, CffiCallDoubleFunc, dcCallDouble)
DEFINEFN_(DCpointer, CffiCallPointerFunc, dcCallPointer)

#undef DEFINEFN_

#define STOREARGFN_(name_, type_, storefn_) \
CFFI_INLINE void CffiStoreArg ## name_ (CffiCall *callP, int ix, type_ val) \
{ \
    storefn_(callP->fnP->vmCtxP->vmP, val); \
}
STOREARGFN_(Pointer, void*, dcArgPointer)
STOREARGFN_(SChar, signed char, dcArgChar)
STOREARGFN_(UChar, unsigned char, dcArgChar)
STOREARGFN_(Short, short, dcArgShort)
STOREARGFN_(UShort, unsigned short, dcArgShort)
STOREARGFN_(Int, int, dcArgInt)
STOREARGFN_(UInt, unsigned int, dcArgInt)
STOREARGFN_(Long, long, dcArgLong)
STOREARGFN_(ULong, unsigned long, dcArgLong)
STOREARGFN_(LongLong, long long, dcArgLongLong)
STOREARGFN_(ULongLong, unsigned long long, dcArgLongLong)
STOREARGFN_(Float, float, dcArgFloat)
STOREARGFN_(Double, double, dcArgDouble)

#undef STOREARGFN_

#else

CffiResult CffiLibffiInitProtoCif(Tcl_Interp *ip, CffiProto *protoP);

CFFI_INLINE CffiABIProtocol CffiDefaultABI() {
    return FFI_DEFAULT_ABI;
}
CFFI_INLINE CffiABIProtocol CffiStdcallABI() {
#if defined(_WIN32) && !defined(_WIN64)
    return FFI_STDCALL;
#else
    return FFI_DEFAULT_ABI;
#endif
}
CFFI_INLINE void
CffiReloadArg(CffiCall *callP, CffiArgument *argP, CffiTypeAndAttrs *typeAttrsP)
{
    /* libcffi does not need reloadinf of args once loaded */
}

CFFI_INLINE CffiResult CffiResetCall(Tcl_Interp *ip, CffiCall *callP) {
    return TCL_OK; /* Libffi does not need to reset a call */
}

CFFI_INLINE void CffiLibffiCall(CffiCall *callP) {
    ffi_call(callP->fnP->protoP->cifP,
             callP->fnP->fnAddr,
             callP->retValueP,
             callP->argValuesPP);
}

CFFI_INLINE void CffiCallVoidFunc (CffiCall *callP) {
    CffiLibffiCall(callP);
}

#define DEFINEFN_(type_, name_, fld_)                \
    CFFI_INLINE type_ name_(CffiCall *callP)         \
    {                                                \
        CffiLibffiCall(callP);                       \
        if (sizeof(type_) <= sizeof(ffi_arg))        \
            return (type_)callP->retValue.u.ffi_val; \
        else                                         \
            return callP->retValue.u.fld_;           \
    }

DEFINEFN_(signed char, CffiCallSCharFunc, schar)
DEFINEFN_(unsigned char, CffiCallUCharFunc, uchar)
DEFINEFN_(short, CffiCallShortFunc, sshort)
DEFINEFN_(unsigned short, CffiCallUShortFunc, ushort)
DEFINEFN_(int, CffiCallIntFunc, sint)
DEFINEFN_(unsigned int, CffiCallUIntFunc, uint)
DEFINEFN_(long, CffiCallLongFunc, slong)
DEFINEFN_(unsigned long, CffiCallULongFunc, ulong)
DEFINEFN_(long long, CffiCallLongLongFunc, slonglong)
DEFINEFN_(unsigned long long, CffiCallULongLongFunc, ulonglong)
DEFINEFN_(void*, CffiCallPointerFunc, ptr)
CFFI_INLINE float CffiCallFloatFunc(CffiCall *callP) {
    CffiLibffiCall(callP);
    return callP->retValue.u.flt;
}
CFFI_INLINE double CffiCallDoubleFunc(CffiCall *callP) {
    CffiLibffiCall(callP);
    return callP->retValue.u.dbl;
}

#undef DEFINEFN_


#endif /* CFFI_USE_DYNCALL */

CffiResult
CffiFunctionSetupArgs(CffiCall *callP, int nArgObjs, Tcl_Obj *const *argObjs);
CffiResult CffiFunctionCall(ClientData cdata,
                            Tcl_Interp *ip,
                            int objArgIndex, /* Where in objv[] args start */
                            int objc,
                            Tcl_Obj *const objv[]);
CffiResult CffiFunctionInstanceCmd(ClientData cdata,
                                   Tcl_Interp *ip,
                                   int objc,
                                   Tcl_Obj *const objv[]);
void CffiFunctionCleanup(CffiFunction *fnP);
CffiResult
CffiDefineOneFunctionFromLib(Tcl_Interp *ip,
                             CffiLibCtx *libCtxP,
                             Tcl_Obj *nameObj,
                             Tcl_Obj *returnTypeObj,
                             Tcl_Obj *paramsObj,
                             CffiABIProtocol callMode);

Tcl_ObjCmdProc CffiAliasObjCmd;
Tcl_ObjCmdProc CffiEnumObjCmd;
Tcl_ObjCmdProc CffiWrapperObjCmd;
Tcl_ObjCmdProc CffiDyncallSymbolsObjCmd;
Tcl_ObjCmdProc CffiHelpObjCmd;
Tcl_ObjCmdProc CffiMemoryObjCmd;
Tcl_ObjCmdProc CffiPointerObjCmd;
Tcl_ObjCmdProc CffiPrototypeObjCmd;
Tcl_ObjCmdProc CffiStructObjCmd;
Tcl_ObjCmdProc CffiTypeObjCmd;

/* TBD - move these to Tclh headers */
typedef struct Tclh_SubCommand {
    const char *cmdName;
    int minargs;
    int maxargs;
    const char *message;
    int (*cmdFn)();
    int flags; /* Command specific usage */
} Tclh_SubCommand;

CffiResult Tclh_SubCommandNameToIndex(Tcl_Interp *ip,
                                      Tcl_Obj *nameObj,
                                      Tclh_SubCommand *cmdTableP,
                                      int *indexP);
CffiResult Tclh_SubCommandLookup(Tcl_Interp *ip,
                                 const Tclh_SubCommand *cmdTableP,
                                 int objc,
                                 Tcl_Obj *const objv[],
                                 int *indexP);
Tcl_Obj *Tclh_ObjHashEnumerateEntries(Tcl_HashTable *htP, Tcl_Obj *patObj);
void Tclh_ObjHashDeleteEntries(Tcl_HashTable *htP,
                               Tcl_Obj *patObj,
                               void (*deleteFn)(Tcl_HashEntry *));

#endif /* CFFIINT_H */
