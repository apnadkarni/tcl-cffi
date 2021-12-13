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
#define CFFI_VALID_INTEGER_ATTRS                           \
    (CFFI_F_ATTR_PARAM_MASK | CFFI_F_ATTR_REQUIREMENT_MASK \
     | CFFI_F_ATTR_ERROR_MASK | CFFI_F_ATTR_ENUM | CFFI_F_ATTR_BITMASK)

#define TOKENANDLEN(t) # t , sizeof(#t) - 1

typedef void (*CFFIARGPROC)();
const struct CffiBaseTypeInfo cffiBaseTypes[] = {
    {TOKENANDLEN(void), CFFI_K_TYPE_VOID, 0, 0},
    {TOKENANDLEN(schar),
     CFFI_K_TYPE_SCHAR,
     CFFI_VALID_INTEGER_ATTRS,
     sizeof(signed char)},
    {TOKENANDLEN(uchar),
     CFFI_K_TYPE_UCHAR,
     CFFI_VALID_INTEGER_ATTRS,
     sizeof(unsigned char)},
    {TOKENANDLEN(short),
     CFFI_K_TYPE_SHORT,
     CFFI_VALID_INTEGER_ATTRS,
     sizeof(signed short)},
    {TOKENANDLEN(ushort),
     CFFI_K_TYPE_USHORT,
     CFFI_VALID_INTEGER_ATTRS,
     sizeof(unsigned short)},
    {TOKENANDLEN(int),
     CFFI_K_TYPE_INT,
     CFFI_VALID_INTEGER_ATTRS,
     sizeof(signed int)},
    {TOKENANDLEN(uint),
     CFFI_K_TYPE_UINT,
     CFFI_VALID_INTEGER_ATTRS,
     sizeof(unsigned int)},
    {TOKENANDLEN(long),
     CFFI_K_TYPE_LONG,
     CFFI_VALID_INTEGER_ATTRS,
     sizeof(signed long)},
    {TOKENANDLEN(ulong),
     CFFI_K_TYPE_ULONG,
     CFFI_VALID_INTEGER_ATTRS,
     sizeof(unsigned long)},
    {TOKENANDLEN(longlong),
     CFFI_K_TYPE_LONGLONG,
     CFFI_VALID_INTEGER_ATTRS,
     sizeof(signed long long)},
    {TOKENANDLEN(ulonglong),
     CFFI_K_TYPE_ULONGLONG,
     CFFI_VALID_INTEGER_ATTRS,
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
    {TOKENANDLEN(struct),
     CFFI_K_TYPE_STRUCT,
     CFFI_F_ATTR_PARAM_MASK | CFFI_F_ATTR_NULLIFEMPTY,
     0},
    /* For pointer, only LASTERROR/ERRNO make sense for reporting errors */
    /*  */
    {TOKENANDLEN(pointer),
     CFFI_K_TYPE_POINTER,
     CFFI_F_ATTR_PARAM_MASK | CFFI_F_ATTR_SAFETY_MASK | CFFI_F_ATTR_NULLOK
         | CFFI_F_ATTR_LASTERROR | CFFI_F_ATTR_ERRNO | CFFI_F_ATTR_ONERROR,
     sizeof(void *)},
    {TOKENANDLEN(string),
     CFFI_K_TYPE_ASTRING,
     /* Note string cannot be INOUT parameter */
     CFFI_F_ATTR_IN | CFFI_F_ATTR_OUT | CFFI_F_ATTR_BYREF
         | CFFI_F_ATTR_NULLIFEMPTY | CFFI_F_ATTR_NULLOK | CFFI_F_ATTR_LASTERROR
         | CFFI_F_ATTR_ERRNO | CFFI_F_ATTR_ONERROR,
     sizeof(void *)},
    {TOKENANDLEN(unistring),
     CFFI_K_TYPE_UNISTRING,
     /* Note unistring cannot be INOUT parameter */
     CFFI_F_ATTR_IN | CFFI_F_ATTR_OUT | CFFI_F_ATTR_BYREF
         | CFFI_F_ATTR_NULLIFEMPTY | CFFI_F_ATTR_NULLOK | CFFI_F_ATTR_LASTERROR
         | CFFI_F_ATTR_ERRNO | CFFI_F_ATTR_ONERROR,
     sizeof(void *)},
    {TOKENANDLEN(binary),
     CFFI_K_TYPE_BINARY,
     /* Note binary cannot be OUT or INOUT parameters */
     CFFI_F_ATTR_IN | CFFI_F_ATTR_BYREF,
     sizeof(unsigned char *)},
    {TOKENANDLEN(chars),
     CFFI_K_TYPE_CHAR_ARRAY,
     CFFI_F_ATTR_PARAM_MASK,
     sizeof(char)},
    {TOKENANDLEN(unichars),
     CFFI_K_TYPE_UNICHAR_ARRAY,
     CFFI_F_ATTR_PARAM_MASK,
     sizeof(Tcl_UniChar)},
    {TOKENANDLEN(bytes),
     CFFI_K_TYPE_BYTE_ARRAY,
     CFFI_F_ATTR_PARAM_MASK,
     sizeof(unsigned char)},
    {NULL}};

static struct {
    const char *modeStr;
    DCint mode;
} cffiCallModes[] = {{"c", DC_CALL_C_DEFAULT}, /* Assumed to be first! */
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
    COUNTED,
    UNSAFE,
    DISPOSE,
    DISPOSEONSUCCESS,
    ZERO,
    NONZERO,
    NONNEGATIVE,
    POSITIVE,
    LASTERROR,
    ERRNO,
    WINERROR,
    DEFAULT,
    NULLIFEMPTY,
    STOREONERROR,
    STOREALWAYS,
    ENUM,
    BITMASK,
    ONERROR,
    NULLOK,
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
    {"disposeonsuccess",
     DISPOSEONSUCCESS,
     CFFI_F_ATTR_DISPOSEONSUCCESS,
     CFFI_F_TYPE_PARSE_PARAM,
     1},
    {"zero", ZERO, CFFI_F_ATTR_ZERO, CFFI_F_TYPE_PARSE_RETURN, 1},
    {"nonzero", NONZERO, CFFI_F_ATTR_NONZERO, CFFI_F_TYPE_PARSE_RETURN, 1},
    {"nonnegative",
     NONNEGATIVE,
     CFFI_F_ATTR_NONNEGATIVE,
     CFFI_F_TYPE_PARSE_RETURN,
     1},
    {"positive", POSITIVE, CFFI_F_ATTR_POSITIVE, CFFI_F_TYPE_PARSE_RETURN, 1},
    {"errno",
     ERRNO,
     CFFI_F_ATTR_ERRNO,
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_FIELD
         | CFFI_F_TYPE_PARSE_RETURN,
     1},
#ifdef _WIN32
    {"lasterror",
     LASTERROR,
     CFFI_F_ATTR_LASTERROR,
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_FIELD
         | CFFI_F_TYPE_PARSE_RETURN,
     1},
    {"winerror",
     WINERROR,
     CFFI_F_ATTR_WINERROR,
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_FIELD
         | CFFI_F_TYPE_PARSE_RETURN,
     1},
#endif
    {"default", DEFAULT, -1, CFFI_F_TYPE_PARSE_PARAM, 2},
    {"nullifempty",
     NULLIFEMPTY,
     CFFI_F_ATTR_NULLIFEMPTY,
     CFFI_F_TYPE_PARSE_PARAM,
     1},
    {"storeonerror",
     STOREONERROR,
     CFFI_F_ATTR_STOREONERROR,
     CFFI_F_TYPE_PARSE_PARAM,
     1},
    {"storealways",
     STOREALWAYS,
     CFFI_F_ATTR_STOREALWAYS,
     CFFI_F_TYPE_PARSE_PARAM,
     1},
    {"enum",
     ENUM,
     CFFI_F_ATTR_ENUM,
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_RETURN,
     2},
    {"bitmask", BITMASK, CFFI_F_ATTR_BITMASK, CFFI_F_TYPE_PARSE_PARAM, 1},
    {"onerror",
     ONERROR,
     CFFI_F_ATTR_ONERROR,
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_FIELD
         | CFFI_F_TYPE_PARSE_RETURN,
     2},
    {"nullok",
     NULLOK,
     CFFI_F_ATTR_NULLOK,
     CFFI_F_TYPE_PARSE_RETURN | CFFI_F_TYPE_PARSE_PARAM
         | CFFI_F_TYPE_PARSE_FIELD,
     1},
    {NULL}};


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
        toP->countHolderObj = fromP->countHolderObj;
        if (toP->countHolderObj)
            Tcl_IncrRefCount(toP->countHolderObj);
        if (fromP->baseType == CFFI_K_TYPE_STRUCT) {
            if (fromP->u.structP)
                CffiStructRef(fromP->u.structP);
            toP->u.structP = fromP->u.structP;
        }
        else {
            if (fromP->u.tagObj)
                Tcl_IncrRefCount(fromP->u.tagObj);
            toP->u.tagObj = fromP->u.tagObj;
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
    const char *tagStr;/* Points to start of tag if any */
    const char *lbStr; /* Points to left bracket, if any */
    const CffiBaseTypeInfo *baseTypeInfoP;
    CffiBaseType baseType;
    CffiResult ret;

    CFFI_ASSERT((sizeof(cffiBaseTypes)/sizeof(cffiBaseTypes[0]) - 1) == CFFI_K_NUM_TYPES);

    CffiTypeInit(typeP, NULL);
    CFFI_ASSERT(typeP->count == 0); /* Code below assumes these values on init */
    CFFI_ASSERT(typeP->countHolderObj == NULL);
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
        lbStr = NULL;
        tagLen   = 0;
    }
    else if (typeStr[tokenLen] == '[') {
        /* TYPE[5] */
        tagStr   = NULL;
        lbStr = typeStr + tokenLen;
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
        lbStr = strchr(tagStr, '[');
        if (lbStr)
            tagLen  = (int)(lbStr - tagStr); /* TYPE.TAG[N] */
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
        if (lbStr) {
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
    if (lbStr) {
        /* An array element count is specified, either as int or a symbol */
        char ch;
        char rightbracket;
        int count;
        int nfields;
        const char *countStr = lbStr + 1;
        nfields = sscanf(countStr, "%d%c%c", &count, &rightbracket, &ch);
        if (nfields == 2) {
            /* Count specified as an integer */
            if (count <= 0 || rightbracket != ']')
                goto invalid_array_size;
            typeP->count = count;
        }
        else {
            /* Count specified as the name of some other thing */
            const char *p;
            if (!isascii(*(unsigned char *)countStr)
                || !isalpha(*(unsigned char *)countStr))
                goto invalid_array_size;
            p = strchr(countStr, ']');
            if (p == NULL || p[1] != '\0' || p == countStr)
                goto invalid_array_size;
            typeP->count = -1;
            typeP->countHolderObj =
                Tcl_NewStringObj(countStr, (int) (p - countStr));
            Tcl_IncrRefCount(typeP->countHolderObj);
        }
    }

    /*
     * chars, unichars and bytes must have the count specified.
     * pointers, astrings, unistrings and bytes - arrays not implemented yet - TBD.
     */
    switch (typeP->baseType) {
    case CFFI_K_TYPE_VOID:
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
    case CFFI_K_TYPE_BINARY:
        if (typeP->count != 0) {
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

invalid_array_size:
    message = "Invalid array size or extra trailing characters.";
    goto invalid_type;
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
    Tclh_ObjClearPtr(&typeP->countHolderObj);

    if (typeP->baseType == CFFI_K_TYPE_STRUCT) {
        if (typeP->u.structP) {
            CffiStructUnref(typeP->u.structP);
            typeP->u.structP = NULL;
        }
    }
    else {
        Tclh_ObjClearPtr(&typeP->u.tagObj);
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
 *           if known, or -1 if size is dynamic.
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
    if (sizeP) {
        if (typeP->count == 0)
            *sizeP = baseSize;
        else if (typeP->count < 0)
            *sizeP = -1; /* Variable size array */
        else
            *sizeP = typeP->count * baseSize;
    }
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
        if (fromP->parseModeSpecificObj)
            Tcl_IncrRefCount(fromP->parseModeSpecificObj);
        toP->parseModeSpecificObj  = fromP->parseModeSpecificObj;
        toP->flags       = fromP->flags;
        CffiTypeInit(&toP->dataType, &fromP->dataType);
    }
    else {
        toP->parseModeSpecificObj  = NULL;
        toP->flags       = 0;
        CffiTypeInit(&toP->dataType, NULL);
    }
}

/* Function: CffiTypeAndAttrsParse
 * Parses a type and attribute definition into an internal form.
 *
 * A definition is a list of the form
 *   TYPE ?attr ...?
 * where *TYPE* is a type definition as parsed by <CffiTypeParse>.
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
    static const char *paramAnnotClashMsg = "Unknown, repeated or conflicting type annotations specified.";
    static const char *defaultNotAllowedMsg =
        "Defaults are not allowed in this declaration context.";
    static const char *typeInvalidForContextMsg = "The specified type is not valid for the type declaration context.";
    const char *message;

    message = paramAnnotClashMsg;

    CHECK( Tcl_ListObjGetElements(ip, typeAttrObj, &nobjs, &objs) );

    if (nobjs == 0) {
        return Tclh_ErrorInvalidValue(ip, typeAttrObj, "Empty type declaration.");
    }

    /* First check for a type definition before base types */
    found = CffiAliasGet(ipCtxP, objs[0], typeAttrP);
    if (found)
        baseType = typeAttrP->dataType.baseType;
    else {
        CffiTypeAndAttrsInit(typeAttrP, NULL);
        CHECK(CffiTypeParse(ip, objs[0], &typeAttrP->dataType));
        baseType = typeAttrP->dataType.baseType;
        typeAttrP->parseModeSpecificObj = NULL;
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

        if (Tcl_ListObjGetElements(ip, objs[i], &nFields, &fieldObjs) != TCL_OK
            || nFields == 0) {
            goto invalid_format;
        }
        if (Tcl_GetIndexFromObjStruct(NULL,
                                      fieldObjs[0],
                                      cffiAttrs,
                                      sizeof(cffiAttrs[0]),
                                      "type annotation",
                                      TCL_EXACT,
                                      &attrIndex)
            != TCL_OK) {
            message = "Unrecognized type annotation.";
            goto invalid_format;
        }
        if (nFields != cffiAttrs[attrIndex].nAttrArgs) {
            message = "A type annotation has the wrong number of fields.";
            goto invalid_format;
        }
        if ((cffiAttrs[attrIndex].attrFlag & validAttrs) == 0) {
            message = "A type annotation is not valid for the data type.";
            goto invalid_format;
        }
        switch (cffiAttrs[attrIndex].attr) {
        case PARAM_IN:
            if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_OUT|CFFI_F_ATTR_INOUT))
                goto invalid_format;
            flags |= CFFI_F_ATTR_IN;
            break;
        case PARAM_OUT:
            if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_OUT|CFFI_F_ATTR_INOUT))
                goto invalid_format;
            flags |= CFFI_F_ATTR_OUT;
            break;
        case PARAM_INOUT:
            if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_OUT|CFFI_F_ATTR_INOUT))
                goto invalid_format;
            flags |= CFFI_F_ATTR_INOUT;
            break;
        case BYREF:
            flags |= CFFI_F_ATTR_BYREF;
            break;
        case DEFAULT:
            /*
             * Note no type checking of value here as some checks, such as
             * dynamic array lengths can only be done at call time.
             */
            if (typeAttrP->parseModeSpecificObj)
                goto invalid_format; /* Duplicate def */
            /* Need next check because DEFAULT is not an attribute and
               thus not part of the table based check below this switch */
            if (!(parseMode & CFFI_F_TYPE_PARSE_PARAM)) {
                message = defaultNotAllowedMsg;
                goto invalid_format;
            }
            Tcl_IncrRefCount(fieldObjs[1]);
            typeAttrP->parseModeSpecificObj = fieldObjs[1];
            break;
        case COUNTED:
            if (flags & CFFI_F_ATTR_UNSAFE)
                goto invalid_format;
            flags |= CFFI_F_ATTR_COUNTED;
            break;
        case UNSAFE:
            if (flags
                & (CFFI_F_ATTR_COUNTED | CFFI_F_ATTR_DISPOSE
                   | CFFI_F_ATTR_DISPOSEONSUCCESS))
                goto invalid_format;
            flags |= CFFI_F_ATTR_UNSAFE;
            break;
        case DISPOSE:
            if (flags & (CFFI_F_ATTR_DISPOSEONSUCCESS | CFFI_F_ATTR_UNSAFE))
                goto invalid_format;
            flags |= CFFI_F_ATTR_DISPOSE;
            break;
        case DISPOSEONSUCCESS:
            if (flags & (CFFI_F_ATTR_DISPOSE | CFFI_F_ATTR_UNSAFE))
                goto invalid_format;
            flags |= CFFI_F_ATTR_DISPOSEONSUCCESS;
            break;
        case ZERO:
            if (flags & CFFI_F_ATTR_REQUIREMENT_MASK)
                goto invalid_format;
            flags |= CFFI_F_ATTR_ZERO;
            break;
        case NONZERO:
            if (flags & CFFI_F_ATTR_REQUIREMENT_MASK)
                goto invalid_format;
            flags |= CFFI_F_ATTR_NONZERO;
            break;
        case NONNEGATIVE:
            if (flags & CFFI_F_ATTR_REQUIREMENT_MASK)
                goto invalid_format;
            flags |= CFFI_F_ATTR_NONNEGATIVE;
            break;
        case POSITIVE:
            if (flags & CFFI_F_ATTR_REQUIREMENT_MASK)
                goto invalid_format;
            flags |= CFFI_F_ATTR_POSITIVE;
            break;
        case ERRNO:
            if (parseMode & CFFI_F_TYPE_PARSE_RETURN) {
                if (flags & CFFI_F_ATTR_ERROR_MASK)
                    goto invalid_format;
                flags |= CFFI_F_ATTR_ERRNO;
            }
            break;
        case LASTERROR:
            if (parseMode & CFFI_F_TYPE_PARSE_RETURN) {
                if (flags & CFFI_F_ATTR_ERROR_MASK)
                    goto invalid_format;
                flags |= CFFI_F_ATTR_LASTERROR;
            }
            break;
        case WINERROR:
            if (parseMode & CFFI_F_TYPE_PARSE_RETURN) {
                if (flags & CFFI_F_ATTR_ERROR_MASK)
                    goto invalid_format;
                flags |= CFFI_F_ATTR_WINERROR;
            }
            break;
        case NULLIFEMPTY:
            flags |= CFFI_F_ATTR_NULLIFEMPTY;
            break;
        case STOREONERROR:
            if (flags & CFFI_F_ATTR_STOREALWAYS)
                goto invalid_format;
            flags |= CFFI_F_ATTR_STOREONERROR;
            break;
        case STOREALWAYS:
            if (flags & CFFI_F_ATTR_STOREONERROR)
                goto invalid_format;
            flags |= CFFI_F_ATTR_STOREALWAYS;
            break;
        case ENUM:
            if (typeAttrP->dataType.u.tagObj)
                goto invalid_format; /* Something already using the slot? */
#if 0
            if (typeAttrP->dataType.count != 0) {
                message = "\"enum\" annotation not valid for arrays.";
                goto invalid_format;
            }
#endif
            flags |= CFFI_F_ATTR_ENUM;
            Tcl_IncrRefCount(fieldObjs[1]);
            typeAttrP->dataType.u.tagObj = fieldObjs[1];
            break;
        case BITMASK:
            if (typeAttrP->dataType.count != 0) {
                message = "\"bitmask\" annotation not valid for arrays.";
                goto invalid_format;
            }
            flags |= CFFI_F_ATTR_BITMASK;
            break;
        case ONERROR:
            /* Ignore excecpt in return mode */
            if (parseMode & CFFI_F_TYPE_PARSE_RETURN) {
                if (typeAttrP->parseModeSpecificObj)
                    goto invalid_format; /* Something already using the slot? */
                if (flags & CFFI_F_ATTR_ERROR_MASK)
                    goto invalid_format;
                flags |= CFFI_F_ATTR_ONERROR;
                Tcl_IncrRefCount(fieldObjs[1]);
                typeAttrP->parseModeSpecificObj = fieldObjs[1];
            }
            break;
        case NULLOK:
            flags |= CFFI_F_ATTR_NULLOK;
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
                message = "A type annotation is not valid for the declaration context.";
                goto invalid_format;
            }
        }
    }

    /* winerror only makes sense for the zero requirement */
    if (flags & CFFI_F_ATTR_WINERROR) {
        int requirements_flags = flags & CFFI_F_ATTR_REQUIREMENT_MASK;
        if (requirements_flags && requirements_flags != CFFI_F_ATTR_ZERO)
            goto invalid_format;
    }

    switch (parseMode) {
    case CFFI_F_TYPE_PARSE_PARAM:
        if (baseType == CFFI_K_TYPE_VOID) {
            message = typeInvalidForContextMsg;
            goto invalid_format;
        }
        /* For parameters at least one of IN, OUT, INOUT should be set */
        if (flags & (CFFI_F_ATTR_INOUT|CFFI_F_ATTR_OUT)) {
            if (typeAttrP->parseModeSpecificObj) {
                message = defaultNotAllowedMsg;
                goto invalid_format;
            }
            /*
             * NULLIFEMPTY never allowed for any output. DISPOSE*, BITMASK,
             * ENUM not allowed for pure OUT.
             */
            if ((flags & CFFI_F_ATTR_NULLIFEMPTY)
                || ((flags & CFFI_F_ATTR_OUT)
                    && (flags
                        & (CFFI_F_ATTR_DISPOSE | CFFI_F_ATTR_DISPOSEONSUCCESS
                           | CFFI_F_ATTR_BITMASK)))) {
                message = "One or more annotations are invalid for the "
                          "parameter direction.";
                goto invalid_format;
            }
            flags |= CFFI_F_ATTR_BYREF; /* out, inout always byref */
        }
        else {
            flags |= CFFI_F_ATTR_IN; /* in, or by default if nothing was said */
            if (flags & (CFFI_F_ATTR_STOREONERROR | CFFI_F_ATTR_STOREALWAYS)) {
                message = "Annotations \"storeonerror\" and \"storealways\" "
                          "not allowed for \"in\" parameters.";
                goto invalid_format;
            }

            if (typeAttrP->dataType.count != 0)
                flags |= CFFI_F_ATTR_BYREF; /* Arrays always by reference */
            else {
                /* Certain types always by reference */
                switch (baseType) {
                case CFFI_K_TYPE_CHAR_ARRAY:
                case CFFI_K_TYPE_UNICHAR_ARRAY:
                case CFFI_K_TYPE_BYTE_ARRAY:
                    flags |=
                        CFFI_F_ATTR_BYREF; /* Arrays always by reference */
                    break;
                case CFFI_K_TYPE_STRUCT:
                    if ((flags & CFFI_F_ATTR_BYREF) == 0) {
                        message = "Passing of structs by value is not "
                                  "supported. Annotate with \"byref\" to pass by "
                                  "reference if function expects a pointer.";
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
        case CFFI_K_TYPE_STRUCT:
        case CFFI_K_TYPE_BINARY:
        case CFFI_K_TYPE_CHAR_ARRAY:
        case CFFI_K_TYPE_UNICHAR_ARRAY:
        case CFFI_K_TYPE_BYTE_ARRAY:
            /* return type not allowed even byref */
            message = typeInvalidForContextMsg;
            goto invalid_format;
        break;
        case CFFI_K_TYPE_VOID: /* FALLTHRU */
        default:
            if (typeAttrP->dataType.count != 0) {
                message = "Function return type must not be an array.";
                goto invalid_format;
            }
            if ((flags & CFFI_F_ATTR_ONERROR)
                && (flags & CFFI_F_ATTR_REQUIREMENT_MASK) == 0
                && baseType != CFFI_K_TYPE_POINTER
                && baseType != CFFI_K_TYPE_ASTRING
                && baseType != CFFI_K_TYPE_UNISTRING) {
                message = "\"onerror\" requires an error checking annotation.";
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
            message = typeInvalidForContextMsg;
            goto invalid_format;
        case CFFI_K_TYPE_CHAR_ARRAY:
        case CFFI_K_TYPE_UNICHAR_ARRAY:
        case CFFI_K_TYPE_BYTE_ARRAY:
            if (typeAttrP->dataType.count <= 0) {
                message = "Fields of type chars, unichars or bytes must be fixed size "
                          "arrays.";
                goto invalid_format;
            }
            break;
        default:
            if (typeAttrP->dataType.count < 0) {
                message = "Fields cannot be arrays of variable size.";
                goto invalid_format;
            }
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
    Tclh_ObjClearPtr(&typeAttrsP->parseModeSpecificObj);
    CffiTypeCleanup(&typeAttrsP->dataType);
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
 * count - number of values pointed to by valueP. *0* indicates a scalar
 *         positive number (even *1*) is size of array. Negative number
 *         will panic (dynamic sizes should have bee resolved before call)
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
                     int count,
                     Tcl_Obj **valueObjP)
{
    int ret;
    Tcl_Obj *valueObj;
    CffiBaseType baseType = typeAttrsP->dataType.baseType;

    CFFI_ASSERT(count >= 0);
    CFFI_ASSERT(baseType != CFFI_K_TYPE_ASTRING
                && baseType != CFFI_K_TYPE_UNISTRING
                && baseType != CFFI_K_TYPE_BINARY);

    switch (baseType) {
    case CFFI_K_TYPE_STRUCT:
        if (count == 0) {
            return CffiStructToObj(
                ip, typeAttrsP->dataType.u.structP, valueP, valueObjP);
        }
        else {
            /* Array, possible even a single element, still represent as list */
            Tcl_Obj *listObj;
            int i;
            int offset;
            int elem_size;

            elem_size = typeAttrsP->dataType.u.structP->size;
            /* TBD - may be allocate Tcl_Obj* array from memlifo for speed */
            listObj = Tcl_NewListObj(count, NULL);
            for (i = 0, offset = 0; i < count; ++i, offset += elem_size) {
                ret = CffiStructToObj(
                    ip, typeAttrsP->dataType.u.structP, offset + (char *)valueP, &valueObj);
                if (ret != TCL_OK) {
                    Tcl_DecrRefCount(listObj);
                    return ret;
                }
                Tcl_ListObjAppendElement(ip, listObj, valueObj);
            }
            *valueObjP = listObj;
            return TCL_OK;
        }
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

            CffiTypeLayoutInfo(&typeAttrsP->dataType, &elem_size, NULL, NULL);
            CFFI_ASSERT(elem_size > 0);
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

/* Function: CffiPointerArgsDispose
 * Disposes pointer arguments
 *
 * Parameters:
 * ip - interpreter
 * protoP - prototype structure with argument descriptors
 * argsP - array of argument values
 * callFailed - 0 if the function invocation had succeeded, else non-0
 *
 * The function loops through all argument values that are pointers
 * and annotated as *dispose* or *disposeonsuccess*. Any such pointers
 * are unregistered.
 *
 * Returns:
 * Nothing.
 */
void
CffiPointerArgsDispose(Tcl_Interp *ip,
                       CffiProto *protoP,
                       CffiArgument *argsP,
                       int callFailed)
{
    int i;
    for (i = 0; i < protoP->nParams; ++i) {
        CffiTypeAndAttrs *typeAttrsP = &protoP->params[i].typeAttrs;
        if (typeAttrsP->dataType.baseType == CFFI_K_TYPE_POINTER
            && (typeAttrsP->flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT))) {
            /*
             * DISPOSE - always dispose of pointer
             * DISPOSEONSUCCESS - dispose if the call had returned successfully
             */
            if ((typeAttrsP->flags & CFFI_F_ATTR_DISPOSE)
                || ((typeAttrsP->flags & CFFI_F_ATTR_DISPOSEONSUCCESS)
                    && !callFailed)) {
                int nptrs = argsP[i].actualCount;
                /* Note no error checks because the CffiFunctionSetup calls
                   above would have already done validation */
                if (nptrs <= 1) {
                    if (argsP[i].savedValue.u.ptr != NULL)
                        Tclh_PointerUnregister(
                            ip, argsP[i].savedValue.u.ptr, NULL);
                }
                else {
                    int j;
                    void **ptrArray = argsP[i].savedValue.u.ptr;
                    CFFI_ASSERT(ptrArray);
                    for (j = 0; j < nptrs; ++j) {
                        if (ptrArray[j] != NULL)
                            Tclh_PointerUnregister(ip, ptrArray[j], NULL);
                    }
                }
            }
        }
    }
}

/* Function: CffiCheckPointer
 * Checks if a pointer meets requirements annotations.
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - descriptor for type and attributes
 * pointer - pointer value to check
 * sysErrorP - location to store system error
 *
 * Returns:
 * *TCL_OK* if requirements pass, else *TCL_ERROR* with an error.
 */
CffiResult
CffiCheckPointer(Tcl_Interp *ip,
                  const CffiTypeAndAttrs *typeAttrsP,
                  void *pointer,
                  Tcl_WideInt *sysErrorP)

{
    int flags = typeAttrsP->flags;

    if (pointer || (flags & CFFI_F_ATTR_NULLOK))
        return TCL_OK;
    *sysErrorP =
        CffiGrabSystemError(typeAttrsP, (Tcl_WideInt)(intptr_t)pointer);
    return TCL_ERROR;
}

/* Function: CffiPointerToObj
 * Wraps a pointer into a Tcl_Obj based on type settings.
 *
 * If the pointer is not NULL, it is registered if the type attributes
 * indicate it should be wrapped a safe pointer.
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

    if (pointer == NULL) {
        /* NULL pointers are never registered */
        *resultObjP = Tclh_PointerWrap(NULL, typeAttrsP->dataType.u.tagObj);
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
 *
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
    Tcl_Obj *tagObj = typeAttrsP->dataType.u.tagObj;
    void *pv;

    CHECK(Tclh_PointerUnwrap(ip, pointerObj, &pv, tagObj));

    if (pv == NULL) {
        if ((typeAttrsP->flags & CFFI_F_ATTR_NULLOK) == 0) {
            return Tclh_ErrorInvalidValue(ip, NULL, "Pointer is NULL.");
        }
    }
    else {
        /*
         * Do checks for safe pointers. Note: Cannot use Tclh_PointerObjVerify
         * because that rejects NULL pointers.
         */
        if (!(typeAttrsP->flags & CFFI_F_ATTR_UNSAFE)) {
            CHECK(Tclh_PointerVerify(ip, pv, tagObj));
        }
    }

    *pointerP = pv;
    return TCL_OK;
}


/* Function: CffiExternalCharPToObj
 * Wraps an encoded string/chars passed as a char* into a Tcl_Obj
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - descriptor for type and encoding
 * srcP - points to null terminated encoded string to wrap. NULL is treated
 *        as an empty string.
 * resultObjP - location to store pointer to Tcl_Obj wrapping the string
 *
 * The string is converted to Tcl's internal form before
 * storing into the returned Tcl_Obj.
 *
 * Returns:
 * *TCL_OK* on success with wrapped pointer in *resultObjP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
CffiResult
CffiExternalCharsToObj(Tcl_Interp *ip,
                       const CffiTypeAndAttrs *typeAttrsP,
                       const char *srcP,
                       Tcl_Obj **resultObjP)
{
    Tcl_Encoding encoding;
    Tcl_DString ds;

    if (srcP == NULL) {
        *resultObjP = Tcl_NewObj();
        return TCL_OK;
    }

    if (typeAttrsP && typeAttrsP->dataType.u.tagObj) {
        CHECK(Tcl_GetEncodingFromObj(
            ip, typeAttrsP->dataType.u.tagObj, &encoding));
    }
    else
        encoding = NULL;

    Tcl_ExternalToUtfDString(encoding, srcP, -1, &ds);
    if (encoding)
        Tcl_FreeEncoding(encoding);

    /* Should optimize this by direct transfer of ds storage - See TclDStringToObj */
    *resultObjP = Tcl_NewStringObj(Tcl_DStringValue(&ds),
                                   Tcl_DStringLength(&ds));
    Tcl_DStringFree(&ds);
    return TCL_OK;
}

/* Function: CffiExternalDStringToObj
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
CffiExternalDStringToObj(Tcl_Interp *ip,
                         const CffiTypeAndAttrs *typeAttrsP,
                         Tcl_DString *dsP,
                         Tcl_Obj **resultObjP)
{
    const char *srcP;
    int outbuf_size;

    srcP = Tcl_DStringValue(dsP);
    outbuf_size = Tcl_DStringLength(dsP); /* ORIGINAL size */
    if (srcP[outbuf_size]) {
        TCLH_PANIC("Buffer for output argument overrun.");
    }

    return CffiExternalCharsToObj(ip, typeAttrsP, srcP, resultObjP);
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

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_CHAR_ARRAY);

    Tcl_DStringInit(&dsDecoded);

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
    return TCL_OK;
}

/* Function: CffiArgPrepareChars
 * Initializes a CffiValue to pass a chars argument.
 *
 * Parameters:
 * callP - the call context
 * arg_index - index into argument array of call context
 * valueObj - the value passed in from script. May be NULL for pure output.
 * valueP - location of native value to store
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with pointer to chars in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
static CffiResult
CffiArgPrepareChars(CffiCall *callP,
                    int arg_index,
                    Tcl_Obj *valueObj,
                    CffiValue *valueP)
{
    CffiInterpCtx *ipCtxP = callP->fnP->vmCtxP->ipCtxP;
    CffiArgument *argP    = &callP->argsP[arg_index];
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_CHAR_ARRAY);
    CFFI_ASSERT(argP->actualCount > 0);

    valueP->u.ptr =
        MemLifoAlloc(&ipCtxP->memlifo, argP->actualCount);

    /* If input, we need to encode appropriately */
    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT))
        return CffiCharsFromObj(ipCtxP->interp,
                                typeAttrsP->dataType.u.tagObj,
                                valueObj,
                                valueP->u.ptr,
                                argP->actualCount);
    else {
        /*
         * To protect against the called C function leaving the output
         * argument unmodified on error which would result in our
         * processing garbage in CffiArgPostProcess, set null terminator.
         */
        *(char *)valueP->u.ptr = '\0';
        /* In case encoding employs double nulls */
        if (argP->actualCount > 1)
            *(1 + (char *)valueP->u.ptr) = '\0';
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
 * callP - the call context
 * arg_index - index into argument array of call context
 * valueObj - the value passed in from script. May be NULL for pure output.
 * valueP - location of native value to store
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with pointer to chars in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
static CffiResult
CffiArgPrepareUniChars(CffiCall *callP,
                       int arg_index,
                       Tcl_Obj *valueObj,
                       CffiValue *valueP)
{
    CffiInterpCtx *ipCtxP = callP->fnP->vmCtxP->ipCtxP;
    CffiArgument *argP    = &callP->argsP[arg_index];
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;

    valueP->u.ptr =
        MemLifoAlloc(&ipCtxP->memlifo, argP->actualCount * sizeof(Tcl_UniChar));
    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_UNICHAR_ARRAY);

    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
        return CffiUniCharsFromObj(
            ipCtxP->interp, valueObj, valueP->u.ptr, argP->actualCount);
    }
    else {
        /*
         * To protect against the called C function leaving the output
         * argument unmodified on error which would result in our
         * processing garbage in CffiArgPostProcess, set null terminator.
         */
        *(Tcl_UniChar *)valueP->u.ptr = 0;
        return TCL_OK;
    }
}

/* Function: CffiArgPrepareInString
 * Initializes a CffiValue to pass a string input argument.
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
 * *TCL_OK* on success with string in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
static CffiResult
CffiArgPrepareInString(Tcl_Interp *ip,
                     const CffiTypeAndAttrs *typeAttrsP,
                     Tcl_Obj *valueObj,
                     CffiValue *valueP)
{
    int len;
    const char *s;
    Tcl_Encoding encoding;

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_ASTRING);
    CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_IN);

    Tcl_DStringInit(&valueP->ancillary.ds);
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
    Tcl_UtfToExternalDString(encoding, s, len, &valueP->ancillary.ds);
    if (encoding)
        Tcl_FreeEncoding(encoding);
    return TCL_OK;
}

/* Function: CffiArgPrepareInUniString
 * Initializes a CffiValue to pass a input unistring argument.
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - parameter type descriptor
 * valueObj - the Tcl_Obj containing string to pass.
 * valueP - location to store the argument
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with string in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
static CffiResult
CffiArgPrepareInUniString(Tcl_Interp *ip,
                        const CffiTypeAndAttrs *typeAttrsP,
                        Tcl_Obj *valueObj,
                        CffiValue *valueP)
{
    int len = 0;
    const Tcl_UniChar *s;

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_UNISTRING);
    CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_IN);

    /* TBD - why do we need valueP->ds at all? Copy straight into memlifo */
    Tcl_DStringInit(&valueP->ancillary.ds);
    s = Tcl_GetUnicodeFromObj(valueObj, &len);
    /* Note we copy the terminating two-byte end of string null as well */
    Tcl_DStringAppend(&valueP->ancillary.ds, (char *)s, (len + 1) * sizeof(*s));
    return TCL_OK;
}


/* Function: CffiArgPrepareInBinary
 * Initializes a CffiValue to pass a input byte array argument.
 *
 * Parameters:
 * ip - interpreter
 * paramP - parameter descriptor
 * valueObj - the Tcl_Obj containing the byte array to pass. May be NULL
 *   for pure output parameters
 * valueP - location to store the argument
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with string in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
static CffiResult
CffiArgPrepareInBinary(Tcl_Interp *ip,
                     const CffiTypeAndAttrs *typeAttrsP,
                     Tcl_Obj *valueObj,
                     CffiValue *valueP)
{
    Tcl_Obj *objP;

    CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_IN);
    /*
     * Pure input but could still shimmer so dup it. Potential
     * future optimizations TBD:
     *  - dup only if shared (ref count > 1)
     *  - use memlifo to copy bytes instead of duping
     */
    objP = Tcl_DuplicateObj(valueObj);
    Tcl_IncrRefCount(objP);
    valueP->ancillary.baObj = objP;

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
 * callP - the call context
 * arg_index - index into argument array of call context
 * valueObj - the value passed in from script. May be NULL for pure output.
 * valueP - location of native value to store
 *
 * The function may allocate storage which must be freed by calling
 * CffiArgCleanup when no longer needed.
 *
 * Returns:
 * *TCL_OK* on success with pointer to bytes in *valueP* or *TCL_ERROR*
 * on failure with error message in the interpreter.
 */
static CffiResult
CffiArgPrepareBytes(CffiCall *callP,
                       int arg_index,
                       Tcl_Obj *valueObj,
                       CffiValue *valueP)
{
    CffiInterpCtx *ipCtxP = callP->fnP->vmCtxP->ipCtxP;
    CffiArgument *argP    = &callP->argsP[arg_index];
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;

    valueP->u.ptr = MemLifoAlloc(&ipCtxP->memlifo, argP->actualCount);
    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_BYTE_ARRAY);

    if (typeAttrsP->flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
        /* NOTE: because of shimmering possibility, we need to copy */
        return CffiBytesFromObj(
            ipCtxP->interp, valueObj, valueP->u.ptr, argP->actualCount);
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
void CffiArgCleanup(CffiCall *callP, int arg_index)
{
    const CffiTypeAndAttrs *typeAttrsP;
    CffiValue *valueP;

    if ((callP->argsP[arg_index].flags & CFFI_F_ARG_INITIALIZED) == 0)
        return;

    typeAttrsP = &callP->fnP->protoP->params[arg_index].typeAttrs;
    valueP = &callP->argsP[arg_index].value;

    /*
     * IMPORTANT: the logic here must be consistent with CffiArgPostProcess
     * and CffiArgPrepare. Any changes here should be reflected there too.
     */

    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_UNISTRING:
        Tcl_DStringFree(&valueP->ancillary.ds);
        break;
    case CFFI_K_TYPE_BINARY:
        Tclh_ObjClearPtr(&valueP->ancillary.baObj);
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

/* Function: CffiArgPrepare
 * Prepares a argument for a DCCall
 *
 * Parameters:
 * callP - function call context
 * arg_index - the index of the argument. The corresponding slot should
 *            have been initialized so the its flags field is 0.
 * valueObj - the Tcl_Obj containing the value or variable name for out
 *             parameters
 *
 * The function modifies the following:
 * callP->argsP[arg_index].flags - sets CFFI_F_ARG_INITIALIZED
 * callP->argsP[arg_index].value - the native value.
 * callP->argsP[arg_index].savedValue - copy of above for some types only
 * callP->argsP[arg_index].varNameObj - will store the variable name
 *            for byref parameters. This will be valueObj[0] for byref
 *            parameters and NULL for byval parameters. The reference
 *            count is unchanged from what is passed in so do NOT call
 *            an *additional* Tcl_DecrRefCount on it.
 *
 * In addition to storing the native value in the *value* field, the
 * function also stores it as a dyncall function argument. The function may
 * allocate additional dynamic storage from the call context that is the
 * caller's responsibility to free.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
CffiResult
CffiArgPrepare(CffiCall *callP, int arg_index, Tcl_Obj *valueObj)
{
    DCCallVM *vmP         = callP->fnP->vmCtxP->vmP;
    CffiInterpCtx *ipCtxP = callP->fnP->vmCtxP->ipCtxP;
    Tcl_Interp *ip        = ipCtxP->interp;
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;
    CffiArgument *argP    = &callP->argsP[arg_index];
    CffiValue *valueP     = &argP->value;
    Tcl_Obj **varNameObjP = &argP->varNameObj;
    enum CffiBaseType baseType;
    CffiResult ret;
    int flags;
    char *p;

    /* Expected initialization to virgin state */
    CFFI_ASSERT(callP->argsP[arg_index].flags == 0);

    /*
     * IMPORTANT: the logic here must be consistent with CffiArgPostProcess
     * and CffiArgCleanup. Any changes here should be reflected there too.
     */

    flags = typeAttrsP->flags;
    baseType = typeAttrsP->dataType.baseType;
    if (typeAttrsP->dataType.count != 0) {
        switch (baseType) {
        case CFFI_K_TYPE_ASTRING:
        case CFFI_K_TYPE_UNISTRING:
        case CFFI_K_TYPE_BINARY:
            return ErrorInvalidValue(ip,
                                     NULL,
                                     "Arrays not supported for "
                                     "string/unistring/binary types.");
        default:
            break;
        }
    }

    /*
     * out/inout parameters are always expected to be byref. Prototype
     * parser should have ensured that.
     */
    CFFI_ASSERT((typeAttrsP->flags & (CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT)) == 0
                || (typeAttrsP->flags & CFFI_F_ATTR_BYREF) != 0);

    /*
     * For pure in parameters, valueObj provides the value itself. For out
     * and inout parameters, valueObj is the variable name. If the parameter
     * is an inout parameter, the variable must exist since the value passed
     * to the called function is taken from there. For pure out parameters,
     * the variable need not exist and will be created if necessary. For
     * both in and inout, on return from the function the corresponding
     * content is stored in that variable.
     */
    *varNameObjP = NULL;
    if (flags & (CFFI_F_ATTR_OUT | CFFI_F_ATTR_INOUT)) {
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        *varNameObjP = valueObj;
        valueObj = Tcl_ObjGetVar2(ip, valueObj, NULL, TCL_LEAVE_ERR_MSG);
        if (valueObj == NULL && (flags & CFFI_F_ATTR_INOUT)) {
            return ErrorInvalidValue(
                ip,
                *varNameObjP,
                "Variable specified as inout argument does not exist.");
        }
        /* TBD - check if existing variable is an array and error out? */
    }

    /*
     * Type parsing should have validated out/inout params specify size if
     * not a fixed size type. Note chars/unichars/bytes are fixed size since
     * their array size is required to be specified.
     */

    /* Non-scalars need to be passed byref. Parsing should have checked */
    CFFI_ASSERT((flags & CFFI_F_ATTR_BYREF)
                 || (typeAttrsP->dataType.count == 0
                     && baseType != CFFI_K_TYPE_CHAR_ARRAY
                     && baseType != CFFI_K_TYPE_UNICHAR_ARRAY
                     && baseType != CFFI_K_TYPE_BYTE_ARRAY
                     && baseType != CFFI_K_TYPE_STRUCT));

    /*
     * Modulo bugs, even dynamic array sizes are supposed to be initialized
     * before calling this function on an argument.
     */
    CFFI_ASSERT(argP->actualCount >= 0);
    if (argP->actualCount < 0) {
        /* Should not happen. Just a failsafe. */
        return ErrorInvalidValue(
            ip, NULL, "Variable size array parameters not implemented.");
    }

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

    /* May the programming gods forgive me for these macro */
#define OBJTONUM(objfn_, obj_, valP_)                                         \
    do {                                                                      \
        /* interp NULL when we do not want errors if enum specified */                         \
        ret = objfn_(lookup_enum ? NULL : ip, obj_, valP_);                   \
        if (ret != TCL_OK) {                                                  \
            Tcl_Obj *enumValueObj;                                            \
            if (!lookup_enum)                                                 \
                return ret;                                                   \
            CHECK(CffiEnumFind(                                               \
                ipCtxP, typeAttrsP->dataType.u.tagObj, obj_, &enumValueObj)); \
            CHECK(objfn_(ip, enumValueObj, valP_));                           \
        }                                                                     \
    } while (0)

#define STORENUM(objfn_, dcfn_, fld_, type_)                                   \
    do {                                                                       \
        int lookup_enum = flags & CFFI_F_ATTR_ENUM;                            \
        CFFI_ASSERT(argP->actualCount >= 0);                                   \
        if (argP->actualCount == 0) {                                          \
            if (flags & CFFI_F_ATTR_BITMASK) {                                 \
                Tcl_WideInt wide;                                              \
                CHECK(CffiEnumBitmask(                                         \
                    ipCtxP,                                                    \
                    lookup_enum ? typeAttrsP->dataType.u.tagObj : NULL,        \
                    valueObj,                                                  \
                    &wide));                                                   \
                valueP->u.fld_ = (type_)wide;                                  \
            }                                                                  \
            else {                                                             \
                if (flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT)) {            \
                    OBJTONUM(objfn_, valueObj, &valueP->u.fld_);               \
                }                                                              \
            }                                                                  \
            if (flags & CFFI_F_ATTR_BYREF)                                     \
                dcArgPointer(vmP, (DCpointer)&valueP->u.fld_);                 \
            else                                                               \
                dcfn_(vmP, valueP->u.fld_);                                    \
        }                                                                      \
        else {                                                                 \
            /* Array - has to be byref */                                      \
            type_ *valueArray;                                                 \
            CFFI_ASSERT(flags &CFFI_F_ATTR_BYREF);                             \
            valueArray = MemLifoAlloc(                                         \
                &ipCtxP->memlifo, argP->actualCount * sizeof(valueP->u.fld_)); \
            if (flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT)) {                \
                Tcl_Obj **valueObjList;                                        \
                int i, nvalues;                                                \
                if (Tcl_ListObjGetElements(                                    \
                        ip, valueObj, &nvalues, &valueObjList)                 \
                    != TCL_OK)                                                 \
                    return TCL_ERROR;                                          \
                /* Note - if caller has specified too few values, it's ok */   \
                /* because perhaps the actual count is specified in another */ \
                /* parameter. If too many, only up to array size */            \
                if (nvalues > argP->actualCount)                               \
                    nvalues = argP->actualCount;                               \
                for (i = 0; i < nvalues; ++i) {                                \
                    OBJTONUM(objfn_, valueObjList[i], &valueArray[i]);         \
                }                                                              \
                /* Fill additional elements with 0 */                          \
                for (; i < argP->actualCount; ++i)                             \
                    valueArray[i] = 0;                                         \
            }                                                                  \
            valueP->u.ptr = valueArray;                                        \
            dcArgPointer(vmP, (DCpointer)valueArray);                          \
        }                                                                      \
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
        CFFI_ASSERT(argP->actualCount >= 0);
        if (argP->actualCount == 0) {
            /* Single struct */
            if (typeAttrsP->flags & CFFI_F_ATTR_NULLIFEMPTY) {
                int dict_size;
                CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_IN);
                CHECK(Tcl_DictObjSize(ip, valueObj, &dict_size));
                if (dict_size == 0) {
                    /* Empty dictionary AND NULLIFEMPTY set */
                    valueP->u.ptr = NULL;
                    dcArgPointer(vmP, valueP->u.ptr);
                    break;
                }
                /* NULLIFEMPTY but dictionary has elements */
            }
            valueP->u.ptr = MemLifoAlloc(&ipCtxP->memlifo,
                                         typeAttrsP->dataType.u.structP->size);
            if (flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_INOUT)) {
                CHECK(CffiStructFromObj(ip,
                                        typeAttrsP->dataType.u.structP,
                                        valueObj,
                                        valueP->u.ptr));
            }
        }
        else {
            /* Array of structs */
            char *valueArray;
            char *toP;
            int struct_size = typeAttrsP->dataType.u.structP->size;
            CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
            valueArray =
                MemLifoAlloc(&ipCtxP->memlifo, argP->actualCount * struct_size);
            if (flags & (CFFI_F_ATTR_IN|CFFI_F_ATTR_INOUT)) {
                /* IN or INOUT */
                Tcl_Obj **valueObjList;
                int i, nvalues;
                CHECK(Tcl_ListObjGetElements(
                    ip, valueObj, &nvalues, &valueObjList));
                if (nvalues > argP->actualCount)
                    nvalues = argP->actualCount;
                for (toP = valueArray, i = 0; i < nvalues;
                     toP += struct_size, ++i) {
                    CHECK(CffiStructFromObj(ip,
                                            typeAttrsP->dataType.u.structP,
                                            valueObjList[i],
                                            toP));
                }
                if (i < argP->actualCount) {
                    /* Fill uninitialized with 0 */
                    memset(toP, 0, (argP->actualCount - i) * struct_size);
                }
            }
            valueP->u.ptr = valueArray;
        }
        dcArgPointer(vmP, valueP->u.ptr);
        break;

    case CFFI_K_TYPE_POINTER:
        /* TBD - can we just use STORENUM here ? */
        CFFI_ASSERT(argP->actualCount >= 0);
        if (argP->actualCount == 0) {
            if (flags & CFFI_F_ATTR_OUT)
                valueP->u.ptr = NULL;
            else {
                CHECK(
                    CffiPointerFromObj(ip, typeAttrsP, valueObj, &valueP->u.ptr));
                if (flags & (CFFI_F_ATTR_DISPOSE | CFFI_F_ATTR_DISPOSEONSUCCESS))
                    argP->savedValue.u.ptr = valueP->u.ptr;
            }
            if (flags & CFFI_F_ATTR_BYREF)
                dcArgPointer(vmP, &valueP->u.ptr);
            else
                dcArgPointer(vmP, valueP->u.ptr);
        } else {
            void **valueArray;
            CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
            valueArray = MemLifoAlloc(&ipCtxP->memlifo,
                                      argP->actualCount * sizeof(void *));
            if (flags & CFFI_F_ATTR_OUT)
                valueP->u.ptr = valueArray;
            else {
                Tcl_Obj **valueObjList;
                int i, nvalues;
                CHECK(Tcl_ListObjGetElements(
                    ip, valueObj, &nvalues, &valueObjList));
                if (nvalues > argP->actualCount)
                    nvalues = argP->actualCount;
                for (i = 0; i < nvalues; ++i) {
                    CHECK(CffiPointerFromObj(
                        ip, typeAttrsP, valueObjList[i], &valueArray[i]));
                }
                CFFI_ASSERT(i == nvalues);
                for ( ; i < argP->actualCount; ++i)
                    valueArray[i] = NULL;/* Fill additional elements */
                valueP->u.ptr = valueArray;
                if (flags & (CFFI_F_ATTR_DISPOSE | CFFI_F_ATTR_DISPOSEONSUCCESS)) {
                    /* Save pointers to dispose after call completion */
                    void **savedValueArray;
                    savedValueArray = MemLifoAlloc(
                        &ipCtxP->memlifo, argP->actualCount * sizeof(void *));
                    for (i = 0; i < argP->actualCount; ++i)
                        savedValueArray[i] = valueArray[i];
                    argP->savedValue.u.ptr = savedValueArray;
                }
            }
            dcArgPointer(vmP, valueP->u.ptr);
        }
        break;

    case CFFI_K_TYPE_CHAR_ARRAY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        CHECK(CffiArgPrepareChars(callP, arg_index, valueObj, valueP));
        dcArgPointer(vmP, valueP->u.ptr);
        break;

    case CFFI_K_TYPE_ASTRING:
        CFFI_ASSERT(!(flags & CFFI_F_ATTR_INOUT));
        if (flags & CFFI_F_ATTR_OUT) {
            CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
            valueP->u.ptr = NULL;
            dcArgPointer(vmP, &valueP->u.ptr);
        }
        else {
            CFFI_ASSERT(flags & CFFI_F_ATTR_IN);
            CHECK(CffiArgPrepareInString(ip, typeAttrsP, valueObj, valueP));
            if ((flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_NULLIFEMPTY))
                    == (CFFI_F_ATTR_IN | CFFI_F_ATTR_NULLIFEMPTY)
                && Tcl_DStringLength(&valueP->ancillary.ds) == 0) {
                valueP->u.ptr = NULL; /* Null if empty */
            }
            else
                valueP->u.ptr = Tcl_DStringValue(&valueP->ancillary.ds);
            if (flags & CFFI_F_ATTR_BYREF) /* In case of future changes */
                dcArgPointer(vmP, &valueP->u.ptr);
            else
                dcArgPointer(vmP, valueP->u.ptr);
        }
        break;

    case CFFI_K_TYPE_UNISTRING:
        CFFI_ASSERT(!(flags & CFFI_F_ATTR_INOUT));
        /*
         * Code below is written to support OUT and BYREF (not INOUT) despite
         * the asserts above which are maintained in the type declaration
         * parsing code.
         */
        if (flags & CFFI_F_ATTR_OUT) {
            CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
            valueP->u.ptr = NULL;
            dcArgPointer(vmP, &valueP->u.ptr);
        }
        else {
            CFFI_ASSERT(flags & CFFI_F_ATTR_IN);
            CHECK(CffiArgPrepareInUniString(ip, typeAttrsP, valueObj, valueP));
            p = Tcl_DStringValue(&valueP->ancillary.ds);
            if ((flags & (CFFI_F_ATTR_IN | CFFI_F_ATTR_NULLIFEMPTY))
                    == (CFFI_F_ATTR_IN | CFFI_F_ATTR_NULLIFEMPTY)
                && p[0] == 0 && p[1] == 0) {
                p = NULL; /* Null if empty */
            }
            valueP->u.ptr = p;
            if (flags & CFFI_F_ATTR_BYREF) /* In case of future changes */
                dcArgPointer(vmP, &valueP->u.ptr);
            else
                dcArgPointer(vmP, valueP->u.ptr);
        }
        break;

    case CFFI_K_TYPE_UNICHAR_ARRAY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        CHECK(CffiArgPrepareUniChars(callP, arg_index, valueObj, valueP));
        dcArgPointer(vmP, valueP->u.ptr);
        break;

    case CFFI_K_TYPE_BINARY:
        CFFI_ASSERT(typeAttrsP->flags & CFFI_F_ATTR_IN);
        CHECK(CffiArgPrepareInBinary(ip, typeAttrsP, valueObj, valueP));
        valueP->u.ptr = Tcl_GetByteArrayFromObj(valueP->ancillary.baObj, NULL);
        if (flags & CFFI_F_ATTR_BYREF) /* In case of future changes */
            dcArgPointer(vmP, &valueP->u.ptr);
        else
            dcArgPointer(vmP, valueP->u.ptr);
        break;

    case CFFI_K_TYPE_BYTE_ARRAY:
        CFFI_ASSERT(flags & CFFI_F_ATTR_BYREF);
        CHECK(CffiArgPrepareBytes(callP, arg_index, valueObj, valueP));
        dcArgPointer(vmP, valueP->u.ptr);
        break;

    default:
        return ErrorInvalidValue( ip, NULL, "Unsupported type.");
    }

    callP->argsP[arg_index].flags |= CFFI_F_ARG_INITIALIZED;

    return TCL_OK;
#undef STORENUM
#undef OBJTONUM
}

/* Function: CffiArgPostProcess
 * Does the post processing of an argument after a call
 *
 * Post processing of the argument consists of checking if the parameter
 * was an *out* or *inout* parameter and storing it in the output Tcl variable.
 * Note no cleanup of argument storage is done.
 *
 * Parameters:
 * callP - the call context
 * arg_index - index of argument to do post processing
 *
 * Returns:
 * *TCL_OK* on success,
 * *TCL_ERROR* on error with message stored in the interpreter.
 */
CffiResult
CffiArgPostProcess(CffiCall *callP, int arg_index)
{
    Tcl_Interp *ip = callP->fnP->vmCtxP->ipCtxP->interp;
    const CffiTypeAndAttrs *typeAttrsP =
        &callP->fnP->protoP->params[arg_index].typeAttrs;
    CffiArgument *argP = &callP->argsP[arg_index];
    CffiValue *valueP = &argP->value;
    Tcl_Obj *varObjP  = argP->varNameObj;
    int ret;
    Tcl_Obj *valueObj;

    CFFI_ASSERT(callP->argsP[arg_index].flags & CFFI_F_ARG_INITIALIZED);
    CFFI_ASSERT(argP->actualCount >= 0); /* Array size known */

    if (typeAttrsP->flags & CFFI_F_ATTR_IN)
        return TCL_OK;

    /*
     * There are three categories:
     *  - scalar values are directly stored in *valueP
     *  - structs and arrays of scalars are stored in at the location
     *    pointed to by valueP->u.ptr
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
    case CFFI_K_TYPE_POINTER:
        /* Scalars stored at valueP, arrays of scalars at valueP->u.ptr */
        if (argP->actualCount == 0)
            ret = CffiNativeValueToObj(
                ip, typeAttrsP, valueP, argP->actualCount, &valueObj);
        else
            ret = CffiNativeValueToObj(
                ip, typeAttrsP, valueP->u.ptr, argP->actualCount, &valueObj);
        break;

    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
        ret = CffiNativeValueToObj(
            ip, typeAttrsP, valueP->u.ptr, argP->actualCount, &valueObj);
        break;

    case CFFI_K_TYPE_STRUCT:
        /* Arrays not supported for struct currently */
        ret = CffiNativeValueToObj(
            ip, typeAttrsP, valueP->u.ptr, argP->actualCount, &valueObj);
        break;

    case CFFI_K_TYPE_ASTRING:
        ret = CffiExternalCharsToObj(
            ip, typeAttrsP, valueP->u.ptr, &valueObj);
        break;

    case CFFI_K_TYPE_UNISTRING:
        if (valueP->u.ptr)
            valueObj = Tcl_NewUnicodeObj((Tcl_UniChar *)valueP->u.ptr, -1);
        else
            valueObj = Tcl_NewObj();
        ret = TCL_OK;
        break;

    case CFFI_K_TYPE_BINARY:
    default:
        /* Should not happen */
        ret = Tclh_ErrorInvalidValue(ip, NULL, "Unsupported argument type");
        break;
    }

    if (ret != TCL_OK)
        return ret;

    /*
     * Check if value is to be converted to a enum name. This is slightly
     * inefficient since we have to convert back from a Tcl_Obj to an
     * integer but currently the required context is not being passed down
     * to the lower level functions that extract scalar values.
     */
    if ((typeAttrsP->flags & CFFI_F_ATTR_ENUM)
        && typeAttrsP->dataType.u.tagObj) {
        Tcl_WideInt wide;
        if (typeAttrsP->dataType.count == 0) {
            /* Note on error, keep the value */
            if (Tcl_GetWideIntFromObj(NULL, valueObj, &wide) == TCL_OK) {
                Tcl_DecrRefCount(valueObj);
                CffiEnumFindReverse(callP->fnP->vmCtxP->ipCtxP,
                                    typeAttrsP->dataType.u.tagObj,
                                    wide,
                                    0,
                                    &valueObj);
            }
        }
        else {
            /* Array of integers */
            Tcl_Obj **elemObjs;
            int nelems;
            /* Note on error, keep the value */
            if (Tcl_ListObjGetElements(NULL, valueObj, &nelems, &elemObjs)
                == TCL_OK) {
                Tcl_Obj *enumValuesObj;
                Tcl_Obj *enumValueObj;
                int i;
                enumValuesObj = Tcl_NewListObj(nelems, NULL);
                for (i = 0; i < nelems; ++i) {
                    if (Tcl_GetWideIntFromObj(NULL, elemObjs[i], &wide)
                        != TCL_OK) {
                        break;
                    }
                    CffiEnumFindReverse(callP->fnP->vmCtxP->ipCtxP,
                                        typeAttrsP->dataType.u.tagObj,
                                        wide,
                                        0,
                                        &enumValueObj);
                    Tcl_ListObjAppendElement(NULL, enumValuesObj, enumValueObj);
                }
                if (i == nelems) {
                    /* All converted successfully */
                    Tcl_DecrRefCount(valueObj);
                    valueObj = enumValuesObj;
                }
                else {
                    /* Keep original */
                    Tcl_DecrRefCount(enumValuesObj);
                }
            }
        }
    }

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
 * callP - call context
 *
 * The function may allocate additional dynamic storage from
 * *ipContextP->memlifo* that is the caller's responsibility to free.
 *
 * Returns:
 * Returns TCL_OK on success and TCL_ERROR on failure with error message
 * in the interpreter.
 */
CffiResult
CffiReturnPrepare(CffiCall *callP)
{
    /*
     * Nothing to do as no allocations needed.
     *  arrays, struct, chars[], unichars[], bytes and anything that requires
     * non-scalar storage is either not supported by C or by dyncall.
     */
    return TCL_OK;
}

CffiResult CffiReturnCleanup(CffiCall *callP)
{
    /*
     * No clean up needed for any types. Any type that needs non-scalar
     * storage is not allowed for a return type.
     */
    return TCL_OK;
}


/* Function: CffiCheckNumeric
 * Checks if the specified value meets requirements stipulated by the
 * type descriptor.
 *
 * Parameters:
 * ip - interpreter
 * typeAttrsP - type and attributes
 * valueP - value to be checked. Must contain an numeric type
 * sysErrorP - location to store system error based on type annotations.
 *          Only set if error detected.
 *
 * Returns:
 * *TCL_OK* if requirements pass, else *TCL_ERROR* with an error.
 */
CffiResult
CffiCheckNumeric(Tcl_Interp *ip,
                  const CffiTypeAndAttrs *typeAttrsP,
                  CffiValue *valueP,
                  Tcl_WideInt *sysErrorP
                  )
{
    int flags = typeAttrsP->flags;
    int is_signed;
    Tcl_WideInt value;

    if ((flags & CFFI_F_ATTR_REQUIREMENT_MASK) == 0)
        return TCL_OK; /* No checks requested */

    /* TBD - needs to be checked for correctness for 64-bit signed/unsigned */

    /* IMPORTANT - do NOT make any function calls until system errors are
     * retrieved at the bottom.
      */

    /* TBD - float and double */
    is_signed = 0;
    switch (typeAttrsP->dataType.baseType) {
    case CFFI_K_TYPE_SCHAR: value = valueP->u.schar; is_signed = 1; break;
    case CFFI_K_TYPE_UCHAR: value = valueP->u.uchar; break;
    case CFFI_K_TYPE_SHORT: value = valueP->u.sshort; is_signed = 1; break;
    case CFFI_K_TYPE_USHORT: value = valueP->u.ushort; break;
    case CFFI_K_TYPE_INT: value = valueP->u.sint; is_signed = 1; break;
    case CFFI_K_TYPE_UINT: value = valueP->u.uint; break;
    case CFFI_K_TYPE_LONG: value = valueP->u.slong; is_signed = 1; break;
    case CFFI_K_TYPE_ULONG: value = valueP->u.ulong; break;
    case CFFI_K_TYPE_LONGLONG: value = valueP->u.slonglong; is_signed = 1; break;
    case CFFI_K_TYPE_ULONGLONG: value = valueP->u.ulonglong; break;
    case CFFI_K_TYPE_FLOAT:
        value     = (Tcl_WideInt)valueP->u.flt;
        is_signed = 1;
        break;
    case CFFI_K_TYPE_DOUBLE:
        value     = (Tcl_WideInt)valueP->u.dbl;
        is_signed = 1;
        break;
    default:
        /* Should not happen. */
        CFFI_PANIC("CffiCheckNumeric called on non-numeric type");
        value = 0;/* Keep compiler happy */
        break;
    }

    if (value == 0) {
        if (flags & (CFFI_F_ATTR_NONZERO|CFFI_F_ATTR_POSITIVE)) {
            goto failed_requirements;
        }
    }
    else if (flags & CFFI_F_ATTR_ZERO) {
        goto failed_requirements;
    }
    else {
        /* Non-0 value. */
        if ((flags & (CFFI_F_ATTR_NONNEGATIVE | CFFI_F_ATTR_POSITIVE))
            && is_signed && value < 0) {
            goto failed_requirements;
        }
    }

    return TCL_OK;

failed_requirements:
    *sysErrorP = CffiGrabSystemError(typeAttrsP, value);
    return TCL_ERROR;
}

#ifdef OBSOLETE
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
#endif

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

    if (count > 0 || (count < 0 && typeP->countHolderObj == NULL))
        Tcl_AppendPrintfToObj(typeObj, "[%d]", count);
    else if (count < 0)
        Tcl_AppendPrintfToObj(
            typeObj, "[%s]", Tcl_GetString(typeP->countHolderObj));
    /* else scalar */

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
    int flags;

    resultObj = Tcl_NewListObj(0, NULL);

    Tcl_ListObjAppendElement(
        NULL, resultObj, CffiTypeUnparse(&typeAttrsP->dataType));

    flags = typeAttrsP->flags;
    for (attrsP = &cffiAttrs[0]; attrsP->attrName != NULL; ++attrsP) {
        /* -1 -> Not a real attribute */
        if (attrsP->attrFlag != -1) {
            if (attrsP->attrFlag & flags) {
                if (attrsP->attrFlag == CFFI_F_ATTR_ENUM) {
                    Tcl_Obj *objs[2];
                    objs[0] = Tcl_NewStringObj(attrsP->attrName, -1);
                    objs[1] = typeAttrsP->dataType.u.tagObj;
                    Tcl_ListObjAppendElement(
                        NULL, resultObj, Tcl_NewListObj(2, objs));
                }
                else if (attrsP->attrFlag == CFFI_F_ATTR_ONERROR) {
                    Tcl_Obj *objs[2];
                    objs[0] = Tcl_NewStringObj(attrsP->attrName, -1);
                    objs[1] = typeAttrsP->parseModeSpecificObj;
                    Tcl_ListObjAppendElement(
                        NULL, resultObj, Tcl_NewListObj(2, objs));
                }
                else
                    Tcl_ListObjAppendElement(
                        NULL,
                        resultObj,
                        Tcl_NewStringObj(attrsP->attrName, -1));
            }
        }
    }

    if (typeAttrsP->parseModeSpecificObj && !(flags & CFFI_F_ATTR_ONERROR)) {
        Tcl_Obj *objs[2];
        objs[0] = Tcl_NewStringObj("default", 7);
        objs[1] = typeAttrsP->parseModeSpecificObj; /* No need for IncrRef - adding to list */
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
        /* type count */
        if (typeAttrs.dataType.count >= 0 || typeAttrs.dataType.countHolderObj == NULL)
            Tcl_SetObjResult(ip, Tcl_NewIntObj(typeAttrs.dataType.count));
        else
            Tcl_SetObjResult(ip, typeAttrs.dataType.countHolderObj);
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
            if (typeAttrs.dataType.count >= 0
                || typeAttrs.dataType.countHolderObj == NULL)
                objs[3] = Tcl_NewIntObj(typeAttrs.dataType.count);
            else
                objs[3] = typeAttrs.dataType.countHolderObj;
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
