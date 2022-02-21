/*
 * Copyright (c) 2021-2022, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#define TCLH_SHORTNAMES
#include "tclCffiInt.h"
#include <errno.h>

#define CFFI_VALID_INTEGER_ATTRS                                       \
    (CFFI_F_ATTR_PARAM_MASK | CFFI_F_ATTR_REQUIREMENT_MASK             \
     | CFFI_F_ATTR_ERROR_MASK | CFFI_F_ATTR_ENUM | CFFI_F_ATTR_BITMASK \
     | CFFI_F_ATTR_STRUCTSIZE | CFFI_F_ATTR_NULLOK)

/*
 * Basic type meta information. The order *MUST* match the order in
 * cffiTypeId.
 */

#define TOKENANDLEN(t) # t , sizeof(#t) - 1

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
     CFFI_F_ATTR_PARAM_MASK | CFFI_F_ATTR_NULLOK,
     sizeof(float)},
    {TOKENANDLEN(double),
     CFFI_K_TYPE_DOUBLE,
     /* Note NUMERIC left out of float and double for now as the same error
        checks do not apply */
     CFFI_F_ATTR_PARAM_MASK | CFFI_F_ATTR_NULLOK,
     sizeof(double)},
    {TOKENANDLEN(struct),
     CFFI_K_TYPE_STRUCT,
     CFFI_F_ATTR_PARAM_MASK | CFFI_F_ATTR_NULLIFEMPTY | CFFI_F_ATTR_NULLOK,
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
     CFFI_F_ATTR_IN | CFFI_F_ATTR_BYREF | CFFI_F_ATTR_NULLIFEMPTY,
     sizeof(unsigned char *)},
    {TOKENANDLEN(chars),
     CFFI_K_TYPE_CHAR_ARRAY,
     CFFI_F_ATTR_PARAM_MASK | CFFI_F_ATTR_NULLOK,
     sizeof(char)},
    {TOKENANDLEN(unichars),
     CFFI_K_TYPE_UNICHAR_ARRAY,
     CFFI_F_ATTR_PARAM_MASK | CFFI_F_ATTR_NULLOK,
     sizeof(Tcl_UniChar)},
    {TOKENANDLEN(bytes),
     CFFI_K_TYPE_BYTE_ARRAY,
     CFFI_F_ATTR_PARAM_MASK | CFFI_F_ATTR_NULLOK,
     sizeof(unsigned char)},
    {NULL}};

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
    STRUCTSIZE,
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
    {"byref",
     BYREF,
     CFFI_F_ATTR_BYREF,
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_RETURN,
     1},
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
    {"default",
     DEFAULT,
     -1,
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_FIELD,
     2},
    {"nullifempty",
     NULLIFEMPTY,
     CFFI_F_ATTR_NULLIFEMPTY,
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_FIELD,
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
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_FIELD
         | CFFI_F_TYPE_PARSE_RETURN,
     2},
    {"bitmask",
     BITMASK,
     CFFI_F_ATTR_BITMASK,
     CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_FIELD
         | CFFI_F_TYPE_PARSE_RETURN,
     1},
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
    {"structsize",
     STRUCTSIZE,
     CFFI_F_ATTR_STRUCTSIZE,
     CFFI_F_TYPE_PARSE_FIELD,
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

Tcl_Obj *
CffiMakePointerTag(Tcl_Interp *ip, const char *tagP, int tagLen)
{
    Tcl_Namespace *nsP;
    Tcl_Obj *tagObj;

    /* Tclh_NsQualify* unusable here because tagP defined by tagLen, not nul */

    if (Tclh_NsIsFQN(tagP))
        return Tcl_NewStringObj(tagP, tagLen);

    /* tag is relative so qualify it */
    nsP = Tcl_GetCurrentNamespace(ip);
    tagObj = Tcl_NewStringObj(nsP->fullName, -1);
    /* Put separator only if not global namespace */
    if (! Tclh_NsIsGlobalNs(nsP->fullName))
        Tcl_AppendToObj(tagObj, "::", 2);
    Tcl_AppendToObj(tagObj, tagP, tagLen);
    return tagObj;
}

Tcl_Obj *CffiMakePointerTagFromObj(Tcl_Interp *ip, Tcl_Obj *tagObj)
{
    int len;
    const char *tag = Tcl_GetStringFromObj(tagObj, &len);
    return CffiMakePointerTag(ip, tag, len);
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
        toP->arraySize = fromP->arraySize;
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
        toP->baseTypeSize = fromP->baseTypeSize;
    }
    else {
        memset(toP, 0, sizeof(*toP));
        toP->baseType = CFFI_K_TYPE_VOID;
        toP->arraySize = -1; /* Scalar */
    }
}

/* Function: CffiTypeParse
 * Parses a type definition into an internal form.
 *
 * A type definition is of the form
 *    BASETYPE?.TAG??[COUNT]?
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
    CFFI_ASSERT(typeP->arraySize < 0); /* Code below assumes these values on init */
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
        typeP->baseTypeSize = typeP->u.structP->size;
        break;

    case CFFI_K_TYPE_POINTER:
        if (tagStr != NULL) {
            typeP->u.tagObj = CffiMakePointerTag(ip, tagStr, tagLen);
                Tcl_IncrRefCount(typeP->u.tagObj);
        }
        typeP->baseType = CFFI_K_TYPE_POINTER;
        typeP->baseTypeSize = baseTypeInfoP->size;
        break;

    case CFFI_K_TYPE_ASTRING:
    case CFFI_K_TYPE_CHAR_ARRAY:
        typeP->baseType = baseType; /* So type->u.tagObj freed on error */
        typeP->baseTypeSize = baseTypeInfoP->size; /* pointer or sizeof(char) */
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

    case CFFI_K_TYPE_UNISTRING:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    default: /* Numerics */
        if (tagLen) {
            message = "Tags are not permitted for this base type.";
            goto invalid_type;
        }
        typeP->baseType = baseType;
        typeP->baseTypeSize = baseTypeInfoP->size;
        break;
    }

    CFFI_ASSERT(typeP->arraySize < 0);/* Should already be default init-ed */
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
            typeP->arraySize = count;
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
            typeP->arraySize = 0;/* Variable size array */
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
    case CFFI_K_TYPE_BINARY:
        if (CffiTypeIsArray(typeP)) {
            message = "The specified type is invalid or unsupported for array "
                      "declarations.";
            goto invalid_type;
        }
        break;
    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
        if (CffiTypeIsNotArray(typeP)) {
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

/* Function: CffiTypeActualSize
 * Returns the size of a concrete type.
 *
 * Parameters:
 * typeP - type descriptor
 *
 * The size returned for arrays is the size of the whole array not just the
 * base size.
 *
 * The type must not be CFFI_K_TYPE_VOID and must not be a variable size
 * array.
 *
 * Returns:
 * Size of the type.
 */
int
CffiTypeActualSize(const CffiType *typeP)
{
    CFFI_ASSERT(typeP->baseTypeSize != 0);
    CFFI_ASSERT(typeP->arraySize != 0); /* Not variable size array */
    if (CffiTypeIsArray(typeP))
        return typeP->arraySize * typeP->baseTypeSize;
    else
        return typeP->baseTypeSize;
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
    baseSize = typeP->baseTypeSize;
    if (baseType == CFFI_K_TYPE_STRUCT)
        alignment = typeP->u.structP->alignment;
    else
        alignment = baseSize;
    if (baseSize == 0 && baseType != CFFI_K_TYPE_VOID) {
        TCLH_PANIC("Unexpected 0 size type %d", baseType);
    }
    if (baseSizeP)
        *baseSizeP = baseSize;
    if (alignP)
        *alignP = alignment;
    if (sizeP) {
        if (CffiTypeIsNotArray(typeP))
            *sizeP = baseSize;
        else if (CffiTypeIsVariableSizeArray(typeP))
            *sizeP = 0; /* Variable size array */
        else
            *sizeP = typeP->arraySize * baseSize;
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
 *   ipCtxP - Interpreter context
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
    int temp;
    int i;
    CffiAttrFlags flags;
    CffiAttrFlags validAttrs;
    enum CffiBaseType baseType;
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
    temp = CffiAliasGet(ipCtxP, objs[0], typeAttrP, CFFI_F_SKIP_ERROR_MESSAGES);
    if (temp)
        baseType = typeAttrP->dataType.baseType; /* Found alias */
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
            if (!(parseMode
                  & (CFFI_F_TYPE_PARSE_PARAM | CFFI_F_TYPE_PARSE_FIELD))) {
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
            /* May be a dictionary or a named enum */
            if (Tcl_DictObjSize(NULL, fieldObjs[1], &temp) == TCL_OK) {
                 /* TBD - check if numeric */
                typeAttrP->dataType.u.tagObj = fieldObjs[1];
            }
            else if (CffiEnumGetMap(
                         ipCtxP, fieldObjs[1], 0, &typeAttrP->dataType.u.tagObj)
                     != TCL_OK) {
                goto error_exit; /* Named Enum does not exist */
            }
            flags |= CFFI_F_ATTR_ENUM;
            Tcl_IncrRefCount(typeAttrP->dataType.u.tagObj);
            break;
        case BITMASK:
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
            if (baseType != CFFI_K_TYPE_POINTER
                && baseType != CFFI_K_TYPE_ASTRING
                && baseType != CFFI_K_TYPE_UNISTRING
                && baseType != CFFI_K_TYPE_BINARY
                && baseType != CFFI_K_TYPE_STRUCT
                && !CffiTypeIsArray(&typeAttrP->dataType)) {
            message = "A type annotation is not valid for the data type.";
            goto invalid_format;
            }

            flags |= CFFI_F_ATTR_NULLOK;
            break;
        case STRUCTSIZE:
            if (CffiTypeIsArray(&typeAttrP->dataType)) {
                message = "\"structsize\" annotation not valid for arrays.";
                goto invalid_format;
            }
            flags |= CFFI_F_ATTR_STRUCTSIZE;
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

    if ((flags & CFFI_F_ATTR_STRUCTSIZE) && typeAttrP->parseModeSpecificObj) {
        /* Conflicting annotation */
        goto invalid_format;
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
        if (flags & (CFFI_F_ATTR_INOUT|CFFI_F_ATTR_OUT)) {
            if (typeAttrP->parseModeSpecificObj) {
                message = defaultNotAllowedMsg;
                goto invalid_format;
            }
            /*
             * NULLIFEMPTY never allowed for any output.
             */
            if ((flags & CFFI_F_ATTR_NULLIFEMPTY)
                || ((flags & CFFI_F_ATTR_OUT)
                    && (flags
                        & (CFFI_F_ATTR_DISPOSE
                           | CFFI_F_ATTR_DISPOSEONSUCCESS)))) {
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

            if (CffiTypeIsArray(&typeAttrP->dataType))
                flags |= CFFI_F_ATTR_BYREF; /* Arrays always by reference */
            else {
                /* Certain types always by reference */
                switch (baseType) {
                case CFFI_K_TYPE_BINARY:
                    flags |= CFFI_F_ATTR_NULLIFEMPTY;
                    break;
                case CFFI_K_TYPE_CHAR_ARRAY:
                case CFFI_K_TYPE_UNICHAR_ARRAY:
                case CFFI_K_TYPE_BYTE_ARRAY:
                    flags |=
                        CFFI_F_ATTR_BYREF; /* Arrays always by reference */
                    break;
                case CFFI_K_TYPE_STRUCT:
                    if ((flags & CFFI_F_ATTR_BYREF) == 0) {
#ifdef CFFI_USE_DYNCALL
                        message = "Passing of structs by value is not "
                                  "supported. Annotate with \"byref\" to pass by "
                                  "reference if function expects a pointer.";
                        goto invalid_format;
#endif
#ifdef CFFI_USE_LIBFFI
                        if (flags & CFFI_F_ATTR_NULLIFEMPTY) {
                            message =
                                "Structs cannot have nullifempty attribute "
                                "when passed as an argument by value.";
                            goto invalid_format;
                        }
#endif
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
        case CFFI_K_TYPE_BINARY:
        case CFFI_K_TYPE_CHAR_ARRAY:
        case CFFI_K_TYPE_UNICHAR_ARRAY:
        case CFFI_K_TYPE_BYTE_ARRAY:
            message = typeInvalidForContextMsg;
            goto invalid_format;
        case CFFI_K_TYPE_STRUCT:
#ifdef CFFI_USE_DYNCALL
            if ((flags & CFFI_F_ATTR_BYREF) == 0) {
                /* dyncall - return type not allowed unless byref */
                message = typeInvalidForContextMsg;
                goto invalid_format;
            }
#endif
            /* FALLTHRU */
        case CFFI_K_TYPE_VOID: /* FALLTHRU */
        default:
            if (CffiTypeIsArray(&typeAttrP->dataType)) {
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
        case CFFI_K_TYPE_BINARY:
            /* Struct/string/bytes return type not allowed even byref */
            message = typeInvalidForContextMsg;
            goto invalid_format;
        case CFFI_K_TYPE_CHAR_ARRAY:
        case CFFI_K_TYPE_UNICHAR_ARRAY:
        case CFFI_K_TYPE_BYTE_ARRAY:
            if (CffiTypeIsNotArray(&typeAttrP->dataType)
                || CffiTypeIsVariableSizeArray(&typeAttrP->dataType)) {
                message = "Fields of type chars, unichars or bytes must be fixed size "
                          "arrays.";
                goto invalid_format;
            }
            break;
        default:
            if (CffiTypeIsVariableSizeArray(&typeAttrP->dataType)) {
                message = "Fields cannot be arrays of variable size.";
                goto invalid_format;
            }
            break;
        }
        break;

    default:
        /*
         * One or more parse modes - preliminary typedef. Accept all flags.
         * Final check will be made the function is called again when a specific
         * mode is parsed.
         */
        break;
    }

    /* Checks that require all flags to have been set */

    if (flags & CFFI_F_ATTR_NULLOK) {
        switch (baseType) {
            case CFFI_K_TYPE_POINTER:
            case CFFI_K_TYPE_ASTRING:
            case CFFI_K_TYPE_UNISTRING:
            case CFFI_K_TYPE_BINARY:
                break;
            default:
                if (flags & CFFI_F_ATTR_BYREF)
                    break;
                /* nullok only valid for pointers at the C level */
                message = "A type annotation is not valid for this data type without the byref annotation.";
                goto invalid_format;
        }
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
error_exit: /* interp must already contain error message */
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


/* Function: CffiIntValueFromObj
 * Converts a *Tcl_Obj* to an integer with support for enums and bitmasks
 *
 * Parameters:
 * ipCtxP - the interpreter context
 * typeAttrsP - the type attributes. May be NULL.
 * valueObj - the *Tcl_Obj* to unwrap
 * valueP - location to store the result
 *
 * The unwrapping takes into account the enum and bitmask settings as
 * per the *typeAttrsP* argument.
 *
 * Returns:
 * *TCL_OK* on success with the integer value stored in the location *valueP*
 * *TCL_ERROR* on error with message stored in the interpreter.
 */
CffiResult
CffiIntValueFromObj(Tcl_Interp *ip,
                    const CffiTypeAndAttrs *typeAttrsP,
                    Tcl_Obj *valueObj,
                    Tcl_WideInt *valueP)
{
    Tcl_WideInt value;
    CffiAttrFlags flags = typeAttrsP ? typeAttrsP->flags : 0;
    int lookup_enum = flags & CFFI_F_ATTR_ENUM;

    if (flags & CFFI_F_ATTR_BITMASK) {
        /* TBD - Does not handle size truncation */
        return CffiEnumMemberBitmask(ip,
                                     lookup_enum ? typeAttrsP->dataType.u.tagObj
                                                 : NULL,
                                     valueObj,
                                     valueP);
    }
    if (lookup_enum) {
        Tcl_Obj *enumValueObj;
        if (CffiEnumMemberFind(NULL,
                               typeAttrsP->dataType.u.tagObj,
                               valueObj,
                               &enumValueObj) == TCL_OK)
            valueObj = enumValueObj;
    }
    if (Tcl_GetWideIntFromObj(ip, valueObj, &value) == TCL_OK) {
        *valueP = value;
        return TCL_OK;
    }
    else
        return TCL_ERROR;
}


/* Function: CffiIntValueToObj
 * Converts an integer value to a Tcl_Obj mapping to an enum or bitmask
 * if appropriate.
 *
 * Parameters:
 * typeAttrsP - the type attributes. May be NULL.
 * value - the integer value to convert
 *
 * The unwrapping takes into account the enum and bitmask settings as
 * per the *typeAttrsP* argument.
 *
 * Returns:
 * A Tcl_Obj with the mapped values or NULL. In the latter case, caller
 * should use the type-specific integer conversion itself.
 *
 */
Tcl_Obj *
CffiIntValueToObj(const CffiTypeAndAttrs *typeAttrsP,
                  Tcl_WideInt value)
{
    Tcl_Obj *valueObj;
    /* TBD - Handles ulonglong correctly? */
    if (typeAttrsP != NULL && ((typeAttrsP->flags & CFFI_F_ATTR_ENUM) != 0)
        && typeAttrsP->dataType.u.tagObj != NULL) {
        CffiResult ret;
        if (typeAttrsP->flags & CFFI_F_ATTR_BITMASK)
            ret = CffiEnumMemberBitUnmask(
                NULL, typeAttrsP->dataType.u.tagObj, value, &valueObj);
        else
            ret = CffiEnumMemberFindReverse(
                NULL, typeAttrsP->dataType.u.tagObj, value, &valueObj);
        return ret == TCL_OK ? valueObj : NULL;
    }
    return NULL;
}

/* Function: CffiNativeScalarFromObj
 * Stores a native scalar value from Tcl_Obj wrapper
 *
 * Parameters:
 * ipCtxP - interpreter context
 * typeAttrsP - type attributes. The base type is used without
 *   considering the BYREF flag.
 * valueObj - the *Tcl_Obj* containing the script level value
 * valueBaseP - the base location where the native value is to be stored.
 * indx - the index at which the value is to be stored i.e. valueBaseP[indx]
 *         is the final target
 * flags - if CFFI_F_PRESERVE_ON_ERROR is set, the target location will
 *   be preserved in case of errors.
 * memlifoP - the memory allocator for fields that are typed as *string*
 *            or *unistring*. If *NULL*, fields of these types will
 *            result in an error being raised. Caller is responsible
 *            for ensuring the memory allocated from *memlifoP* stays
 *            allocated as long as the stored pointer is live.
 *
 * Note this stores a single value of the type indicated in *typeAttrsP*, even
 * if the *count* field in the descriptor indicates an array.
 *
 * On error return, the location where the value is to be stored is not
 * guaranteed to be preserved unless the CFFI_F_PRESERVE_ON_ERROR bit is
 * set in the flags argument.
 *
 * Returns:
 * *TCL_OK* on success with a pointer to the structure stored in *structPP* or
 * *TCL_ERROR* on error with message stored in the interpreter.
 */
CffiResult
CffiNativeScalarFromObj(CffiInterpCtx *ipCtxP,
                        const CffiTypeAndAttrs *typeAttrsP,
                        Tcl_Obj *valueObj,
                        CffiFlags flags,
                        void *valueBaseP,
                        int indx,
                        MemLifo *memlifoP)
{
    Tcl_Interp *ip = ipCtxP->interp;
    CffiValue value;
    CffiResult ret;
    Tcl_DString ds;
    char *tempP;
    int len;

    /*
     * Note if CFFI_F_PRESERVE_ON_ERROR is set, output must be preserved on error
     */

#define STOREINT_(objfn_, type_)                                            \
    do {                                                                    \
        type_ val;                                                          \
        if (typeAttrsP->flags & (CFFI_F_ATTR_BITMASK | CFFI_F_ATTR_ENUM)) { \
            Tcl_WideInt wide;                                               \
            CHECK(CffiIntValueFromObj(ip, typeAttrsP, valueObj, &wide));    \
            val = (type_)wide;                                              \
        }                                                                   \
        else {                                                              \
            CHECK(objfn_(ip, valueObj, &val));                              \
        }                                                                   \
        *(indx + (type_ *)valueBaseP) = val;                                \
    } while (0)

    switch (typeAttrsP->dataType.baseType) {
        case CFFI_K_TYPE_SCHAR:
            STOREINT_(ObjToChar, signed char);
            break;
        case CFFI_K_TYPE_UCHAR:
            STOREINT_(ObjToUChar, unsigned char);
            break;
        case CFFI_K_TYPE_SHORT:
            STOREINT_(ObjToShort, short);
            break;
        case CFFI_K_TYPE_USHORT:
            STOREINT_(ObjToUShort, unsigned short);
            break;
        case CFFI_K_TYPE_INT:
            STOREINT_(ObjToInt, int);
            break;
        case CFFI_K_TYPE_UINT:
            STOREINT_(ObjToUInt, unsigned int);
            break;
        case CFFI_K_TYPE_LONG:
            STOREINT_(ObjToLong, long);
            break;
        case CFFI_K_TYPE_ULONG:
            STOREINT_(ObjToULong, unsigned long);
            break;
        case CFFI_K_TYPE_LONGLONG:
            STOREINT_(ObjToLongLong, long long);
            break;
        case CFFI_K_TYPE_ULONGLONG:
            STOREINT_(ObjToULongLong, unsigned long long);
            break;
        case CFFI_K_TYPE_FLOAT:
            if (ObjToFloat(ip, valueObj, &value.u.flt) != TCL_OK)
                return TCL_ERROR;
            *(indx + (float *)valueBaseP) = value.u.flt;
            break;
        case CFFI_K_TYPE_DOUBLE:
            if (ObjToDouble(ip, valueObj, &value.u.dbl) != TCL_OK)
                return TCL_ERROR;
            *(indx + (double *)valueBaseP) = value.u.dbl;
            break;
        case CFFI_K_TYPE_STRUCT:
            len = typeAttrsP->dataType.u.structP->size;
            if (flags & CFFI_F_PRESERVE_ON_ERROR) {
                Tcl_DStringInit(&ds);
                Tcl_DStringSetLength(&ds, len);
                tempP = Tcl_DStringValue(&ds);
                /* TBD - turn off PRESERVE_ON_ERROR in flags? */
                ret = CffiStructFromObj(ipCtxP,
                                        typeAttrsP->dataType.u.structP,
                                        valueObj,
                                        flags,
                                        tempP,
                                        memlifoP);
                if (ret == TCL_OK) {
                    memcpy((indx * len) + (char *)valueBaseP, tempP, len);
                }
                Tcl_DStringFree(&ds);
                if (ret != TCL_OK)
                    return ret;
            }
            else {
                /* This is more efficient if no promise to preserve on error */
                CHECK(CffiStructFromObj(ipCtxP,
                                        typeAttrsP->dataType.u.structP,
                                        valueObj,
                                        flags,
                                        (indx * len) + (char *)valueBaseP,
                                        memlifoP));
            }
            break;
        case CFFI_K_TYPE_POINTER:
            CHECK(CffiPointerFromObj(ip, typeAttrsP, valueObj, &value.u.ptr));
            *(indx + (void **)valueBaseP) = value.u.ptr;
            break;
        case CFFI_K_TYPE_CHAR_ARRAY:
            CFFI_ASSERT(typeAttrsP->dataType.arraySize > 0);
            if (flags & CFFI_F_PRESERVE_ON_ERROR) {
                CHECK(
                    CffiCharsFromObjSafe(ip,
                                         typeAttrsP->dataType.u.tagObj,
                                         valueObj,
                                         (indx * typeAttrsP->dataType.arraySize)
                                             + (char *)valueBaseP,
                                         typeAttrsP->dataType.arraySize));
            }
            else {
                /* This is more efficient if no promise to preserve on error */
                CHECK(CffiCharsFromObj(ip,
                                       typeAttrsP->dataType.u.tagObj,
                                       valueObj,
                                       (indx * typeAttrsP->dataType.arraySize)
                                           + (char *)valueBaseP,
                                       typeAttrsP->dataType.arraySize));
            }
            break;
        case CFFI_K_TYPE_UNICHAR_ARRAY:
            CFFI_ASSERT(typeAttrsP->dataType.arraySize > 0);
            CHECK(
                CffiUniCharsFromObjSafe(ip,
                                        valueObj,
                                        (indx * typeAttrsP->dataType.arraySize)
                                            + (Tcl_UniChar *)valueBaseP,
                                        typeAttrsP->dataType.arraySize));
            break;
        case CFFI_K_TYPE_BYTE_ARRAY:
            CFFI_ASSERT(typeAttrsP->dataType.arraySize > 0);
            CHECK(CffiBytesFromObjSafe(ip,
                                   valueObj,
                                   (indx * typeAttrsP->dataType.arraySize)
                                       + (unsigned char *)valueBaseP,
                                   typeAttrsP->dataType.arraySize));
            break;
        case CFFI_K_TYPE_ASTRING:
            if (memlifoP == NULL) {
                return Tclh_ErrorInvalidValue(
                    ip,
                    NULL,
                    "string type not supported in this struct context.");
            }
            else {
                char **charPP = indx + (char **)valueBaseP;
                char *charP; /* Do not modify *charPP in case of errors */
                CHECK(CffiCharsInMemlifoFromObj(ip,
                                                typeAttrsP->dataType.u.tagObj,
                                                valueObj,
                                                memlifoP,
                                                &charP));
                if ((typeAttrsP->flags & CFFI_F_ATTR_NULLIFEMPTY)
                    && *charP == 0) {
                    *charPP = NULL;
                }
                else
                    *charPP = charP;
            }
            break;
        case CFFI_K_TYPE_UNISTRING:
            if (memlifoP == NULL) {
                return ErrorInvalidValue(ip, NULL, "unistring type not supported in this struct context.");
            }
            else {
                int space;
                Tcl_UniChar *fromP = Tcl_GetUnicodeFromObj(valueObj, &space);
                Tcl_UniChar *toP;
                Tcl_UniChar **uniPP = indx + (Tcl_UniChar **)valueBaseP;
                if (space == 0
                    && (typeAttrsP->flags & CFFI_F_ATTR_NULLIFEMPTY)) {
                    *uniPP = NULL;
                }
                else {
                    space = sizeof(Tcl_UniChar) * (space + 1);
                    toP   = MemLifoAlloc(memlifoP, space);
                    memcpy(toP, fromP, space);
                    *uniPP = toP;
                }
            }
            break;
        default:
            return ErrorInvalidValue(
                ip, NULL, "Unsupported type.");
    }
    return TCL_OK;
#undef STOREINT_
}

/* Function: CffiNativeValueFromObj
 * Stores a native value of any type from Tcl_Obj wrapper
 *
 * Parameters:
 * ipCtxP - interpreter context
 * typeAttrsP - type attributes. The base type is used without
 *   considering the BYREF flag.
 * realArraySize - the actual array size to use when typeAttrsP specifies
 *   a dynamic array (typeAttrsP->dataType.arraySize == 0). Ignored otherwise.
 * valueObj - the *Tcl_Obj* containing the script level value
 * flags - if CFFI_F_PRESERVE_ON_ERROR is set, the target location will
 *   be preserved in case of errors.
 * valueBaseP - the base location where the native value is to be stored.
 * valueIndex - the index at which the value is to be stored
 * i.e. valueBaseP[valueIndex] is the final target
 * memlifoP - the memory allocator for fields that are typed as *string*
 *            or *unistring*. If *NULL*, fields of these types will
 *            result in an error being raised. Caller is responsible
 *            for ensuring the memory allocated from *memlifoP* stays
 *            allocated as long as the stored pointer is live.
 *
 * On error return, the location where the value is to be stored is not
 * guaranteed to be preserved unless the CFFI_F_PRESERVE_ON_ERROR bit is
 * set in the flags argument.
 *
 * Returns:
 * *TCL_OK* on success with a pointer to the structure stored in *structPP* or
 * *TCL_ERROR* on error with message stored in the interpreter.
 */
CffiResult
CffiNativeValueFromObj(CffiInterpCtx *ipCtxP,
                       const CffiTypeAndAttrs *typeAttrsP,
                       int realArraySize,
                       Tcl_Obj *valueObj,
                       CffiFlags flags,
                       void *valueBaseP,
                       int valueIndex,
                       MemLifo *memlifoP)
{
    Tcl_Interp *ip = ipCtxP->interp;
    void *valueP; /* Where value should be stored */
    int offset;

    CFFI_ASSERT(typeAttrsP->dataType.baseTypeSize != 0);

    /*
     * Note if CFFI_F_PRESERVE_ON_ERROR is set, output must be preserved on error
     */

    /* Calculate offset into memory where this value is to be stored */
    offset = valueIndex * CffiTypeActualSize(&typeAttrsP->dataType);
    valueP = offset + (char *)valueBaseP;

    if (CffiTypeIsNotArray(&typeAttrsP->dataType)) {
        CHECK(CffiNativeScalarFromObj(
            ipCtxP, typeAttrsP, valueObj, flags, valueP, 0, memlifoP));
    }
    else {
        Tcl_Obj **valueObjList;
        int indx;
        int nvalues;
        int count;
        int baseSize;

        baseSize = typeAttrsP->dataType.baseTypeSize;
        count = typeAttrsP->dataType.arraySize;
        if (count == 0)
            count = realArraySize;
        if (count <= 0)
            return Tclh_ErrorGeneric(
                ip,
                NULL,
                "Internal error: array size passed to "
                "CffiNativeValueFromObj is not a positive number.");

        switch (typeAttrsP->dataType.baseType) {
        case CFFI_K_TYPE_CHAR_ARRAY:
            if (flags & CFFI_F_PRESERVE_ON_ERROR) {
                CHECK(CffiCharsFromObjSafe(ip,
                                           typeAttrsP->dataType.u.tagObj,
                                           valueObj,
                                           (char *)valueP,
                                           count));
            }
            else {
                /* More efficient if no preservation on error */
                CHECK(CffiCharsFromObj(ip,
                                       typeAttrsP->dataType.u.tagObj,
                                       valueObj,
                                       (char *)valueP,
                                       count));
            }
            break;
        case CFFI_K_TYPE_UNICHAR_ARRAY:
            CHECK(CffiUniCharsFromObjSafe(
                ip, valueObj, (Tcl_UniChar *)valueP, count));
            break;
        case CFFI_K_TYPE_BYTE_ARRAY:
            CHECK(CffiBytesFromObjSafe(ip, valueObj, valueP, count));
            break;
        default:
            /* Store each contained element */
            if (Tcl_ListObjGetElements(ip, valueObj, &nvalues, &valueObjList)
                != TCL_OK)
                return TCL_ERROR; /* Note - if caller has specified too
                                   * few values, it's ok         \
                                   * because perhaps the actual count is
                                   * specified in another       \
                                   * parameter. If too many, only up to
                                   * array size */
            if (nvalues > count)
                nvalues = count;
            if (1 || ! (flags & CFFI_F_PRESERVE_ON_ERROR)) {
                for (indx = 0; indx < nvalues; ++indx) {
                    CHECK(CffiNativeScalarFromObj(ipCtxP,
                                                  typeAttrsP,
                                                  valueObjList[indx],
                                                  flags,
                                                  valueP,
                                                  indx,
                                                  memlifoP));
                }
            }
            else {
                /* Need temporary space so output not modified on error */
                Tcl_DString ds;
                CffiResult ret;
                int totalSize = nvalues * baseSize;
                void *tempP;
                Tcl_DStringInit(&ds);
                Tcl_DStringSetLength(&ds, totalSize);
                tempP = Tcl_DStringValue(&ds);
                /*
                 * NOTE: We are using temp output space so no need for
                 * called function to do so too so turn off PRESERVE_ON_ERROR
                 */
                for (indx = 0; indx < nvalues; ++indx) {
                    ret = CffiNativeScalarFromObj(
                        ipCtxP,
                        typeAttrsP,
                        valueObjList[indx],
                        flags & ~CFFI_F_PRESERVE_ON_ERROR,
                        tempP,
                        indx,
                        memlifoP);
                    if (ret != TCL_OK)
                        break;/* Need to free ds */
                }
                if (ret == TCL_OK)
                    memcpy(valueP, tempP, totalSize);
                Tcl_DStringFree(&ds);
                if (ret != TCL_OK)
                    return ret;
            }
            /* Fill additional unspecified elements with 0 */
            CFFI_ASSERT(indx == nvalues);
            if (indx < count) {
                memset((baseSize * indx) + (char *)valueP,
                       0,
                       baseSize * (count - indx));
            }

            break;
        }
    }
    return TCL_OK;
}

/* Function: CffiNativeScalarToObj
 * Wraps a scalar C binary value into a Tcl_Obj.
 *
 * Parameters:
 * ip - Interpreter
 * typeP - type descriptor for the binary value
 * valueBaseP - value to be wrapped in valueBaseP[valueIndex]
 * valueIndex - index as above
 * valueObjP - location to store the pointer to the returned Tcl_Obj.
 *    Following standard practice, the reference count on the Tcl_Obj is 0.
 *
 * This function handles single values that are void (function return),
 * numerics and pointers (astring, binary are pointers and hence scalars).
 * Note this wraps a single value of the type indicated in *typeAttrsP*,
 * even if the *count* field in the descriptor indicates an array.
 *
 * Returns:
 * *TCL_OK* on success with a pointer to the *Tcl_Obj* stored in *valueObjP*
 * *TCL_ERROR* on error with message stored in the interpreter.
 */
CffiResult
CffiNativeScalarToObj(Tcl_Interp *ip,
                      const CffiTypeAndAttrs *typeAttrsP,
                      void *valueBaseP,
                      int indx,
                      Tcl_Obj **valueObjP)
{
    int ret;
    Tcl_Obj *valueObj;
    enum CffiBaseType  baseType;

    baseType = typeAttrsP->dataType.baseType;

#define MAKEINTOBJ_(objfn_, type_)                                         \
    do {                                                                   \
        type_ value_ = *(indx + (type_ *)valueBaseP);                      \
        valueObj     = CffiIntValueToObj(typeAttrsP, (Tcl_WideInt)value_); \
        if (valueObj == NULL)                                              \
            valueObj = objfn_(value_);                                     \
    } while (0)

    switch (baseType) {
    case CFFI_K_TYPE_VOID:
        /* basically for functions returning void */
        valueObj = Tcl_NewObj();
        break;
    case CFFI_K_TYPE_SCHAR:
        MAKEINTOBJ_(Tcl_NewIntObj,signed char);
        break;
    case CFFI_K_TYPE_UCHAR:
        MAKEINTOBJ_(Tcl_NewIntObj,unsigned char);
        break;
    case CFFI_K_TYPE_SHORT:
        MAKEINTOBJ_(Tcl_NewIntObj,signed short);
        break;
    case CFFI_K_TYPE_USHORT:
        MAKEINTOBJ_(Tcl_NewIntObj,unsigned short);
        break;
    case CFFI_K_TYPE_INT:
        MAKEINTOBJ_(Tcl_NewIntObj,signed int);
        break;
    case CFFI_K_TYPE_UINT:
        MAKEINTOBJ_(Tcl_NewWideIntObj,unsigned int);
        break;
    case CFFI_K_TYPE_LONG:
        MAKEINTOBJ_(Tcl_NewLongObj,signed long);
        break;
    case CFFI_K_TYPE_ULONG:
        MAKEINTOBJ_(Tclh_ObjFromULong,unsigned long);
        break;
    case CFFI_K_TYPE_LONGLONG:
        CFFI_ASSERT(sizeof(signed long long) == sizeof(Tcl_WideInt));
        MAKEINTOBJ_(Tcl_NewWideIntObj,signed long long);
        break;
    case CFFI_K_TYPE_ULONGLONG:
        MAKEINTOBJ_(Tclh_ObjFromULongLong,unsigned long long);
        break;
    case CFFI_K_TYPE_FLOAT:
        valueObj = Tcl_NewDoubleObj(*(indx + (float *)valueBaseP));
        break;
    case CFFI_K_TYPE_DOUBLE:
        valueObj = Tcl_NewDoubleObj(*(indx + (double *)valueBaseP));
        break;
    case CFFI_K_TYPE_POINTER:
        ret = CffiPointerToObj(
            ip, typeAttrsP, *(indx + (void **)valueBaseP), &valueObj);
        if (ret != TCL_OK)
            return ret;
        break;
    case CFFI_K_TYPE_ASTRING:
        ret = CffiCharsToObj(ip, typeAttrsP, *(indx + (char **)valueBaseP), &valueObj);
        if (ret != TCL_OK)
            return ret;
        break;
    case CFFI_K_TYPE_UNISTRING:
        valueObj = Tcl_NewUnicodeObj(*(indx + (Tcl_UniChar **)valueBaseP), -1);
        break;
    case CFFI_K_TYPE_STRUCT:
    case CFFI_K_TYPE_CHAR_ARRAY:
    case CFFI_K_TYPE_UNICHAR_ARRAY:
    case CFFI_K_TYPE_BYTE_ARRAY:
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
 * typeP - type descriptor for the binary value. Should not be of type binary
 * valueBaseP - pointer to base of C binary value to wrap
 * startIndex - starting index into valueBaseP
 * count - number of values pointed to by valueP. *< 0* indicates a scalar,
 *         positive number (even *1*) is size of array. Size of 0
 *         will raise error
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
                     void *valueBaseP,
                     int startIndex,
                     int count,
                     Tcl_Obj **valueObjP)
{
    int ret;
    int offset;
    void *valueP;
    Tcl_Obj *valueObj;
    CffiBaseType baseType = typeAttrsP->dataType.baseType;

    CFFI_ASSERT(baseType != CFFI_K_TYPE_BINARY);
    if (count == 0) {
        Tcl_SetResult(ip, "Internal error: count=0 passed to CffiNativeValueToObj.", TCL_STATIC);
        return TCL_ERROR;
    }

    /* Calculate offset into memory where this value is to be stored */
    offset = startIndex * CffiTypeActualSize(&typeAttrsP->dataType);
    valueP = offset + (char *)valueBaseP;

    switch (baseType) {
    case CFFI_K_TYPE_STRUCT:
        if (count < 0) {
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
        if (count < 0) {
            return CffiNativeScalarToObj(ip, typeAttrsP, valueP, 0, valueObjP);
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
                    ip, typeAttrsP, offset + (char *)valueP, 0, &valueObj);
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
    CffiAttrFlags flags = typeAttrsP->flags;

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



/* Function: CffiCharsFromTclString
 * Encodes a Tcl utf8 string to a character array based on a type encoding.
 *
 * Parameters:
 * ip - interpreter
 * encObj - *Tcl_Obj* holding encoding or *NULL*
 * fromP - Tcl utf8 string to be converted
 * fromLen - length of the Tcl string. If < 0, null terminated
 * toP - buffer to store the encoded string
 * toSize - size of buffer
 *
 * Note that the output buffer contents may be overwritten even on error.
 *
 * Returns:
 * *TCL_OK* on success with  or *TCL_ERROR* * on failure with error message
 * in the interpreter.
 */
CffiResult
CffiCharsFromTclString(Tcl_Interp *ip,
                       Tcl_Obj *encObj,
                       const char *fromP,
                       int fromLen,
                       char *toP,
                       int toSize)
{
    Tcl_Encoding encoding;
    CffiResult ret;

    if (fromLen < 0)
        fromLen = Tclh_strlen(fromP);

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
    if (ret == TCL_OK)
        return TCL_OK;
    return Tclh_ErrorEncodingFromUtf8(ip, ret, fromP, fromLen);
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
 * Note that the output buffer contents may be overwritten even on error.
 * Use CffiCharsFromObjSafe if this is not acceptable.
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

    fromP = Tcl_GetStringFromObj(fromObj, &fromLen);
    return CffiCharsFromTclString(ip, encObj, fromP, fromLen, toP, toSize);
}

/* Function: CffiCharsInMemlifoFromObj
 * Encodes a Tcl_Obj to a character array in a *MemLifo* based on a type
 * encoding.
 *
 * Parameters:
 * ip - interpreter
 * encObj - *Tcl_Obj* holding encoding or *NULL*
 * fromObj - *Tcl_Obj* containing value to be stored
 * memlifoP - *MemLifo* to allocate memory from
 * outPP - location to store pointer to encoded string in MemLifo
 *
 * Returns:
 * *TCL_OK* on success with  or *TCL_ERROR* * on failure with error message
 * in the interpreter.
 */
CffiResult
CffiCharsInMemlifoFromObj(Tcl_Interp *ip,
                          Tcl_Obj *encObj,
                          Tcl_Obj *fromObj,
                          MemLifo *memlifoP,
                          char **outPP)
{
    const char *fromP;
    int fromLen, dstLen, srcLen, dstSpace;
    int srcLatestRead, dstLatestWritten;
    const char *srcP;
    char *dstP;
    int flags;
    int status;
    Tcl_EncodingState state;
    Tcl_Encoding encoding;

    if (encObj) {
        /* Should not really fail since check should have happened at
         * prototype parsing time */
        CHECK(CffiGetEncodingFromObj(ip, encObj, &encoding));
    }
    else
        encoding = NULL;

    fromP = Tcl_GetStringFromObj(fromObj, &fromLen);
    srcP = fromP;               /* Keep fromP unchanged for error messages */
    srcLen = fromLen;

    flags = TCL_ENCODING_START | TCL_ENCODING_END;

    dstSpace = srcLen + 2; /* Possibly two nuls */
    dstP = MemLifoAlloc(memlifoP, dstSpace);
    dstLen = 0;
    while (1) {
        /* dstP is buffer. dstLen is what's written so far */
        status = Tcl_UtfToExternal(ip, encoding, srcP, srcLen, flags, &state,
                                   dstP+dstLen,
                                dstSpace-dstLen, &srcLatestRead,
                                &dstLatestWritten, NULL);
        CFFI_ASSERT(dstSpace >= dstLatestWritten);
        dstLen += dstLatestWritten;
        /* Terminate loop on any  */
        if (status != TCL_CONVERT_NOSPACE) {
            if (status != TCL_OK)
                break;
            /* Tack on two nuls because we don't know how many nuls encoding uses */
            if ((dstSpace-dstLen) < 2)
                dstP = MemLifoResizeLast(memlifoP, dstSpace+(dstSpace-dstLen), 0);
            dstP[dstLen] = 0;
            dstP[dstLen+1] = 0;
            *outPP = dstP;
            if (encoding)
                Tcl_FreeEncoding(encoding);
            return TCL_OK;
        }
        flags &= ~ TCL_ENCODING_START;

        CFFI_ASSERT(srcLatestRead <= srcLen);
        srcP += srcLatestRead;
        srcLen -= srcLatestRead;
        if ((INT_MAX-dstSpace) < dstSpace)
            dstSpace = INT_MAX;
        else
            dstSpace = 2 * dstSpace;
        dstP = MemLifoResizeLast(memlifoP, dstSpace, 0);
    }
    if (encoding)
        Tcl_FreeEncoding(encoding);
    return Tclh_ErrorEncodingFromUtf8(ip, status, fromP, fromLen);
}

/* Function: CffiCharsFromObjSafe
 * Encodes a Tcl_Obj to a character array based on a type encoding.
 *
 * Parameters:
 * ip - interpreter
 * encObj - *Tcl_Obj* holding encoding or *NULL*
 * fromObj - *Tcl_Obj* containing value to be stored
 * toP - buffer to store the encoded string
 * toSize - size of buffer
 *
 * Note that the output buffer contents will not be overwritten on error.
 * Use CffiCharsFromObj for efficiency if overwriting on error is not a
 * problem.
 *
 * Returns:
 * *TCL_OK* on success with  or *TCL_ERROR* * on failure with error message
 * in the interpreter.
 */
CffiResult
CffiCharsFromObjSafe(
    Tcl_Interp *ip, Tcl_Obj *encObj, Tcl_Obj *fromObj, char *toP, int toSize)
{
    Tcl_DString ds;
    char *dsBuf;
    CffiResult ret;

    Tcl_DStringInit(&ds);
    Tcl_DStringSetLength(&ds, toSize);
    dsBuf = Tcl_DStringValue(&ds);

    /*
     * Note as always we cannot use Tcl_UtfToExternalDString here because
     * of the usual string termination reasons - see comments in
     * CffiCharsFromTclString
     */
    ret = CffiCharsFromObj(ip, encObj, fromObj, dsBuf, toSize);
    if (ret == TCL_OK)
        memcpy(toP, dsBuf, toSize);
    Tcl_DStringFree(&ds);
    return ret;
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
               const char *srcP,
               Tcl_Obj **resultObjP)
{
    Tcl_DString dsDecoded;
    Tcl_Encoding encoding;

    CFFI_ASSERT(typeAttrsP->dataType.baseType == CFFI_K_TYPE_CHAR_ARRAY
                || typeAttrsP->dataType.baseType == CFFI_K_TYPE_ASTRING);

    if (srcP == NULL) {
        *resultObjP = Tcl_NewObj();
        return TCL_OK;
    }

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

/* Function: CffiUniCharsFromObjSafe
 * Encodes a Tcl_Obj to a *Tcl_UniChar* array.
 *
 * Parameters:
 * ip - interpreter
 * fromObj - *Tcl_Obj* containing value to be stored
 * toP - buffer to store the encoded string
 * toSize - size of buffer in *Tcl_UniChar* units
 *
 * The output buffer will be unmodified in case of an error return.
 *
 * Returns:
 * *TCL_OK* on success with  or *TCL_ERROR* * on failure with error message
 * in the interpreter.
 */
CffiResult
CffiUniCharsFromObjSafe(
    Tcl_Interp *ip, Tcl_Obj *fromObj, Tcl_UniChar *toP, int toSize)
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

/* Function: CffiBytesFromObjSafe
 * Encodes a Tcl_Obj to a C array of bytes
 *
 * Parameters:
 * ip - interpreter
 * fromObj - *Tcl_Obj* containing value to be stored
 * toP - buffer to store the encoded string
 * toSize - size of buffer
 *
 * The function leaves the output buffer unmodified on error.
 *
 * Returns:
 * *TCL_OK* on success with  or *TCL_ERROR* * on failure with error message
 * in the interpreter.
 */
CffiResult
CffiBytesFromObjSafe(Tcl_Interp *ip, Tcl_Obj *fromObj, unsigned char *toP, int toSize)
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
    CffiAttrFlags flags = typeAttrsP->flags;
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
    int count = typeP->arraySize;


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

    if (count > 0 || (count == 0 && typeP->countHolderObj == NULL))
        Tcl_AppendPrintfToObj(typeObj, "[%d]", count);
    else if (count == 0)
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
    CffiAttrFlags flags;

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
    int baseSize, size, alignment;
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
    ret = CffiTypeAndAttrsParse(
        ipCtxP, objv[2], parse_mode, &typeAttrs);
    if (ret == TCL_ERROR)
        return ret;

    if (cmdIndex == COUNT) {
        /* type count */
        if (typeAttrs.dataType.arraySize != 0 || typeAttrs.dataType.countHolderObj == NULL)
            Tcl_SetObjResult(ip, Tcl_NewIntObj(typeAttrs.dataType.arraySize));
        else
            Tcl_SetObjResult(ip, typeAttrs.dataType.countHolderObj);
    }
    else {
        CffiTypeLayoutInfo(&typeAttrs.dataType, &baseSize, &size, &alignment);
        if (cmdIndex == SIZE)
            Tcl_SetObjResult(ip, Tcl_NewIntObj(size));
        else {
            /* type info */
            Tcl_Obj *objs[10];
            objs[0] = Tcl_NewStringObj("Size", 4);
            objs[1] = Tcl_NewIntObj(size);
            objs[2] = Tcl_NewStringObj("Count", 5);
            if (typeAttrs.dataType.arraySize != 0
                || typeAttrs.dataType.countHolderObj == NULL)
                objs[3] = Tcl_NewIntObj(typeAttrs.dataType.arraySize);
            else
                objs[3] = typeAttrs.dataType.countHolderObj;
            objs[4] = Tcl_NewStringObj("Alignment", 9);
            objs[5] = Tcl_NewIntObj(alignment);
            objs[6] = Tcl_NewStringObj("Definition", 10);
            objs[7] = CffiTypeAndAttrsUnparse(&typeAttrs);
            objs[8] = Tcl_NewStringObj("BaseSize", 8);
            objs[9] = Tcl_NewIntObj(baseSize);
            Tcl_SetObjResult(ip, Tcl_NewListObj(10, objs));
        }
    }

    CffiTypeAndAttrsCleanup(&typeAttrs);
    return ret;
}
