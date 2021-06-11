/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#define TCLH_SHORTNAMES
#include "tclCffiInt.h"
#include <errno.h>

/*
 * Basic type meta information. The order *MUST* match the order in
 * cffiTypeId.
 */
#define CFFI_VALID_NUMERIC_ATTRS \
    (CFFI_F_ATTR_PARAM_MASK|CFFI_F_ATTR_REQUIREMENT_MASK|CFFI_F_ATTR_ERROR_MASK)

#define TOKENANDLEN(t) # t , sizeof(#t) - 1

typedef void (*CFFIARGPROC)();
const struct CffiBaseTypeInfo cffiBaseTypes[] = {
    {TOKENANDLEN(void), CFFI_K_TYPE_VOID, 0, 0},
    {TOKENANDLEN(schar),
     CFFI_K_TYPE_SCHAR,
     CFFI_VALID_NUMERIC_ATTRS,
     sizeof(signed char)},
    {TOKENANDLEN(uchar),
     CFFI_K_TYPE_UCHAR,
     CFFI_VALID_NUMERIC_ATTRS,
     sizeof(unsigned char)},
    {TOKENANDLEN(short),
     CFFI_K_TYPE_SHORT,
     CFFI_VALID_NUMERIC_ATTRS,
     sizeof(signed short)},
    {TOKENANDLEN(ushort),
     CFFI_K_TYPE_USHORT,
     CFFI_VALID_NUMERIC_ATTRS,
     sizeof(unsigned short)},
    {TOKENANDLEN(int),
     CFFI_K_TYPE_INT,
     CFFI_VALID_NUMERIC_ATTRS,
     sizeof(signed int)},
    {TOKENANDLEN(uint),
     CFFI_K_TYPE_UINT,
     CFFI_VALID_NUMERIC_ATTRS,
     sizeof(unsigned int)},
    {TOKENANDLEN(long),
     CFFI_K_TYPE_LONG,
     CFFI_VALID_NUMERIC_ATTRS,
     sizeof(signed long)},
    {TOKENANDLEN(ulong),
     CFFI_K_TYPE_ULONG,
     CFFI_VALID_NUMERIC_ATTRS,
     sizeof(unsigned long)},
    {TOKENANDLEN(longlong),
     CFFI_K_TYPE_LONGLONG,
     CFFI_VALID_NUMERIC_ATTRS,
     sizeof(signed long long)},
    {TOKENANDLEN(ulonglong),
     CFFI_K_TYPE_ULONGLONG,
     CFFI_VALID_NUMERIC_ATTRS,
     sizeof(unsigned long long)},
    {TOKENANDLEN(float),
     CFFI_K_TYPE_FLOAT,
     /* Note NUMERIC left out of float and double for now as the same error
        checks do not apply */
     CFFI_F_ATTR_PARAM_MASK,
     sizeof(float)},
    {TOKENANDLEN(double),
     CFFI_K_TYPE_DOUBLE,
     /* Note NUMERIC left out of float and double for now as the same error
        checks do not apply */
     CFFI_F_ATTR_PARAM_MASK,
     sizeof(double)},
    {TOKENANDLEN(struct), CFFI_K_TYPE_STRUCT, CFFI_F_ATTR_PARAM_MASK, 0},
    /* For pointer, only LASTERROR/ERRNO make sense for reporting errors */
    {TOKENANDLEN(pointer),
     CFFI_K_TYPE_POINTER,
     CFFI_F_ATTR_PARAM_MASK | CFFI_F_ATTR_SAFETY_MASK | CFFI_F_ATTR_NONZERO
         | CFFI_F_ATTR_LASTERROR | CFFI_F_ATTR_ERRNO,
     sizeof(void *)},
    {TOKENANDLEN(string), CFFI_K_TYPE_ASTRING, CFFI_F_ATTR_PARAM_MASK|CFFI_F_ATTR_NULLIFEMPTY, sizeof(void*)},
    {TOKENANDLEN(unistring), CFFI_K_TYPE_UNISTRING, CFFI_F_ATTR_PARAM_MASK|CFFI_F_ATTR_NULLIFEMPTY, sizeof(void*)},
    {TOKENANDLEN(binary), CFFI_K_TYPE_BINARY, CFFI_F_ATTR_PARAM_MASK, sizeof(unsigned char *)},
    {TOKENANDLEN(chars), CFFI_K_TYPE_CHAR_ARRAY, CFFI_F_ATTR_PARAM_MASK, sizeof(char)},
    {TOKENANDLEN(unichars), CFFI_K_TYPE_UNICHAR_ARRAY, CFFI_F_ATTR_PARAM_MASK, sizeof(Tcl_UniChar)},
    {TOKENANDLEN(bytes), CFFI_K_TYPE_BYTE_ARRAY, CFFI_F_ATTR_PARAM_MASK, sizeof(unsigned char)},
    {NULL}};

static struct {
    const char *modeStr;
    DCint mode;
} cffiCallModes[] = {{"default", DC_CALL_C_DEFAULT}, /* Assumed to be first! */
                      {"ellipsis", DC_CALL_C_ELLIPSIS},
                      {"ellipsis_varargs", DC_CALL_C_ELLIPSIS_VARARGS},
#if defined(_WIN32) && !defined(_WIN64)
                      {"stdcall", DC_CALL_C_X86_WIN32_STD},
                      {"x86_win32_std", DC_CALL_C_X86_WIN32_STD},
                      {"x86_win32_fast_ms", DC_CALL_C_X86_WIN32_FAST_MS},
                      {"fastcall", DC_CALL_C_X86_WIN32_FAST_MS},
                      {"x86_win32_fast_gnu", DC_CALL_C_X86_WIN32_FAST_GNU},
                      {"x86_win32_this_ms", DC_CALL_C_X86_WIN32_THIS_MS},
                      {"x86_win32_this_gnu", DC_CALL_C_X86_WIN32_THIS_GNU},
#else
                      {"stdcall", DC_CALL_C_DEFAULT},
                      {"fastcall", DC_CALL_C_DEFAULT},
#if !defined(_WIN64)
                      {"syscall", DC_CALL_SYS_DEFAULT},
#endif
#endif


#ifdef TBD /* Not really needed because default suffices? */
                      {"x64_win64", DC_CALL_C_X64_WIN64},
                      {"x86_cdecl", DC_CALL_C_X86_CDECL},
                      {"x86_plan9", DC_CALL_C_X86_PLAN9},
                      {"x64_sysv", DC_CALL_C_X64_SYSV},
                      {"ppc32_darwin", DC_CALL_C_PPC32_DARWIN},
                      {"ppc32_osx", DC_CALL_C_PPC32_OSX},
                      {"ppc32_sysv", DC_CALL_C_PPC32_SYSV},
                      {"ppc32_linux", DC_CALL_C_PPC32_LINUX},
                      {"ppc64", DC_CALL_C_PPC64},
                      {"ppc64_linux", DC_CALL_C_PPC64_LINUX},
                      {"arm_arm", DC_CALL_C_ARM_ARM},
                      {"arm_thumb", DC_CALL_C_ARM_THUMB},
                      {"arm_arm_EABI", DC_CALL_C_ARM_ARM_EABI},
                      {"arm_thumb_EABI", DC_CALL_C_ARM_THUMB_EABI},
                      {"arm_armhf", DC_CALL_C_ARM_ARMHF},
                      {"arm64", DC_CALL_C_ARM64},
                      {"mips32_eabi", DC_CALL_C_MIPS32_EABI},
                      {"mips32_pspsdk", DC_CALL_C_MIPS32_PSPSDK},
                      {"mips32_o32", DC_CALL_C_MIPS32_O32},
                      {"mips64_n64", DC_CALL_C_MIPS64_N64},
                      {"mips64_n32", DC_CALL_C_MIPS64_N32},
                      {"sparc32", DC_CALL_C_SPARC32},
                      {"sparc64", DC_CALL_C_SPARC64},
                      {"sys_x86_int80h_BSD", DC_CALL_SYS_X86_INT80H_BSD},
                      {"sys_x86_int80h_LINUX", DC_CALL_SYS_X86_INT80H_LINUX},
                      {"sys_x64_syscall_SYSV", DC_CALL_SYS_X64_SYSCALL_SYSV},
                      {"sys_ppc32", DC_CALL_SYS_PPC32},
                      {"sys_ppc64", DC_CALL_SYS_PPC64},
#endif /* TBD */
                      {NULL, 0}};

enum cffiTypeAttrOpt {
    PARAM_IN,
    PARAM_OUT,
    PARAM_INOUT,
    BYREF,
#ifdef OBSOLETE
    SAFE,
#endif
    COUNTED,
    UNSAFE,
    DISPOSE,
    ZERO,
    NONZERO,
    NONNEGATIVE,
    POSITIVE,
    LASTERROR,
    ERRNO,
    WINERROR,
    DEFAULT,
    NULLIFEMPTY
};
typedef struct CffiAttrs {
    const char *attrName; /* Token */
    enum cffiTypeAttrOpt attr;    /* Integral id. Cannot use attrFlag for this since
                             some tokens are not attributes */
    int attrFlag;    /* Corresponding CFFI_F_ATTR flag / -1 if not attribute */
    char parseModes; /* Parse modes in which the attribute is valid */
    char nAttrArgs;  /* Attribute arguments including itself */
} CffiAttrs;
static CffiAttrs cffiAttrs[] = {
    {"in", PARAM_IN, CFFI_F_ATTR_IN, CFFI_F_TYPE_PARSE_PARAM, 1},
    {"out", PARAM_OUT, CFFI_F_ATTR_OUT, CFFI_F_TYPE_PARSE_PARAM, 1},
    {"inout", PARAM_INOUT, CFFI_F_ATTR_INOUT, CFFI_F_TYPE_PARSE_PARAM, 1},
    {"byref", BYREF, CFFI_F_ATTR_BYREF, CFFI_F_TYPE_PARSE_PARAM, 1},
#ifdef OBSOLETE
    {"safe",
     SAFE,
     CFFI_F_ATTR_COUNTED, /* Not really, but same validity */
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_RETURN
         | CFFI_F_TYPE_PARSE_FIELD,
     1},
#endif
    {"counted",
     COUNTED,
     CFFI_F_ATTR_COUNTED,
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_RETURN
         | CFFI_F_TYPE_PARSE_FIELD,
     1},
    {"unsafe",
     UNSAFE,
     CFFI_F_ATTR_UNSAFE,
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_RETURN
         | CFFI_F_TYPE_PARSE_FIELD,
     1},
    {"dispose", DISPOSE, CFFI_F_ATTR_DISPOSE, CFFI_F_TYPE_PARSE_PARAM, 1},
    {"zero", ZERO, CFFI_F_ATTR_ZERO, CFFI_F_TYPE_PARSE_RETURN, 1},
    {"nonzero", NONZERO, CFFI_F_ATTR_NONZERO, CFFI_F_TYPE_PARSE_RETURN, 1},
    {"nonnegative",
     NONNEGATIVE,
     CFFI_F_ATTR_NONNEGATIVE,
     CFFI_F_TYPE_PARSE_RETURN,
     1},
    {"positive", POSITIVE, CFFI_F_ATTR_POSITIVE, CFFI_F_TYPE_PARSE_RETURN, 1},
    {"errno", ERRNO, CFFI_F_ATTR_ERRNO, CFFI_F_TYPE_PARSE_RETURN, 1},
#ifdef _WIN32
    {"lasterror",
     LASTERROR,
     CFFI_F_ATTR_LASTERROR,
     CFFI_F_TYPE_PARSE_RETURN,
     1},
    {"winerror", WINERROR, CFFI_F_ATTR_WINERROR, CFFI_F_TYPE_PARSE_RETURN, 1},
#endif
    {"default", DEFAULT, -1, CFFI_F_TYPE_PARSE_PARAM, 2},
    {"nullifempty",
     NULLIFEMPTY,
     CFFI_F_ATTR_NULLIFEMPTY,
     CFFI_F_TYPE_PARSE_PARAM,
     1},
    {NULL}
};


/* Function: CffiBaseTypeInfoGet
 * Returns pointer to type information for a basic type.
 *
 * Parameters:
 * ip - interpreter
 * baseTypeObj - base type token
 *
 * Returns:
 * Pointer to the type information structure or *NULL* on error with error
 * message in interpreter.
 */
const CffiBaseTypeInfo *
CffiBaseTypeInfoGet(Tcl_Interp *ip, Tcl_Obj *baseTypeObj)
{
    int baseTypeIndex;
    if (Tcl_GetIndexFromObjStruct(ip,
                                  baseTypeObj,
                                  cffiBaseTypes,
                                  sizeof(cffiBaseTypes[0]),
                                  "base type",
                                  TCL_EXACT,
                                  &baseTypeIndex)
        == TCL_OK) {
        return &cffiBaseTypes[baseTypeIndex];
    }
    else
        return NULL;
}

CffiResult
CffiCallModeParse(Tcl_Interp *ip, Tcl_Obj *modeObj, DCint *modeP)
{
    int modeIndex;

    CHECK(Tcl_GetIndexFromObjStruct(ip,
                                    modeObj,
                                    cffiCallModes,
                                    sizeof(cffiCallModes[0]),
                                    "callmode",
                                    TCL_EXACT,
                                    &modeIndex));
    *modeP = cffiCallModes[modeIndex].mode;
    return TCL_OK;
}

int Tclh_PointerTagMatch(Tclh_PointerTypeTag pointer_tag, Tclh_PointerTypeTag expected_tag)
{
    /*
     * Note Tclh_Pointer library already checks whether pointer_tag == expected_tag so
     * no need for that optimization here.
     */
    if (expected_tag == NULL)
        return 1;               /* Anything can be a void pointer */

    if (pointer_tag == NULL)
        return 0;               /* But not the other way */

    return !strcmp(Tcl_GetString(pointer_tag), Tcl_GetString(expected_tag));
}

/* Function: CffiTypeInit
 * Initializes a CffiType structure
 *
 * Parameters:
 * toP - pointer to structure to initialize
 * fromP - if not NULL, values to use for initialization.
 */
void CffiTypeInit(CffiType *toP, CffiType *fromP)
{
    if (fromP) {
        toP->baseType = fromP->baseType;
        toP->count = fromP->count;
        switch (fromP->baseType) {
        case CFFI_K_TYPE_POINTER:
        case CFFI_K_TYPE_ASTRING:
        case CFFI_K_TYPE_CHAR_ARRAY:
            if (fromP->u.tagObj)
                Tcl_IncrRefCount(fromP->u.tagObj);
            toP->u.tagObj = fromP->u.tagObj;
            break;
        case CFFI_K_TYPE_STRUCT:
            if (fromP->u.structP)
                CffiStructRef(fromP->u.structP);
            toP->u.structP = fromP->u.structP;
            break;
        case CFFI_K_TYPE_UNISTRING:
        case CFFI_K_TYPE_UNICHAR_ARRAY:
            /* These always use Tcl_GetUnicode etc., no need for u.tagObj */
            /* FALLTHRU */
        default:
            break;
        }
    }
    else {
        memset(toP, 0, sizeof(*toP));
        toP->baseType = CFFI_K_TYPE_VOID;
    }
}

/* Function: CffiTypeParse
 * Parses a type definition into an internal form.
 *
 * A type definition is a list of the form
 *    BASETYPE ?TAG? ?COUNT?
 * where *COUNT* defaults to 0 indicating a scalar. *TAG* is valid only
 * for types *struct*, *pointer* and *string* and mandatory for them.
 * It specifies the structure type, pointer target type and string
 * encoding respectively for those types.
 *
 * Depending on the data type, references to Tcl_Obj's may be stored in the
 * returned structure. Caller should at some point call <CffiTypeCleanup> to
 * free the same.
 *
 * Parameters:
 *   ip - Current interpreter
 *   typeObj - Contains the type definition to be parsed
 *   typeP - pointer to structure to hold the parsed type
 *
 * Returns:
 *  TCL_OK on success else TCL_ERROR with message in interpreter.
 */
CffiResult
CffiTypeParse(Tcl_Interp *ip, Tcl_Obj *typeObj, CffiType *typeP)
{
    int tokenLen;
    int tagLen;
    const char *message;
    const char *typeStr;
    const char *tagStr;
    const char *countStr;
    const CffiBaseTypeInfo *baseTypeInfoP;
    CffiBaseType baseType;
    CffiResult ret;

    CFFI_ASSERT((sizeof(cffiBaseTypes)/sizeof(cffiBaseTypes[0]) - 1) == CFFI_K_NUM_TYPES);

    CffiTypeInit(typeP, NULL);
    CFFI_ASSERT(typeP->count == 0); /* Code below assumes these values on init */
    CFFI_ASSERT(typeP->u.tagObj == NULL);
    CFFI_ASSERT(typeP->u.structP == NULL);

    typeStr = Tcl_GetString(typeObj);
    for (baseTypeInfoP = &cffiBaseTypes[0]; baseTypeInfoP->token;
         ++baseTypeInfoP) {
        if (!strncmp(baseTypeInfoP->token, typeStr, baseTypeInfoP->token_len)) {
            /* Even on match, check it is not prefix match of longer type! */
            char ch = typeStr[baseTypeInfoP->token_len];
            if (ch == '\0' || ch == '.' || ch == '[')
                break;
        }
    }
    if (baseTypeInfoP->token == NULL) {
        message = "Invalid base type.";
        goto invalid_type;
    }

    baseType  = baseTypeInfoP->baseType;
    tokenLen = baseTypeInfoP->token_len;

    if (typeStr[tokenLen] == '\0') {
        /* TYPE */
        tagStr   = NULL;
        countStr = NULL;
        tagLen   = 0;
    }
    else if (typeStr[tokenLen] == '[') {
        /* TYPE[5] */
        tagStr   = NULL;
        countStr = typeStr + tokenLen;
        tagLen   = 0;
    }
    else if (typeStr[tokenLen] == '.') {
        /* TYPE.TAG */
        tagStr = typeStr + tokenLen + 1;
        /* Note the isascii also takes care of not permitting '[' */
        if (*tagStr == '\0' || !isascii((unsigned char)*tagStr)) {
            message = "Missing or invalid encoding or tag.";
            goto invalid_type;
        }
        countStr = strchr(tagStr, '[');
        if (countStr)
            tagLen  = (int)(countStr - tagStr); /* TYPE.TAG[N] */
        else
            tagLen = Tclh_strlen(tagStr); /* TYPE.TAG */
    }
    else {
        message = "Invalid base type";
        goto invalid_type;
    }

    switch (baseType) {
    case CFFI_K_TYPE_STRUCT:
        if (tagLen == 0) {
            message = "Missing struct name.";
            goto invalid_type;
        }
        if (countStr) {
            /* Tag is not null terminated. We cannot modify that string */
            char *structNameP = ckalloc(tagLen);
            memmove(structNameP, tagStr, tagLen);
            structNameP[tagLen] = '\0';
            ret = CffiStructResolve(ip, structNameP, &typeP->u.structP);
            ckfree(structNameP);
        }
        else {
            ret = CffiStructResolve(ip, tagStr, &typeP->u.structP);
        }
        if (ret != TCL_OK)
            goto error_return;
        CffiStructRef(typeP->u.structP);
        typeP->baseType = CFFI_K_TYPE_STRUCT;
        break;

    case CFFI_K_TYPE_POINTER:
        if (tagStr != NULL &&
            (tagLen != 4 || strncmp(tagStr, "void", 4))) {
                typeP->u.tagObj = Tcl_NewStringObj(tagStr, tagLen);
                Tcl_IncrRefCount(typeP->u.tagObj);
        }
        typeP->baseType = CFFI_K_TYPE_POINTER;
        break;

    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_CHAR_ARRAY:
        typeP->baseType = baseType; /* So type->u.tagObj freed on error */
        if (tagLen) {
            Tcl_Encoding encoding;
            /* Verify the encoding exists */
            typeP->u.tagObj = Tcl_NewStringObj(tagStr, tagLen);
            Tcl_IncrRefCount(typeP->u.tagObj);
            ret = Tcl_GetEncodingFromObj(ip, typeP->u.tagObj, &encoding);
            if (ret != TCL_OK)
                goto error_return;
            Tcl_FreeEncoding(encoding);
        }
        break;

    default:
        if (tagLen) {
            message = "Tags are not permitted for this base type.";
            goto invalid_type;
        }
        typeP->baseType = baseType;
        break;
    }

    CFFI_ASSERT(typeP->count == 0);/* Should already be default init-ed */
    if (countStr) {
        /* An array element count is specified */
        char ch;
        char rightbracket;
        int count;
        int nfields;
        if ((nfields = sscanf(countStr+1, "%d%c%c", &count, &rightbracket, &ch)) != 2 || count <= 0) {
            message = "Invalid array size or extra trailing characters.";
            goto invalid_type;
        }
        typeP->count = count;
    }

    /*
     * chars, unichars and bytes must have the count specified.
     * pointers, astrings, unistrings and bytes - arrays not implemented yet - TBD.
     */
    switch (typeP->baseType) {
    case CFFI_K_TYPE_VOID:
    case CFFI_K_TYPE_POINTER:
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
    case CFFI_K_TYPE_BINARY:
    case CFFI_K_TYPE_STRUCT:
        if (typeP->count > 0) {
            message = "The specified type is invalid or unsupported for array "
                      "declarations.";
            goto invalid_type;
        }
        break;
    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
        if (typeP->count == 0) {
            message = "Declarations of type chars, unichars and bytes must be arrays.";
            goto invalid_type;
        }
    default:
        break;
    }

    return TCL_OK;

invalid_type:
    (void) Tclh_ErrorInvalidValue(ip, typeObj, message);
error_return:
    CffiTypeCleanup(typeP);
    return TCL_ERROR;
}

/* Function: CffiTypeCleanup
 * Cleans up a previously initialized CffiType structure.
 *
 * The structure must have been previously initialized with <CffiTypeParse>.
 *
 * Parameters:
 *    typeP - pointer to the structure to clean up
 *
 * Returns:
 *    Nothing.
 */
void CffiTypeCleanup (CffiType *typeP)
{
    switch (typeP->baseType) {
    case CFFI_K_TYPE_POINTER:
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_CHAR_ARRAY:
        if (typeP->u.tagObj) {
            Tcl_DecrRefCount(typeP->u.tagObj);
            typeP->u.tagObj = NULL;
        }
        break;
    case CFFI_K_TYPE_STRUCT:
        if (typeP->u.structP) {
            CffiStructUnref(typeP->u.structP);
            typeP->u.structP = NULL;
        }
        break;
    default:
        break;
    }
    typeP->baseType = CFFI_K_TYPE_VOID;
}

/* Function: CffiTypeLayoutInfo
 * Returns size and alignment information for a type.
 *
 * The size information for base scalar types is simply the C size.
 * For base type *string* and *bytes*, the type is treated as a pointer
 * for sizing purposes.
 *
 * Parameters:
 *   typeP - pointer to the CffiType
 *   baseSizeP - if non-NULL, location to store base size of the type.
 *   sizeP - if non-NULL, location to store size of the type. For scalars,
 *           the size and base size are the same. For arrays, the former is
 *           the base size multiplied by the number of elements in the array
 *   alignP - if non-NULL, location to store the alignment for the type
 */
void
CffiTypeLayoutInfo(const CffiType *typeP,
                    int *baseSizeP,
                    int *sizeP,
                    int *alignP)
{
    int baseSize;
    int alignment;
    CffiBaseType baseType;

    baseType = typeP->baseType;
    baseSize = cffiBaseTypes[baseType].size;
    alignment = baseSize;
    if (baseSize == 0) {
        switch (baseType) {
        case CFFI_K_TYPE_STRUCT:
            baseSize  = typeP->u.structP->size;
            alignment = typeP->u.structP->alignment;
            break;
        case CFFI_K_TYPE_VOID:
            baseSize  = 0;
            alignment = 0;
            break;
        default:
            TCLH_PANIC("Unexpected 0 size type %d", baseType);
            break;
        }
    }
    if (baseSizeP)
        *baseSizeP = baseSize;
    if (alignP)
        *alignP = alignment;
    if (sizeP)
        *sizeP = typeP->count > 1 ? typeP->count * baseSize : baseSize;
}

/* Function: CffiTypeAndAttrsInit
 * Initializes a CffiTypeAndAttrs structure.
 *
 * Parameters:
 * toP - pointer to structure to initialize. Contents are overwritten.
 * fromP - if non-NULL, the structure to use for initialization.
 */
void
CffiTypeAndAttrsInit(CffiTypeAndAttrs *toP, CffiTypeAndAttrs *fromP)
{
    if (fromP) {
        if (fromP->defaultObj)
            Tcl_IncrRefCount(fromP->defaultObj);
        toP->defaultObj  = fromP->defaultObj;
        toP->callMode = fromP->callMode;
        toP->flags       = fromP->flags;
        CffiTypeInit(&toP->dataType, &fromP->dataType);
    }
    else {
        toP->defaultObj  = NULL;
        toP->callMode = DC_CALL_C_DEFAULT;
        toP->flags       = 0;
        CffiTypeInit(&toP->dataType, NULL);
    }
}

/* Function: CffiTypeAndAttrsParse
 * Parses a type and attribute definition into an internal form.
 *
 * A definition is a list of the form
 *   TYPE ?attr ...?
 * where *TYPE* is a type definition as parsed by <CffiTypeParse>,
 * and *ATTR* is one of the following:
 *
 * byref - indicates parameter passed by reference
 * byval - indicates parameter passed by value
 * in - indicates an input parameter
 * out - indicates and output parameter
 * inout - parameter is used for both input and output
 * safe - indicates pointer values must be checked
 * unsafe - indicates pointer values are not checked
 * dispose - indicates that the pointer should be unregistered
 * nonnull - indicates that the pointer must not be NULL
 * default DEFAULT - specifies a default value
 *
 * Parameters:
 *   ip - Interpreter
 *   nameObj - name of the function/structure/field to which this type pertains
 *   typeAttrObj - parameter definition object
 *   parseMode - indicates whether the definition should accept attributes
 *               for function parameter, return type or struct field. Caller
 *               may specify multiple modes bu OR-ing the flags.
 *   typeAttrP - pointer to structure to hold parsed information.
 *
 * Returns:
 * TCL_OK on success, else TCL_ERROR with an error message in the interpreter.
 */
CffiResult
CffiTypeAndAttrsParse(CffiInterpCtx *ipCtxP,
                       Tcl_Obj *typeAttrObj,
                       CffiTypeParseMode parseMode,
                       CffiTypeAndAttrs *typeAttrP)
{
    Tcl_Interp *ip = ipCtxP->interp;
    Tcl_Obj **objs;
    int nobjs;
    int flags;
    int i;
    enum CffiBaseType baseType;
    int validAttrs;
    int found;
    const char *message = "Invalid or conflicting modifiers in parameter definition.";

    CHECK( Tcl_ListObjGetElements(ip, typeAttrObj, &nobjs, &objs) );

    if (nobjs == 0) {
        return Tclh_ErrorInvalidValue(ip, typeAttrObj, "Empty type declaration.");
    }

    /* First check for a type definition before base types */
    found = CffiAliasGet(ipCtxP, objs[0], typeAttrP);
    if (found)
        baseType = typeAttrP->dataType.baseType;
    else {
        /* Get base type */
        CHECK(CffiTypeParse(ip, objs[0], &typeAttrP->dataType));
        baseType = typeAttrP->dataType.baseType;
        typeAttrP->defaultObj = NULL;
        typeAttrP->flags      = 0;
    }

    flags = typeAttrP->flags; /* May be set in CffiAliasGet */

    /* Flags that determine valid attributes for this type */
    validAttrs = cffiBaseTypes[baseType].validAttrFlags;

    /* Parse optional attributes */
    for (i = 1; i < nobjs; ++i) {
        Tcl_Obj **fieldObjs;
        int       attrIndex;
        int       nFields;

        if (Tcl_ListObjGetElements(ip, objs[i], &nFields, &fieldObjs) !=
                TCL_OK ||
            nFields                                                   == 0)  {
            goto invalid_format;
        }
        if (parseMode & CFFI_F_TYPE_PARSE_RETURN) {
            if (CffiCallModeParse(NULL, fieldObjs[0], &typeAttrP->callMode) == TCL_OK)
                continue;
        }
        if (Tcl_GetIndexFromObjStruct(NULL,
                                      fieldObjs[0],
                                      cffiAttrs,
                                      sizeof(cffiAttrs[0]),
                                      "type attribute",
                                      TCL_EXACT,
                                      &attrIndex) !=
                TCL_OK) {
            message = "Unrecognized attribute in type declaration.";
            goto invalid_format;
        }
        if (nFields != cffiAttrs[attrIndex].nAttrArgs) {
            message = "An attribute in the type declaration has the wrong number of fields.";
            goto invalid_format;
        }
        if ((cffiAttrs[attrIndex].attrFlag & validAttrs) == 0) {
            message = "An attribute in the type declaration is not valid for the data type.";
            goto invalid_format;
        }
        switch (cffiAttrs[attrIndex].attr) {
        case PARAM_IN:
            if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_OUT|CFFI_F_ATTR_INOUT))
                goto conflict;
            flags |= CFFI_F_ATTR_IN;
            break;
        case PARAM_OUT:
            if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_OUT|CFFI_F_ATTR_INOUT))
                goto conflict;
            flags |= CFFI_F_ATTR_OUT;
            break;
        case PARAM_INOUT:
            if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_OUT|CFFI_F_ATTR_INOUT))
                goto conflict;
            flags |= CFFI_F_ATTR_INOUT;
            break;
        case BYREF:
            flags |= CFFI_F_ATTR_BYREF;
            break;
        case DEFAULT:
            if (typeAttrP->defaultObj)
                goto conflict;
            Tcl_IncrRefCount(fieldObjs[1]);
            typeAttrP->defaultObj = fieldObjs[1];
            break;
#ifdef OBSOLETE
        case SAFE: flags &= ~CFFI_F_ATTR_UNSAFE; break;
#endif
        case COUNTED:
            if (flags & CFFI_F_ATTR_UNSAFE)
                goto conflict;
            flags |= CFFI_F_ATTR_COUNTED;
            break;
        case UNSAFE:
            if (flags & (CFFI_F_ATTR_COUNTED | CFFI_F_ATTR_DISPOSE))
                goto conflict;
            flags |= CFFI_F_ATTR_UNSAFE;
            break;
        case DISPOSE:
            if (flags & CFFI_F_ATTR_UNSAFE)
                goto conflict;
            flags |= CFFI_F_ATTR_DISPOSE;
            break;
        case ZERO:
            if (flags & CFFI_F_ATTR_REQUIREMENT_MASK)
                goto conflict;
            flags |= CFFI_F_ATTR_ZERO;
            break;
        case NONZERO:
            if (flags & CFFI_F_ATTR_REQUIREMENT_MASK)
                goto conflict;
            flags |= CFFI_F_ATTR_NONZERO;
            break;
        case NONNEGATIVE:
            if (flags & CFFI_F_ATTR_REQUIREMENT_MASK)
                goto conflict;
            flags |= CFFI_F_ATTR_NONNEGATIVE;
            break;
        case POSITIVE:
            if (flags & CFFI_F_ATTR_REQUIREMENT_MASK)
                goto conflict;
            flags |= CFFI_F_ATTR_POSITIVE;
            break;
        case ERRNO:
            if (flags & CFFI_F_ATTR_ERROR_MASK)
                goto conflict;
            flags |= CFFI_F_ATTR_ERRNO;
            break;
        case LASTERROR:
            if (flags & CFFI_F_ATTR_ERROR_MASK)
                goto conflict;
            flags |= CFFI_F_ATTR_LASTERROR;
            break;
        case WINERROR:
            if (flags & CFFI_F_ATTR_ERROR_MASK)
                goto conflict;
            flags |= CFFI_F_ATTR_WINERROR;
            break;
        case NULLIFEMPTY:
            flags |= CFFI_F_ATTR_NULLIFEMPTY;
            break;
        }
    }

    /*
     * Now check whether any attributes are set that are not valid for the
     * allowed parse modes. We do this separately here rather than in the
     * loop above to handle merging of attributes from typeAliases, which are
     * not parse mode specific, with attributes specified in function
     * prototypes or structs.
     */
    for (i = 0; i < sizeof(cffiAttrs) / sizeof(cffiAttrs[0]); ++i) {
        if (cffiAttrs[i].attrFlag == -1)
            continue;/* Not an attribute flag */
        if (cffiAttrs[i].attrFlag & flags) {
            /* Attribute is present, check if allowed by parse mode */
            if (! (cffiAttrs[i].parseModes & parseMode)) {
                message = "An attribute in the type declaration is not valid for the declaration context.";
                goto invalid_format;
            }
        }
    }

    /* winerror only makes sense for the zero requirement */
    if (flags & CFFI_F_ATTR_WINERROR) {
        int requirements_flags = flags & CFFI_F_ATTR_REQUIREMENT_MASK;
        if (requirements_flags && requirements_flags != CFFI_F_ATTR_ZERO)
            goto conflict;
    }

    switch (parseMode) {
    case CFFI_F_TYPE_PARSE_PARAM:
        if (baseType == CFFI_K_TYPE_VOID) {
            message =
                "The specified type is not valid for the type declaration context.";
            goto invalid_format;
        }
        /* For parameters at least one of IN, OUT, INOUT should be set */
        if (flags & CFFI_F_ATTR_INOUT) {
            if (flags & CFFI_F_ATTR_NULLIFEMPTY) {
                message = "One or more annotations are invalid for inout "
                          "parameters.";
                goto invalid_format;
            }
            flags |= CFFI_F_ATTR_BYREF; /* inout default to byref */
        }
        else if (flags & CFFI_F_ATTR_OUT) {
            if (flags & (CFFI_F_ATTR_DISPOSE|CFFI_F_ATTR_NULLIFEMPTY)) {
                message = "One or more annotations are invalid for out "
                          "parameters.";
                goto invalid_format;
            }
            flags |= CFFI_F_ATTR_BYREF; /* out default to byref */
        }
        else {
            flags |= CFFI_F_ATTR_IN; /* in, or by default if nothing was said */

            if (typeAttrP->dataType.count > 0)
                flags |= CFFI_F_ATTR_BYREF; /* Arrays always by reference */
            else {
                /* Certain types always by reference */
                switch (baseType) {
                case CFFI_K_TYPE_ASTRING:
                case CFFI_K_TYPE_UNISTRING:
                case CFFI_K_TYPE_BINARY:
                case CFFI_K_TYPE_CHAR_ARRAY:
                case CFFI_K_TYPE_UNICHAR_ARRAY:
                case CFFI_K_TYPE_BYTE_ARRAY:
                    flags |=
                        CFFI_F_ATTR_BYREF; /* Arrays always by reference */
                    break;
                case CFFI_K_TYPE_STRUCT:
                    if ((flags & CFFI_F_ATTR_BYREF) == 0) {
                        message = "Parameters of type struct must have the "
                                  "byref annotation.";
                        goto invalid_format;
                    }
                    break;
                default:
                    break;
                }
            }
        }
        break;
    case CFFI_F_TYPE_PARSE_RETURN:
        /* Return - parameter-mode flags should not be set */
        CFFI_ASSERT((flags & CFFI_F_ATTR_PARAM_MASK) == 0);
        switch (baseType) {
        case CFFI_K_TYPE_ASTRING:
        case CFFI_K_TYPE_UNISTRING:
        case CFFI_K_TYPE_STRUCT:
        case CFFI_K_TYPE_BINARY:
        case CFFI_K_TYPE_CHAR_ARRAY:
        case CFFI_K_TYPE_UNICHAR_ARRAY:
        case CFFI_K_TYPE_BYTE_ARRAY:
            /* Struct/string/bytes return type not allowed even byref */
            message =
                "The specified type is not valid for the type declaration context.";
            goto invalid_format;
        break;
        case CFFI_K_TYPE_VOID: /* FALLTHRU */
        default:
            if (typeAttrP->dataType.count > 0) {
                message = "Function return type declaration must not specify "
                          "an array.";
                goto invalid_format;
            }
            break;
        }
        break;
    case CFFI_F_TYPE_PARSE_FIELD:
        /* Struct field - parameter-mode flags should not be set */
        CFFI_ASSERT((flags & CFFI_F_ATTR_PARAM_MASK) == 0);
        switch (baseType) {
        case CFFI_K_TYPE_VOID:
        case CFFI_K_TYPE_ASTRING:
        case CFFI_K_TYPE_UNISTRING:
        case CFFI_K_TYPE_BINARY:
            /* Struct/string/bytes return type not allowed even byref */
            message = "The specified type is not valid for the type "
                      "declaration context.";
            goto invalid_format;
        case CFFI_K_TYPE_CHAR_ARRAY:
        case CFFI_K_TYPE_UNICHAR_ARRAY:
        case CFFI_K_TYPE_BYTE_ARRAY:
            CFFI_ASSERT(typeAttrP->dataType.count > 0);
            break;
        default:
            break;
        }
        break;

    default:
        /*
         * One or more parse modes - preliminary typedef. Accept all flags.
         * Final check will be made when a specific mode is parsed.
         */
        break;
    }

    typeAttrP->flags = flags;
    return TCL_OK;

conflict:
    message = "Type declaration has repeated or conflicting attributes.";

invalid_format:
    /* NOTE: jump source should have incremented ref count if errorObj was
     * passed directly or indirectly from caller (ok if errorObj is NULL) */
    (void)Tclh_ErrorInvalidValue(
        ip,
        typeAttrObj,
        message);
    CffiTypeAndAttrsCleanup(typeAttrP);
    return TCL_ERROR;
}


/* Function: CffiTypeAndAttrsCleanup
 * Cleans up any allocation in the parameter representation.
 *
 * Parameters:
 * paramP - pointer to parameter representation
 *
 * Returns:
 * Nothing. Any allocated resources are released.
 */
void CffiTypeAndAttrsCleanup (CffiTypeAndAttrs *typeAttrsP)
{
    if (typeAttrsP->defaultObj) {
        Tcl_DecrRefCount(typeAttrsP->defaultObj);
        typeAttrsP->defaultObj = NULL;
    }
    CffiTypeCleanup(&typeAttrsP->dataType);
}

/* Function: CffiPrepareArg
 * Prepares a argument for a DCCall
 *
 * Parameters:
 * ctxP     - pointer to the interpreter and DcCall context
 * typeP    - the expected type of the value
 * valueObj - the Tcl_Obj containing the value or variable name.
 * valueP - location to store the native value.
 * varNameObjP - location to store the *Tcl_Obj* holding the variable name
 *            for byref parameters. This will be valueObj[0] for byref
 *            parameters and NULL for byval parameters. The reference
 *            count is unchanged from what is passed in so do NOT call
 *            an *additional* Tcl_DecrRefCount on it.
 *
 * In addition to storing the native value at *valueP*, the function also
 * stores it as a dyncall function argument. The function may allocate
 * additional dynamic storage from *ipContextP->memlifo* that is the
 * caller's responsibility to free.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
CffiResult
CffiArgPrepare(CffiCallVmCtx *vmCtxP,
               const CffiTypeAndAttrs *typeAttrsP,
               Tcl_Obj *valueObj,
               CffiValue *valueP,
               Tcl_Obj **varNameObjP)
{
    DCCallVM *vmP  = vmCtxP->vmP;
    CffiInterpCtx *ipCtxP = vmCtxP->ipCtxP;
    Tcl_Interp *ip = ipCtxP->interp;
    enum CffiBaseType baseType;
    CffiResult ret;
    int memory_size;
    int flags;
    char *p;

    /*
     * IMPORTANT: the logic here must be consistent with CffiArgPostProcess
     * and CffiArgCleanup. Any changes here should be reflected there too.
     */

    flags = typeAttrsP->flags;
    baseType = typeAttrsP->dataType.baseType;
    if (typeAttrsP->dataType.count) {
        switch (baseType) {
        case CFFI_K_TYPE_ASTRING:
        case CFFI_K_TYPE_UNISTRING:
        case CFFI_K_TYPE_STRUCT:
        case CFFI_K_TYPE_BINARY:
            return ErrorInvalidValue(ip,
                                     NULL,
                                     "Arrays not supported for "
                                     "struct/string/unistring/binary types.");
        default:
            break;
        }
    }

    /*
     * out/inout parameters are always expected to be byref. Prototype
     * parser should have ensured that.
     */
    CFFI_ASSERT((typeAttrsP->flags
                  & (CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT))
                     == 0
                 || (typeAttrsP->flags & CFFI_F_ATTR_BYREF) != 0);

    /*
     * For pure in parameters, valueObj provides the value itself.
     * For out and inout parameters, valueObj is a list consisting of a variable
     * name followed by an optional size parameter. If the parameter is an
     * inout parameter, the variable must exist since the value passed
     * to the called function is taken from there. For pure out parameters, the
     * variable need not exist and will be created if necessary. For both
     * in and inout, on return from the function the corresponding  content
     * is stored in that variable.
     *
     * The optional second element of out/inout argument is a size
     * parameter. This is the number of bytes (for string and binary) or
     * number of Tcl_UniChar (for unistring) to allocate to receive the
     * output value and must be specified for string, unistring and binary
     * types. It is not permitted for other types.
     */
    *varNameObjP = NULL;
    memory_size = 0;
    if (flags & (CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT)) {
        Tcl_Obj **value_and_memsize;
        int n;
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        ret = Tcl_ListObjGetElements(NULL, valueObj, &n, &value_and_memsize);
        if (ret != TCL_OK)
            return ret;
        if (n == 0)
            return ErrorInvalidValue(ip,
                                     valueObj,
                                     "A variable name must be specified for "
                                     "out and inout parameters");
        if (baseType != CFFI_K_TYPE_ASTRING
            && baseType != CFFI_K_TYPE_UNISTRING
            && baseType != CFFI_K_TYPE_BINARY) {
            if (n != 1)
                return ErrorInvalidValue(
                    ip,
                    valueObj,
                    "Arguments for out and inout parameters of type other than "
                    "string, unistring and binary should only be specified as "
                    "a variable name.");
        }
        else {
            /* string, unistring or binary */
            if (n != 2 ||
                Tclh_ObjToInt(ip, value_and_memsize[1], &memory_size) != TCL_OK
                || memory_size <= 0) {
                return ErrorInvalidValue(
                    ip,
                    valueObj,
                    "Arguments for out and inout "
                    "parameters of type string and bytes should "
                    "be of the form \"varname size\".");
            }
        }
        valueObj = Tcl_ObjGetVar2(ip, value_and_memsize[0], NULL, TCL_LEAVE_ERR_MSG);
        if (valueObj == NULL && (flags & CFFI_F_ATTR_INOUT)) {
            return ErrorInvalidValue(
                ip,
                value_and_memsize[0],
                "Variable specified as inout argument does not exist.");
        }
        *varNameObjP = value_and_memsize[0];
        /* TBD - check if existing variable is an array and error out? */
    }

    /*
     * Type parsing should have validated out/inout params specify size if
     * not a fixed size type. Note chars/unichars/bytes are fixed size since
     * their array size is required to be specified.
     */
    CFFI_ASSERT((flags & (CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT))
                     == 0
                 || memory_size > 0
                 || (baseType != CFFI_K_TYPE_ASTRING
                     && baseType != CFFI_K_TYPE_UNISTRING
                     && baseType != CFFI_K_TYPE_BINARY));

    /* Non-scalars need to be passed byref. Parsing should have checked */
    CFFI_ASSERT((flags & CFFI_F_ATTR_BYREF)
                 || (typeAttrsP->dataType.count == 0
                     && baseType != CFFI_K_TYPE_ASTRING
                     && baseType != CFFI_K_TYPE_UNISTRING
                     && baseType != CFFI_K_TYPE_BINARY
                     && baseType != CFFI_K_TYPE_CHAR_ARRAY
                     && baseType != CFFI_K_TYPE_UNICHAR_ARRAY
                     && baseType != CFFI_K_TYPE_BYTE_ARRAY
                     && baseType != CFFI_K_TYPE_STRUCT));

    /*
     * STORENUM - for storing numerics only.
     * For storing an argument, if the parameter is byval, extract the
     * value into native form and pass it to dyncall via the specified
     * dcfn_ function. Note byval parameter are ALWAYS in-only.
     * For in and inout, the value is extracted into native storage.
     * For byval parameters (can only be in, never out/inout), this
     * value is passed in to dyncall via the argument specific function
     * specified by dcfn_. For byref parameters, the argument is always
     * passed via the dcArgPointer function with a pointer to the
     * native value.
     *
     * NOTE: We explicitly pass in objfn_ and dcfn_ instead of using
     * conversion functions from cffiBaseTypes as it is more efficient
     * (compile time function binding)
     */
#define STORENUM(objfn_, dcfn_, fld_, type_)                                  \
    do {                                                                      \
        int count = typeAttrsP->dataType.count;                               \
        if (count <= 1) {                                                     \
            if (flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT)) {               \
                ret = objfn_(ip, valueObj, &valueP->fld_);                    \
                if (ret != TCL_OK)                                            \
                    return ret;                                               \
            }                                                                 \
            if (flags & CFFI_F_ATTR_BYREF)                                    \
                dcArgPointer(vmP, (DCpointer)&valueP->fld_);                  \
            else {                                                            \
                CFFI_ASSERT(count == 0);                                      \
                dcfn_(vmP, valueP->fld_);                                     \
            }                                                                 \
        }                                                                     \
        else {                                                                \
            /* Array - has to be byref */                                     \
            type_ *valueArray;                                                \
            CFFI_ASSERT(flags &CFFI_F_ATTR_BYREF);                            \
            valueArray =                                                      \
                MemLifoAlloc(&ipCtxP->memlifo, count * sizeof(valueP->fld_)); \
            if (flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT)) {               \
                Tcl_Obj **valueObjList;                                       \
                int i, nvalues;                                               \
                if (Tcl_ListObjGetElements(                                   \
                        ip, valueObj, &nvalues, &valueObjList)                \
                    != TCL_OK)                                                \
                    return TCL_ERROR;                                         \
                /* Note - if caller has specified too few values, it's ok     \
                 * because perhaps the actual count is specified in another   \
                 * parameter. If too many, only up to array size */           \
                if (nvalues > count)                                          \
                    nvalues = count;                                          \
                for (i = 0; i < nvalues; ++i) {                               \
                    ret = objfn_(ip, valueObjList[i], &valueArray[i]);        \
                    if (ret != TCL_OK)                                        \
                        return ret;                                           \
                }                                                             \
                /* Fill additional elements with 0 */                         \
                for (i = nvalues; i < count; ++i)                             \
                    valueArray[i] = 0;                                        \
            }                                                                 \
            valueP->ptr = valueArray;                                         \
            dcArgPointer(vmP, (DCpointer)valueArray);                         \
        }                                                                     \
    } while (0)

    switch (baseType) {
    case CFFI_K_TYPE_SCHAR: STORENUM(ObjToChar, dcArgChar, schar, signed char); break;
    case CFFI_K_TYPE_UCHAR: STORENUM(ObjToUChar, dcArgChar, uchar, unsigned char); break;
    case CFFI_K_TYPE_SHORT: STORENUM(ObjToShort, dcArgShort, sshort, short); break;
    case CFFI_K_TYPE_USHORT: STORENUM(ObjToUShort, dcArgShort, ushort, unsigned short); break;
    case CFFI_K_TYPE_INT: STORENUM(ObjToInt, dcArgInt, sint, int); break;
    case CFFI_K_TYPE_UINT: STORENUM(ObjToUInt, dcArgInt, uint, unsigned int); break;
    case CFFI_K_TYPE_LONG: STORENUM(ObjToLong, dcArgLong, slong, long); break;
    case CFFI_K_TYPE_ULONG: STORENUM(ObjToULong, dcArgLong, ulong, unsigned long); break;
    case CFFI_K_TYPE_LONGLONG: STORENUM(ObjToLongLong, dcArgLongLong, slonglong, signed long long); break;
        /*  TBD - unsigned 64-bit is broken */
    case CFFI_K_TYPE_ULONGLONG: STORENUM(ObjToULongLong, dcArgLongLong, ulonglong, unsigned long long); break;
    case CFFI_K_TYPE_FLOAT: STORENUM(ObjToFloat, dcArgFloat, flt, float); break;
    case CFFI_K_TYPE_DOUBLE: STORENUM(ObjToDouble, dcArgDouble, dbl, double); break;
    case CFFI_K_TYPE_STRUCT:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        valueP->ptr = MemLifoAlloc(&ipCtxP->memlifo,
                                   typeAttrsP->dataType.u.structP->size);
        if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
            ret = CffiStructFromObj(
                ip, typeAttrsP->dataType.u.structP, valueObj, valueP->ptr);
            if (ret != TCL_OK)
                return ret;
        }
        else {
            /* TBD - should we zero out the memory */
        }
        dcArgPointer(vmP, valueP->ptr);
        break;

    case CFFI_K_TYPE_POINTER:
        /* TBD - can we just use STORENUM here ? */
        if (typeAttrsP->dataType.count <= 1) {
            if ((flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) == 0)
                valueP->ptr = NULL;
            else {
                ret = CffiPointerFromObj(
                    ip, typeAttrsP, valueObj, &valueP->ptr);
                if (ret != TCL_OK)
                    return ret;
            }
            if (flags & CFFI_F_ATTR_BYREF)
                dcArgPointer(vmP, &valueP->ptr);
            else
                dcArgPointer(vmP, valueP->ptr);
        } else {
            void **valueArray;
            CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
            valueArray = MemLifoAlloc(&ipCtxP->memlifo,
                                          typeAttrsP->dataType.count * sizeof(void *));
            if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
                Tcl_Obj **valueObjList;
                int i, nvalues;
                if (Tcl_ListObjGetElements(
                        ip, valueObj, &nvalues, &valueObjList)
                    != TCL_OK)
                    return TCL_ERROR;
                for (i = 0; i < nvalues; ++i) {
                    ret = CffiPointerFromObj(ip, typeAttrsP, valueObjList[i], &valueArray[i]);
                    if (ret != TCL_OK)
                        return ret;
                }
            }
            valueP->ptr = valueArray;
            dcArgPointer(vmP, valueP->ptr);
        }
        break;

    case CFFI_K_TYPE_CHAR_ARRAY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        ret = CffiArgPrepareChars(ipCtxP, typeAttrsP, valueObj, valueP);
        if (ret != TCL_OK)
            return ret;
        dcArgPointer(vmP, valueP->ptr);
        break;

    case CFFI_K_TYPE_ASTRING:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        /* Note this also handles case of pure OUT parameter */
        ret =
            CffiArgPrepareString(ip, typeAttrsP, valueObj, memory_size, valueP);
        if (ret != TCL_OK)
            return ret;
        if ((typeAttrsP->flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_NULLIFEMPTY))
                == (CFFI_F_ATTR_IN | CFFI_F_ATTR_NULLIFEMPTY)
            && Tcl_DStringLength(&valueP->ds) == 0) {
            p = NULL; /* Null if empty */
        }
        else {
            p = Tcl_DStringValue(&valueP->ds);
        }
        dcArgPointer(vmP, p);
        break;

    case CFFI_K_TYPE_UNISTRING:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        /* Note this also handles case of pure OUT parameter */
        ret = CffiArgPrepareUniString(
            ip, typeAttrsP, valueObj, memory_size, valueP);
        if (ret != TCL_OK)
            return ret;
        p = Tcl_DStringValue(&valueP->ds);
        if ((typeAttrsP->flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_NULLIFEMPTY))
                == (CFFI_F_ATTR_IN | CFFI_F_ATTR_NULLIFEMPTY)
            && p[0] == 0 && p[1] == 0) {
            p = NULL; /* Null if empty */
        }
        dcArgPointer(vmP, p);
        break;

    case CFFI_K_TYPE_UNICHAR_ARRAY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        ret = CffiArgPrepareUniChars(ipCtxP, typeAttrsP, valueObj, valueP);
        if (ret != TCL_OK)
            return ret;
        dcArgPointer(vmP, valueP->ptr);
        break;

    case CFFI_K_TYPE_BINARY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        ret = CffiArgPrepareBinary(ip, typeAttrsP, valueObj, memory_size, valueP);
        if (ret != TCL_OK)
            return ret;
        dcArgPointer(vmP, Tcl_GetByteArrayFromObj(valueP->baObj, NULL));
        break;

    case CFFI_K_TYPE_BYTE_ARRAY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        ret = CffiArgPrepareBytes(ipCtxP, typeAttrsP, valueObj, valueP);
        if (ret != TCL_OK)
            return ret;
        dcArgPointer(vmP, valueP->ptr);
        break;

    default:
        return ErrorInvalidValue(
            ip, NULL, "Unsupported type.");
    }
    return TCL_OK;
#undef STOREARG
}

/* Function: CffiArgPostProcess
 * Does the post processing of an argument after a call
 *
 * Post processing of the argument consists of checking if the parameter
 * was an *out* or *inout* parameter and storing it the variable passed
 * through *varObjP*. Note no cleanup of argument storage is done.
 *
 * Parameters:
 * ip - Interpreter
 * typeP - type descriptor for the binary value
 * valueP - pointer to binary value to wrap
 * varObjP - contains the name of the variable in which to store the
 *    argument value if an output parameters.
 *
 * Returns:
 * *TCL_OK* on success,
 * *TCL_ERROR* on error with message stored in the interpreter.
 */
CffiResult
CffiArgPostProcess(Tcl_Interp *ip,
                   const CffiTypeAndAttrs *typeAttrsP,
                   CffiValue *valueP,
                   Tcl_Obj *varObjP)
{
    int ret;
    Tcl_Obj *valueObj;

    if ((typeAttrsP->flags & (CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT)) == 0)
        return TCL_OK;

    /*
     * There are three categories:
     *  - scalar values are directly stored in *valueP
     *  - structs and arrays of scalars are stored in at the location
     *    pointed to by valueP->ptr
     *  - strings/unistring/binary are stored in *valueP but not as
     *    native C values.
     */
    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_SCHAR:
    case CFFI_K_TYPE_UCHAR:
    case CFFI_K_TYPE_SHORT:
    case CFFI_K_TYPE_USHORT:
    case CFFI_K_TYPE_INT:
    case CFFI_K_TYPE_UINT:
    case CFFI_K_TYPE_LONG:
    case CFFI_K_TYPE_ULONG:
    case CFFI_K_TYPE_LONGLONG:
    case CFFI_K_TYPE_ULONGLONG:
    case CFFI_K_TYPE_FLOAT:
    case CFFI_K_TYPE_DOUBLE:
        /* Scalars stored at valueP, arrays of scalars at valueP->ptr */
        if (typeAttrsP->dataType.count)
            ret = CffiNativeValueToObj(ip, typeAttrsP, valueP->ptr, &valueObj);
        else
            ret = CffiNativeValueToObj(ip, typeAttrsP, valueP, &valueObj);
        break;

    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
        ret = CffiNativeValueToObj(ip, typeAttrsP, valueP->ptr, &valueObj);
        break;

    case CFFI_K_TYPE_POINTER:
        /* Arrays not supported for pointers currently */
        CFFI_ASSERT(typeAttrsP->dataType.count == 0);
        ret = CffiNativeValueToObj(ip, typeAttrsP, valueP, &valueObj);
        break;

    case CFFI_K_TYPE_STRUCT:
        /* Arrays not supported for struct currently */
        CFFI_ASSERT(typeAttrsP->dataType.count == 0);
        ret = CffiNativeValueToObj(ip, typeAttrsP, valueP->ptr, &valueObj);
        break;

    case CFFI_K_TYPE_ASTRING:
        ret = CffiExternalStringToObj(
            ip, typeAttrsP, (Tcl_DString *)valueP, &valueObj);
        break;

    case CFFI_K_TYPE_UNISTRING:
        ret = CffiUniStringToObj(
            ip, typeAttrsP, (Tcl_DString *)valueP, &valueObj);
        break;

    case CFFI_K_TYPE_BINARY:
        valueObj = valueP->baObj;
        ret      = TCL_OK;
        break;

    default:
        /* Should not happen */
        ret = Tclh_ErrorInvalidValue(ip, NULL, "Unsupported argument type");
        break;
    }

    if (ret != TCL_OK)
        return ret;

    /*
     * Tcl_ObjSetVar2 will release valueObj if its ref count is 0
     * preventing us from trying again after deleting the array so
     * preserve the value obj.
     */
    Tcl_IncrRefCount(valueObj);
    if (Tcl_ObjSetVar2(ip, varObjP, NULL, valueObj, 0) == NULL) {
        /* Perhaps it is an array in which case we need to delete first */
        Tcl_UnsetVar(ip, Tcl_GetString(varObjP), 0);
        /* Retry */
        if (Tcl_ObjSetVar2(ip, varObjP, NULL, valueObj, TCL_LEAVE_ERR_MSG)
            == NULL) {
            Tcl_DecrRefCount(valueObj);
            return TCL_ERROR;
        }
    }
    Tcl_DecrRefCount(valueObj);
    return TCL_OK;
}

/* Function: CffiReturnPrepare
 * Prepares storage for DCCall function return value
 *
 * Parameters:
 * ctxP     - pointer to the interpreter and DcCall context
 * typeP    - the expected type of the value
 * valueP - location to store the native value.
 *
 * The function may allocate additional dynamic storage from
 * *ipContextP->memlifo* that is the caller's responsibility to free.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
CffiResult
CffiReturnPrepare(CffiCallVmCtx *vmCtxP,
                  const CffiTypeAndAttrs *typeAttrP,
                  CffiValue *valueP)
{
    /* Function prototype parser should already have disallowed these */
    CFFI_ASSERT(typeAttrP->dataType.baseType != CFFI_K_TYPE_STRUCT
                 && typeAttrP->dataType.baseType != CFFI_K_TYPE_ASTRING
                 && typeAttrP->dataType.baseType != CFFI_K_TYPE_UNISTRING
                 && typeAttrP->dataType.baseType != CFFI_K_TYPE_CHAR_ARRAY
                 && typeAttrP->dataType.baseType != CFFI_K_TYPE_UNICHAR_ARRAY
                 && typeAttrP->dataType.baseType != CFFI_K_TYPE_BYTE_ARRAY
                 && typeAttrP->dataType.baseType != CFFI_K_TYPE_BINARY);

    if (typeAttrP->dataType.count) {
        return ErrorInvalidValue(vmCtxP->ipCtxP->interp, NULL, "Arrays not supported.");
    }
    /*
     * Scalars all fit within CffiValue so need for additional storage.
     */
    return TCL_OK;
}

static CffiStruct *CffiStructCkalloc(int nfields)
{
    int         sz;
    CffiStruct *structP;

    sz      = offsetof(CffiStruct, fields) + (nfields * sizeof(structP->fields[0]));
    structP = ckalloc(sz);
    memset(structP, 0, sz);
    return structP;
}

void
CffiStructUnref(CffiStruct *structP)
{
    if (structP->nRefs <= 1) {
        int i;
        if (structP->name)
            Tcl_DecrRefCount(structP->name);
        for (i = 0; i < structP->nFields; ++i) {
            if (structP->fields[i].nameObj)
                Tcl_DecrRefCount(structP->fields[i].nameObj);
            CffiTypeAndAttrsCleanup(&structP->fields[i].field);
        }
        ckfree(structP);
    }
    else {
        structP->nRefs -= 1;
    }
}


/* Function: CffiStructParse
 * Parses a *struct* definition into an internal form.
 *
 * The returned *CffiStruct* structure has a reference count of 0. It should
 * be eventually be released by calling <CffiStructUnref>.
 *
 * Parameters:
 * ip - interpreter
 * nameObj - name of the struct
 * structObj - structure definition
 * structPP - pointer to location to store pointer to internal form.
 *
 * Returns:
 * Returns *TCL_OK* on success with a pointer to the internal struct
 * representation in *structPP* and *TCL_ERROR* on failure with error message
 * in the interpreter.
 *
 */
CffiResult
CffiStructParse(CffiInterpCtx *ipCtxP,
                 Tcl_Obj *nameObj,
                 Tcl_Obj *structObj,
                 CffiStruct **structPP)
{
    Tcl_Obj **objs;
    int          nobjs;
    int          nfields;
    int i, j;
    int offset;
    int struct_alignment;
    CffiStruct *structP;
    Tcl_Interp *ip = ipCtxP->interp;

    if (Tcl_GetCharLength(nameObj) == 0) {
        return Tclh_ErrorInvalidValue(
            ip, nameObj, "Empty string specified for structure name.");
    }

    CHECK(Tcl_ListObjGetElements(ip, structObj, &nobjs, &objs));

    if (nobjs == 0 || (nobjs & 1))
        return Tclh_ErrorInvalidValue(
            ip, structObj, "Empty struct or missing type definition for field.");
    nfields = nobjs / 2; /* objs[] is alternating name, type list */

    structP          = CffiStructCkalloc(nfields);
    structP->nFields = 0; /* Update as we go along */
    for (i = 0, j = 0; i < nobjs; i += 2, ++j) {
        int k;
        if (CffiTypeAndAttrsParse(ipCtxP,
                                  objs[i + 1],
                                  CFFI_F_TYPE_PARSE_FIELD,
                                  &structP->fields[j].field)
            != TCL_OK) {
            CffiStructUnref(structP);
            return TCL_ERROR;
        }
        /* Check for dup field names */
        for (k = 0; k < j; ++k) {
            if (!strcmp(Tcl_GetString(objs[i]),
                        Tcl_GetString(structP->fields[k].nameObj))) {
                CffiStructUnref(structP);
                return Tclh_ErrorExists(
                    ip,
                    "Field",
                    objs[i],
                    "Field names in a struct must be unique.");
            }
        }
        Tcl_IncrRefCount(objs[i]);
        structP->fields[j].nameObj = objs[i];
        /* Note: update incrementally for structP cleanup in case of errors */
        structP->nFields += 1;
    }

    /* Calculate metadata for all fields */
    offset           = 0;
    struct_alignment = 1;

    for (i = 0; i < nfields; ++i) {
        CffiField *fieldP = &structP->fields[i];
        int field_size;
        int field_alignment;
        CFFI_ASSERT(fieldP->field.dataType.baseType != CFFI_K_TYPE_VOID);
        CffiTypeLayoutInfo(
            &fieldP->field.dataType, NULL, &field_size, &field_alignment);
        if (field_alignment > struct_alignment)
            struct_alignment = field_alignment;

        /* See if offset needs to be aligned for this field */
        offset = (offset + field_alignment - 1) & ~(field_alignment - 1);
        fieldP->offset = offset;
        fieldP->size   = field_size;
        offset += field_size;
    }

    Tcl_IncrRefCount(nameObj);
    structP->name      = nameObj;
    structP->nRefs     = 0;
    structP->alignment = struct_alignment;
    structP->size      = (offset + struct_alignment - 1) & ~(struct_alignment - 1);
    *structPP          = structP;
    return TCL_OK;
}

/* Function: CffiStructResolve
 * Returns the internal representation of a name struct.
 *
 * Parameters:
 * ip - interpreter
 * nameP - name of the struct
 * structPP - location to hold pointer to resolved struct representation
 *
 * *NOTE:* The reference count on the returned structure is *not* incremented.
 * The caller should do that if it wishes to hold on to it.
 *
 * Returns:
 * *TCL_OK* on success with pointer to the <CffiStruct> stored in
 * *structPP*, or *TCL_ERROR* on error with message in interpreter.
 */
CffiResult CffiStructResolve (Tcl_Interp *ip, const char *nameP, CffiStruct **structPP)
{
    Tcl_CmdInfo tci;
    Tcl_Obj *nameObj;

    if (Tcl_GetCommandInfo(ip, nameP, &tci)) {
        CffiStructCtx *structCtxP;
        CFFI_ASSERT(tci.clientData);
        structCtxP = (CffiStructCtx *)tci.objClientData;
        *structPP  = structCtxP->structP;
        return TCL_OK;
    }
    nameObj = Tcl_NewStringObj(nameP, -1);
    Tcl_IncrRefCount(nameObj);
    (void) Tclh_ErrorNotFound(ip, "Struct definition", nameObj, NULL);
    Tcl_DecrRefCount(nameObj);
    return TCL_ERROR;
}

/* Function: CffiStructDescribe
 * Stores a human readable description of the internal representation of
 * a <CffiStruct> as the interpreter result.
 *
 * Parameters:
 * ip - Interpreter
 * structP - Pointer to a <CffiStruct>
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on error.
 */
CffiResult
CffiStructDescribeCmd(Tcl_Interp *ip,
                      int objc,
                      Tcl_Obj *const objv[],
                      CffiStructCtx *structCtxP)
{
    int i;
    CffiStruct *structP = structCtxP->structP;
    Tcl_Obj *objP =
        Tcl_ObjPrintf("Struct %s nRefs=%d size=%d alignment=%d nFields=%d",
                      Tcl_GetString(structP->name),
                      structP->nRefs,
                      structP->size,
                      structP->alignment,
                      structP->nFields);
    for (i = 0; i < structP->nFields; ++i) {
        CffiField *fieldP = &structP->fields[i];
        enum CffiBaseType baseType = fieldP->field.dataType.baseType;
        switch (baseType) {
        case CFFI_K_TYPE_POINTER:
        case CFFI_K_TYPE_ASTRING:
        case CFFI_K_TYPE_UNISTRING:
        case CFFI_K_TYPE_CHAR_ARRAY:
        case CFFI_K_TYPE_UNICHAR_ARRAY:
            Tcl_AppendPrintfToObj(objP, "\n  %s", cffiBaseTypes[baseType].token);
            if (fieldP->field.dataType.u.tagObj)
                Tcl_AppendPrintfToObj(
                    objP,
                    ".%s",
                    Tcl_GetString(fieldP->field.dataType.u.tagObj));
            Tcl_AppendPrintfToObj(objP, " %s", Tcl_GetString(fieldP->nameObj));
            if (fieldP->field.dataType.count)
                Tcl_AppendPrintfToObj(
                    objP, "[%d]", fieldP->field.dataType.count);
            break;
        case CFFI_K_TYPE_STRUCT:
            Tcl_AppendPrintfToObj(
                objP,
                "\n  %s %s %s",
                cffiBaseTypes[baseType].token,
                Tcl_GetString(fieldP->field.dataType.u.structP->name),
                Tcl_GetString(fieldP->nameObj));
            if (fieldP->field.dataType.count)
                Tcl_AppendPrintfToObj(
                    objP, "[%d]", fieldP->field.dataType.count);
            break;

        default:
            Tcl_AppendPrintfToObj(objP,
                                  "\n  %s %s",
                                  cffiBaseTypes[baseType].token,
                                  Tcl_GetString(fieldP->nameObj));
            if (fieldP->field.dataType.count)
                Tcl_AppendPrintfToObj(
                    objP, "[%d]", fieldP->field.dataType.count);
            break;
        }
        Tcl_AppendPrintfToObj(
            objP, " offset=%d size=%d", fieldP->offset, fieldP->size);
    }

    Tcl_SetObjResult(ip, objP);

    return TCL_OK;
}



/* Function: CffiStructInfo
 * Returns a dictionary describing the structure as the interp result.
 *
 * Parameters:
 * ip - Interpreter
 * structP - Pointer to a <CffiStruct>
 *
 * The dictionary returned in the interpreter has the following fields:
 *
 * size - size of the structure
 * alignment - structure alignment
 * fields - dictionary of fields keyed by field name. The value is another
 *   nested dictionary with keys *size*, *offset*, and *definition*
 *   containing the size, offset in struct, and type definition of the field.
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on failure.
 */
CffiResult
CffiStructInfoCmd(Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[],
                  CffiStructCtx *structCtxP)
{
    int i;
    CffiStruct *structP = structCtxP->structP;
    Tcl_Obj *objs[6];

    objs[0] = Tcl_NewStringObj("size", 4);
    objs[1] = Tcl_NewLongObj(structP->size);
    objs[2] = Tcl_NewStringObj("alignment", 9);
    objs[3] = Tcl_NewLongObj(structP->alignment);
    objs[4] = Tcl_NewStringObj("fields", 6);
    objs[5] = Tcl_NewListObj(structP->nFields, NULL);

    for (i = 0; i < structP->nFields; ++i) {
        CffiField *fieldP = &structP->fields[i];
        Tcl_Obj *attrObjs[6];
        attrObjs[0] = Tcl_NewStringObj("size", 4);
        attrObjs[1] = Tcl_NewLongObj(fieldP->size);
        attrObjs[2] = Tcl_NewStringObj("offset", 6);
        attrObjs[3] = Tcl_NewLongObj(fieldP->offset);
        attrObjs[4] = Tcl_NewStringObj("definition", 10);
        attrObjs[5] = CffiTypeAndAttrsUnparse(&fieldP->field);
        Tcl_ListObjAppendElement(NULL, objs[5], fieldP->nameObj);
        Tcl_ListObjAppendElement(NULL, objs[5], Tcl_NewListObj(6, attrObjs));
    }

    Tcl_SetObjResult(ip, Tcl_NewListObj(6, objs));

    return TCL_OK;
}

/* Function: CffiStructFromObj
 * Constructs a C struct from a *Tcl_Obj* wrapper.
 *
 * Parameters:
 * ip - Interpreter
 * structP - pointer to the struct definition internal form
 * structValueObj - the *Tcl_Obj* containing the script level struct value
 * resultP - the location where the struct is to be constructed. Caller
 *            has to ensure size is large enough (as per *structP->size*)
 *
 * Returns:
 * *TCL_OK* on success with a pointer to the structure stored in *structPP* or
 * *TCL_ERROR* on error with message stored in the interpreter.
 */
CffiResult
CffiStructFromObj(Tcl_Interp *ip,
                   const CffiStruct *structP,
                   Tcl_Obj    *structValueObj,
                   void       *resultP)
{
    int i;
    void *pv;
    CffiResult ret;


#define STOREFIELD(objfn_, type_)                                             \
    do {                                                                      \
        int indx;                                                             \
        type_ *valueP = (type_ *)(fieldP->offset + (char *)resultP);          \
        if (count == 0) {                                                     \
            if ((ret = objfn_(ip, valueObj, valueP)) != TCL_OK)               \
                return ret;                                                   \
        }                                                                     \
        else {                                                                \
            Tcl_Obj **valueObjList;                                           \
            int nvalues;                                                      \
            if (Tcl_ListObjGetElements(ip, valueObj, &nvalues, &valueObjList) \
                != TCL_OK)                                                    \
                return TCL_ERROR;                                             \
            /* Note - if caller has specified too few values, it's ok         \
             * because perhaps the actual count is specified in another       \
             * parameter. If too many, only up to array size */               \
            if (nvalues > count)                                              \
                nvalues = count;                                              \
            for (indx = 0; indx < nvalues; ++indx) {                          \
                ret = objfn_(ip, valueObjList[indx], &valueP[indx]);          \
                if (ret != TCL_OK)                                            \
                    return ret;                                               \
            }                                                                 \
            /* Fill additional unspecified elements with 0 */                 \
            for (indx = nvalues; indx < count; ++indx)                        \
                valueP[indx] = 0;                                             \
        }                                                                     \
    } while (0)

    /*
     * NOTE: On failure, we can just return as there are no allocations
     * for any fields that need to be cleaned up.
     */
    for (i = 0; i < structP->nFields; ++i) {
        Tcl_Obj *valueObj;
        const CffiField *fieldP = &structP->fields[i];
        int count               = fieldP->field.dataType.count;

        if (Tcl_DictObjGet(ip,structValueObj, fieldP->nameObj, &valueObj) != TCL_OK)
            return TCL_ERROR;
        if (valueObj == NULL) {
            return Tclh_ErrorNotFound(
                ip,
                "Struct field",
                fieldP->nameObj,
                "Field missing in struct dictionary value.");
        }
        switch (fieldP->field.dataType.baseType) {
        case CFFI_K_TYPE_SCHAR:
            STOREFIELD(ObjToChar, signed char);
            break;
        case CFFI_K_TYPE_UCHAR:
            STOREFIELD(ObjToUChar, unsigned char);
            break;
        case CFFI_K_TYPE_SHORT:
            STOREFIELD(ObjToShort, short);
            break;
        case CFFI_K_TYPE_USHORT:
            STOREFIELD(ObjToUShort, unsigned short);
            break;
        case CFFI_K_TYPE_INT:
            STOREFIELD(ObjToInt, int);
            break;
        case CFFI_K_TYPE_UINT:
            STOREFIELD(ObjToUInt, unsigned int);
            break;
        case CFFI_K_TYPE_LONG:
            STOREFIELD(ObjToLong, long);
            break;
        case CFFI_K_TYPE_ULONG:
            STOREFIELD(ObjToULong, unsigned long);
            break;
        case CFFI_K_TYPE_LONGLONG:
            STOREFIELD(ObjToLongLong, long long);
            break;
        case CFFI_K_TYPE_ULONGLONG:
            STOREFIELD(ObjToULongLong, unsigned long long);
            break;
        case CFFI_K_TYPE_FLOAT:
            STOREFIELD(ObjToFloat, float);
            break;
        case CFFI_K_TYPE_DOUBLE:
            STOREFIELD(ObjToDouble, double);
            break;
        case CFFI_K_TYPE_STRUCT:
            if (count) {
                /* TBD - nested array of structs */
                return ErrorInvalidValue(ip, NULL, "Arrays not supported for struct type.");
            }
            ret =
                CffiStructFromObj(ip,
                                   fieldP->field.dataType.u.structP,
                                   valueObj,
                                   (void *)(fieldP->offset + (char *)resultP));
            if (ret != TCL_OK)
                return ret;
            break;
        case CFFI_K_TYPE_POINTER:
            if (count) {
                /* TBD - nested array of pointers - just use STOREFIELD? */
                return ErrorInvalidValue(ip, NULL, "Arrays not supported for pointer type.");
            }
            ret = CffiPointerFromObj(
                ip, &structP->fields[i].field, valueObj, &pv);
            if (ret != TCL_OK)
                return ret;
            *(void **)(fieldP->offset + (char *)resultP) = pv;
            break;
        case CFFI_K_TYPE_CHAR_ARRAY:
            CFFI_ASSERT(count > 0);
            ret = CffiCharsFromObj(ip,
                                   structP->fields[i].field.dataType.u.tagObj,
                                   valueObj,
                                   fieldP->offset + (char *)resultP,
                                   count);
            if (ret != TCL_OK)
                return ret;
            break;
        case CFFI_K_TYPE_UNICHAR_ARRAY:
            CFFI_ASSERT(count > 0);
            ret = CffiUniCharsFromObj(
                ip, valueObj, fieldP->offset + (char *)resultP, count);
            if (ret != TCL_OK)
                return ret;
            break;
        case CFFI_K_TYPE_BYTE_ARRAY:
            CFFI_ASSERT(count > 0);
            ret = CffiBytesFromObj(
                ip, valueObj, fieldP->offset + (char *)resultP, count);
            if (ret != TCL_OK)
                return ret;
            break;
        default:
            return ErrorInvalidValue(
                ip, NULL, "Unsupported type.");
        }
    }
    return TCL_OK;
#undef STOREFIELD
}

/* Function: CffiStructToObj
 * Wraps a C structure into a Tcl_Obj.
 *
 * Parameters:
 * ip - Interpreter
 * structP - pointer to the struct definition internal descriptor
 * valueP - pointer to C structure to wrap
 * valueObjP - location to store the pointer to the returned Tcl_Obj.
 *    Following standard practice, the reference ocunt on the Tcl_Obj is 0.
 *
 * Returns:
 *
 */
CffiResult CffiStructToObj(Tcl_Interp        *ip,
                             const CffiStruct *structP,
                             void              *valueP,
                             Tcl_Obj **valueObjP)
{
    int i;
    Tcl_Obj *valueObj;
    int ret;

    valueObj = Tcl_NewListObj(structP->nFields, NULL);
    for (i = 0; i < structP->nFields; ++i) {
        const CffiField *fieldP  = &structP->fields[i];
        Tcl_Obj          *fieldObj;
        ret = CffiNativeValueToObj(
            ip, &fieldP->field, fieldP->offset + (char *)valueP, &fieldObj);
        if (ret != TCL_OK)
            goto error_return;
        Tcl_ListObjAppendElement( ip, valueObj, fieldP->nameObj);
        Tcl_ListObjAppendElement( ip, valueObj, fieldObj);
    }
    *valueObjP = valueObj;
    return TCL_OK;

error_return:
    Tcl_DecrRefCount(valueObj);
    return TCL_ERROR;
}

/* Function: CffiNativeScalarToObj
 * Wraps a scalar C binary value into a Tcl_Obj.
 *
 * Parameters:
 * ip - Interpreter
 * typeP - type descriptor for the binary value
 * valueP - pointer to C binary value to wrap
 * valueObjP - location to store the pointer to the returned Tcl_Obj.
 *    Following standard practice, the reference count on the Tcl_Obj is 0.
 *
 * Note this wraps a single value of the type indicated in *typeAttrsP*, even
 * if the *count* field in the descriptor indicates an array. Exception
 * is the chars, unichars and bytes types.
 *
 * Returns:
 * *TCL_OK* on success with a pointer to the *Tcl_Obj* stored in *valueObjP*
 * *TCL_ERROR* on error with message stored in the interpreter.
 *
 */
static CffiResult
CffiNativeScalarToObj(Tcl_Interp *ip,
                      const CffiTypeAndAttrs *typeAttrsP,
                      void *valueP,
                      Tcl_Obj **valueObjP)
{
    int ret;
    Tcl_Obj *valueObj;
    enum CffiBaseType  baseType;

    baseType = typeAttrsP->dataType.baseType;

    switch (baseType) {
    case CFFI_K_TYPE_VOID:
        valueObj = Tcl_NewObj();
    case CFFI_K_TYPE_SCHAR:
        valueObj = Tcl_NewIntObj(*(signed char *)valueP);
        break;
    case CFFI_K_TYPE_UCHAR:
        valueObj = Tcl_NewIntObj(*(unsigned char *)valueP);
        break;
    case CFFI_K_TYPE_SHORT:
        valueObj = Tcl_NewIntObj(*(signed short *)valueP);
        break;
    case CFFI_K_TYPE_USHORT:
        valueObj = Tcl_NewIntObj(*(unsigned short *)valueP);
        break;
    case CFFI_K_TYPE_INT:
        valueObj = Tcl_NewIntObj(*(signed int *)valueP);
        break;
    case CFFI_K_TYPE_UINT:
        valueObj = Tcl_NewWideIntObj(*(unsigned int *)valueP);
        break;
    case CFFI_K_TYPE_LONG:
        valueObj = Tcl_NewLongObj(*(signed long *)valueP);
        break;
    case CFFI_K_TYPE_ULONG:
        /* TBD - incorrect when sizeof(long) == sizeof(Tcl_WideInt) */
        valueObj = Tcl_NewWideIntObj(*(unsigned long *)valueP);
        break;
    case CFFI_K_TYPE_LONGLONG:
        CFFI_ASSERT(sizeof(signed long long) == sizeof(Tcl_WideInt));
        valueObj = Tcl_NewWideIntObj(*(signed long long *)valueP);
        break;
    case CFFI_K_TYPE_ULONGLONG: /* TBD - 64 bit signedness? */
        valueObj = Tcl_NewWideIntObj(*(unsigned long long *)valueP);
        break;
    case CFFI_K_TYPE_FLOAT:
        valueObj = Tcl_NewDoubleObj(*(float *)valueP);
        break;
    case CFFI_K_TYPE_DOUBLE:
        valueObj = Tcl_NewDoubleObj(*(double *)valueP);
        break;
    case CFFI_K_TYPE_POINTER:
        ret = CffiPointerToObj(ip, typeAttrsP, *(void **)valueP, &valueObj);
        if (ret != TCL_OK)
            return ret;
        break;
    case CFFI_K_TYPE_STRUCT:
    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
    case CFFI_K_TYPE_BINARY:
        /* FALLTHRU - this function should not have been called for these */
    default:
        return ErrorInvalidValue(ip, NULL, "Unsupported type.");
    }
    *valueObjP = valueObj;
    return TCL_OK;
}


/* Function: CffiNativeValueToObj
 * Wraps a C binary value into a Tcl_Obj.
 *
 * Parameters:
 * ip - Interpreter
 * typeP - type descriptor for the binary value. Should not be one of
 *   astring/unistring/binary.
 * valueP - pointer to C binary value to wrap
 * valueObjP - location to store the pointer to the returned Tcl_Obj.
 *    Following standard practice, the reference count on the Tcl_Obj is 0.
 *
 * Returns:
 * *TCL_OK* on success with a pointer to the *Tcl_Obj* stored in *valueObjP*
 * *TCL_ERROR* on error with message stored in the interpreter.
 *
 */
CffiResult
CffiNativeValueToObj(Tcl_Interp *ip,
                     const CffiTypeAndAttrs *typeAttrsP,
                     void *valueP,
                     Tcl_Obj **valueObjP)
{
    int ret;
    int count = typeAttrsP->dataType.count;
    Tcl_Obj *valueObj;
    CffiBaseType baseType = typeAttrsP->dataType.baseType;

    CFFI_ASSERT(baseType != CFFI_K_TYPE_ASTRING
                && baseType != CFFI_K_TYPE_UNISTRING
                && baseType != CFFI_K_TYPE_BINARY);

    switch (baseType) {
    case CFFI_K_TYPE_STRUCT:
        return CffiStructToObj(
            ip, typeAttrsP->dataType.u.structP, valueP, valueObjP);
    case CFFI_K_TYPE_CHAR_ARRAY:
        CFFI_ASSERT(count > 0);
        return CffiCharsToObj(ip, typeAttrsP, valueP, valueObjP);
    case CFFI_K_TYPE_UNICHAR_ARRAY:
        CFFI_ASSERT(count > 0);
        *valueObjP = Tcl_NewUnicodeObj((Tcl_UniChar *)valueP, -1);
        return TCL_OK;
    case CFFI_K_TYPE_BYTE_ARRAY:
        CFFI_ASSERT(count > 0);
        *valueObjP = Tcl_NewByteArrayObj((unsigned char *)valueP, count);
        return TCL_OK;
    default:
        /*
         * A non-zero count indicates an array type except that for chars
         * and unichars base types, it is treated as a string scalar value.
         */
        if (count == 0) {
            return CffiNativeScalarToObj(ip, typeAttrsP, valueP, valueObjP);
        }
        else {
            /* Array, possible even a single element, still represent as list */
            Tcl_Obj *listObj;
            int i;
            int offset;
            int elem_size;

            CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_BYREF);

            CffiTypeLayoutInfo(&typeAttrsP->dataType, &elem_size, NULL, NULL);

            listObj = Tcl_NewListObj(count, NULL);
            for (i = 0, offset = 0; i < count; ++i, offset += elem_size) {
                ret = CffiNativeScalarToObj(
                    ip, typeAttrsP, offset + (char *)valueP, &valueObj);
                if (ret != TCL_OK) {
                    Tcl_DecrRefCount(listObj);
                    return ret;
                }
                Tcl_ListObjAppendElement(ip, listObj, valueObj);
            }
            *valueObjP = listObj;
            return TCL_OK;
        }
    }

}

/* Function: CffiPointerToObj
 * Wraps a pointer into a Tcl_Obj based on type settings.
 *
 * The function also checks any requirements on pointer values and generates
 * an error if not met.
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - descriptor for type and attributes
 * pointer - pointer value to wrap
 * resultObjP - location to store pointer to Tcl_Obj wrapping the pointer
 *
 * Returns:
 * *TCL_OK* on success with wrapped pointer in *resultObjP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
CffiResult
CffiPointerToObj(Tcl_Interp *ip,
                  const CffiTypeAndAttrs *typeAttrsP,
                  void *pointer,
                  Tcl_Obj **resultObjP)
{
    CffiResult ret;
    int         flags = typeAttrsP->flags;

    if (((flags & CFFI_F_ATTR_ZERO) && pointer != NULL)
        || ((flags & CFFI_F_ATTR_NONZERO) && pointer == NULL)) {
        Tcl_Obj *valueObj;
        Tcl_WideInt sysError;
        const char *message;
        sysError = CffiGrabSystemError(typeAttrsP, (Tcl_WideInt)(intptr_t)pointer);
        valueObj = Tclh_PointerWrap(pointer, NULL);
        message  = pointer == NULL ? "Function returned NULL pointer."
                                   : "Function returned non-NULL pointer.";
        Tcl_IncrRefCount(valueObj);
        CffiReportRequirementError(ip, typeAttrsP, (Tcl_WideInt) (intptr_t) pointer, valueObj, sysError, message);
        Tcl_DecrRefCount(valueObj);
        return TCL_ERROR;
    }

    if (pointer == NULL) {
        /* NULL pointers are never tagged with a type - TBD */
        *resultObjP = Tclh_PointerWrap(NULL, NULL);
        ret         = TCL_OK;
    } else {
        if (flags & CFFI_F_ATTR_UNSAFE) {
            *resultObjP = Tclh_PointerWrap(
                pointer, typeAttrsP->dataType.u.tagObj);
            ret         = TCL_OK;
        } else {
            if (flags & CFFI_F_ATTR_COUNTED)
                ret = Tclh_PointerRegisterCounted(
                    ip,
                    pointer,
                    typeAttrsP->dataType.u.tagObj,
                    resultObjP);
            else
                ret = Tclh_PointerRegister(ip,
                                           pointer,
                                           typeAttrsP->dataType.u.tagObj,
                                           resultObjP);
        }
    }
    return ret;
}

/* Function: CffiPointerFromObj
 * Unwraps a single pointer value from a *Tcl_Obj* based on type settings.
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - descriptor for the type and attributes
 * pointerObj - *Tcl_Obj* wrapping the pointer
 * pointerP - location to store the unwrapped pointer
 *
 * The function checks that the pointer meets the requirements such as type,
 * non-null etc.
 * Returns:
 * *TCL_OK* on success with unwrapped pointer in *pointerP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 *
 */
CffiResult
CffiPointerFromObj(Tcl_Interp *ip,
                    const CffiTypeAndAttrs *typeAttrsP,
                    Tcl_Obj *pointerObj,
                    void **pointerP)
{
    CffiResult ret;
    Tcl_Obj *tagObj = typeAttrsP->dataType.u.tagObj;
    void *pv;

    if (typeAttrsP->flags & CFFI_F_ATTR_UNSAFE)
        ret = Tclh_PointerUnwrap(ip, pointerObj, &pv, tagObj);
    else
        ret = Tclh_PointerObjVerify(ip, pointerObj, &pv, tagObj);
    if (ret != TCL_OK)
        return ret;
    if (typeAttrsP->flags & CFFI_F_ATTR_NONZERO && pv == NULL) {
        Tcl_SetResult(ip, "NULL pointer", TCL_STATIC);
        return TCL_ERROR;
    }

    *pointerP = pv;
    return TCL_OK;
}


/* Function: CffiArgPrepareString
 * Initializes a CffiValue to pass a string/chars argument.
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - parameter type descriptor
 * valueObj - the Tcl_Obj containing string to pass. May be NULL for pure
 *   output parameters
 * valueP - location to store the argument
 *
 * Arguments that are marked as pure input need not have a count specified
 * as the null terminator indicates end of string.
 *
 * For out and inout parameters, the parameter type definition must always
 * specify a count which indicates the max number of bytes (not characters).
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with string in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
CffiResult
CffiArgPrepareString(Tcl_Interp *ip,
                     const CffiTypeAndAttrs *typeAttrsP,
                     Tcl_Obj *valueObj,
                     int memory_size,
                     CffiValue *valueP)
{
    int len;

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_ASTRING);

    Tcl_DStringInit(&valueP->ds);
    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
        const char *s;
        Tcl_Encoding encoding;
        s = Tcl_GetStringFromObj(valueObj, &len);
        /*
         * Note this encoding step is required even for UTF8 since Tcl's
         * internal UTF8 is not exactly UTF8
         */
        if (typeAttrsP->dataType.u.tagObj) {
            CHECK(Tcl_GetEncodingFromObj(
                ip, typeAttrsP->dataType.u.tagObj, &encoding));
        }
        else
            encoding = NULL;
        /*
         * NOTE: UtfToExternalDString will append more than one null byte
         * for multibyte encodings if necessary. These are NOT included
         * in the DString length.
         */
        Tcl_UtfToExternalDString(encoding, s, len, &valueP->ds);
        if (encoding)
            Tcl_FreeEncoding(encoding);
    }
    if (typeAttrsP->flags & (CFFI_F_ATTR_OUT|CFFI_F_ATTR_INOUT)) {
        if (memory_size <= 0) {
            return Tclh_ErrorInvalidValue(
                ip,
                NULL,
                "A count attribute must be specified for out and inout "
                "parameters of type string.");
        }
        /*
         * Ensure storage size is at least count bytes. Note that there
         * may already be stuff in ds in case of inout parameters that would
         * have been initialized above.
         */
        len = Tcl_DStringLength(&valueP->ds); /* Does NOT include terminating NULL */
        /*
         * It does not make sense if passed in length is greater than max output
         * buffer size. Something likely wrong on caller's part.
         */
        if (len > memory_size) {
            return Tclh_ErrorInvalidValue(ip,
                                        NULL,
                                        "Encoded string length is greater than "
                                        "specified maximum buffer size.");

        }
        if (len < memory_size) {
            /* Note Tcl_DStringSetLength actually leaves room for trailing \0 byte */
            Tcl_DStringSetLength(&valueP->ds, memory_size);
        }
        /*
         * Note unlike for chars[] case, we do not need to explicitly
         * store nulls for pure-out parameters to protect against called
         * function leaving garbage in output buffer on error. The
         * DString function already ensure that.
         */
    }
    return TCL_OK;
}

/* Function: CffiArgPrepareUniString
 * Initializes a CffiValue to pass a unistring argument.
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - parameter type descriptor
 * valueObj - the Tcl_Obj containing string to pass. May be NULL for pure
 *   output parameters
 * nUnichars - max number of Tcl_UniChar characters for unistring type.
 * valueP - location to store the argument
 *
 * Arguments that are marked as pure input need not have a count specified
 * as the null terminator indicates end of string.
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with string in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
CffiResult
CffiArgPrepareUniString(Tcl_Interp *ip,
                        const CffiTypeAndAttrs *typeAttrsP,
                        Tcl_Obj *valueObj,
                        int nUnichars,
                        CffiValue *valueP)
{
    int len = 0;

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_UNISTRING);

    Tcl_DStringInit(&valueP->ds);
    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
        const Tcl_UniChar *s;
        s = Tcl_GetUnicodeFromObj(valueObj, &len);
        /* Note we copy the terminating two-byte end of string null as well */
        Tcl_DStringAppend(&valueP->ds, (char *)s, (len + 1) * sizeof(*s));
    }
    /* len is number of unichars stored in ds, excluding terminating \0\0 */
    if (typeAttrsP->flags & (CFFI_F_ATTR_OUT|CFFI_F_ATTR_INOUT)) {
        if (nUnichars <= 0) {
            return Tclh_ErrorInvalidValue(
                ip,
                NULL,
                "A count attribute must be specified for out and inout "
                "parameters of type unistring.");
        }

        /*
         * Ensure storage size is at least nUnichar Tcl_UniChar characters.
         * Note that there may already be stuff in ds in case of inout
         * parameters that would have been initialized above.
         */
        /*
         * It does not make sense if passed in length is greater than max output
         * buffer size. Something likely wrong on caller's part.
         */
        if (len > nUnichars) {
            return Tclh_ErrorInvalidValue(ip,
                                        NULL,
                                        "Inout unistring length is greater than "
                                        "specified maximum buffer size.");

        }
        if (len < nUnichars) {
            /* Note Tcl_DStringSetLength actually leaves room for trailing
             * \0 byte That is a single \0 byte, not a UniChar but we do not
             * use that anyways on output
             */
            Tcl_DStringSetLength(&valueP->ds,
                                 (nUnichars + 1) * sizeof(Tcl_UniChar));
        }
        if (typeAttrsP->flags & CFFI_F_ATTR_OUT) {
            /*
             * To protect against the called C function leaving the output
             * argument unmodified on error which would result in our
             * processing garbage in CffiArgPostProcess, set null terminator.
             * For inout, this is already initialized above. For pure out
             * we need to guard against this.
             */
            Tcl_UniChar *uniP = (Tcl_UniChar*)Tcl_DStringValue(&valueP->ds);
            *uniP             = 0;
        }
    }
    return TCL_OK;
}

/* Function: CffiExternalStringToObj
 * Wraps an encoded string/chars into a Tcl_Obj
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - descriptor for type and encoding
 * dsP - contains encoded string to wrap
 * resultObjP - location to store pointer to Tcl_Obj wrapping the string
 *
 * The string contained in *dsP* is converted to Tcl's internal form before
 * storing into the returned Tcl_Obj.
 *
 * Returns:
 * *TCL_OK* on success with wrapped pointer in *resultObjP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
CffiResult
CffiExternalStringToObj(Tcl_Interp *ip,
                        const CffiTypeAndAttrs *typeAttrsP,
                        Tcl_DString *dsP,
                        Tcl_Obj **resultObjP)
{
    char *srcP;
    int outbuf_size;
    Tcl_DString dsDecoded;
    Tcl_Encoding encoding;

    Tcl_DStringInit(&dsDecoded);

    srcP = Tcl_DStringValue(dsP);
    outbuf_size = Tcl_DStringLength(dsP); /* ORIGINAL size */
    if (srcP[outbuf_size]) {
        TCLH_PANIC("Buffer for output argument overrun.");
    }

    if (typeAttrsP->dataType.u.tagObj) {
        CHECK(Tcl_GetEncodingFromObj(
            ip, typeAttrsP->dataType.u.tagObj, &encoding));
    }
    else
        encoding = NULL;

    Tcl_ExternalToUtfDString(encoding, srcP, -1, &dsDecoded);
    if (encoding)
        Tcl_FreeEncoding(encoding);

    /* Should optimize this by direct transfer of ds storage - See TclDStringToObj */
    *resultObjP = Tcl_NewStringObj(Tcl_DStringValue(&dsDecoded),
                                   Tcl_DStringLength(&dsDecoded));
    Tcl_DStringFree(&dsDecoded);
    return TCL_OK;
}

/* Function: CffiUniStringToObj
 * Wraps an encoded unistring/unichars into a *Tcl_Obj*
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - descriptor for type and attributes
 * dsP - contains encoded string to wrap
 * resultObjP - location to store pointer to Tcl_Obj wrapping the string
 *
 * The string contained in *dsP* is converted to Tcl's internal form before
 * storing into the returned Tcl_Obj.
 *
 * Returns:
 * *TCL_OK* on success with wrapped pointer in *resultObjP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
CffiResult
CffiUniStringToObj(Tcl_Interp *ip,
                       const CffiTypeAndAttrs *typeAttrsP,
                       Tcl_DString *dsP,
                       Tcl_Obj **resultObjP)
{
    char *srcP;
    int outbuf_size;

    srcP = Tcl_DStringValue(dsP);
    outbuf_size = Tcl_DStringLength(dsP); /* ORIGINAL size */
    if (srcP[outbuf_size]) {
        TCLH_PANIC("Buffer for output argument overrun.");
    }

    /* TBD - Should optimize this by direct transfer of ds storage - See TclDStringToObj */
    *resultObjP = Tcl_NewUnicodeObj((Tcl_UniChar *) srcP, -1);
    return TCL_OK;
}

/* Function: CffiCharsFromObj
 * Encodes a Tcl_Obj to a character array based on a type encoding.
 *
 * Parameters:
 * ip - interpreter
 * encObj - *Tcl_Obj* holding encoding or *NULL*
 * fromObj - *Tcl_Obj* containing value to be stored
 * toP - buffer to store the encoded string
 * toSize - size of buffer
 *
 * Returns:
 * *TCL_OK* on success with  or *TCL_ERROR* * on failure with error message
 * in the interpreter.
 */
CffiResult
CffiCharsFromObj(
    Tcl_Interp *ip, Tcl_Obj *encObj, Tcl_Obj *fromObj, char *toP, int toSize)
{
    int fromLen;
    const char *fromP;
    Tcl_Encoding encoding;
    CffiResult ret;

    fromP = Tcl_GetStringFromObj(fromObj, &fromLen);
    /*
     * Note this encoding step is required even for UTF8 since Tcl's
     * internal UTF8 is not exactly UTF8
     */
    if (encObj) {
        /* Should not really fail since check should have happened at
         * prototype parsing time */
        CHECK(CffiGetEncodingFromObj(ip, encObj, &encoding));
    }
    else
        encoding = NULL;

    /*
     * TBD - Should we including TCL_ENCODING_STOPONERROR ? Currently
     * we do not because the fallback method using DStrings does not
     * support that behaviour.
     */
    ret = Tcl_UtfToExternal(ip,
                            encoding,
                            fromP,
                            fromLen,
                            TCL_ENCODING_START | TCL_ENCODING_END,
                            NULL,
                            toP,
                            toSize,
                            NULL,
                            NULL,
                            NULL);

    /*
     * The Tcl encoding routines need extra space while encoding to convert
     * a single max length character even though the actual input may not
     * have a character that needs that much space. So an attempt to
     * encode "abc" into (for example) utf-8 using a 4 byte buffer fails.
     * The Tcl_UtfToExternalDString do not work either because they
     * do not provide information as to number of null terminators for
     * the encoding. So we do the horrible hack below. - TBD
     */
    if (ret == TCL_CONVERT_NOSPACE) {
        Tcl_DString ds;
        char *external;
        int externalLen = toSize + 6; /* Max needed depending on TCL_UTF_MAX setting */
        Tcl_DStringInit(&ds);
        /* Preset the length leaving extra space */
        Tcl_DStringSetLength(&ds, externalLen);
        external    = Tcl_DStringValue(&ds);
        /* Set two bytes to ff so we know whether one null or two for encoding */
        external[toSize] = 0xff;
        external[toSize+1] = 0xff;

        ret = Tcl_UtfToExternal(ip,
                                encoding,
                                fromP,
                                fromLen,
                                TCL_ENCODING_START | TCL_ENCODING_END,
                                NULL,
                                external,
                                externalLen,
                                NULL,
                                &externalLen,
                                NULL);
        /* externalLen contains number of encoded bytes */
        if (ret == TCL_OK) {
            CFFI_ASSERT(external[externalLen] == '\0');
            ++externalLen; /* Terminating null */
            /* See if double terminator */
            if (external[externalLen] == '\0') {
                ++externalLen;
            }
            if (externalLen <= toSize)
                memmove(toP, external, externalLen);
            else {
                /* Really was a valid no space */
                ret = TCL_CONVERT_NOSPACE;
            }
        }
        Tcl_DStringFree(&ds);
    }
    if (encoding)
        Tcl_FreeEncoding(encoding);
    if (ret != TCL_OK) {
        const char *message;
        switch (ret) {
        case TCL_CONVERT_NOSPACE:
            message =
                "String length is greater than specified maximum buffer size.";
            break;
        case TCL_CONVERT_MULTIBYTE:
            message = "String ends in a partial multibyte encoding fragment.";
            break;
        case TCL_CONVERT_SYNTAX:
            message = "String contains invalid character sequence";
            break;
        case TCL_CONVERT_UNKNOWN:
            message = "String cannot be encoded in target encoding.";
            break;
        default:
            message = NULL;
            break;
        }
        return Tclh_ErrorInvalidValue(ip, fromObj, message);
    }
    return TCL_OK;
}

/* Function: CffiCharsToObj
 * Wraps an encoded chars into a Tcl_Obj
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - descriptor for type and encoding
 * srcP - points to null terminated encoded string to wrap
 * resultObjP - location to store pointer to Tcl_Obj wrapping the string
 *
 * The string pointed by *srcP* is converted to Tcl's internal form before
 * storing into the returned Tcl_Obj.
 *
 * Returns:
 * *TCL_OK* on success with wrapped pointer in *resultObjP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
CffiResult
CffiCharsToObj(Tcl_Interp *ip,
               const CffiTypeAndAttrs *typeAttrsP,
               char *srcP,
               Tcl_Obj **resultObjP)
{
    Tcl_DString dsDecoded;
    Tcl_Encoding encoding;
    int srcLen;

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_CHAR_ARRAY);
    CFFI_ASSERT(typeAttrsP->dataType.count > 0);

    Tcl_DStringInit(&dsDecoded);

    srcLen = Tclh_strlen(srcP);
    if (srcLen >= typeAttrsP->dataType.count) {
        TCLH_PANIC("Buffer for output argument overrun.");
    }

    if (typeAttrsP->dataType.u.tagObj) {
        CHECK(Tcl_GetEncodingFromObj(
            ip, typeAttrsP->dataType.u.tagObj, &encoding));
    }
    else
        encoding = NULL;

    Tcl_ExternalToUtfDString(encoding, srcP, srcLen, &dsDecoded);
    if (encoding)
        Tcl_FreeEncoding(encoding);

    /* Should optimize this by direct transfer of ds storage - See TclDStringToObj */
    *resultObjP = Tcl_NewStringObj(Tcl_DStringValue(&dsDecoded),
                                   Tcl_DStringLength(&dsDecoded));
    return TCL_OK;
}

/* Function: CffiArgPrepareChars
 * Initializes a CffiValue to pass a chars argument.
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - parameter type descriptor
 * valueObj - the Tcl_Obj containing string to pass. May be NULL for pure
 *   output parameters
 * valueP - location to store the argument
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with pointer to chars in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
CffiResult
CffiArgPrepareChars(CffiInterpCtx *ipCtxP,
                    const CffiTypeAndAttrs *typeAttrsP,
                    Tcl_Obj *valueObj,
                    CffiValue *valueP)
{
    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_CHAR_ARRAY);
    CFFI_ASSERT(typeAttrsP->dataType.count > 0);

    valueP->ptr = MemLifoAlloc(&ipCtxP->memlifo, typeAttrsP->dataType.count);

    /* If input, we need to encode appropriately */
    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT))
        return CffiCharsFromObj(ipCtxP->interp,
                                typeAttrsP->dataType.u.tagObj,
                                valueObj,
                                valueP->ptr,
                                typeAttrsP->dataType.count);
    else {
        /*
         * To protect against the called C function leaving the output
         * argument unmodified on error which would result in our
         * processing garbage in CffiArgPostProcess, set null terminator.
         */
        *(char *)valueP->ptr = '\0';
        /* In case encoding employs double nulls */
        if (typeAttrsP->dataType.count > 1)
            *(1 + (char *)valueP->ptr) = '\0';
        return TCL_OK;
    }
}

/* Function: CffiUniCharsFromObj
 * Encodes a Tcl_Obj to a *Tcl_UniChar* array.
 *
 * Parameters:
 * ip - interpreter
 * fromObj - *Tcl_Obj* containing value to be stored
 * toP - buffer to store the encoded string
 * toSize - size of buffer in *Tcl_UniChar* units
 *
 * Returns:
 * *TCL_OK* on success with  or *TCL_ERROR* * on failure with error message
 * in the interpreter.
 */
CffiResult
CffiUniCharsFromObj(
    Tcl_Interp *ip, Tcl_Obj *fromObj, char *toP, int toSize)
{
    int fromLen;
    Tcl_UniChar *fromP = Tcl_GetUnicodeFromObj(fromObj, &fromLen);
    ++fromLen; /* For terminating null */

    if (fromLen > toSize) {
        return Tclh_ErrorInvalidValue(ip,
                                      fromObj,
                                      "String length is greater than "
                                      "specified maximum buffer size.");
    }
    memmove(toP, fromP, fromLen * sizeof(Tcl_UniChar));
    return TCL_OK;
}

/* Function: CffiArgPrepareUniChars
 * Initializes a CffiValue to pass a unichars argument.
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - parameter type descriptor
 * valueObj - the Tcl_Obj containing value to pass. May be NULL for pure
 *   output parameters
 * valueP - location to store the argument
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with pointer to chars in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
CffiResult
CffiArgPrepareUniChars(CffiInterpCtx *ipCtxP,
                       const CffiTypeAndAttrs *typeAttrsP,
                       Tcl_Obj *valueObj,
                       CffiValue *valueP)
{
    int dstLen;

    dstLen = typeAttrsP->dataType.count;
    valueP->ptr =
        MemLifoAlloc(&ipCtxP->memlifo, dstLen * sizeof(Tcl_UniChar));
    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_UNICHAR_ARRAY);

    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT))
        return CffiUniCharsFromObj(
            ipCtxP->interp, valueObj, valueP->ptr, dstLen);
    else {
        /*
         * To protect against the called C function leaving the output
         * argument unmodified on error which would result in our
         * processing garbage in CffiArgPostProcess, set null terminator.
         */
        *(Tcl_UniChar *)valueP->ptr = 0;
        return TCL_OK;
    }
}

/* Function: CffiArgPrepareBinary
 * Initializes a CffiValue to pass a byte array argument.
 *
 * Parameters:
 * ip - interpreter
 * paramP - parameter descriptor
 * valueObj - the Tcl_Obj containing the byte array to pass. May be NULL
 *   for pure output parameters
 * nBytes - size of output buffer needed
 * valueP - location to store the argument
 *
 * For type *binary* out and inout parameters, the parameter type definition
 * must always specify a count which indicates the max number of bytes. For
 * *bytes* the type includes the count.
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with string in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
CffiResult
CffiArgPrepareBinary(Tcl_Interp *ip,
                     const CffiTypeAndAttrs *typeAttrsP,
                     Tcl_Obj *valueObj,
                     int nBytes,
                     CffiValue *valueP)
{
    Tcl_Obj *objP;
    int flags = typeAttrsP->flags;

    if (flags & (CFFI_F_ATTR_OUT|CFFI_F_ATTR_INOUT)) {
        int   len;
        if (nBytes <= 0) {
            return Tclh_ErrorInvalidValue(
                ip,
                NULL,
                "A count attribute must be specified for out and inout "
                "parameters of type binary.");
        }

        if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
            /* inout parameter */
            objP = Tcl_DuplicateObj(valueObj);
            (void)Tcl_GetByteArrayFromObj(objP, &len);
            if (len < nBytes)
                Tcl_SetByteArrayLength(objP, nBytes);
            /* Invalidate unshared string rep as called function may modify */
            Tcl_InvalidateStringRep(objP);
        }
        else {
            /* Out parameter */
            objP = Tcl_NewByteArrayObj(NULL, nBytes);
        }
    } else {
        /*
         * Pure input but could still shimmer so dup it. Potential
         * future optimizations TBD:
         *  - dup only if shared (ref count > 1)
         *  - use memlifo to copy bytes instead of duping
         */
        objP = Tcl_DuplicateObj(valueObj);
    }
    Tcl_IncrRefCount(objP);
    valueP->baObj = objP;

    return TCL_OK;
}

/* Function: CffiBytesFromObj
 * Encodes a Tcl_Obj to a C array of bytes
 *
 * Parameters:
 * ip - interpreter
 * fromObj - *Tcl_Obj* containing value to be stored
 * toP - buffer to store the encoded string
 * toSize - size of buffer
 *
 * Returns:
 * *TCL_OK* on success with  or *TCL_ERROR* * on failure with error message
 * in the interpreter.
 */
CffiResult
CffiBytesFromObj(Tcl_Interp *ip, Tcl_Obj *fromObj, char *toP, int toSize)
{
    int fromLen;
    unsigned char *fromP;

    fromP = Tcl_GetByteArrayFromObj(fromObj, &fromLen);
    if (fromLen > toSize) {
        return Tclh_ErrorInvalidValue(ip,
                                      NULL,
                                      "Byte array length is greater than "
                                      "specified maximum buffer size.");
    }
    memmove(toP, fromP, fromLen);
    return TCL_OK;
}

/* Function: CffiArgPrepareBytes
 * Initializes a CffiValue to pass a bytes argument.
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - parameter type descriptor
 * valueObj - the Tcl_Obj containing value to pass. May be NULL for pure
 *   output parameters
 * valueP - location to store the argument
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with pointer to chars in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
CffiResult
CffiArgPrepareBytes(CffiInterpCtx *ipCtxP,
                    const CffiTypeAndAttrs *typeAttrsP,
                    Tcl_Obj *valueObj,
                    CffiValue *valueP)
{
    int dstLen;

    dstLen = typeAttrsP->dataType.count;
    valueP->ptr = MemLifoAlloc(&ipCtxP->memlifo, dstLen);
    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_BYTE_ARRAY);

    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
        /* NOTE: because of shimmering possibility, we need to copy */
        return CffiBytesFromObj(ipCtxP->interp, valueObj, valueP->ptr, dstLen);
    }
    else
        return TCL_OK;
}

/* Function: CffiArgCleanup
 * Releases any resources stored within a CffiValue
 *
 * Parameters:
 * valueP - pointer to a value
 */
void CffiArgCleanup(const CffiTypeAndAttrs *typeAttrsP, CffiValue *valueP)
{
    /*
     * IMPORTANT: the logic here must be consistent with CffiArgPostProcess
     * and CffiArgPrepare. Any changes here should be reflected there too.
     */

    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
        Tcl_DStringFree(&valueP->ds);
        break;
    case CFFI_K_TYPE_BINARY:
        Tcl_DecrRefCount(valueP->baObj);
        break;
    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
        /* valueP->u.{charP,unicharP,bytesP} points to memlifo storage */
        /* FALLTHRU */
    default:
        /* Scalars have no storage to deallocate */
        break;
    }
}


/* Function: CffiCheckNumeric
 * Checks if the specified value meets requirements stipulated by the
 * type descriptor.
 *
 * Parameters:
 * ip - interpreter
 * valueP - value to be checked. Must contain an numeric type
 * typeAttrsP - type and attributes
 *
 * Returns:
 * *TCL_OK* if requirements pass, else *TCL_ERROR* with an error message in the
 * interpreter.
 */
CffiResult
CffiCheckNumeric(Tcl_Interp *ip,
                  CffiValue *valueP,
                  const CffiTypeAndAttrs *typeAttrsP)
{
    int flags = typeAttrsP->flags;
    int is_signed;
    Tcl_WideInt value;
    const char *message;
    Tcl_Obj    *valueObj;
    Tcl_WideInt sysError;

    if ((flags & CFFI_F_ATTR_REQUIREMENT_MASK) == 0)
        return TCL_OK; /* No checks requested */

    /* TBD - needs to be checked for correctness for 64-bit signed/unsigned */

    /* TBD - float and double */
    is_signed = 0;
    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_SCHAR: value = valueP->schar; is_signed = 1; break;
    case CFFI_K_TYPE_UCHAR: value = valueP->uchar; break;
    case CFFI_K_TYPE_SHORT: value = valueP->sshort; is_signed = 1; break;
    case CFFI_K_TYPE_USHORT: value = valueP->ushort; break;
    case CFFI_K_TYPE_INT: value = valueP->sint; is_signed = 1; break;
    case CFFI_K_TYPE_UINT: value = valueP->uint; break;
    case CFFI_K_TYPE_LONG: value = valueP->slong; is_signed = 1; break;
    case CFFI_K_TYPE_ULONG: value = valueP->ulong; break;
    case CFFI_K_TYPE_LONGLONG: value = valueP->slonglong; is_signed = 1; break;
    case CFFI_K_TYPE_ULONGLONG: value = valueP->ulonglong; break;
    case CFFI_K_TYPE_FLOAT:
        value     = (Tcl_WideInt)valueP->flt;
        is_signed = 1;
        break;
    case CFFI_K_TYPE_DOUBLE:
        value     = (Tcl_WideInt)valueP->dbl;
        is_signed = 1;
        break;
    default:
        return Tclh_ErrorWrongType(ip, NULL, "Invalid type for numeric check.");
    }

    if (value == 0) {
        if (flags & (CFFI_F_ATTR_NONZERO|CFFI_F_ATTR_POSITIVE)) {
            message = "Function returned zero.";
            goto failed_requirements;
        }
    }
    else if (flags & CFFI_F_ATTR_ZERO) {
        message = "Function returned non-zero value.";
        goto failed_requirements;
    }
    else {
        /* Non-0 value. */
        if ((flags & (CFFI_F_ATTR_NONNEGATIVE | CFFI_F_ATTR_POSITIVE))
            && is_signed && value < 0) {
            message = "Function returned negative value.";
            goto failed_requirements;
        }
    }

    return TCL_OK;

failed_requirements:
    /*
    * As soon as we know we failed, need to preserve system error. On
    * Windows, even Tcl_NewWideIntObj() will modify GetLastError()
    */
    sysError = CffiGrabSystemError(typeAttrsP, value);
    valueObj = Tcl_NewWideIntObj(value);
    Tcl_IncrRefCount(valueObj);
    CffiReportRequirementError(
        ip, typeAttrsP, value, valueObj, sysError, message);
    Tcl_DecrRefCount(valueObj);
    return TCL_ERROR;
}

/* Function: CffiReportRequirementError
 * Stores an error message in the interpreter based on the error reporting
 * mechanism for the type
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - type descriptor
 * value - the value, as *Tcl_WideInt* that failed requirements
 * valueObj - the value, as an *Tcl_Obj* that failed requirements (may be NULL)
 * message - additional message (may be NULL)
 *
 * Returns:
 * Always returns *TCL_ERROR*
 */
CffiResult
CffiReportRequirementError(Tcl_Interp *ip,
                            const CffiTypeAndAttrs *typeAttrsP,
                            Tcl_WideInt value,
                            Tcl_Obj *valueObj,
                            Tcl_WideInt sysError,
                            const char *message)
{
    int flags = typeAttrsP->flags;

#ifdef _WIN32
    if (flags & (CFFI_F_ATTR_LASTERROR | CFFI_F_ATTR_WINERROR)) {
        return Tclh_ErrorWindowsError(ip, (unsigned int) sysError, NULL);
    }
#endif

    if (flags & CFFI_F_ATTR_ERRNO) {
        char buf[256];
        char *bufP;
        buf[0] = 0;             /* Safety check against misconfig */
#ifdef _WIN32
        strerror_s(buf, sizeof(buf) / sizeof(buf[0]), (int) sysError);
        bufP = buf;
#else
        /*
         * This is tricky. See manpage for gcc/linux for the strerror_r
         * to be selected. BUT Apple for example does not follow the same
         * feature test macro conventions.
         */
#if _GNU_SOURCE || (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE < 200112L)
        bufP = strerror_r((int) sysError, buf, sizeof(buf) / sizeof(buf[0]));
        /* Returned bufP may or may not be buf! */
#else
        /* XSI/POSIX standard version */
        strerror_r((int) sysError, buf, sizeof(buf) / sizeof(buf[0]));
        bufP = buf;
#endif
#endif
        Tcl_SetResult(ip, bufP, TCL_VOLATILE);
        return TCL_ERROR;
    }

    /* Generic error */
    Tclh_ErrorInvalidValue(ip, valueObj, message);
    return TCL_ERROR;
}

/* Function: CffiTypeUnparse
 * Returns the script level definition of the internal form of a
 * CffiType structure.
 *
 * Parameters:
 * typeP - pointer to the CffiType structure
 *
 * Returns:
 * A Tcl_Obj containing the script level definition.
 */
Tcl_Obj *
CffiTypeUnparse(const CffiType *typeP)
{
    Tcl_Obj *typeObj;
    Tcl_Obj *suffix = NULL;
    int count = typeP->count;


    typeObj = Tcl_NewStringObj(cffiBaseTypes[typeP->baseType].token, -1);

    /* Note no need for IncrRefCount when adding to list */
    switch (typeP->baseType) {
    case CFFI_K_TYPE_POINTER:
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_CHAR_ARRAY:
        suffix = typeP->u.tagObj;
        break;
    case CFFI_K_TYPE_STRUCT:
        suffix = typeP->u.structP->name;
        break;
    default:
        break; /* Keep gcc happy about unlisted enum values */
    }

    if (suffix)
        Tcl_AppendStringsToObj(typeObj, ".", Tcl_GetString(suffix), NULL);

    if (count != 0)
        Tcl_AppendPrintfToObj(typeObj, "[%d]", count);

    return typeObj;
}

/* Function: CffiTypeAndAttrsUnparse
 * Returns the script level definition of the internal form of a
 * CffiTypeAndAttrs structure.
 *
 * Parameters:
 * typeAttrsP - pointer to the CffiTypeAndAttrs structure
 *
 * Returns:
 * A Tcl_Obj containing the script level definition.
 */
Tcl_Obj *
CffiTypeAndAttrsUnparse(const CffiTypeAndAttrs *typeAttrsP)
{
    Tcl_Obj *resultObj;
    CffiAttrs *attrsP;
    DCint callMode;
    int flags;
    int i;

    resultObj = Tcl_NewListObj(0, NULL);

    Tcl_ListObjAppendElement(
        NULL, resultObj, CffiTypeUnparse(&typeAttrsP->dataType));

    /* Only include calling mode if not default */
    callMode = typeAttrsP->callMode;
    if (callMode != DC_CALL_C_DEFAULT) {
        for (i = 0; i < sizeof(cffiCallModes) / sizeof(cffiCallModes[0]);
             ++i) {
            if (callMode == cffiCallModes[i].mode) {
                Tcl_ListObjAppendElement(
                    NULL,
                    resultObj,
                    Tcl_NewStringObj(cffiCallModes[i].modeStr, -1));
                break;
            }
        }
    }

    flags = typeAttrsP->flags;
    for (attrsP = &cffiAttrs[0]; attrsP->attrName != NULL; ++attrsP) {
        /* -1 -> Not a real attribute */
        if (attrsP->attrFlag != -1) {
            if (attrsP->attrFlag & flags) {
                Tcl_ListObjAppendElement(
                    NULL,
                    resultObj,
                    Tcl_NewStringObj(attrsP->attrName, -1));
            }
        }
    }

    if (typeAttrsP->defaultObj) {
        Tcl_Obj *objs[2];
        objs[0] = Tcl_NewStringObj("default", 7);
        objs[1] = typeAttrsP->defaultObj; /* No need for IncrRef - adding to list */
        Tcl_ListObjAppendElement(NULL, resultObj, Tcl_NewListObj(2, objs));
    }

    return resultObj;
}

Tcl_WideInt
CffiGrabSystemError(const CffiTypeAndAttrs *typeAttrsP,
                     Tcl_WideInt winError)
{
    Tcl_WideInt sysError = 0;
    if (typeAttrsP->flags & CFFI_F_ATTR_ERRNO)
        sysError = errno;
#ifdef _WIN32
    else if (typeAttrsP->flags & CFFI_F_ATTR_LASTERROR)
        sysError = GetLastError();
    else if (typeAttrsP->flags & CFFI_F_ATTR_WINERROR)
        sysError = winError;
#endif
    return sysError;
}

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
    /* First letter must be alpha, _ or : */
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

CffiResult
CffiTypeObjCmd(ClientData cdata,
                Tcl_Interp *ip,
                int objc,
                Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    CffiTypeAndAttrs typeAttrs;
    CffiResult ret;
    enum cmdIndex { INFO, SIZE, COUNT };
    int cmdIndex;
    int size, alignment;
    int parse_mode;
    static const Tclh_SubCommand subCommands[] = {
        {"info", 1, 2, "TYPE ?PARSEMODE?", NULL},
        {"size", 1, 1, "TYPE", NULL},
        {"count", 1, 1, "TYPE", NULL},
        {NULL}
    };

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));

    /* For type info, check if a parse mode is specified */
    parse_mode = -1;
    if (cmdIndex == INFO && objc == 4) {
        const char *s = Tcl_GetString(objv[3]);
        if (*s == '\0')
            parse_mode = -1;
        else if (!strcmp(s, "param"))
            parse_mode = CFFI_F_TYPE_PARSE_PARAM;
        else if (!strcmp(s, "return"))
            parse_mode = CFFI_F_TYPE_PARSE_RETURN;
        else if (!strcmp(s, "field"))
            parse_mode = CFFI_F_TYPE_PARSE_FIELD;
        else
            return Tclh_ErrorInvalidValue(ip, objv[3], "Invalid parse mode.");
    }
    ret = CffiTypeAndAttrsParse(ipCtxP, objv[2], parse_mode, &typeAttrs);
    if (ret == TCL_ERROR)
        return ret;

    if (cmdIndex == COUNT) {
        /* type nelems */
        Tcl_SetObjResult(ip, Tcl_NewIntObj(typeAttrs.dataType.count));
    }
    else {
        CffiTypeLayoutInfo(&typeAttrs.dataType, NULL, &size, &alignment);
        if (cmdIndex == SIZE)
            Tcl_SetObjResult(ip, Tcl_NewIntObj(size));
        else {
            /* type info */
            Tcl_Obj *objs[8];
            objs[0] = Tcl_NewStringObj("size", 4);
            objs[1] = Tcl_NewIntObj(size);
            objs[2] = Tcl_NewStringObj("count", 5);
            objs[3] = Tcl_NewIntObj(typeAttrs.dataType.count);
            objs[4] = Tcl_NewStringObj("alignment", 9);
            objs[5] = Tcl_NewIntObj(alignment);
            objs[6] = Tcl_NewStringObj("definition", 10);
            objs[7] = CffiTypeAndAttrsUnparse(&typeAttrs);
            Tcl_SetObjResult(ip, Tcl_NewListObj(8, objs));
        }
    }

    CffiTypeAndAttrsCleanup(&typeAttrs);
    return ret;
}
