/*
 * Copyright (c) 2021-2023, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#define TCLH_SHORTNAMES
#include "tclCffiInt.h"

static int CffiStructFindField(Tcl_Interp *ip,
                               CffiStruct *structP,
                               const char *fieldNameP);

static CffiStruct *CffiStructCkalloc(Tcl_Size nfields)
{
    size_t         sz;
    CffiStruct *structP;

    sz = offsetof(CffiStruct, fields) + (nfields * sizeof(structP->fields[0]));
    structP = ckalloc(sz);
    memset(structP, 0, sz);
    return structP;
}

void
CffiStructUnref(CffiStruct *structP)
{
    if (structP->nRefs <= 1) {
        int i;
#ifdef CFFI_USE_LIBFFI
        if (! CffiStructIsUnion(structP)) {
            CffiLibffiStruct *nextP = structP->libffiTypes;
            while (nextP) {
                CffiLibffiStruct *nextNextP = nextP->nextP;
                ckfree(nextP);
                nextP = nextNextP;
            }
        }
#endif
#if defined(CFFI_USE_DYNCALL) && defined(CFFI_HAVE_STRUCT_BYVAL)
        if (structP->dcAggrP)
            dcFreeAggr(structP->dcAggrP);
#endif
        if (structP->name)
            Tcl_DecrRefCount(structP->name);
        for (i = 0; i < structP->nFields; ++i) {
            if (structP->fields[i].nameObj)
                Tcl_DecrRefCount(structP->fields[i].nameObj);
            CffiTypeAndAttrsCleanup(&structP->fields[i].fieldType);
        }
        ckfree(structP);
    }
    else {
        structP->nRefs -= 1;
    }
}

/* Function: CffiFindDynamicCountField
 * Returns the index of the field holding count of a variable sized
 * array in a struct
 *
 * Parameters:
 * ip - interpreter
 * structP - parent struct descriptor
 * fieldNameObj - name of field holding the count
 *
 * Returns:
 * The index of the parameter or -1 if not found with error message
 * in the interpreter
 */
static int
CffiFindDynamicCountField(Tcl_Interp *ip,
                          CffiStruct *structP,
                          Tcl_Obj *fieldNameObj)
{
    int i;
    const char *name = Tcl_GetString(fieldNameObj);

    CFFI_ASSERT(!CffiStructIsUnion(structP));

    /* Could use CffiStructFindField but order of tests makes this faster */

    /* Note only last field can be variable sized and cannot be the count */
    for (i = 0; i < structP->nFields-1; ++i) {
        CffiField *fieldP = &structP->fields[i];
        if (CffiTypeIsNotArray(&fieldP->fieldType.dataType)
            && CffiTypeIsInteger(fieldP->fieldType.dataType.baseType)
            && !strcmp(name, Tcl_GetString(fieldP->nameObj))) {
            return i;
        }
    }
    (void)Tclh_ErrorNotFound(ip,
                             "Field",
                             fieldNameObj,
                             "Could not find referenced count for dynamic "
                             "array, possibly wrong type or not scalar.");
    return -1;

}

/* Function: CffiStructGetDynamicCountNative
 * Returns the count from the field storing the size of a VLA in a variable
 * sized struct.
 *
 * Parameters:
 * ipCtxP - interp context
 * structP - struct descriptor
 * valueP - struct value
 *
 * Returns:
 * The count (> 0) or -1 on error.
 */
static int
CffiStructGetDynamicCountNative(CffiInterpCtx *ipCtxP,
                                const CffiStruct *structP,
                                void *valueP)
{
    int fldIndex = structP->dynamicCountFieldIndex;

    CFFI_ASSERT(!CffiStructIsUnion(structP));
    CFFI_ASSERT(structP->dynamicCountFieldIndex >= 0);
    /* fldIndex can never be last one, already checked in parse */
    CFFI_ASSERT(fldIndex < structP->nFields - 1);

    const CffiField *fieldP = &structP->fields[fldIndex];
    ptrdiff_t offset = fieldP->offset;
    int vlaCount = CffiGetCountFromNative(
        offset + (char *)valueP, fieldP->fieldType.dataType.baseType);

    if (vlaCount < 0) {
        Tclh_ErrorInvalidValue(
            ipCtxP->interp,
            NULL,
            "Variable length array size must be greater than 0.");
        return -1;
    }
    return vlaCount;
}

/* Function: CffiStructGetDynamicCountFromObj
 * Returns the count from the field storing the size of a VLA in a variable
 * sized struct.
 *
 * Parameters:
 * ipCtxP - interp context
 * structP - struct descriptor
 * structValueObj - script level struct value
 *
 * Returns:
 * The count (> 0) or -1 on error.
 */
static int
CffiStructGetDynamicCountFromObj(CffiInterpCtx *ipCtxP,
                             const CffiStruct *structP,
                             Tcl_Obj *structValueObj)
{
    int fldIndex = structP->dynamicCountFieldIndex;
    Tcl_Interp *ip = ipCtxP ? ipCtxP->interp : NULL;

    CFFI_ASSERT(!CffiStructIsUnion(structP));
    CFFI_ASSERT(structP->dynamicCountFieldIndex >= 0);
    /* fldIndex can never be last one, already checked in parse */
    CFFI_ASSERT(fldIndex < structP->nFields - 1);

    if (structValueObj == NULL) {
        goto unknown_count;
    }
    Tcl_Obj *countObj;
    if (Tcl_DictObjGet(
            ip, structValueObj, structP->fields[fldIndex].nameObj, &countObj)
            != TCL_OK) {
        return -1; /* Invalid dict */
    }
    if (countObj == NULL)
        goto unknown_count; /* Valid dict but no count field value */

    int count;
    if (Tcl_GetIntFromObj(ip, countObj, &count) != TCL_OK)
        return -1;
    if (count < 0) {
        Tclh_ErrorInvalidValue(
            ip,
            countObj,
            "Variable length array size must be greater than 0.");
        return -1;
    }
    return count;

unknown_count:
    Tclh_ErrorInvalidValue(ip,
                           structP->fields[fldIndex].nameObj,
                           "No value supplied for dynamic field count.");
    return -1;
}

/* Function: CffiStructSizeForVLACount
 * Get the number of bytes needed to store a struct with a variable length
 * array field.
 *
 * Parameters:
 * ipCtxP - interp context. Used for error messages. May be NULL.
 * structP - struct descriptor. Must be a struct of variable size
 * vlaCount - number of array elements in variable component of the struct.
 *   This may be either the passed struct or a nested field. Note there
 *   can be only one variable sized array in any structure even taking
 *   nesting into account.
 * sizeP - output location to hold size of struct (may be NULL)
 * fixedSizeP - output location to hold the fixed size of the struct
 *   i.e. size with vlacount == 0 (may be NULL)
 *
 * Returns:
 * TCL_OK on success, TCL_ERROR on failure.
 * The interp result holds an error message on failure.
 */
CffiResult
CffiStructSizeForVLACount(CffiInterpCtx *ipCtxP,
                          CffiStruct *structP,
                          int vlaCount,
                          int *sizeP,
                          int *fixedSizeP)
{
    if (!CffiStructIsVariableSize(structP) || sizeP == NULL) {
        if (sizeP)
            *sizeP = structP->size;
        if (fixedSizeP)
            *fixedSizeP = structP->size;
        return TCL_OK;
    }

    Tcl_Interp *ip = ipCtxP ? ipCtxP->interp : NULL;

    if (vlaCount < 0) {
        return Tclh_ErrorInvalidValue(
            ip, NULL, "Variable length array size must be greater than 0.");
    }

    int fixedSize = structP->size; /* Base size - should include alignment */
    int size;

    /*
     * There are two possibilities. The struct is variable size because
     *  1 the last field is a variable sized array whose
     *    count is given by another field
     *  2 the last field is a struct whose last field is variable size
     *    because of either of these two reasons.
     * Note that arrays of variable sized structs are not permitted.
     */

    /* typeP -> type for last field, only one that can be variably sized */
    CffiType *typeP  = &structP->fields[structP->nFields-1].fieldType.dataType;
    CFFI_ASSERT(CffiTypeIsVariableSize(typeP));
    if (structP->dynamicCountFieldIndex >= 0) {
        /* Case 1 */
        CFFI_ASSERT(CffiTypeIsVLA(typeP));

        int elemAlignment;
        int elemSize;
        CffiTypeLayoutInfo(ipCtxP, typeP, 0, &elemSize, NULL, &elemAlignment);
        /* Fixed size should already be aligned to element size */
        CFFI_ASSERT((fixedSize & (elemAlignment - 1)) == 0);
        size = fixedSize + vlaCount * elemSize;
    } else {
        /* Case 2 - vlaCount is meant for nested component */
        CFFI_ASSERT(typeP->baseType == CFFI_K_TYPE_STRUCT);
        CffiStruct *innerStructP = typeP->u.structP;
        int innerSize;
        int innerFixedSize;
        CHECK(CffiStructSizeForVLACount(
            ipCtxP, innerStructP, vlaCount, &innerSize, &innerFixedSize));
        /* Fixed size should already be aligned to inner struct size */
        CFFI_ASSERT((fixedSize & (innerStructP->alignment - 1)) == 0);
        /*
         * The fixed portion of the inner struct is already included in the
         * outer struct size as well as in the inner struct fixed size. So
         * we have to subtract one of these to avoid double counting.
         */
        size = fixedSize + innerSize - innerFixedSize;
    }
    /* Now ensure whole thing aligned to struct size */
    size = (size + structP->alignment - 1) & ~(structP->alignment - 1);
    if (sizeP)
        *sizeP = size;
    if (fixedSizeP)
        *fixedSizeP = fixedSize;
    return TCL_OK;
}

/* Function: CffiStructSizeForObj
 * Get the number of bytes needed to store a struct.
 *
 * Parameters:
 * ipCtxP - interp context. Used for error messages. May be NULL.
 * structP - struct descriptor
 * structValueObj - struct value as a dictionary mapping field names to values.
 *   May be NULL for unions and fixed size structs.
 * sizeP - output location to hold size of struct
 * fixedSizeP - output location to hold the fixed size of the struct
 *   i.e. size with vlacount == 0
 *
 * The function takes into account variable sized structs.
 *
 * Returns:
 * TCL_OK on success, TCL_ERROR on failure.
 * The interp result holds an error message on failure.
 */
CffiResult
CffiStructSizeForObj(CffiInterpCtx *ipCtxP,
                     const CffiStruct *structP,
                     Tcl_Obj *structValueObj,
                     int *sizeP,
                     int *fixedSizeP)
{
    if (!CffiStructIsVariableSize(structP) || sizeP == NULL
        || CffiStructIsUnion(structP)) {
        if (sizeP)
            *sizeP = structP->size;
        if (fixedSizeP)
            *fixedSizeP = structP->size;
        return TCL_OK;
    }

    int fixedSize = structP->size; /* Should include alignment */
    int size;

    /*
     * There are two possibilities. The struct is variable size because
     *  1 the last field is a variable sized array whose
     *    count is given by another field
     *  2 the last field is a struct whose last field is variable size
     *    because of either of these two reasons.
     * Note that arrays of variable sized structs are not permitted.
     */
    int lastFldIndex = structP->nFields - 1;
    const CffiType *typeP  = &structP->fields[lastFldIndex].fieldType.dataType;
    CFFI_ASSERT(CffiTypeIsVariableSize(typeP));
    if (structP->dynamicCountFieldIndex >= 0) {
        /* Case 1 */
        CFFI_ASSERT(CffiTypeIsVLA(typeP));

        int elemAlignment;
        int elemSize;
        CffiTypeLayoutInfo(ipCtxP, typeP, 0, &elemSize, NULL, &elemAlignment);
        /* Fixed size should already be aligned to element size */
        CFFI_ASSERT((fixedSize & (elemAlignment - 1)) == 0);

        int count;
        count = CffiStructGetDynamicCountFromObj(ipCtxP, structP, structValueObj);
        if (count < 0)
            return TCL_ERROR;

        size = fixedSize + count * elemSize;
    } else {
        /* Case 2 - recurse to get variable inner struct size and add */
        CFFI_ASSERT(typeP->baseType == CFFI_K_TYPE_STRUCT);
        CffiStruct *innerStructP = typeP->u.structP;
        Tcl_Obj *innerStructObj;
        if (Tcl_DictObjGet(ipCtxP->interp,
                           structValueObj,
                           structP->fields[lastFldIndex].nameObj,
                           &innerStructObj)
            != TCL_OK) {
            return TCL_ERROR;
        }
        if (innerStructObj == NULL) {
            /* Dict is valid but field not found */
            return Tclh_ErrorInvalidValue(
                ipCtxP->interp,
                structP->fields[lastFldIndex].nameObj,
                "No value supplied for variable sized field.");
        }
        int innerFixedSize;
        int innerSize;
        CHECK(CffiStructSizeForObj(
            ipCtxP, innerStructP, innerStructObj, &innerSize, &innerFixedSize));
        /* Fixed size should already be aligned to inner struct size */
        CFFI_ASSERT((fixedSize & (innerStructP->alignment - 1)) == 0);
        /*
         * The fixed portion of the inner struct is already included in the
         * outer struct size as well as in the inner struct fixed size. So
         * we have to subtract one of these to avoid double counting.
         */
        size = fixedSize + innerSize - innerFixedSize;
    }
    /* Now ensure whole thing aligned to struct size */
    size = (size + structP->alignment - 1) & ~(structP->alignment - 1);
    if (sizeP)
        *sizeP = size;
    if (fixedSizeP)
        *fixedSizeP = fixedSize;
    return TCL_OK;
}

/* Function: CffiStructSizeForNative
 * Get the number of bytes in a native struct.
 *
 * Parameters:
 * ipCtxP - interp context. Used for error messages. May be NULL.
 * structP - struct descriptor
 * valueP - pointer to a native struct
 * sizeP - output location to hold size of struct
 * fixedSizeP - output location to hold the fixed size of the struct
 *   i.e. size with vlacount == 0
 *
 * The function takes into account variable sized structs.
 *
 * Returns:
 * TCL_OK on success, TCL_ERROR on failure.
 * The interp result holds an error message on failure.
 */
CffiResult
CffiStructSizeForNative(CffiInterpCtx *ipCtxP,
                        const CffiStruct *structP,
                        void *valueP,
                        int *sizeP,
                        int *fixedSizeP)
{
    if (!CffiStructIsVariableSize(structP) || sizeP == NULL
        || CffiStructIsUnion(structP)) {
        if (sizeP)
            *sizeP = structP->size;
        if (fixedSizeP)
            *fixedSizeP = structP->size;
        return TCL_OK;
    }

    int fixedSize = structP->size; /* Should include alignment */
    int size;

    /*
     * There are two possibilities. The struct is variable size because
     *  1 the last field is a variable sized array whose
     *    count is given by another field
     *  2 the last field is a struct whose last field is variable size
     *    because of either of these two reasons.
     * Note that arrays of variable sized structs are not permitted.
     */
    int lastFldIndex = structP->nFields - 1;
    const CffiType *typeP  = &structP->fields[lastFldIndex].fieldType.dataType;
    CFFI_ASSERT(CffiTypeIsVariableSize(typeP));
    if (structP->dynamicCountFieldIndex >= 0) {
        /* Case 1 */
        CFFI_ASSERT(CffiTypeIsVLA(typeP));

        int elemAlignment;
        int elemSize;
        CffiTypeLayoutInfo(ipCtxP, typeP, 0, &elemSize, NULL, &elemAlignment);
        /* Fixed size should already be aligned to element size */
        CFFI_ASSERT((fixedSize & (elemAlignment - 1)) == 0);

        int count;
        count = CffiStructGetDynamicCountNative(ipCtxP, structP, valueP);
        if (count < 0)
            return TCL_ERROR;

        size = fixedSize + count * elemSize;
    }
    else {
        /* Case 2 - recurse to get variable inner struct size and add */
        CFFI_ASSERT(typeP->baseType == CFFI_K_TYPE_STRUCT);
        CffiStruct *innerStructP = typeP->u.structP;
        void *innerValueP;
        int innerFixedSize;
        int innerSize;

        innerValueP = structP->fields[lastFldIndex].offset + (char *)valueP;
        CHECK(CffiStructSizeForNative(
            ipCtxP, innerStructP, innerValueP, &innerSize, &innerFixedSize));
        /* Fixed size should already be aligned to inner struct size */
        CFFI_ASSERT((fixedSize & (innerStructP->alignment - 1)) == 0);
        /*
         * The fixed portion of the inner struct is already included in the
         * outer struct size as well as in the inner struct fixed size. So
         * we have to subtract one of these to avoid double counting.
         */
        size = fixedSize + innerSize - innerFixedSize;
    }
    /* Now ensure whole thing aligned to struct size */
    size = (size + structP->alignment - 1) & ~(structP->alignment - 1);
    if (sizeP)
        *sizeP = size;
    if (fixedSizeP)
        *fixedSizeP = fixedSize;
    return TCL_OK;
}

/* Function: CffiStructComputeFieldAddress
 * Calculates the offset of a field within a struct in an array of structs.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * structP - struct descriptor
 * structAddr - address of the native struct
 * fldNameObj - name of the field.
 * fldIndexP - (out) location to hold index of field
 * fldAddrP - (out) location to hold the address of the field within the struct
 * fldArraySizeP - (out) number of elements in the field (-1 if scalar)
 *
 * Returns:
 * TCL_OK on success, TCL_ERROR on failure with error message in interpreter
 */
static CffiResult
CffiStructComputeFieldAddress(CffiInterpCtx *ipCtxP,
                              CffiStruct *structP,
                              void *structAddr,
                              Tcl_Obj *fldNameObj,
                              int *fldIndexP,
                              void **fldAddrP,
                              int *fldArraySizeP)
{
    Tcl_Interp *ip  = ipCtxP->interp;

    int fldIndex = CffiStructFindField(ip, structP, Tcl_GetString(fldNameObj));
    if (fldIndex < 0)
        return TCL_ERROR;

    CffiField *fieldP = &structP->fields[fldIndex];
    int fldArraySize     = fieldP->fieldType.dataType.arraySize;

    /*
     * Struct may be variable size either because
     *  - last field is an VLA, or
     *  - because last field is nested struct with a VLA. I
     * If we are retrieving the last field and it is a VLA (first case),
     * retrieve the dynamic array size
     */
    if (fldIndex == (structP->nFields - 1)
        && CffiTypeIsVLA(&fieldP->fieldType.dataType)) {
        CFFI_ASSERT(!CffiStructIsUnion(structP));
        fldArraySize =
            CffiStructGetDynamicCountNative(ipCtxP, structP, structAddr);
        if (fldArraySize < 0) {
            Tclh_ErrorGeneric(
                ip, NULL, "Internal error: field array size is negative.");
            return TCL_ERROR;
        }
    }

    *fldIndexP     = fldIndex;
    *fldAddrP      = fieldP->offset + (char *)structAddr;
    *fldArraySizeP = fldArraySize;
    return TCL_OK;
}

/* Function: CffiStructComputeAddress
 * Calculates the address of a struct in an array of structs.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * structP - struct descriptor
 * nativePointerObj - Tcl_Obj holding the pointer to the native struct location
 * safe - if non-0, nativePointerObj must be a registered pointer
 * indexObj - Tcl_Obj containing index into array of structs. If NULL, defaults
 *  to index value of 0.
 * structAddrP - (out) location to hold address of the struct
 *
 * Returns:
 * TCL_OK on success, TCL_ERROR on failure with error message in interpreter
 */
static CffiResult
CffiStructComputeAddress(CffiInterpCtx *ipCtxP,
                         CffiStruct *structP,
                         Tcl_Obj *nativePointerObj,
                         int safe,
                         Tcl_Obj *indexObj,
                         void **structAddrP)
{
    Tcl_Interp *ip  = ipCtxP->interp;
    void *structAddr;
    Tclh_ReturnCode ret;
    Tclh_PointerTypeTag tag;
    Tclh_PointerTagRelation tagRelation;
    Tclh_PointerRegistrationStatus registration;

    ret = Tclh_PointerObjDissect(ip,
                                 ipCtxP->tclhCtxP,
                                 nativePointerObj,
                                 structP->name,
                                 &structAddr,
                                 &tag,
                                 &tagRelation,
                                 &registration);
    if (ret != TCL_OK)
        return ret;

    if (structAddr == NULL)
        return Tclh_ErrorPointerNull(ip);

    switch (tagRelation) {
    case TCLH_TAG_RELATION_EQUAL:
    case TCLH_TAG_RELATION_IMPLICITLY_CASTABLE:
        break;
    case TCLH_TAG_RELATION_UNRELATED:
    case TCLH_TAG_RELATION_EXPLICITLY_CASTABLE:
    default:
        return Tclh_ErrorPointerObjType(ip, nativePointerObj, structP->name);
    }

    if (safe) {
        switch (registration) {
        case TCLH_POINTER_REGISTRATION_OK:
        case TCLH_POINTER_REGISTRATION_DERIVED:
            break;
        case TCLH_POINTER_REGISTRATION_MISSING:
        case TCLH_POINTER_REGISTRATION_WRONGTAG:
        default:
            return Tclh_ErrorPointerObjRegistration(
                ip, nativePointerObj, registration);
        }
    }

    int structIndex; /* Index into array of structs */
    if (indexObj) {
        Tcl_WideInt wide;
        CHECK(Tclh_ObjToRangedInt(ip, indexObj, 0, INT_MAX, &wide));
        structIndex = (int)wide;
    } else
        structIndex = 0;

    if (CffiStructIsVariableSize(structP)) {
        /* Arrays of variable sized structs are not allowed. */
        if (structIndex > 0)
            return CffiErrorStructIsVariableSize(ip, structP, "indexing");
    } else {
        /* Not variable size struct. Indexing allowed. */
        structAddr = (structP->size * structIndex) + (char *)structAddr;
    }
    *structAddrP = structAddr;
    return TCL_OK;
}

/* Function: CffiStructInitSizeField
 * Initializes a field with the structsize annotation
 *
 * Parameters:
 * structP - struct descriptor. Must have a field with structsize annotation
 * structAddress - pointer to native struct
 */
void
CffiStructInitSizeField(const CffiStruct *structP,
                        void *structAddress)
{
    CFFI_ASSERT(structP->structSizeFieldIndex >= 0);
    CFFI_ASSERT(!CffiStructIsVariableSize(structP));

    Tcl_Size structSize     = structP->size;
    const CffiField *fieldP = &structP->fields[structP->structSizeFieldIndex];
    void *fieldAddress      = fieldP->offset + (char *)structAddress;

    switch (fieldP->fieldType.dataType.baseType) {
    case CFFI_K_TYPE_SCHAR:
        *(signed char *)fieldAddress = (signed char)structSize;
        break;
    case CFFI_K_TYPE_UCHAR:
        *(unsigned char *)fieldAddress = (unsigned char)structSize;
        break;
    case CFFI_K_TYPE_SHORT:
        *(short *)fieldAddress = (short)structSize;
        break;
    case CFFI_K_TYPE_USHORT:
        *(unsigned short *)fieldAddress = (unsigned short)structSize;
        break;
    case CFFI_K_TYPE_INT:
        *(int *)fieldAddress = (int)structSize;
        break;
    case CFFI_K_TYPE_UINT:
        *(unsigned int *)fieldAddress = (unsigned int)structSize;
        break;
    case CFFI_K_TYPE_LONG:
        *(long *)fieldAddress = (long)structSize;
        break;
    case CFFI_K_TYPE_ULONG:
        *(unsigned long *)fieldAddress = (unsigned long)structSize;
        break;
    case CFFI_K_TYPE_LONGLONG:
        *(long long *)fieldAddress = (long long)structSize;
        break;
    case CFFI_K_TYPE_ULONGLONG:
        *(unsigned long long *)fieldAddress = (unsigned long long)structSize;
        break;
    default:
        TCLH_PANIC("Field type not valid for structsize annotation");
        break;
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
 * baseType - CFFI_K_TYPE_STRUCT or CFFI_K_TYPE_UNION
 * pack - packing (0 for natural alignment)
 * structObj - structure definition
 * structPP - pointer to location to store pointer to internal form.
 *
 * Returns:
 * Returns *TCL_OK* on success with a pointer to the internal struct
 * representation in *structPP* and *TCL_ERROR* on failure with error message
 * in the interpreter.
 *
 */
static CffiResult
CffiStructParse(CffiInterpCtx *ipCtxP,
                Tcl_Obj *nameObj,
                Tcl_Obj *structObj,
                CffiBaseType baseType,
                int pack,
                CffiStruct * *structPP)
{
    Tcl_Obj **objs;
    Tcl_Size i, j, nobjs, nfields;
    CffiStruct *structP;
    Tcl_Interp *ip = ipCtxP->interp;
    int isUnion = (baseType == CFFI_K_TYPE_UNION);

    if (Tcl_GetCharLength(nameObj) == 0) {
        return Tclh_ErrorInvalidValue(
            ip, nameObj, "Empty string specified for name.");
    }

    CHECK(Tcl_ListObjGetElements(ip, structObj, &nobjs, &objs));

    if (nobjs == 0 || (nobjs & 1))
        return Tclh_ErrorInvalidValue(
            ip, structObj, "Empty definition or missing type for field.");
    nfields = nobjs / 2; /* objs[] is alternating name, type list */

    structP = CffiStructCkalloc(nfields);
    structP->dynamicCountFieldIndex = -1;
    structP->structSizeFieldIndex = -1;
    structP->nFields = 0;     /* Update as we go along */
    structP->pack    = pack;

    for (i = 0, j = 0; i < nobjs; i += 2, ++j) {
        int k;
        if (CffiTypeAndAttrsParse(ipCtxP,
                                  objs[i + 1],
                                  CFFI_F_TYPE_PARSE_FIELD,
                                  &structP->fields[j].fieldType)
            != TCL_OK) {
            CffiStructUnref(structP);
            return TCL_ERROR;
        }
        if (structP->fields[j].fieldType.flags & CFFI_F_ATTR_STRUCTSIZE) {
            if (structP->structSizeFieldIndex >= 0) {
                return Tclh_ErrorInvalidValue(
                    ip,
                    objs[i + 1],
                    "A struct cannot have a multiple fields with the structsize annotation.");
            }
            structP->structSizeFieldIndex = (int) j;
        }
        if (CffiTypeIsVariableSize(&structP->fields[j].fieldType.dataType)) {
            if (isUnion) {
                CffiStructUnref(structP);
                return Tclh_ErrorInvalidValue(
                    ip,
                    objs[i + 1],
                    "A union cannot have a field of variable size.");
            }
            if (j < (nfields-1) || j == 0) {
                /* Only last field may be variable size and cannot be only one */
                CffiStructUnref(structP);
                return Tclh_ErrorInvalidValue(
                    ip,
                    objs[i + 1],
                    "Field of variable size must be the last and must not be the only field.");
            }
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
                    "Field names must be unique.");
            }
        }
        Tcl_IncrRefCount(objs[i]);
        structP->fields[j].nameObj = objs[i];
        /* Note: update incrementally for structP cleanup in case of errors */
        structP->nFields += 1;
    }

    /* Calculate metadata for all fields */
    int offset           = 0;
    int struct_alignment = 1;

    for (i = 0; i < nfields; ++i) {
        CffiField *fieldP = &structP->fields[i];
        int field_size;
        int field_alignment;
        CFFI_ASSERT(fieldP->fieldType.dataType.baseType != CFFI_K_TYPE_VOID);
        CffiTypeLayoutInfo(ipCtxP,
                           &fieldP->fieldType.dataType,
                           0,
                           NULL,
                           &field_size,
                           &field_alignment);
        /*
         * Note field_size will be 0 for dynamic fields - last one since we
         * do not know VLA size when defining type
         */
        CFFI_ASSERT(field_size > 0 || i == nfields - 1);

        if (structP->pack && structP->pack < field_alignment) {
            field_alignment = structP->pack;
        }

        if (field_alignment > struct_alignment)
            struct_alignment = field_alignment;

        fieldP->size = field_size;
        if (isUnion) {
            fieldP->offset = 0;
            if (field_size > offset)
                offset = field_size;
        } else {
            /* See if offset needs to be aligned for this field */
            offset = (offset + field_alignment - 1) & ~(field_alignment - 1);
            fieldP->offset = offset;
            if (field_size > 0) {
                /* Fixed size field */
                offset += field_size;
            }
        }
    }

    /*
     * Calculate and cache the field (if any) holding dynamic count of
     * elements in the last field
     */
    CffiType *lastFldTypeP = &structP->fields[nfields - 1].fieldType.dataType;
    if (CffiTypeIsVariableSize(lastFldTypeP)) {
        if (structP->structSizeFieldIndex >= 0) {
            CffiStructUnref(structP);
            return Tclh_ErrorInvalidValue(ip,
                                          NULL,
                                          "A variable size struct cannot have "
                                          "a field with the structsize "
                                          "annotation.");
        }
        if (CffiTypeIsVLA(lastFldTypeP)) {
            CFFI_ASSERT(lastFldTypeP->countHolderObj != NULL);
            int countFieldIndex;
            countFieldIndex = CffiFindDynamicCountField(
                ipCtxP->interp, structP, lastFldTypeP->countHolderObj);
            if (countFieldIndex < 0) {
                CffiStructUnref(structP);
                return TCL_ERROR;
            }
            structP->dynamicCountFieldIndex = countFieldIndex;
        } else {
            /* Nested variable component. Nought to do */
        }
        /*
         * Mark as variable size - last field is variable size array
         * or a nested struct with a variable size array.
         */
        structP->flags |= CFFI_F_STRUCT_VARSIZE;
    }

    Tcl_IncrRefCount(nameObj);
    structP->name      = nameObj;
#ifdef CFFI_USE_LIBFFI
    structP->libffiTypes = NULL;
#endif
    structP->nRefs     = 0;
    structP->alignment = struct_alignment;
    structP->size      = (offset + struct_alignment - 1) & ~(struct_alignment - 1);
    if (isUnion) {
        CFFI_ASSERT((structP->flags & CFFI_F_STRUCT_VARSIZE) == 0);
        structP->flags |= CFFI_F_STRUCT_UNION;
    }
    *structPP          = structP;
    return TCL_OK;
}

/* Function: CffiStructFindField
 * Returns the field index corresponding to a field name
 *
 * Parameters:
 * ip - interpreter. May be NULL if error messages not required.
 * structP - struct descriptor
 * fieldNameP - name of field
 *
 * Returns:
 * Index of field or -1 if not found.
 */
static int
CffiStructFindField(Tcl_Interp *ip, CffiStruct *structP, const char *fieldNameP)
{
    int i;
    char message[100];

    for (i = 0; i < structP->nFields; ++i) {
        if (!strcmp(fieldNameP, Tcl_GetString(structP->fields[i].nameObj)))
            return i;
    }
    snprintf(message,
             sizeof(message),
             "No such field in struct definition %s.",
             Tcl_GetString(structP->name));
    Tclh_ErrorNotFoundStr(ip, "Field", fieldNameP, message);
    return -1;
}

static void
CffiStructDescribeArray(CffiField *fieldP, Tcl_Obj *objP)
{
    if (CffiTypeIsVLA(&fieldP->fieldType.dataType)) {
        Tcl_Obj *countObj = fieldP->fieldType.dataType.countHolderObj;
        Tcl_AppendPrintfToObj(
            objP, "[%s]", countObj? Tcl_GetString(countObj) : "");

    }
    else if (CffiTypeIsArray(&fieldP->fieldType.dataType))
        Tcl_AppendPrintfToObj(
            objP, "[%d]", fieldP->fieldType.dataType.arraySize);
}

/* Function: CffiStructDescribeCmd
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
static CffiResult
CffiStructDescribeCmd(Tcl_Interp *ip,
                      int objc,
                      Tcl_Obj *const objv[],
                      CffiStructCmdCtx *structCtxP)
{
    int i;
    CffiStruct *structP = structCtxP->structP;
    Tcl_Obj *objP =
        Tcl_ObjPrintf("%s %s nRefs=%d size=%d alignment=%d flags=%d nFields=%d",
                      CffiStructIsUnion(structP) ? "Union" : "Struct",
                      Tcl_GetString(structP->name),
                      structP->nRefs,
                      structP->size,
                      structP->alignment,
                      structP->flags,
                      structP->nFields);
    for (i = 0; i < structP->nFields; ++i) {
        CffiField *fieldP = &structP->fields[i];
        enum CffiBaseType baseType = fieldP->fieldType.dataType.baseType;
        switch (baseType) {
        case CFFI_K_TYPE_POINTER:
            Tcl_AppendPrintfToObj(objP, "\n  %s", cffiBaseTypes[baseType].token);
            if (fieldP->fieldType.dataType.u.tagNameObj)
                Tcl_AppendPrintfToObj(
                    objP,
                    ".%s",
                    Tcl_GetString(fieldP->fieldType.dataType.u.tagNameObj));
            Tcl_AppendPrintfToObj(objP, " %s", Tcl_GetString(fieldP->nameObj));
            CffiStructDescribeArray(fieldP, objP);
            break;
        case CFFI_K_TYPE_ASTRING:
        case CFFI_K_TYPE_CHAR_ARRAY:
            Tcl_AppendPrintfToObj(objP, "\n  %s", cffiBaseTypes[baseType].token);
            if (fieldP->fieldType.dataType.u.encoding) {
                const char *encName =
                    Tcl_GetEncodingName(fieldP->fieldType.dataType.u.encoding);
                if (encName) {
                    Tcl_AppendPrintfToObj(objP, ".%s", encName);
                }
            }
            Tcl_AppendPrintfToObj(objP, " %s", Tcl_GetString(fieldP->nameObj));
            CffiStructDescribeArray(fieldP, objP);
            break;
        case CFFI_K_TYPE_UNISTRING:
        case CFFI_K_TYPE_UNICHAR_ARRAY:
#ifdef _WIN32
        case CFFI_K_TYPE_WINSTRING:
        case CFFI_K_TYPE_WINCHAR_ARRAY:
#endif
            Tcl_AppendPrintfToObj(objP, "\n  %s", cffiBaseTypes[baseType].token);
            Tcl_AppendPrintfToObj(objP, " %s", Tcl_GetString(fieldP->nameObj));
            CffiStructDescribeArray(fieldP, objP);
            break;
        case CFFI_K_TYPE_STRUCT:
            Tcl_AppendPrintfToObj(
                objP,
                "\n  %s %s %s",
                CffiStructIsUnion(fieldP->fieldType.dataType.u.structP) ? "union" : cffiBaseTypes[baseType].token,
                Tcl_GetString(fieldP->fieldType.dataType.u.structP->name),
                Tcl_GetString(fieldP->nameObj));
            if (CffiTypeIsVLA(&fieldP->fieldType.dataType)) {
                Tcl_AppendPrintfToObj(
                    objP, "[%s]", Tcl_GetString(fieldP->fieldType.dataType.countHolderObj));
            }
            else if (CffiTypeIsArray(&fieldP->fieldType.dataType)) {
                Tcl_AppendPrintfToObj(
                    objP, "[%d]", fieldP->fieldType.dataType.arraySize);
            }
            break;

        default:
            /* TODO - include enum name for integers */
            Tcl_AppendPrintfToObj(objP,
                                  "\n  %s %s",
                                  cffiBaseTypes[baseType].token,
                                  Tcl_GetString(fieldP->nameObj));
            CffiStructDescribeArray(fieldP, objP);
            break;
        }
        Tcl_AppendPrintfToObj(
            objP, " offset=%d size=%d", fieldP->offset, fieldP->size);
    }

    Tcl_SetObjResult(ip, objP);

    return TCL_OK;
}

/* Function: CffiStructSizeCmd
 * Returns the size of the struct as the interp result.
 *
 * Parameters:
 * ip - Interpreter
 * structP - Pointer to a <CffiStruct>
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on failure.
 */
static CffiResult
CffiStructSizeCmd(Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[],
                  CffiStructCmdCtx *structCtxP)
{
    CffiStruct *structP = structCtxP->structP;
    int structSize;
    int structFixedSize;
    enum Opts { VLACOUNT };
    static const char *const opts[] = {"-vlacount", NULL};
    int vlaCount = -1;

    if (CffiStructIsVariableSize(structP)) {
        if (objc < 4) {
            return CffiErrorMissingVLACountOption(ip);
        }
        int optIndex;
        int i;
        for (i = 2; i < objc; ++i) {
            Tcl_WideInt wide;
            CHECK(Tcl_GetIndexFromObj(ip, objv[i], opts, "option", 0, &optIndex));
            if (i == objc-1)
                return Tclh_ErrorOptionValueMissing(ip, objv[i], NULL);
            ++i;
            switch (optIndex) {
            case VLACOUNT:
                CHECK(Tclh_ObjToRangedInt(ip, objv[i], 0, INT_MAX, &wide));
                vlaCount = (int)wide;
                break;
            }
        }

        CHECK(CffiStructSizeForVLACount(structCtxP->ipCtxP,
                                     structP,
                                     vlaCount,
                                     &structSize,
                                     &structFixedSize));
    }
    else /* Fixed size struct */
        structSize = structP->size;

    Tcl_SetObjResult(ip, Tcl_NewWideIntObj(structSize));
    return TCL_OK;
}


/* Function: CffiStructInfoCmd
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
static CffiResult
CffiStructInfoCmd(Tcl_Interp *ip,
                  int objc,
                  Tcl_Obj *const objv[],
                  CffiStructCmdCtx *structCtxP)
{
    int i;
    CffiStruct *structP = structCtxP->structP;
    Tcl_Obj *objs[8];
    int structSize;
    int structFixedSize;
    enum Opts { VLACOUNT };
    static const char *const opts[] = {"-vlacount", NULL};
    int vlaCount = -1;

    if (CffiStructIsVariableSize(structP)) {
        if (objc < 4) {
            return CffiErrorMissingVLACountOption(ip);
        }
        int optIndex;
        int i;
        for (i = 2; i < objc; ++i) {
            Tcl_WideInt wide;
            CHECK(Tcl_GetIndexFromObj(ip, objv[i], opts, "option", 0, &optIndex));
            if (i == objc-1)
                return Tclh_ErrorOptionValueMissing(ip, objv[i], NULL);
            ++i;
            switch (optIndex) {
            case VLACOUNT:
                CHECK(Tclh_ObjToRangedInt(ip, objv[i], 0, INT_MAX, &wide));
                vlaCount = (int)wide;
                break;
            }
        }

        CHECK(CffiStructSizeForVLACount(structCtxP->ipCtxP,
                                     structP,
                                     vlaCount,
                                     &structSize,
                                     &structFixedSize));
    }
    else /* Fixed size struct */
        structSize = structP->size;

    objs[0] = Tcl_NewStringObj("Size", 4);
    objs[1] = Tcl_NewLongObj(structSize);
    objs[2] = Tcl_NewStringObj("Alignment", 9);
    objs[3] = Tcl_NewLongObj(structP->alignment);
    objs[4] = Tcl_NewStringObj("Flags", 5);
    objs[5] = Tcl_NewLongObj(structP->flags);
    objs[6] = Tcl_NewStringObj("Fields", 6);
    objs[7] = Tcl_NewListObj(structP->nFields, NULL);

    for (i = 0; i < structP->nFields; ++i) {
        CffiField *fieldP = &structP->fields[i];
        Tcl_Obj *attrObjs[6];
        int fldSize = 0;
        CffiTypeLayoutInfo(structCtxP->ipCtxP,
                           &fieldP->fieldType.dataType,
                           vlaCount,
                           NULL,
                           &fldSize,
                           NULL);
        attrObjs[0] = Tcl_NewStringObj("Size", 4);
        attrObjs[1] = Tcl_NewLongObj(fldSize);
        attrObjs[2] = Tcl_NewStringObj("Offset", 6);
        attrObjs[3] = Tcl_NewLongObj(fieldP->offset);
        attrObjs[4] = Tcl_NewStringObj("Definition", 10);
        attrObjs[5] = CffiTypeAndAttrsUnparse(&fieldP->fieldType);
        Tcl_ListObjAppendElement(NULL, objs[7], fieldP->nameObj);
        Tcl_ListObjAppendElement(NULL, objs[7], Tcl_NewListObj(6, attrObjs));
    }

    Tcl_SetObjResult(ip, Tcl_NewListObj(8, objs));

    return TCL_OK;
}

/* Function: CffiStructFromObj
 * Constructs a C struct or union from a *Tcl_Obj* wrapper.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * structP - pointer to the struct definition internal form
 * structValueObj - the *Tcl_Obj* containing the script level struct value.
 *           This is a dictionary for structs and byte array for unions.
 * flags - if CFFI_F_PRESERVE_ON_ERROR is set, the target location will
 *   be preserved in case of errors.
 * structResultP - the location where the struct is to be constructed. Caller
 *           has to ensure size is large enough (taking into account
 *           variable sized structs)
 * memlifoP - the memory allocator for fields that are typed as *string*
 *            or *unistring*. If *NULL*, fields of these types will
 *            result in an error being raised. Caller is responsible
 *            for ensuring the memory allocated from *memlifoP* stays
 *            allocated for the lifetime of the constructed struct.
 * Returns:
 * *TCL_OK* on success with a pointer to the structure stored in *structPP* or
 * *TCL_ERROR* on error with message stored in the interpreter.
 */
CffiResult
CffiStructFromObj(CffiInterpCtx *ipCtxP,
                  const CffiStruct *structP,
                  Tcl_Obj *structValueObj,
                  CffiFlags flags,
                  void *structResultP,
                  Tclh_Lifo *memlifoP)
{
    Tcl_Interp *ip = ipCtxP->interp;
    int i;
    Tcl_DString ds;
    CffiResult ret;
    void *structAddress;
    int structSize;

    CHECK(CffiStructSizeForObj(
        ipCtxP, structP, structValueObj, &structSize, NULL));

    /*
     * The code later below handles unions as well as a dictionary with
     * one element. However, for symmetry with CffiObjFromStruct, we
     * currently require unions to be passed as binaries.
     * TODO - remove the dictionary based code from the main code below
     * if we stick with treating unions as binaries.
     */
    if (CffiStructIsUnion(structP)) {
        Tcl_Size len;
        const char *p;
        if ((p = Tclh_ObjGetBytesByRef(ip, structValueObj, &len)) == NULL)
            return TCL_ERROR;
        if (len != structSize) {
            return Tclh_ErrorInvalidValue(
                ip, NULL, "Binary value does not match size of union.");
        }
        memmove(structResultP, p, len);
        return TCL_OK;
    }

    /*
     * If we have to preserve, make a copy. Note we cannot just rely on
     * flags passed to NativeValueFromObj because we are clearing the
     * target first if -clear option was enabled
     */
    if (flags & CFFI_F_PRESERVE_ON_ERROR) {
        Tcl_DStringInit(&ds);
        Tcl_DStringSetLength(&ds, structSize);
        structAddress = Tcl_DStringValue(&ds);
    }
    else
        structAddress = structResultP;

    if (structP->flags & CFFI_F_STRUCT_CLEAR)
        memset(structAddress, 0, structSize);

    ret = TCL_OK;
    for (i = 0; i < structP->nFields; ++i) {
        Tcl_Obj *valueObj;
        const CffiField *fieldP = &structP->fields[i];
        void *fieldAddress      = fieldP->offset + (char *)structAddress;
        const CffiTypeAndAttrs *typeAttrsP = &fieldP->fieldType;

        if ((ret =
                 Tcl_DictObjGet(ip, structValueObj, fieldP->nameObj, &valueObj))
            != TCL_OK)
            break; /* Invalid dictionary. Note TCL_OK does not mean found */
        if (CffiStructIsUnion(structP)) {
            continue; /* Move on to checking for next field */
        }
        else {
            if (valueObj == NULL) {
                /* Dictionary valid but field not found. See if size or
                 * defaulted */
                if (fieldP->fieldType.flags & CFFI_F_ATTR_STRUCTSIZE) {
                    CFFI_ASSERT(structP->structSizeFieldIndex == i);
                    CffiStructInitSizeField(structP, structAddress);
                    continue;
                }

                /* Default. May be NULL */
                valueObj = fieldP->fieldType.parseModeSpecificObj;
            }
            if (valueObj == NULL) {
                /*
                 * If still NULL, error unless the CLEAR bit is set which
                 * indicates zeroing suffices
                 */
                if (structP->flags & CFFI_F_STRUCT_CLEAR)
                    continue; /* Move on to next field leaving this cleared */
                ret = Tclh_ErrorNotFound(
                    ip,
                    "Struct field",
                    fieldP->nameObj,
                    "Field missing in struct dictionary value.");
                break;
            }
        }
        CFFI_ASSERT(valueObj);

        int realArraySize = 0;
        /* The last field may be a variable sized array */
        if (i == (structP->nFields - 1)
            && CffiTypeIsVLA(&fieldP->fieldType.dataType)) {
            realArraySize =
                CffiStructGetDynamicCountFromObj(ipCtxP, structP, structValueObj);
            if (realArraySize < 0) {
                ret = TCL_ERROR;
                break;
            }
        }
        /* Turn off PRESERVE_ON_ERROR as we are already taken care to preserve */
        ret = CffiNativeValueFromObj(ipCtxP,
                                     typeAttrsP,
                                     realArraySize,
                                     valueObj,
                                     flags & ~CFFI_F_PRESERVE_ON_ERROR,
                                     fieldAddress,
                                     0,
                                     memlifoP);
        if (ret != TCL_OK || CffiStructIsUnion(structP))
            break;
    }

    if (flags & CFFI_F_PRESERVE_ON_ERROR) {
        /* If preserving, result was built in temp storage, copy it */
        if (ret == TCL_OK)
            memcpy(structResultP, structAddress, structSize);
        Tcl_DStringFree(&ds);
    }

    if (ret != TCL_OK) {
        CFFI_ASSERT(i < structP->nFields);
        if (i < structP->nFields)
            Tcl_AppendResult(ip,
                             " Error converting field ",
                             Tcl_GetString(structP->name),
                             ".",
                             Tcl_GetString(structP->fields[i].nameObj),
                             " to a native value.",
                             NULL);
    } else if (CffiStructIsUnion(structP)) {
        Tcl_Size numDictElems;
        if (i == structP->nFields) {
            ret = Tclh_ErrorNotFound(
                ip,
                "Union field",
                NULL,
                "No union fields found in dictionary value.");
        } else if (Tcl_DictObjSize(NULL, structValueObj, &numDictElems) == TCL_OK && numDictElems != 1) {
            ret = Tclh_ErrorGeneric(
                ip,
                NULL,
                "Union value dictionary must have exactly one field.");
        }
    }

    return ret;
}

/* Function: CffiStructToObj
 * Wraps a C structure into a Tcl_Obj.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * structP - pointer to the struct definition internal descriptor
 * valueP - pointer to C structure to wrap
 * valueObjP - location to store the pointer to the returned Tcl_Obj.
 *    Following standard practice, the reference ocunt on the Tcl_Obj is 0.
 *
 * For structs valueObjP will hold a dictionary. For unions, it is a binary.
 *
 * Returns:
 * *TCL_OK* on success with the wrapper Tcl_Obj pointer stored in valueObjP.
 * *TCL_ERROR* on error with message stored in the interpreter.
 */
CffiResult
CffiStructToObj(CffiInterpCtx *ipCtxP,
                const CffiStruct *structP,
                void *valueP,
                Tcl_Obj **valueObjP)
{
    int i;
    Tcl_Obj *valueObj;
    int ret;
    Tcl_Interp *ip    = ipCtxP->interp;

    /* Unions are always treated as binary as we do not know the field type */
    if (CffiStructIsUnion(structP)) {
        int unionSize;
        if (CffiStructSizeForNative(ipCtxP, structP, valueP, &unionSize, NULL)
            != TCL_OK)
            return TCL_ERROR;
        *valueObjP = Tcl_NewByteArrayObj(valueP, unionSize);
        return TCL_OK;
    }

    valueObj = Tcl_NewListObj(structP->nFields, NULL);
    for (i = 0; i < structP->nFields; ++i) {
        const CffiField *fieldP = &structP->fields[i];
        Tcl_Obj *fieldObj;
        int count;
        if (CffiTypeIsVLA(&fieldP->fieldType.dataType)) {
            count = CffiStructGetDynamicCountNative(ipCtxP, structP, valueP);
            if (count < 0)
                return TCL_ERROR;
        } else {
            count = fieldP->fieldType.dataType.arraySize;
        }
        /* fields other than last cannot be dynamic size */
        CFFI_ASSERT((i == structP->nFields - 1) || ! CffiTypeIsVLA(&fieldP->fieldType.dataType));
        ret = CffiNativeValueToObj(ipCtxP,
                                   &fieldP->fieldType,
                                   fieldP->offset + (char *)valueP,
                                   0,
                                   count,
                                   &fieldObj);
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

/* Function: CffiErrorStructCountField
 * Stores an error message in the interpreter stating the count field
 * in a variable size struct cannot be modified.
 *
 * Parameters:
 * ip - interpreter. May be NULL
 * fldNameObj - name of the field
 *
 * Returns:
 * Always returns TCL_ERROR.
 */
CffiResult
CffiErrorStructCountField(Tcl_Interp *ip, Tcl_Obj *fldNameObj)
{
    return Tclh_ErrorInvalidValue(
        ip, fldNameObj, "The count field in a variable size struct must not be modified.");
}

/* Function: CffiErrorMissingVLACountOption
 * Stores an error message in the interpreter stating the count field
 * in a variable size struct cannot be modified.
 *
 * Parameters:
 * ip - interpreter. May be NULL
 *
 * Returns:
 * Always returns TCL_ERROR.
 */
CffiResult
CffiErrorMissingVLACountOption(Tcl_Interp *ip)
{
    return Tclh_ErrorOptionMissingStr(
        ip, "-vlacount", "This option is required for variable size structs.");
}


/* Function: CffiErrorStructIsVariableSize
 * Stores an error message in the interpreter stating the operation is not
 * valid for a variable sized struct.
 *
 * Parameters:
 * ip - interpreter. May be NULL>
 * structP - struct descriptor
 * oper - operation that was attempted
 *
 * Returns:
 * Always returns TCL_ERROR.
 */
CffiResult
CffiErrorStructIsVariableSize(Tcl_Interp *ip, CffiStruct *structP, const char *oper)
{
    return Tclh_ErrorOperFailed(
        ip, oper, structP->name, "Operation not permitted on variable sized structs.");
}

/* Function: CffiStructAllocateCmd
 * Allocates memory for one or more structures.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[].
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level. See the command manpages for details.
 * scructCtxP - pointer to struct context
 *
 * Returns:
 * *TCL_OK* on success with the wrapped pointer as the interpreter result.
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructAllocateCmd(Tcl_Interp *ip,
                      int objc,
                      Tcl_Obj *const objv[],
                      CffiStructCmdCtx *structCtxP)
{
    CffiStruct *structP = structCtxP->structP;
    int count;
    int vlaCount = -1;
    static const char *const opts[] = {"-count", "-vlacount", NULL};
    enum Opts { COUNT, VLACOUNT };
    int optIndex;
    Tcl_WideInt wide;

    CFFI_ASSERT(objc >= 2 && objc <= 6);

    if (objc == 2)
        count = 1;
    else if (objc == 3) {
        /* Old style - S allocate COUNT */
        CHECK(Tclh_ObjToRangedInt(ip, objv[2], 1, INT_MAX, &wide));
        count = (int)wide;
    } else {
        /* New style - S allocate ?-count N? ?-vlacount VLACOUNT? */
        int i;
        count = 1; /* Default */
        for (i = 2; i < objc; ++i) {
            CHECK( Tcl_GetIndexFromObj(ip, objv[i], opts, "option", 0, &optIndex));
            if (i == objc-1)
                return Tclh_ErrorOptionValueMissing(ip, objv[i], NULL);
            ++i;
            switch (optIndex) {
            case COUNT:
                CHECK(Tclh_ObjToRangedInt(ip, objv[i], 1, INT_MAX, &wide));
                count = (int)wide;
                break;
            case VLACOUNT:
                CHECK(Tclh_ObjToRangedInt(ip, objv[i], 0, INT_MAX, &wide));
                vlaCount = (int)wide;
                break;
            }
        }
    }

    if (CffiStructIsVariableSize(structP)) {
        CFFI_ASSERT(!CffiStructIsUnion(structP));
        if (count != 1) {
            return Tclh_ErrorInvalidValue(
                ip, NULL, "Allocation count must 1 for variable sized structs.");
        }
        if (vlaCount < 0) {
            return Tclh_ErrorOptionMissingStr(
                ip,
                "-vlacount",
                "Variable size structs must specify the size of the contained "
                "variable length array.");
        }
    }

    Tcl_Obj *resultObj;
    void *resultP;
    int structSize;

    CHECK(CffiStructSizeForVLACount(
        structCtxP->ipCtxP, structP, vlaCount, &structSize, NULL));
    if (count >= TCL_SIZE_MAX/structSize) {
        return Tclh_ErrorAllocation(ip, "Struct", "Array size too large.");
    }
    resultP = ckalloc(count * structSize);

    if (Tclh_PointerRegister(ip,
                             structCtxP->ipCtxP->tclhCtxP,
                             resultP,
                             structP->name,
                             &resultObj)
        != TCL_OK) {
        ckfree(resultP);
        return TCL_ERROR;
    }
    Tcl_SetObjResult(ip, resultObj);
    return TCL_OK;
}

/* Function: CffiStructNewCmd
 * Allocates memory for a struct and initializes it
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[].
 * objv - argument array.
 * scructCtxP - pointer to struct context
 *
 * obj[2] - initializer for the struct
 *
 * Returns:
 * *TCL_OK* on success with the wrapped pointer as the interpreter result.
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructNewCmd(Tcl_Interp *ip,
                 int objc,
                 Tcl_Obj *const objv[],
                 CffiStructCmdCtx *structCtxP)
{
    CffiStruct *structP = structCtxP->structP;
    CffiInterpCtx *ipCtxP = structCtxP->ipCtxP;
    Tcl_Obj *resultObj;
    void *resultP;
    int ret;
    int structSize;

    CFFI_ASSERT(objc == 2 || objc == 3);

    /* Note - this will fail for variable size structs when objc==2. TODO */
    CHECK(CffiStructSizeForObj(
        ipCtxP, structP, objc == 2 ? NULL : objv[2], &structSize, NULL));

    resultP = ckalloc(structSize);
    if (objc == 3)
        ret = CffiStructFromObj(
            structCtxP->ipCtxP, structP, objv[2], 0, resultP, NULL);
    else
        ret = CffiStructObjDefault(structCtxP->ipCtxP, structP, resultP);

    if (ret == TCL_OK) {
        ret = Tclh_PointerRegister(ip,
                                   structCtxP->ipCtxP->tclhCtxP,
                                   resultP,
                                   structP->name,
                                   &resultObj);
        if (ret == TCL_OK) {
            Tcl_SetObjResult(ip, resultObj);
            return TCL_OK;
        }
    }
    ckfree(resultP);
    return TCL_ERROR;
}

/* Function: CffiStructObjDefault
 * Constructs a native struct with default values.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * structP - struct descriptor
 * valueP - location to store constructed native struct.
 *
 * Memory may be allocated from ipCtxP->memlifo. Caller's responsibility
 * to clean up.
 *
 * Returns:
 * Tcl result code
 */
CffiResult
CffiStructObjDefault(CffiInterpCtx *ipCtxP,
                     CffiStruct *structP,
                     void *valueP)
{
    CffiResult ret;
    /*
     * Have to construct a struct from defaults. Fake out a
     * native struct that will be converted back below. Note
     * Directly converting from a struct definition field
     * defaults is non-trivial to take care of enums etc.
     */
    Tcl_Obj *defaultStructValue = Tcl_NewObj();
    Tcl_IncrRefCount(defaultStructValue);
    ret = CffiStructFromObj(ipCtxP,
                            structP,
                            defaultStructValue,
                            0,
                            valueP,
                            &ipCtxP->memlifo);
    Tcl_DecrRefCount(defaultStructValue);

    if (ret != TCL_OK)
        Tcl_SetObjResult(ipCtxP->interp,
                         Tcl_ObjPrintf("Cannot construct a default value for "
                                       "struct %s.",
                                       Tcl_GetString(structP->name)));

    return ret;
}

/* Function: CffiStructFromNativePointer
 * Returns a Tcl dictionary value from a C struct in memory.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 3-4 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * structCtxP - safe pointer to struct context
 * safe - if non-0, structCtxP must be a registered pointer
 *
 * The **objv** contains the following arguments:
 * objv[2] - pointer to memory
 * objv[3] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with the dictionary value as interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructFromNativePointer(Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[],
                        CffiStructCmdCtx *structCtxP,
                        int safe
                        )
{
    CffiStruct *structP = structCtxP->structP;
    CffiInterpCtx *ipCtxP = structCtxP->ipCtxP;
    void *structAddr;
    Tcl_Obj *resultObj;

    CHECK(CffiStructComputeAddress(ipCtxP,
                                   structP,
                                   objv[2],
                                   safe,
                                   objc > 3 ? objv[3] : NULL,
                                   &structAddr));

    CHECK(CffiStructToObj(ipCtxP, structP, structAddr, &resultObj));

    Tcl_SetObjResult(ip, resultObj);
    return TCL_OK;
}

/* Function: CffiStructFromNativeUnsafeCmd
 * Returns a Tcl dictionary value from a C struct referencecd by an unsafe
 * pointer.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 3-4 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * scructCtxP - unsafe pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - pointer to memory
 * objv[3] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with the dictionary value as interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructFromNativeUnsafeCmd(Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[],
                        CffiStructCmdCtx *structCtxP)
{
    return CffiStructFromNativePointer(ip, objc, objv, structCtxP, 0);
}

/* Function: CffiStructFromNativeCmd
 * Returns a Tcl dictionary value from a C struct in memory.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 3-4 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * scructCtxP - safe pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - pointer to memory
 * objv[3] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with the dictionary value as interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructFromNativeCmd(Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[],
                        CffiStructCmdCtx *structCtxP)
{
    return CffiStructFromNativePointer(ip, objc, objv, structCtxP, 1);
}


/* Function: CffiStructToNativePointer
 * Initializes memory as a C struct from a Tcl dictionary representation.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 4-5 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * structCtxP - pointer to struct context
 * safe - if non-0, objv[2] must be a registered pointer
 *
 * The **objv** contains the following arguments:
 * objv[2] - pointer to memory
 * objv[3] - dictionary value to use as initializer
 * objv[4] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructToNativePointer(Tcl_Interp *ip,
                          int objc,
                          Tcl_Obj *const objv[],
                          CffiStructCmdCtx *structCtxP,
                          int safe)
{
    void *structAddr;
    CffiStruct *structP = structCtxP->structP;
    CffiInterpCtx *ipCtxP = structCtxP->ipCtxP;

    if (CffiStructIsVariableSize(structP)) {
        return CffiErrorStructIsVariableSize(ip, structP, "tonative");
    }

    CHECK(CffiStructComputeAddress(ipCtxP,
                                   structP,
                                   objv[2],
                                   safe,
                                   objc > 4 ? objv[4] : NULL,
                                   &structAddr));

    CHECK(CffiStructFromObj(ipCtxP,
                            structP,
                            objv[3],
                            CFFI_F_PRESERVE_ON_ERROR,
                            structAddr,
                            NULL));

    return TCL_OK;
}

/* Function: CffiStructToNativeCmd
 * Initializes memory as a C struct from a Tcl dictionary representation.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 4-5 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * scructCtxP - pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - safe pointer to memory
 * objv[3] - dictionary value to use as initializer
 * objv[4] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructToNativeCmd(Tcl_Interp *ip,
                      int objc,
                      Tcl_Obj *const objv[],
                      CffiStructCmdCtx *structCtxP)
{
    return CffiStructToNativePointer(ip, objc, objv, structCtxP, 1);
}

/* Function: CffiStructToNativeCmd
 * Initializes memory as a C struct from a Tcl dictionary representation.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 4-5 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * scructCtxP - pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - unsafe pointer to memory
 * objv[3] - dictionary value to use as initializer
 * objv[4] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructToNativeUnsafeCmd(Tcl_Interp *ip,
                            int objc,
                            Tcl_Obj *const objv[],
                            CffiStructCmdCtx *structCtxP)
{
    return CffiStructToNativePointer(ip, objc, objv, structCtxP, 0);
}

/* Function: CffiStructGetNativePointer
 * Gets the value of a field in a native struct.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 4-5 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * structCtxP - pointer to struct context
 * safe - if non-0, objv[2] must be a registered pointer
 *
 * The **objv** contains the following arguments:
 * objv[2] - pointer to memory holding the struct value
 * objv[3] - field name
 * objv[4] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructGetNativePointer(Tcl_Interp *ip,
                           int objc,
                           Tcl_Obj *const objv[],
                           CffiStructCmdCtx *structCtxP,
                           int safe)
{
    /* S fieldpointer POINTER FIELDNAME ?INDEX? */
    CFFI_ASSERT(objc >= 4);

    CffiStruct *structP = structCtxP->structP;
    CffiInterpCtx *ipCtxP = structCtxP->ipCtxP;
    Tcl_Obj *valueObj;
    void *structAddr;
    void *fldAddr;
    int fldArraySize;
    int fldIndex;

    CHECK(CffiStructComputeAddress(ipCtxP,
                                   structP,
                                   objv[2],
                                   safe,
                                   objc > 4 ? objv[4] : NULL,
                                   &structAddr));
    CHECK(CffiStructComputeFieldAddress(ipCtxP,
                                        structP,
                                        structAddr,
                                        objv[3],
                                        &fldIndex,
                                        &fldAddr,
                                        &fldArraySize));
    CHECK(CffiNativeValueToObj(ipCtxP,
                               &structP->fields[fldIndex].fieldType,
                               fldAddr,
                               0,
                               fldArraySize,
                               &valueObj));
    Tcl_SetObjResult(ip, valueObj);
    return TCL_OK;
}

/* Function: CffiStructGetNativeCmd
 * Gets the value of a field in a native struct.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 4-5 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * structCtxP - pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - safe pointer to memory holding the struct value
 * objv[3] - field name
 * objv[4] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructGetNativeCmd(Tcl_Interp *ip,
                       int objc,
                       Tcl_Obj *const objv[],
                       CffiStructCmdCtx *structCtxP)
{
    return CffiStructGetNativePointer(ip, objc, objv, structCtxP, 1);
}

/* Function: CffiStructGetNativeUnsafeCmd
 * Gets the value of a field in a native struct.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 4-5 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * structCtxP - pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - unsafe pointer to memory holding the struct value
 * objv[3] - field name
 * objv[4] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructGetNativeUnsafeCmd(Tcl_Interp *ip,
                             int objc,
                             Tcl_Obj *const objv[],
                             CffiStructCmdCtx *structCtxP)
{
    return CffiStructGetNativePointer(ip, objc, objv, structCtxP, 0);
}

/* Function: CffiStructSetNativePointer
 * Sets the value of a field in a native struct.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 5-6 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * structCtxP - pointer to struct context
 * safe - if non-0, structCtxP must be a registered pointer
 *
 * The **objv** contains the following arguments:
 * objv[2] - pointer to memory holding the struct value
 * objv[3] - field name
 * objv[4] - value to store
 * objv[5] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructSetNativePointer(Tcl_Interp *ip,
                           int objc,
                           Tcl_Obj *const objv[],
                           CffiStructCmdCtx *structCtxP,
                           int safe)
{
    /* S fieldpointer POINTER FIELDNAME VALUE ?INDEX? */
    CFFI_ASSERT(objc >= 5);
    CffiStruct *structP = structCtxP->structP;
    CffiInterpCtx *ipCtxP = structCtxP->ipCtxP;

    void *structAddr;
    void *fldAddr;
    int fldArraySize;
    int fldIndex;
    CHECK(CffiStructComputeAddress(ipCtxP,
                                   structP,
                                   objv[2],
                                   safe,
                                   objc > 5 ? objv[5] : NULL,
                                   &structAddr));
    CHECK(CffiStructComputeFieldAddress(ipCtxP,
                                        structP,
                                        structAddr,
                                        objv[3],
                                        &fldIndex,
                                        &fldAddr,
                                        &fldArraySize));
    /* A field that holds a VLA count must not be modified. */
    if (structP->dynamicCountFieldIndex == fldIndex) {
        return CffiErrorStructCountField(ip, objv[3]);
    }
    if (CffiTypeIsVariableSize(&structP->fields[fldIndex].fieldType.dataType)) {
        return CffiErrorStructIsVariableSize(ip, structP, "setnative");
    }
    CHECK(CffiNativeValueFromObj(ipCtxP,
                                 &structP->fields[fldIndex].fieldType,
                                 fldArraySize,
                                 objv[4],
                                 CFFI_F_PRESERVE_ON_ERROR,
                                 fldAddr,
                                 0,
                                 NULL));
    return TCL_OK;
}

/* Function: CffiStructSetNativeCmd
 * Sets the value of a field in a native struct.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 5-6 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * structCtxP - pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - safe pointer to memory holding the struct value
 * objv[3] - field name
 * objv[4] - value to store
 * objv[5] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructSetNativeCmd(Tcl_Interp *ip,
                       int objc,
                       Tcl_Obj *const objv[],
                       CffiStructCmdCtx *structCtxP)
{
    return CffiStructSetNativePointer(ip, objc, objv, structCtxP, 1);
}

/* Function: CffiStructSetNativeUnsafeCmd
 * Sets the value of a field in a native struct.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 5-6 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * structCtxP - safe pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - unsafe pointer to memory holding the struct value
 * objv[3] - field name
 * objv[4] - value to store
 * objv[5] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructSetNativeUnsafeCmd(Tcl_Interp *ip,
                             int objc,
                             Tcl_Obj *const objv[],
                             CffiStructCmdCtx *structCtxP)
{
    return CffiStructSetNativePointer(ip, objc, objv, structCtxP, 0);
}

/* Function: CffiStructGetNativeFieldsPointer
 * Gets the values of multiple fields from a native struct.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 4-5 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * structCtxP - pointer to struct context
 * safe - if non-0, objv[2] must be a registered pointer
 *
 * The **objv** contains the following arguments:
 * objv[2] - pointer to memory holding the struct value
 * objv[3] - list of field names
 * objv[4] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructGetNativeFieldsPointer(Tcl_Interp *ip,
                                 int objc,
                                 Tcl_Obj *const objv[],
                                 CffiStructCmdCtx *structCtxP,
                                 int safe)
{
    CffiStruct *structP = structCtxP->structP;
    CffiInterpCtx *ipCtxP = structCtxP->ipCtxP;
    void *structAddr;
    Tcl_Size nNames;
    int ret;

    /* S fieldpointer POINTER FIELDNAMES ?INDEX? */
    CFFI_ASSERT(objc >= 4);

    CHECK(CffiStructComputeAddress(structCtxP->ipCtxP,
                                   structP,
                                   objv[2],
                                   safe,
                                   objc > 4 ? objv[4] : NULL,
                                   &structAddr));

    /*
     * Just a little note here about not using Tcl_ListObjGetElements
     * here - the fear is that it may shimmer in one of the calls.
     * To guard against that we would need to call Tcl_DuplicateObj.
     * That is slower than the below approach since number of fields
     * is likely quite small.
     */
    ret = Tcl_ListObjLength(ip, objv[3], &nNames);
    if (ret == TCL_OK) {
        Tcl_Obj *valueObj;
        Tcl_Obj *valuesObj;
        int i;
        valuesObj = Tcl_NewListObj(nNames, NULL);
        for (i = 0; ret == TCL_OK && i < nNames; ++i) {
            Tcl_Obj *nameObj;
            int fldIndex;
            void *fldAddr;
            int fldArraySize;
            Tcl_ListObjIndex(NULL, objv[3], i, &nameObj);
            Tcl_IncrRefCount(nameObj);
            ret = CffiStructComputeFieldAddress(ipCtxP,
                                                structP,
                                                structAddr,
                                                nameObj,
                                                &fldIndex,
                                                &fldAddr,
                                                &fldArraySize);
            Tcl_DecrRefCount(nameObj);
            if (ret == TCL_OK) {
                ret = CffiNativeValueToObj(ipCtxP,
                                           &structP->fields[fldIndex].fieldType,
                                           fldAddr,
                                           0,
                                           fldArraySize,
                                           &valueObj);
                if (ret == TCL_OK) {
                    Tcl_ListObjAppendElement(NULL, valuesObj, valueObj);
                }
            }
        }
        if (ret == TCL_OK)
            Tcl_SetObjResult(ip, valuesObj);
        else
            Tcl_DecrRefCount(valuesObj);
    }
    return ret;
}

/* Function: CffiStructGetNativeFieldsCmd
 * Gets the values of multiple fields from a native struct.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 4-5 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * structCtxP - pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - safe pointer to memory holding the struct value
 * objv[3] - list of field names
 * objv[4] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructGetNativeFieldsCmd(Tcl_Interp *ip,
                             int objc,
                             Tcl_Obj *const objv[],
                             CffiStructCmdCtx *structCtxP)
{
    return CffiStructGetNativeFieldsPointer(ip, objc, objv, structCtxP, 1);
}

/* Function: CffiStructGetNativeFieldsUnsafeCmd
 * Gets the values of multiple fields from a native struct.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 4-5 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * structCtxP - pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - unsafe pointer to memory holding the struct value
 * objv[3] - list of field names
 * objv[4] - optional, index into array of structs pointed to by objv[2]
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructGetNativeFieldsUnsafeCmd(Tcl_Interp *ip,
                                   int objc,
                                   Tcl_Obj *const objv[],
                                   CffiStructCmdCtx *structCtxP)
{
    return CffiStructGetNativeFieldsPointer(ip, objc, objv, structCtxP, 0);
}

/* Function: CffiStructFieldPointerCmd
 * Returns a pointer to a field in a native struct.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        total of 4-5 arguments.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * structCtxP - pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - pointer to memory
 * objv[3] - field name
 * objv[4] - tag for returned pointer, optional
 * objv[5] - index into array of structs pointed to by objv[2], optional
 *
 * Returns:
 * *TCL_OK* on success with an empty interp result;
 * *TCL_ERROR* on failure with an error message in the interpreter.
 */
static CffiResult
CffiStructFieldPointerCmd(Tcl_Interp *ip,
                          int objc,
                          Tcl_Obj *const objv[],
                          CffiStructCmdCtx *structCtxP)
{
    /* S fieldpointer POINTER FIELDNAME ?TAG? ?INDEX? */
    CFFI_ASSERT(objc >= 4);

    CffiStruct *structP = structCtxP->structP;
    CffiInterpCtx *ipCtxP = structCtxP->ipCtxP;
    void *structAddr;
    void *fldAddr;
    int fldArraySize;
    int fldIndex;
    CHECK(CffiStructComputeAddress(ipCtxP,
                                   structP,
                                   objv[2],
                                   1,
                                   objc > 5 ? objv[5] : NULL,
                                   &structAddr));
    CHECK(CffiStructComputeFieldAddress(ipCtxP,
                                        structP,
                                        structAddr,
                                        objv[3],
                                        &fldIndex,
                                        &fldAddr,
                                        &fldArraySize));
    Tcl_SetObjResult(ip,
                     Tclh_PointerWrap(fldAddr, objc > 4 ? objv[4] : NULL));

    return TCL_OK;
}

/* Function: CffiStructFreeCmd
 * Releases the memory allocated for a struct instance.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[]. Caller should have checked for
 *        a single argument.
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level.
 * scructCtxP - pointer to struct context
 *
 * The function verifies validity of the passed pointer and unregisters it
 * before freeing it.
 *
 * Returns:
 * *TCL_OK* on success, *TCL_ERROR* on failure with message in interpreter.
 */
static CffiResult
CffiStructFreeCmd(Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[],
                   CffiStructCmdCtx *structCtxP)
{
    void *valueP;
    CffiResult ret;

    ret = Tclh_PointerObjUnregister(ip,
                                    structCtxP->ipCtxP->tclhCtxP,
                                    objv[2],
                                    &valueP,
                                    structCtxP->structP->name);
    if (ret == TCL_OK && valueP)
        ckfree(valueP);
    return ret;
}

/* Function: CffiStructToBinaryCmd
 * Implements the *STRUCT tobinary* command converting a dictionary to a
 * native struct in a Tcl byte array object.
 *
 * Parameters:
 * ip - interpreter
 * objc - number of elements in *objv*
 * objv - argument array. Caller should have checked it has exactly 3
 *        3 elements with objv[2] holding the dictionary representation.
 * scructCtxP - pointer to struct context
 *
 * Returns:
 * *TCL_OK* on success with the *Tcl_Obj* byte array in the interpreter
 * result and *TCL_ERROR* on failure with the error message in the
 * interpreter result.
 */
static CffiResult
CffiStructToBinaryCmd(Tcl_Interp *ip,
                       int objc,
                       Tcl_Obj *const objv[],
                       CffiStructCmdCtx *structCtxP)
{
    void *valueP;
    CffiResult ret;
    Tcl_Obj *resultObj;
    CffiStruct *structP = structCtxP->structP;
    int structSize;

    CHECK(CffiStructSizeForObj(structCtxP->ipCtxP, structP, objv[2], &structSize, NULL));

    resultObj = Tcl_NewByteArrayObj(NULL, structSize);
    valueP    = Tclh_ObjGetBytesByRef(ip, resultObj, NULL);
    TCLH_ASSERT(valueP);
    ret       = CffiStructFromObj(
        structCtxP->ipCtxP, structP, objv[2], 0, valueP, NULL);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, resultObj);
    else
        Tcl_DecrRefCount(resultObj);
    return ret;
}

/* Function: CffiStructFromBinaryCmd
 * Implements the *STRUCT frombinary* command that converts a native
 * structure stored in a Tcl byte array to a dictionary.
 *
 * Parameters:
 * ip - interpreter
 * objc - number of elements in *objv*. Must be 3.
 * objv - argument array. Caller should have checked it has 3-4 elements
 *        with objv[2] holding the byte array representation and optional
 *        objv[3] holding the offset into it.
 * structCtxP - pointer to struct context
 *
 * Returns:
 * *TCL_OK* on success with the *Tcl_Obj* dictionary in the interpreter
 * result and *TCL_ERROR* on failure with the error message in the
 * interpreter result.
 */
static CffiResult
CffiStructFromBinaryCmd(Tcl_Interp *ip,
                         int objc,
                         Tcl_Obj *const objv[],
                         CffiStructCmdCtx *structCtxP)
{
    unsigned char *valueP;
    Tcl_Size len;
    unsigned int offset;
    CffiResult ret;
    Tcl_Obj *resultObj;
    CffiStruct *structP = structCtxP->structP;

    valueP = Tcl_GetByteArrayFromObj(objv[2], &len);
    if (objc == 4)
        CHECK(Tclh_ObjToUInt(ip, objv[3], &offset));
    else
        offset = 0;

    /* First ensure binary is minimal size. */
    if (len < structP->size || (len - structP->size) < (int) offset) {
        goto truncation;
    }
    /* Now ensure get the actual size in case it is varsize struct */
    int structSize;
    CHECK(CffiStructSizeForNative(
        structCtxP->ipCtxP, structP, valueP, &structSize, NULL));

    if (len < structSize)
        goto truncation;

    ret = CffiStructToObj(
        structCtxP->ipCtxP, structP, offset + valueP, &resultObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, resultObj);
    return ret;

truncation:
    /* Do not pass objv[2] to error function -> binary values not printable */
    return Tclh_ErrorInvalidValue(
        ip, NULL, "Truncated structure binary value.");
}

/* Function: CffiStructNameCmd
 * Implements the *STRUCT name* command to retrieve a struct name.
 *
 * Parameters:
 * ip - interpreter
 * objc - number of elements in *objv*
 * objv - argument array. Caller should have checked it has exactly 2 elements
 * scructCtxP - pointer to struct context
 *
 * Returns:
 * *TCL_OK* on success with the struct name as the interpreter
 * result and *TCL_ERROR* on failure with the error message in the
 * interpreter result.
 */
static CffiResult
CffiStructNameCmd(Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[],
                   CffiStructCmdCtx *structCtxP)
{
    Tcl_SetObjResult(ip, structCtxP->structP->name);
    return TCL_OK;
}

static CffiResult
CffiStructDestroyCmd(Tcl_Interp *ip,
                      int objc,
                      Tcl_Obj *const objv[],
                      CffiStructCmdCtx *structCtxP)
{
    /*
    * objv[0] is the command name for the struct instance. Deleteing
    * the command will also release associated resources including structP.
    */
    if (Tcl_DeleteCommand(ip, Tcl_GetString(objv[0])) == 0)
        return TCL_OK;
    else
        return Tclh_ErrorOperFailed(ip, "delete", objv[0], NULL);
}

/* Function: CffiStructInstanceCmd
 * Implements the script level command for struct instances.
 *
 * Parameters:
 * cdata - not used
 * ip - interpreter
 * objc - argument count
 * objv - argument array
 *
 * Returns:
 * TCL_OK or TCL_ERROR, with result in interpreter.
 */
static CffiResult
CffiStructInstanceCmd(ClientData cdata,
                        Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[])
{
    CffiStructCmdCtx *structCtxP = (CffiStructCmdCtx *)cdata;
    static const Tclh_SubCommand subCommands[] = {
        {"allocate", 0, 4, "?-count COUNT? ?-vlacount VLACOUNT?", CffiStructAllocateCmd},
        {"describe", 0, 0, "", CffiStructDescribeCmd},
        {"destroy", 0, 0, "", CffiStructDestroyCmd},
        {"fieldpointer", 2, 4, "POINTER FIELD ?TAG? ?INDEX?", CffiStructFieldPointerCmd},
        {"getnative", 2, 3, "POINTER FIELD ?INDEX?", CffiStructGetNativeCmd},
        {"getnative!", 2, 3, "POINTER FIELD ?INDEX?", CffiStructGetNativeUnsafeCmd},
        {"getnativefields", 2, 3, "POINTER FIELDNAMES ?INDEX?", CffiStructGetNativeFieldsCmd},
        {"getnativefields!", 2, 3, "POINTER FIELDNAMES ?INDEX?", CffiStructGetNativeFieldsUnsafeCmd},
        {"free", 1, 1, "POINTER", CffiStructFreeCmd},
        {"frombinary", 1, 1, "BINARY", CffiStructFromBinaryCmd},
        {"fromnative", 1, 2, "POINTER ?INDEX?", CffiStructFromNativeCmd},
        {"fromnative!", 1, 2, "POINTER ?INDEX?", CffiStructFromNativeUnsafeCmd},
        {"info", 0, 2, "?-vlacount VLACOUNT?", CffiStructInfoCmd},
        {"name", 0, 0, "", CffiStructNameCmd},
        {"new", 0, 1, "?INITIALIZER?", CffiStructNewCmd},
        {"setnative", 3, 4, "POINTER FIELD VALUE ?INDEX?", CffiStructSetNativeCmd},
        {"setnative!", 3, 4, "POINTER FIELD VALUE ?INDEX?", CffiStructSetNativeUnsafeCmd},
        {"size", 0, 2, "?-vlacount VLACOUNT?", CffiStructSizeCmd},
        {"tobinary", 1, 1, "DICTIONARY", CffiStructToBinaryCmd},
        {"tonative", 2, 3, "POINTER INITIALIZER ?INDEX?", CffiStructToNativeCmd},
        {"tonative!", 2, 3, "POINTER INITIALIZER ?INDEX?", CffiStructToNativeUnsafeCmd},
        {NULL}
    };
    int cmdIndex;

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ip, objc, objv, structCtxP);
}

static void
CffiStructOrUnionInstanceDeleter(ClientData cdata)
{
    CffiStructCmdCtx *ctxP = (CffiStructCmdCtx *)cdata;
    if (ctxP->structP)
        CffiStructUnref(ctxP->structP);
    /* Note ctxP->ipCtxP is interp-wide and not to be freed here */
    ckfree(ctxP);
}

/* Function: CffiUnionDecodeCmd
 * Returns a union field value from a binary
 *
 * Parameters:
 * ip - interpreter
 * objc - number of elements in *objv*. Must be 1.
 * objv - argument array. Caller should have checked it has four elements
 *        with objv[3] holding the byte array representation and
 *        objv[2] holding the field name.
 * structCtxP - pointer to struct context
 *
 * Returns:
 * TCL_OK on success, TCL_ERROR on failure
 */
static CffiResult
CffiUnionDecodeCmd(Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[],
                   CffiStructCmdCtx *structCtxP)
{
    unsigned char *valueP;
    Tcl_Size len;
    CffiResult ret;
    CffiStruct *structP = structCtxP->structP;

    CFFI_ASSERT(objc == 4);
    CFFI_ASSERT(CffiStructIsUnion(structP));

    valueP = Tcl_GetByteArrayFromObj(objv[3], &len);
    if (len < structP->size)
        return Tclh_ErrorInvalidValue(
            ip, objv[2], "Union binary value is truncated.");

    int fldIndex;
    void *fldAddr;
    int fldArraySize;
    ret = CffiStructComputeFieldAddress(structCtxP->ipCtxP,
                                        structP,
                                        valueP,
                                        objv[2], /* Field name */
                                        &fldIndex,
                                        &fldAddr,
                                        &fldArraySize);
    if (ret != TCL_OK)
        return ret;

    Tcl_Obj *resultObj;
    ret = CffiNativeValueToObj(structCtxP->ipCtxP,
                               &structP->fields[fldIndex].fieldType,
                               fldAddr,
                               0,
                               fldArraySize,
                               &resultObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, resultObj);
    return ret;
}

/* Function: CffiUnionEncodeCmd
 * Returns a binary for a union from a field value
 *
 * Parameters:
 * ip - interpreter
 * objc - number of elements in *objv*. Must be 1.
 * objv - argument array. Caller should have checked it has four elements
 *        with objv[2] holding the field name and objv[3] holding value
 * structCtxP - pointer to struct context
 *
 * Returns:
 * TCL_OK on success, TCL_ERROR on failure
 */
static CffiResult
CffiUnionEncodeCmd(Tcl_Interp *ip,
                int objc,
                Tcl_Obj *const objv[],
                CffiStructCmdCtx *structCtxP)
{
    unsigned char *valueP;
    Tcl_Size len;
    CffiResult ret;
    CffiStruct *structP = structCtxP->structP;

    CFFI_ASSERT(objc == 4);
    CFFI_ASSERT(CffiStructIsUnion(structP));

    Tcl_Obj *resultObj;
    resultObj = Tcl_NewByteArrayObj(NULL, structP->size);
    valueP = Tcl_GetByteArrayFromObj(resultObj, &len);

    int fldIndex;
    void *fldAddr;
    int fldArraySize;
    ret = CffiStructComputeFieldAddress(structCtxP->ipCtxP,
                                        structP,
                                        valueP,
                                        objv[2], /* Field name */
                                        &fldIndex,
                                        &fldAddr,
                                        &fldArraySize);
    if (ret != TCL_OK)
        goto error_exit;

    ret = CffiNativeValueFromObj(structCtxP->ipCtxP,
                                 &structP->fields[fldIndex].fieldType,
                                 fldArraySize,
                                 objv[3],
                                 0,
                                 fldAddr,
                                 0,
                                 NULL);

    if (ret != TCL_OK)
        goto error_exit;

    Tcl_SetObjResult(ip, resultObj);
    return TCL_OK;

error_exit:
    Tcl_DecrRefCount(resultObj);
    return TCL_ERROR;
}


/* Function: CffiUnionInstanceCmd
 * Implements the script level command for struct instances.
 *
 * Parameters:
 * cdata - not used
 * ip - interpreter
 * objc - argument count
 * objv - argument array
 *
 * Returns:
 * TCL_OK or TCL_ERROR, with result in interpreter.
 */
static CffiResult
CffiUnionInstanceCmd(ClientData cdata,
                     Tcl_Interp *ip,
                     int objc,
                     Tcl_Obj *const objv[])
{
    CffiStructCmdCtx *structCtxP = (CffiStructCmdCtx *)cdata;
    static const Tclh_SubCommand subCommands[] = {
        {"decode", 2, 2, "FIELD BINARY", CffiUnionDecodeCmd},
        {"describe", 0, 0, "", CffiStructDescribeCmd},
        {"destroy", 0, 0, "", CffiStructDestroyCmd},
        {"encode", 2, 2, "FIELD VALUE", CffiUnionEncodeCmd},
        {"info", 0, 0, "", CffiStructInfoCmd},
        {"name", 0, 0, "", CffiStructNameCmd},
        {"size", 0, 2, "?-vlacount VLACOUNT?", CffiStructSizeCmd},
        {NULL}
    };
    int cmdIndex;
    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    return subCommands[cmdIndex].cmdFn(ip, objc, objv, structCtxP);
}

/* Function: CffiStructResolve
 * Returns the internal representation of a name struct.
 *
 * Parameters:
 * ip - interpreter
 * nameP - name of the struct
 * baseType - CFFI_K_TYPE_STRUCT or CFFI_K_TYPE_UNION
 * structPP - location to hold pointer to resolved struct representation
 *
 * *NOTE:* The reference count on the returned structure is *not* incremented.
 * The caller should do that if it wishes to hold on to it.
 *
 * Returns:
 * *TCL_OK* on success with pointer to the <CffiStruct> stored in
 * *structPP*, or *TCL_ERROR* on error with message in interpreter.
 */
CffiResult
CffiStructResolve(Tcl_Interp *ip,
                  const char *nameP,
                  CffiBaseType baseType,
                  CffiStruct **structPP)
{
    Tcl_CmdInfo tci;
    Tcl_Obj *nameObj;
    int found;

    found = Tcl_GetCommandInfo(ip, nameP, &tci);
    if (!found)
        goto notfound;
    if (baseType == CFFI_K_TYPE_STRUCT && tci.objProc != CffiStructInstanceCmd)
        goto notfound;
    if (baseType == CFFI_K_TYPE_UNION && tci.objProc != CffiUnionInstanceCmd)
        goto notfound;

    CffiStructCmdCtx *structCtxP;
    CFFI_ASSERT(tci.clientData);
    structCtxP          = (CffiStructCmdCtx *)tci.objClientData;
    CffiStruct *structP = structCtxP->structP;
    if ((baseType == CFFI_K_TYPE_STRUCT && !CffiStructIsUnion(structP))
        || (baseType == CFFI_K_TYPE_UNION && CffiStructIsUnion(structP))) {
        *structPP = structP;
        return TCL_OK;
    }

notfound:
    nameObj = Tcl_NewStringObj(nameP, -1);
    Tcl_IncrRefCount(nameObj);
    (void)Tclh_ErrorNotFound(
        ip, baseType == CFFI_K_TYPE_UNION ? "Union" : "Struct", nameObj, NULL);
    Tcl_DecrRefCount(nameObj);
    return TCL_ERROR;
}

static CffiResult
CffiStructOrUnionObjCmd(ClientData cdata,
                        Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[],
                        CffiBaseType baseType
                        )
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    CffiStruct *structP = NULL;
    CffiStructCmdCtx *structCtxP;
    static const Tclh_SubCommand structCommands[] = {
        {"new", 1, 4, "STRUCTDEF ?-clear? ?-pack N?", NULL},
        {"create", 2, 5, "OBJNAME STRUCTDEF ?-clear? ?-pack N?", NULL},
        {NULL}
    };
    static const Tclh_SubCommand unionCommands[] = {
        {"new", 1, 4, "STRUCTDEF ?-clear?", NULL},
        {"create", 2, 5, "OBJNAME STRUCTDEF ?-clear?", NULL},
        {NULL}
    };
    Tcl_Obj *cmdNameObj;
    Tcl_Obj *defObj;
    int cmdIndex;
    int optIndex, objIndex;
    enum OPT { CLEAR, PACK };
    static const char *const structOpts[] = {"-clear", "-pack", NULL};
    static const char *const unionOpts[] = {"-clear", NULL};
    int pack;
    int clear;
    int ret;
    static unsigned int name_generator; /* No worries about thread safety as
                                           generated names are interp-local */

    CHECK(Tclh_SubCommandLookup(ip,
                                baseType == CFFI_K_TYPE_STRUCT ? structCommands
                                                               : unionCommands,
                                objc,
                                objv,
                                &cmdIndex));

    if (cmdIndex == 0) {
        /* new */
        Tcl_Namespace *nsP = Tcl_GetCurrentNamespace(ip);
        const char *sep;
        sep        = strcmp(nsP->fullName, "::") ? "::" : "";
        cmdNameObj = Tcl_ObjPrintf(
            "%s%scffiStruct%u", nsP->fullName, sep, ++name_generator);
        Tcl_IncrRefCount(cmdNameObj);
        defObj  = objv[2];
        objIndex = 3;
    }
    else {
        /* create */
        if (Tcl_GetCharLength(objv[2]) == 0) {
            return Tclh_ErrorInvalidValue(
                ip, objv[2], "Empty string specified for name.");
        }
        cmdNameObj = Tclh_NsQualifyNameObj(ip, objv[2], NULL);
        Tcl_IncrRefCount(cmdNameObj);
        defObj  = objv[3];
        objIndex = 4;
    }

    /* See if an option is specified */
    clear = 0;
    pack  = 0;
    for (; objIndex < objc; ++objIndex) {
        CHECK(Tcl_GetIndexFromObj(ip,
                                  objv[objIndex],
                                  baseType == CFFI_K_TYPE_STRUCT ? structOpts
                                                                 : unionOpts,
                                  "option",
                                  0,
                                  &optIndex));
        switch (optIndex) {
        case CLEAR:
            clear = 1;
            break;
        case PACK:
            if (objIndex == objc-1)
                return Tclh_ErrorOptionValueMissing(ip, objv[objIndex], NULL);
            ++objIndex;
            CHECK(Tclh_ObjToInt(ip, objv[objIndex], &pack));
            switch (pack) {
            case 0: case 1: case 2: case 4: case 8: case 16: break;
            default:
                return Tclh_ErrorInvalidValue(
                    ip,
                    objv[objIndex],
                    "-pack option value must be 0, 1, 2, 4, 8 or 16.");
            }
            break;
        }
    }

    ret = CffiStructParse(
        ipCtxP, cmdNameObj, defObj, baseType, pack, &structP);
    if (ret == TCL_OK) {
        if (clear)
            structP->flags |= CFFI_F_STRUCT_CLEAR;
        structCtxP          = ckalloc(sizeof(*structCtxP));
        structCtxP->ipCtxP  = ipCtxP;
        CffiStructRef(structP);
        structCtxP->structP = structP;

        Tcl_CreateObjCommand(ip,
                             Tcl_GetString(cmdNameObj),
                             baseType == CFFI_K_TYPE_STRUCT
                                 ? CffiStructInstanceCmd
                                 : CffiUnionInstanceCmd,
                             structCtxP,
                             CffiStructOrUnionInstanceDeleter);
        Tcl_SetObjResult(ip, cmdNameObj);
    }
    Tcl_DecrRefCount(cmdNameObj);
    return ret;
}

CffiResult
CffiStructObjCmd(ClientData cdata,
                   Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[])
{
    return CffiStructOrUnionObjCmd(cdata, ip, objc, objv, CFFI_K_TYPE_STRUCT);
}

CffiResult
CffiUnionObjCmd(ClientData cdata,
                   Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[])
{
    return CffiStructOrUnionObjCmd(cdata, ip, objc, objv, CFFI_K_TYPE_UNION);
}

