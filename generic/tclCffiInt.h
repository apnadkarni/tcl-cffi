#ifndef TCLCFFIINT_H
#define TCLCFFIINT_H

#ifdef _WIN32
#include <windows.h> /* Needed for the dyncall workaround referencing IMAGE_EXPORT_DIRECTORY etc  - TBD */
#endif

#include "tcl.h"
#include <sys/types.h>
#include <stddef.h>

#include "tclhBase.h"
#include "tclhObj.h"
#include "tclhPointer.h"
#include "tclhNamespace.h"
#include "tclhHash.h"
#include "tclhAtom.h"
#include "tclhCmd.h"
#include "tclhLifo.h"
#include "tclhEncoding.h"

/*
 * Which back end system are we using? Currently only two supported. At most
 * one must be defined with libffi as default.
 */
#if !(defined(CFFI_USE_DYNCALL) || defined(CFFI_USE_LIBFFI))
# define CFFI_USE_LIBFFI
#endif

#ifdef CFFI_USE_DYNCALL

# ifdef CFFI_USE_LIBFFI
#  error At most one of CFFI_USE_DYNCALL and CFFI_USE_LIBFFI must be defined.
# endif

# include "dynload.h"
# include "dyncall.h"

#endif /* CFFI_USE_DYNCALL */

#ifdef CFFI_USE_LIBFFI

/*
 * When statically linking libffi, we need to define FFI_BUILDING before
 * including ffi.h. This is done on the command
 * line depending on the build option chosen. However, the vcpkg distribution
 * of libffi hard codes FFI_BUILDING in the ffi.h header file as a patch
 * leading to "C4005" macro redefinition warnings. Thus we need to disable
 * the warning. (Note the standard libffi does NOT hardcode it so we do
 * need to define it ourselves)
 */

# ifdef _MSC_VER
#  pragma warning(push )
#  pragma warning(disable:4005 )
# endif
# include "ffi.h"
# ifdef _MSC_VER
#  pragma warning(default:4005 )
#  pragma warning(pop)
# endif

#endif /* CFFI_USE_LIBFFI */

#if defined(CFFI_USE_LIBFFI) && defined(FFI_CLOSURES)
#define CFFI_ENABLE_CALLBACKS
#endif

/*
 * Libffi does not have its own load. For consistency, always use Tcl_Load even for
 * dyncall.
 */
#define CFFI_USE_TCLLOAD


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

#define CFFI_K_MAX_NAME_LENGTH 511 /* Max length for various names */

/*
 * Base types - IMPORTANT!!! order must match cffiBaseTypes array
 */
typedef enum CffiBaseType {
    CFFI_K_TYPE_VOID,
    CFFI_K_FIRST_INTEGER_TYPE,
    CFFI_K_TYPE_SCHAR = CFFI_K_FIRST_INTEGER_TYPE,
    CFFI_K_TYPE_UCHAR,
    CFFI_K_TYPE_SHORT,
    CFFI_K_TYPE_USHORT,
    CFFI_K_TYPE_INT,
    CFFI_K_TYPE_UINT,
    CFFI_K_TYPE_LONG,
    CFFI_K_TYPE_ULONG,
    CFFI_K_TYPE_LONGLONG,
    CFFI_K_TYPE_ULONGLONG,
    CFFI_K_LAST_INTEGER_TYPE = CFFI_K_TYPE_ULONGLONG,
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
#ifdef _WIN32
    CFFI_K_TYPE_WINSTRING,
    CFFI_K_TYPE_WINCHAR_ARRAY,
#endif
    CFFI_K_NUM_TYPES
} CffiBaseType;
CFFI_INLINE int CffiTypeIsInteger(CffiBaseType type) {
    return (type >= CFFI_K_FIRST_INTEGER_TYPE
            && type <= CFFI_K_LAST_INTEGER_TYPE);
}


/*
 * Context types when parsing type definitions.
 */
typedef enum CffiTypeParseMode {
    CFFI_F_TYPE_PARSE_PARAM  = 1, /* Function parameter */
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

typedef enum CffiTypeFlags {
    CFFI_F_TYPE_VARSIZE = 1 /* Type is of variable size */
} CffiTypeFlags;

typedef struct CffiType {
    enum CffiBaseType baseType;
    int arraySize;           /* -1 -> scalar. TODO - make Tcl_Size
                                 0 -> array of unknown size specified
                                 through countHolderObj
                                 >0 -> array of count base type elements */
    union {
        Tcl_Obj *tagNameObj;   /* POINTER tag, Enum name */
        Tcl_Encoding encoding; /* ASTRING, CHAR_ARRAY */
        CffiStruct *structP;   /* STRUCT */
    } u;
    Tcl_Obj *countHolderObj; /* Holds the name of the slot (e.g. parameter name)
                                that contains the actual count at call time */
    int baseTypeSize;        /* size of baseType */
    CffiTypeFlags flags;
} CffiType;
CFFI_INLINE int CffiTypeIsArray(const CffiType *typeP) {
    return typeP->arraySize >= 0;
}
CFFI_INLINE int CffiTypeIsNotArray(const CffiType *typeP) {
    return ! CffiTypeIsArray(typeP);
}
CFFI_INLINE int CffiTypeIsVLA(const CffiType *typeP) {
    return typeP->arraySize == 0;
}
CFFI_INLINE int CffiTypeIsVariableSize(const CffiType *typeP) {
    return CffiTypeIsVLA(typeP)
        || (typeP->flags & CFFI_F_TYPE_VARSIZE);
}

typedef enum CffiAttrFlags {
    CFFI_F_ATTR_IN      = 0x00000001, /* In parameter */
    CFFI_F_ATTR_OUT     = 0x00000002, /* Out parameter */
    CFFI_F_ATTR_INOUT   = 0x00000004, /* Out parameter */
    CFFI_F_ATTR_BYREF   = 0x00000008, /* Parameter is a reference */
    CFFI_F_ATTR_DISPOSE = 0x00000010, /* Unregister the pointer */
    CFFI_F_ATTR_COUNTED = 0x00000020, /* Counted safe pointer */
    CFFI_F_ATTR_UNSAFE  = 0x00000040, /* Pointers need not be checked */
    CFFI_F_ATTR_DISPOSEONSUCCESS = 0x00000080, /* Unregister on success */
    CFFI_F_ATTR_ZERO             = 0x00000100, /* Must be zero/null */
    CFFI_F_ATTR_NONZERO          = 0x00000200, /* Must be nonzero/nonnull */
    CFFI_F_ATTR_NONNEGATIVE      = 0x00000400, /* Must be >= 0 */
    CFFI_F_ATTR_POSITIVE         = 0x00000800, /* Must be >= 0 */
    CFFI_F_ATTR_LASTERROR    = 0x00001000, /* Windows GetLastError handler */
    CFFI_F_ATTR_ERRNO        = 0x00002000, /* Error in errno */
    CFFI_F_ATTR_WINERROR     = 0x00004000, /* Windows error code */
    CFFI_F_ATTR_ONERROR      = 0x00008000, /* Error handler */
    CFFI_F_ATTR_STOREONERROR = 0x00010000, /* Store only on error */
    CFFI_F_ATTR_STOREALWAYS  = 0x00020000, /* Store on success and error */
    CFFI_F_ATTR_RETVAL       = 0x00040000, /* if param - treat as return value
                                              if return - a parameter is
                                              the return value */
    CFFI_F_ATTR_ENUM        = 0x00100000,  /* Use enum names */
    CFFI_F_ATTR_BITMASK     = 0x00200000,  /* Treat as a bitmask */
    CFFI_F_ATTR_NULLIFEMPTY = 0x00400000,  /* Empty -> null pointer */
    CFFI_F_ATTR_NULLOK      = 0x00800000,  /* Null pointers allowed */
    CFFI_F_ATTR_STRUCTSIZE  = 0x01000000,  /* Field contains struct size */
} CffiAttrFlags;

/*
 * Type descriptor
 */
typedef struct CffiTypeAndAttrs {
    Tcl_Obj *parseModeSpecificObj; /* Parameter - Default for parameter,
                                      Field - Default for field
                                      Return parse - error handler */
    CffiType dataType;         /* Data type */
    CffiAttrFlags flags;
} CffiTypeAndAttrs;

/* Attributes allowed on a parameter declaration */
#define CFFI_F_ATTR_PARAM_DIRECTION_MASK \
    (CFFI_F_ATTR_IN | CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT)

#define CFFI_F_ATTR_PARAM_MASK                            \
    (CFFI_F_ATTR_PARAM_DIRECTION_MASK | CFFI_F_ATTR_BYREF \
     | CFFI_F_ATTR_STOREONERROR | CFFI_F_ATTR_STOREALWAYS \
     | CFFI_F_ATTR_RETVAL)
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
#endif
#ifdef CFFI_USE_LIBFFI
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
#ifdef CFFI_USE_LIBFFI
    ffi_arg ffi_val;
#endif
    } u;
} CffiValue;

/*
 * C struct and struct field descriptors
 */
#ifdef CFFI_USE_LIBFFI
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

/* Struct: CffiField
 * Descriptor for fields within a struct definition.
 */
typedef struct CffiStruct CffiStruct;
typedef struct CffiField {
    Tcl_Obj *nameObj;           /* Field name */
    CffiTypeAndAttrs fieldType; /* base type, cardinality, tag etc. */
    unsigned int offset;        /* Field offset from beginning of struct */
    unsigned int size;          /* Size of the field */
} CffiField;

typedef enum CffiStructFlags {
    CFFI_F_STRUCT_CLEAR   = 0x0001,
    CFFI_F_STRUCT_VARSIZE = 0x0002,
} CffiStructFlags;

/* Struct: CffiStruct
 * Descriptor for a struct layout.
 *
 * Note this is not a fixed size structure. The *fields* array at the
 * end of the structure is variable sized.
 */
struct CffiStruct {
    Tcl_Obj *name;              /* Struct type name */
#ifdef CFFI_USE_LIBFFI
    CffiLibffiStruct *libffiTypes; /* Corresponding libffi type descriptors */
#endif
    int nRefs;                /* Shared, so need ref count */
    unsigned int size;        /* Fixed size of struct not including variable
                                 sized component if any. */
    unsigned short alignment; /* Alignment required for struct */
    CffiStructFlags flags;     /* Misc CFFI_F_STRUCT_ flags */
    int dynamicCountFieldIndex; /* Index into fields[] of field holding
                                   array size of variable-sized last field.
                                   -1 if not variable size */
    int nFields;              /* Cardinality of fields[] */
    CffiField fields[1];      /* Actual count given by nFields */
    /* !!!DO NOT ADD FIELDS HERE!!! */
};
CFFI_INLINE void CffiStructRef(CffiStruct *structP) {
    structP->nRefs += 1;
}
CFFI_INLINE int CffiStructIsVariableSize(const CffiStruct *structP) {
    return structP->dynamicCountFieldIndex >= 0
        || (structP->flags & CFFI_F_STRUCT_VARSIZE);
}

/* Struct: CffiScope
 * Contains scope-specific definitions.
 *
 * This is a remnant of the time there were multiple scopes. The abstraction
 * is maintained. Now there is a single scope and the names of program
 * elements themself include the scope prefix.
 */
typedef struct CffiScope {
    Tcl_HashTable aliases;  /* typedef name -> CffiTypeAndAttrs */
    Tcl_HashTable enums;    /* Enum -> (name->value table) */
    Tcl_HashTable prototypes; /* prototype name -> CffiProto */
} CffiScope;

/* Struct: CffiInterpCtx
 * Holds the CFFI related context for an interpreter.
 *
 * The structure is allocated when the extension is loaded into an
 * interpreter and deleted when the interpreter is deleted. It is not
 * reference counted because nothing should be referencing it once the
 * interpreter is itself deleted.
 */
typedef struct CffiInterpCtx {
    Tcl_Interp *interp;       /* The interpreter in which the DL is registered.
                                 Note this does not need to be protected against
                                 deletion as contexts are unregistered before
                                 interp deletion */
    CffiScope scope;
#ifdef CFFI_USE_LIBFFI
    Tcl_HashTable callbackClosures;   /* Maps FFI callback function pointers
                                         to CffiCallback */
#endif
#ifdef CFFI_USE_DYNCALL
    DCCallVM *vmP; /* The dyncall call context to use */
#endif
    Tclh_Lifo memlifo;        /* Software stack */
    Tclh_LibContext *tclhCtxP;
} CffiInterpCtx;

/* Context for dll commands. */
#ifdef CFFI_USE_TCLLOAD
typedef Tcl_LoadHandle CffiLoadHandle;
#else
typedef DLLib *CffiLoadHandle;
#endif

typedef struct CffiLibCtx {
    CffiInterpCtx *ipCtxP;
    CffiLoadHandle libH; /* The dyncall library context */
    Tcl_Obj *pathObj;    /* Path to the library. May be NULL */
    int nRefs; /* To ensure library not released with bound functions */
} CffiLibCtx;
CFFI_INLINE void CffiLibCtxRef(CffiLibCtx *libCtxP) {
    libCtxP->nRefs += 1;
}

/* Struct: CffiStrutCmdCtx
 * Holds the context for a *Struct* command.
 */
typedef struct CffiStructCmdCtx {
    CffiInterpCtx *ipCtxP;
    CffiStruct *structP;
} CffiStructCmdCtx;

/* Struct: CffiParam
 * Descriptor for a function parameter
 */
typedef struct CffiParam {
    Tcl_Obj *nameObj;
    CffiTypeAndAttrs typeAttrs;
    int arraySizeParamIndex; /* For dynamically sized arrays this holds
                                the index of the parameter holding
                                the array size. */
} CffiParam;

/* Struct: CffiProto
 * Descriptor for a function prototype including parameters and return
 * types. Note this is a variable size structure as the number of
 * parameters is variable.
 */
typedef struct CffiProto {
    int nRefs;            /* Reference count */
    int nParams;          /* Number of fixed params, sizeof params array */
    int flags;
#define CFFI_F_PROTO_VARARGS 0x1
    CffiABIProtocol abi;  /* cdecl, stdcall etc. */
    CffiParam returnType; /* Name and return type of function */
#ifdef CFFI_USE_LIBFFI
    ffi_cif *cifP; /* Descriptor used by cffi */
#endif
    CffiParam params[1]; /* Real size depends on nparams which
                             may even be 0!*/
    /* !!!DO NOT ADD FIELDS HERE AT END OF STRUCT!!! */
} CffiProto;
CFFI_INLINE void CffiProtoRef(CffiProto *protoP) {
    protoP->nRefs += 1;
}

/* Struct: CffiFunction
 * Descriptor for a callable function including its address, prototype
 * and other optional information
 */
typedef struct CffiFunction {
    CffiInterpCtx *ipCtxP; /* Interpreter context */
    void *fnAddr;          /* Pointer to the function to call */
    CffiProto *protoP;     /* Prototype for the call */
    CffiLibCtx *libCtxP;   /* Containing library for bound functions or
                              NULL for free standing functions */
    Tcl_Obj *cmdNameObj;   /* Name of Tcl command. May be NULL */
} CffiFunction;

/* Struct: CffiArgument
 * Storage for argument values for a function call.
 */
typedef struct CffiArgument {
    CffiValue value;      /* Native value being constructed. */
    CffiValue savedValue; /* Copy of above - needed after call in some cases
                             like disposable pointers. NOT used in all cases */
    Tcl_Obj *varNameObj;  /* Name of output variable or NULL */
    CffiTypeAndAttrs *typeAttrsP; /* Type of the argument. For a fixed param
                                     this will point to the type definition
                                     in the CffiProto structure. For varargs
                                     this will point to a volatile type def
                                     generated for that call. */
#ifdef CFFI_USE_LIBFFI
    void *valueP;         /* Always points to the value field. Used for libcffi
                             which needs an additional level of indirection for
                             byref parameters. Only set as needed in CffiPrepareArg */
#endif
    int arraySize;      /* For arrays, stores the actual size of an
                           array parameter, > 0 for arrays, < 0  for scalars.
                           Should never be 0. */
    int flags;
#define CFFI_F_ARG_INITIALIZED 0x1
} CffiArgument;

/* Struct: CffiCall
 * Complete context for a call invocation
 */
typedef struct CffiCall {
    CffiFunction *fnP;         /* Function being called */
#ifdef CFFI_USE_LIBFFI
    void **argValuesPP; /* Array of pointers into the actual value fields within
                           argsP[] elements */
    void *retValueP;    /* Points to storage to use for return value */
    CffiValue retValue; /* Holds return value */
#endif
    int nArgs;             /* Size of argsP. */
    CffiArgument *argsP;   /* Arguments */
} CffiCall;

#ifdef CFFI_ENABLE_CALLBACKS
/* Struct: CffiCallback
 * Contains context needed for processing callbacks.
 */
typedef struct CffiCallback {
    CffiInterpCtx *ipCtxP;
    CffiProto *protoP;
    Tcl_Obj *cmdObj;
    Tcl_Obj *errorResultObj;
#ifdef CFFI_USE_LIBFFI
    ffi_closure *ffiClosureP;
    void *ffiExecutableAddress;
#endif
    int depth;
} CffiCallback;
void CffiCallbackCleanupAndFree(CffiCallback *cbP);
#endif

/*
 * Common flags used across multiple functions.
 */
typedef enum CffiFlags {
    CFFI_F_ALLOW_UNSAFE        = 0x1, /* No pointer validity check */
    CFFI_F_PRESERVE_ON_ERROR   = 0x2, /* Preserve original content on error */
    CFFI_F_SKIP_ERROR_MESSAGES = 0x4, /* Don't store error in interp */
} CffiFlags;

/*
 * Prototypes
 */
CffiResult CffiNameSyntaxCheck(Tcl_Interp *ip, Tcl_Obj *nameObj);
CffiResult
CffiTagSyntaxCheck(Tcl_Interp *interp, const char *tagStr, Tcl_Size tagLen);

CffiResult CffiTypeParseArraySize(const char *lbStr, CffiType *typeP);
const CffiBaseTypeInfo *CffiBaseTypeInfoGet(Tcl_Interp *ip,
                                            Tcl_Obj *baseTypeObj);
CffiResult CffiTypeParse(CffiInterpCtx *ipCtxP,
                         Tcl_Obj *typeObj,
                         CffiType *typeP);
void CffiTypeCleanup(CffiType *);
int CffiGetCountFromNative (const void *valueP, CffiBaseType baseType);
int CffiTypeActualSize(const CffiType *typeP);
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
CffiResult CffiErrorVariableSizeStruct(Tcl_Interp *ip, CffiStruct *structP);
Tcl_Size CffiStructSize(CffiInterpCtx *ipCtxP,
                        CffiStruct *structP,
                        Tcl_Obj *structValueObj);
Tcl_Size CffiStructSizeVLACount(CffiInterpCtx *ipCtxP,
                                CffiStruct *structP,
                                Tcl_Size vlaCount);
CffiResult
CffiStructResolve(Tcl_Interp *ip, const char *nameP, CffiStruct **structPP);
CffiResult
CffiBytesFromObjSafe(Tcl_Interp *ip, Tcl_Obj *fromObj, unsigned char *toP, Tcl_Size toSize);
CffiResult CffiUniCharsFromObjSafe(Tcl_Interp *ip,
                               Tcl_Obj *fromObj,
                               Tcl_UniChar *toP,
                               Tcl_Size toSize);
#ifdef _WIN32
CffiResult CffiWinCharsFromObjSafe(CffiInterpCtx *ipCtxP,
                                   Tcl_Obj *fromObj,
                                   WCHAR *toP,
                                   Tcl_Size toSize);
#endif
CffiResult CffiCharsFromTclString(Tcl_Interp *ip,
                                  Tcl_Encoding enc,
                                  const char *fromP,
                                  Tcl_Size fromLen,
                                  char *toP,
                                  Tcl_Size toSize);
CffiResult CffiCharsFromObj(
    Tcl_Interp *ip, Tcl_Encoding enc, Tcl_Obj *fromObj, char *toP, Tcl_Size toSize);
CffiResult CffiCharsFromObjSafe(
    Tcl_Interp *ip, Tcl_Encoding enc, Tcl_Obj *fromObj, char *toP, Tcl_Size toSize);
CffiResult CffiCharsInMemlifoFromObj(Tcl_Interp *ip,
                                     Tcl_Encoding enc,
                                     Tcl_Obj *fromObj,
                                     Tclh_Lifo *memlifoP,
                                     char **outPP);
CffiResult CffiCharsToObj(Tcl_Interp *ip,
                          const CffiTypeAndAttrs *typeAttrsP,
                          const char *srcP,
                          Tcl_Obj **resultObjP);
CffiResult CffiUniStringToObj(Tcl_Interp *ip,
                              const CffiTypeAndAttrs *typeAttrsP,
                              Tcl_DString *dsP,
                              Tcl_Obj **resultObjP);
CffiResult CffiStructFromObj(CffiInterpCtx *ipCtxP,
                             const CffiStruct *structP,
                             Tcl_Obj *structValueObj,
                             CffiFlags flags,
                             void *resultP,
                             Tclh_Lifo *memlifoP);
CffiResult CffiStructToObj(CffiInterpCtx *ipCtxP,
                           const CffiStruct *structP,
                           void *valueP,
                           Tcl_Obj **valueObjP);
CffiResult
CffiStructObjDefault(CffiInterpCtx *ipCtxP, CffiStruct *structP, void *valueP);

CffiResult CffiNativeScalarFromObj(CffiInterpCtx *ipCtxP,
                                   const CffiTypeAndAttrs *typeAttrsP,
                                   Tcl_Obj *valueObj,
                                   CffiFlags flags,
                                   void *resultP,
                                   Tcl_Size indx,
                                   Tclh_Lifo *memlifoP);
CffiResult CffiNativeValueFromObj(CffiInterpCtx *ipCtxP,
                                  const CffiTypeAndAttrs *typeAttrsP,
                                  int realArraySize,
                                  Tcl_Obj *valueObj,
                                  CffiFlags flags,
                                  void *valueBaseP,
                                  int valueIndex,
                                  Tclh_Lifo *memlifoP);
CffiResult CffiNativeScalarToObj(CffiInterpCtx *ipCtxP,
                                 const CffiTypeAndAttrs *typeAttrsP,
                                 void *valueP,
                                 int indx,
                                 Tcl_Obj **valueObjP);
CffiResult CffiNativeValueToObj(CffiInterpCtx *ipCtxP,
                                const CffiTypeAndAttrs *typeAttrsP,
                                void *valueP,
                                int startIndex,
                                int count,
                                Tcl_Obj **valueObjP);
Tcl_Obj *CffiMakePointerTagFromObj(CffiInterpCtx *ipCtxP, Tcl_Obj *tagObj);
Tcl_Obj *
CffiMakePointerTag(CffiInterpCtx *, const char *tagP, Tcl_Size tagLen);
CffiResult CffiCheckPointer(Tcl_Interp *ip,
                            const CffiTypeAndAttrs *typeAttrsP,
                            void *pointer, Tcl_WideInt *sysErrorP);
CffiResult CffiPointerToObj(CffiInterpCtx *ipCtxP,
                            const CffiTypeAndAttrs *typeAttrsP,
                            void *pointer,
                            Tcl_Obj **resultObjP);
CffiResult CffiPointerFromObj(CffiInterpCtx *ipCtxP,
                              const CffiTypeAndAttrs *typeAttrsP,
                              Tcl_Obj *pointerObj,
                              void **pointerP);

CffiResult
CffiGetEncodingFromObj(Tcl_Interp *ip, Tcl_Obj *encObj, Tcl_Encoding *encP);
CffiResult CffiCheckNumeric(Tcl_Interp *ip,
                            const CffiTypeAndAttrs *typeAttrsP,

                            CffiValue *valueP, Tcl_WideInt *sysErrorP);
Tcl_WideInt CffiGrabSystemError(const CffiTypeAndAttrs *typeAttrsP,
                                Tcl_WideInt winError);
Tcl_Obj *CffiQualifyName(Tcl_Interp *ip, Tcl_Obj *nameObj);

int CffiAliasGet(CffiInterpCtx *ipCtxP,
                 Tcl_Obj *aliasNameObj,
                 CffiTypeAndAttrs *typeAttrP,
                 CffiFlags flags);
CffiResult CffiAliasAdd(CffiInterpCtx *ipCtxP,
                        Tcl_Obj *nameObj,
                        Tcl_Obj *typedefObj,
                        Tcl_Obj **fqnObjP);
CffiResult CffiAliasAddStr(CffiInterpCtx *ipCtxP,
                           const char *nameStr,
                           const char *typedefStr,
                           Tcl_Obj **fqnObjP);
int CffiAddBuiltinAliases(CffiInterpCtx *ipCtxP, Tcl_Obj *objP);
void CffiAliasesCleanup(CffiInterpCtx *ipCtxP);

CffiResult CffiPrototypeParse(CffiInterpCtx *ipCtxP,
                              CffiABIProtocol abi,
                              Tcl_Obj *fnNameObj,
                              Tcl_Obj *returnTypeObj,
                              Tcl_Obj *paramsObj,
                              CffiProto **protoPP);
void CffiProtoUnref(CffiProto *protoP);
void CffiPrototypesCleanup(CffiInterpCtx *ipCtxP);
CffiProto *
CffiProtoGet(CffiInterpCtx *ipCtxP, Tcl_Obj *protoNameObj);

void CffiLibCtxUnref(CffiLibCtx *ctxP);
void *CffiLibFindSymbol(Tcl_Interp *ip, CffiLoadHandle libH, Tcl_Obj *symbolObj);
CffiResult CffiLibLoad(Tcl_Interp *ip, Tcl_Obj *pathObj, CffiLibCtx **ctxPP);
Tcl_Obj *CffiLibPath(Tcl_Interp *ip, CffiLibCtx *ctxP);


CffiResult CffiEnumGetMap(CffiInterpCtx *ipCtxP,
                          Tcl_Obj *enumObj,
                          CffiFlags flags,
                          Tcl_Obj **mapObjP);
void CffiEnumsCleanup(CffiInterpCtx *ipCtxP);
CffiResult CffiEnumMemberFind(Tcl_Interp *ip,
                              Tcl_Obj *mapObj,
                              Tcl_Obj *memberNameObj,
                              Tcl_Obj **valueObjP);
CffiResult CffiEnumMemberFindReverse(Tcl_Interp *ip,
                                     Tcl_Obj *mapObj,
                                     Tcl_WideInt needle,
                                     Tcl_Obj **nameObjP);
CffiResult CffiEnumMemberBitmask(Tcl_Interp *ip,
                                 Tcl_Obj *enumObj,
                                 Tcl_Obj *valueListObj,
                                 Tcl_WideInt *maskP);
CffiResult CffiEnumMemberBitUnmask(Tcl_Interp *ip,
                                   Tcl_Obj *mapObj,
                                   Tcl_WideInt bitmask,
                                   Tcl_Obj **listObjP);
CffiResult CffiIntValueFromObj(Tcl_Interp *ip,
                               const CffiTypeAndAttrs *typeAttrsP,
                               Tcl_Obj *valueObj,
                               Tcl_WideInt *valueP);
Tcl_Obj *CffiIntValueToObj(const CffiTypeAndAttrs *typeAttrsP,
                           Tcl_WideInt value);

/* Scope prototypes */
CffiResult CffiScopesInit(CffiInterpCtx *ipCtxP);
void CffiScopesCleanup(CffiInterpCtx *ipCtxP);

/* Name management API */
void CffiNameTableInit(Tcl_HashTable *htP);
void CffiNameTableFinit(Tcl_Interp *ip,
                        Tcl_HashTable *htP,
                        void (*deleteFn)(ClientData));
CffiResult CffiNameLookup(Tcl_Interp *ip,
                          Tcl_HashTable *htP,
                          const char *nameP,
                          const char *nameTypeP,
                          CffiFlags flags,
                          ClientData *valueP,
                          Tcl_Obj **fqnObjP);
CffiResult CffiNameAdd(Tcl_Interp *ip,
                       Tcl_HashTable *htP,
                       const char *nameP,
                       const char *nameTypeP,
                       ClientData value,
                       Tcl_Obj **fqnObjP);
CffiResult CffiNameObjAdd(Tcl_Interp *ip,
                          Tcl_HashTable *htP,
                          Tcl_Obj *nameObj,
                          const char *nameTypeP,
                          ClientData value,
                          Tcl_Obj **fqnObjP);
CffiResult CffiNameListNames(Tcl_Interp *ip,
                             Tcl_HashTable *htP,
                             const char *pattern,
                             Tcl_Obj **namesObjP);
CffiResult CffiNameDeleteNames(Tcl_Interp *ip,
                               Tcl_HashTable *htP,
                               const char *pattern,
                               void (*deleteFn)(ClientData));

#ifdef CFFI_USE_DYNCALL

CffiResult CffiDyncallInit(CffiInterpCtx *ipCtxP);
void CffiDyncallFinit(CffiInterpCtx *ipCtxP);

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
    return fn_ (callP->fnP->ipCtxP->vmP, callP->fnP->fnAddr); \
}
CFFI_INLINE void CffiCallVoidFunc (CffiCall *callP) {
    dcCallVoid(callP->fnP->ipCtxP->vmP, callP->fnP->fnAddr);
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
    storefn_(callP->fnP->ipCtxP->vmP, val); \
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

#endif

#ifdef CFFI_USE_LIBFFI

CffiResult CffiLibffiInit(CffiInterpCtx *ipCtxP);
void CffiLibffiFinit(CffiInterpCtx *ipCtxP);

CffiResult CffiLibffiInitProtoCif(CffiInterpCtx *ipCtxP,
                                  CffiProto *protoP,
                                  int numVarArgs,
                                  Tcl_Obj * const *varArgObjs,
                                  CffiTypeAndAttrs *typeAttrsP);

# ifdef CFFI_ENABLE_CALLBACKS
void CffiLibffiCallback(ffi_cif *cifP, void *retP, void **args, void *userdata);
# endif

CFFI_INLINE CffiABIProtocol CffiDefaultABI() {
    return FFI_DEFAULT_ABI;
}
CFFI_INLINE CffiABIProtocol CffiStdcallABI() {
# if defined(_WIN32) && !defined(_WIN64)
    return FFI_STDCALL;
# else
    return FFI_DEFAULT_ABI;
# endif
}
CFFI_INLINE void
CffiReloadArg(CffiCall *callP, CffiArgument *argP, CffiTypeAndAttrs *typeAttrsP)
{
    /* libcffi does not need reloading of args once loaded */
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
//DEFINEFN_(void*, CffiCallPointerFunc, ptr)
CFFI_INLINE void* CffiCallPointerFunc(CffiCall *callP) {
    CffiLibffiCall(callP);
    return callP->retValue.u.ptr;
}
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

#ifdef CFFI_ENABLE_CALLBACKS
Tcl_ObjCmdProc CffiCallbackObjCmd;
#endif


#endif /* CFFIINT_H */
