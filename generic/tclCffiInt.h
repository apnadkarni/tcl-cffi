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
#include "dynload.h"
#include "dyncall.h"

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
    int               xcount;    /* 0->scalar
                                    <0 -> array of unknown size specified
                                          through countHolderObj
                                    >0 -> array of count base type elements */
    union {
        Tcl_Obj *tagObj;      /* TCL_F_TYPE_POINTER,ASTRING,CHAR_ARRAY. NOT used
                                 UNISTRING and UNICHAR_ARRAY */
        CffiStruct *structP; /* TCL_F_TYPE_STRUCT */
    } u;
    Tcl_Obj *countHolderObj; /* Holds the name of the slot (e.g. parameter name)
                                that contains the actual count at call time */
} CffiType;

/*
 * Function parameter descriptor
 */
typedef struct CffiTypeAndAttrs {
    Tcl_Obj *defaultObj;        /* Default for parameter, if there is one */
    CffiType dataType;         /* Data type */
    DCint callMode;             /* Only valid for return types */
    int flags;
#define CFFI_F_ATTR_IN    0x0001 /* In parameter */
#define CFFI_F_ATTR_OUT   0x0002 /* Out parameter */
#define CFFI_F_ATTR_INOUT 0x0004 /* Out parameter */
#define CFFI_F_ATTR_BYREF 0x0008 /* Parameter is a reference */

#define CFFI_F_ATTR_DISPOSE     0x0010 /* Unregister the pointer */
#define CFFI_F_ATTR_COUNTED     0x0020 /* Counted safe pointer */
#define CFFI_F_ATTR_UNSAFE      0x0040 /* Pointers need not be checked */

#define CFFI_F_ATTR_ZERO        0x0100 /* Must be zero/null */
#define CFFI_F_ATTR_NONZERO     0x0200 /* Must be nonzero/nonnull */
#define CFFI_F_ATTR_NONNEGATIVE 0x0400 /* Must be >= 0 */
#define CFFI_F_ATTR_POSITIVE    0x0800 /* Must be >= 0 */
#define CFFI_F_ATTR_LASTERROR 0x10000 /* Windows GetLastError for error message */
#define CFFI_F_ATTR_ERRNO     0x20000 /* Error in errno */
#define CFFI_F_ATTR_WINERROR  0x40000 /* Windows error code */

#define CFFI_F_ATTR_NULLIFEMPTY 0x100000 /* Empty value -> null pointer */
#define CFFI_F_ATTR_NOEXCEPT    0x200000 /* No exception on error */
#define CFFI_F_ATTR_STOREONERROR 0x400000 /* Store only on error */
#define CFFI_F_ATTR_STOREALWAYS  0x800000 /* Store on success and error */
} CffiTypeAndAttrs;

#define CFFI_F_ATTR_PARAM_MASK                                                \
    (CFFI_F_ATTR_IN | CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT | CFFI_F_ATTR_BYREF \
     | CFFI_F_ATTR_STOREONERROR | CFFI_F_ATTR_STOREALWAYS)
#define CFFI_F_ATTR_SAFETY_MASK \
    (CFFI_F_ATTR_UNSAFE | CFFI_F_ATTR_DISPOSE | CFFI_F_ATTR_COUNTED)
#define CFFI_F_ATTR_REQUIREMENT_MASK                                  \
    (CFFI_F_ATTR_ZERO | CFFI_F_ATTR_NONZERO | CFFI_F_ATTR_NONNEGATIVE \
     | CFFI_F_ATTR_POSITIVE)
#define CFFI_F_ATTR_ERROR_MASK \
    (CFFI_F_ATTR_LASTERROR | CFFI_F_ATTR_ERRNO | CFFI_F_ATTR_WINERROR)

/*
 * Union of value types supported by dyncall
 */
typedef struct CffiValue {
    union data {
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
    };
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
typedef struct CffiStruct CffiStruct;
typedef struct CffiField {
    Tcl_Obj *nameObj;        /* Field name */
    CffiTypeAndAttrs field; /* base type, cardinality, tag etc. */
    unsigned int offset;     /* Field offset from beginning of struct */
    unsigned int size;       /* Size of the field */
} CffiField;

struct CffiStruct {
    Tcl_Obj *name;              /* Struct type name */
    int nRefs;                  /* Shared, so need ref count */
    unsigned int size;          /* Size of struct */
    unsigned int alignment;     /* Alignment required for struct */
    int nFields;                /* Cardinality of fields[] */
    CffiField fields[1];       /* Actual size given by nFields */
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
    Tcl_HashTable aliases; /* typedef name -> CffiTypeAndAttrs */
    Tcl_HashTable prototypes; /* typedef name -> CffiTypeAndAttrs */
    MemLifo memlifo;        /* Software stack */
} CffiInterpCtx;

/* Context required for making calls to a function in a DLL. */
typedef struct CffiCallVmCtx {
    CffiInterpCtx *ipCtxP;
    DCCallVM *vmP; /* The dyncall call context to use */
} CffiCallVmCtx;

/* Context for dll commands. */
typedef struct CffiLibCtx {
    CffiCallVmCtx *vmCtxP;
    DLLib *dlP;    /* The dyncall library context */
} CffiLibCtx;

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
    CffiParam returnType;  /* Name and return type of function */
    CffiParam params[1];   /* Real size depends on nparams which
                               may even be 0!*/
} CffiProto;
CFFI_INLINE void CffiProtoRef(CffiProto *protoP) {
    protoP->nRefs += 1;
}

/* Function definition */
typedef struct CffiFunction {
    CffiCallVmCtx *vmCtxP; /* Context for the call */
    void *fnAddr;          /* Pointer to the function to call */
    CffiProto *protoP;     /* Prototype for the call */
} CffiFunction;

/* Used to store argument context when preparing to call a function */
typedef struct CffiArgument {
    CffiValue value; /* Native value being constructed. */
    Tcl_Obj *varNameObj; /* Name of output variable or NULL */
    int actualCount; /* For dynamic arrays, stores the actual size.
                        Always >= 0 (0 being scalar) */
    int flags;
#define CFFI_F_ARG_INITIALIZED 0x1
} CffiArgument;

/* Complete context for a call invocation */
typedef struct CffiCall {
    CffiFunction *fnP;     /* Function being called */
    CffiArgument *argsP;   /* Argument contexts */
    int nArgs; /* Size of argsP. This is one more than number of function
                  parameters as it includes the return value slot */
} CffiCall;

/*
 * Prototypes
 */
CffiResult CffiNameSyntaxCheck(Tcl_Interp *ip, Tcl_Obj *nameObj);
CffiResult CffiCallModeParse(Tcl_Interp *ip, Tcl_Obj *modeObj, DCint *modeP);

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
CffiResult CffiArgPrepare(CffiCall *callP, int arg_index, Tcl_Obj *valueObj);
CffiResult CffiArgPostProcess(CffiCall *callP, int arg_index);
void CffiArgCleanup(CffiCall *callP, int arg_index);
CffiResult CffiReturnPrepare(CffiCall *callP);
CffiResult CffiReturnCleanup(CffiCall *callP);
CffiResult
CffiStructResolve(Tcl_Interp *ip, const char *nameP, CffiStruct **structPP);
CffiResult CffiStructDescribeCmd(Tcl_Interp *ip,
                                 int objc,
                                 Tcl_Obj *const objv[],
                                 CffiStructCtx *structCtxP);
CffiResult CffiStructInfoCmd(Tcl_Interp *ip,
                             int objc,
                             Tcl_Obj *const objv[],
                             CffiStructCtx *structCtxP);
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
                            void *pointer);
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
                            CffiValue *valueP);
CffiResult CffiReportRequirementError(Tcl_Interp *ip,
                                      const CffiTypeAndAttrs *typeAttrsP,
                                      Tcl_WideInt value,
                                      Tcl_Obj *valueObj,
                                      Tcl_WideInt sysError,
                                      const char *message);
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

CffiResult CffiFunctionCall(ClientData cdata,
                            Tcl_Interp *ip,
                            int objArgIndex, /* Where in objv[] args start */
                            int objc,
                            Tcl_Obj *const objv[]);

Tcl_ObjCmdProc CffiAliasObjCmd;
Tcl_ObjCmdProc CffiDyncallLibraryObjCmd;
Tcl_ObjCmdProc CffiDyncallSymbolsObjCmd;
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

#endif /* CFFIINT_H */
