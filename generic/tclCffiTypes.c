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
#define CFFI_VALID_INTEGER_ATTRS                                       \
    (CFFI_F_ATTR_PARAM_MASK | CFFI_F_ATTR_REQUIREMENT_MASK             \
     | CFFI_F_ATTR_ERROR_MASK | CFFI_F_ATTR_ENUM | CFFI_F_ATTR_BITMASK \
     | CFFI_F_ATTR_STRUCTSIZE)

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
        if (tagStr != NULL) {
            typeP->u.tagObj = CffiMakePointerTag(ip, tagStr, tagLen);
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
    int flags;
    int i;
    enum CffiBaseType baseType;
    int validAttrs;
    int temp;
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
    temp = CffiAliasGet(ipCtxP, objs[0], typeAttrP);
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
            flags |= CFFI_F_ATTR_NULLOK;
            break;
        case STRUCTSIZE:
            if (typeAttrP->dataType.count != 0) {
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
             * NULLIFEMPTY never allowed for any output. DISPOSE*
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
CffiIntValueFromObj(CffiInterpCtx *ipCtxP,
                    const CffiTypeAndAttrs *typeAttrsP,
                    Tcl_Obj *valueObj,
                    Tcl_WideInt *valueP)
{
    Tcl_WideInt value;
    int flags       = typeAttrsP ? typeAttrsP->flags : 0;
    int lookup_enum = flags & CFFI_F_ATTR_ENUM;

    if (flags & CFFI_F_ATTR_BITMASK) {
        /* TBD - Does not handle size truncation */
        return
            CffiEnumMemberBitmask(ipCtxP->interp,
                            lookup_enum ? typeAttrsP->dataType.u.tagObj : NULL,
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
    if (Tcl_GetWideIntFromObj(ipCtxP->interp, valueObj, &value) == TCL_OK) {
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
 */
CffiResult
CffiNativeScalarToObj(Tcl_Interp *ip,
                      const CffiTypeAndAttrs *typeAttrsP,
                      void *valueP,
                      Tcl_Obj **valueObjP)
{
    int ret;
    Tcl_Obj *valueObj;
    enum CffiBaseType  baseType;

    baseType = typeAttrsP->dataType.baseType;

#define MAKEINTOBJ_(objfn_, type_)                                         \
    do {                                                                   \
        type_ value_ = *(type_ *)valueP;                                   \
        valueObj     = CffiIntValueToObj(typeAttrsP, (Tcl_WideInt)value_); \
        if (valueObj == NULL)                                              \
            valueObj = objfn_(value_);                                     \
    } while (0)

    switch (baseType) {
    case CFFI_K_TYPE_VOID:
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
    case CFFI_K_TYPE_ASTRING:
        ret = CffiCharsToObj(ip, typeAttrsP, *(char **)valueP, &valueObj);
        if (ret != TCL_OK)
            return ret;
        break;
    case CFFI_K_TYPE_UNISTRING:
        valueObj = Tcl_NewUnicodeObj(*(Tcl_UniChar **)valueP, -1);
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
    CFFI_ASSERT(baseType != CFFI_K_TYPE_BINARY);

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

    return CffiCharsToObj(ip, typeAttrsP, srcP, resultObjP);
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
CffiCharsInMemlifoFromObj(
    Tcl_Interp *ip, Tcl_Obj *encObj, Tcl_Obj *fromObj, MemLifo *memlifoP, char **outPP)
{
    Tcl_DString ds;
    Tcl_Encoding encoding;
    char *fromP;
    char *p;
    int len;

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

    fromP = Tcl_UtfToExternalDString(encoding, Tcl_GetString(fromObj), -1, &ds);
    if (encoding)
        Tcl_FreeEncoding(encoding);
    len = Tcl_DStringLength(&ds);

    /*
     * The encoded string in ds may be terminated by one or two nulls
     * depending on the encoding. We do not know which. Moreover,
     * Tcl_DStringLength does not tell us either. So we just tack on an
     * extra two null bytes;
     */
    p = MemLifoAlloc(memlifoP, len+2);
    memmove(p, fromP, len);
    p[len]     = 0;
    p[len + 1] = 0;
    *outPP = p;

    Tcl_DStringFree(&ds);
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
    ret = CffiTypeAndAttrsParse(
        ipCtxP, objv[2], parse_mode, &typeAttrs);
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
