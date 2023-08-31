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
        CffiLibffiStruct *nextP = structP->libffiTypes;
        while (nextP) {
            CffiLibffiStruct *nextNextP = nextP->nextP;
            ckfree(nextP);
            nextP = nextNextP;
        }
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
    CFFI_ASSERT(structP->dynamicCountFieldIndex >= 0);
    /* fldIndex can never be last one, already checked in parse */
    CFFI_ASSERT(fldIndex < structP->nFields - 1);

    const CffiField *fieldP = &structP->fields[fldIndex];
    ptrdiff_t offset = fieldP->offset;
    int vlaCount = CffiGetCountFromNative(
        offset + (char *)valueP, fieldP->fieldType.dataType.baseType);

    if (vlaCount <= 0) {
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

    CFFI_ASSERT(structP->dynamicCountFieldIndex >= 0);
    /* fldIndex can never be last one, already checked in parse */
    CFFI_ASSERT(fldIndex < structP->nFields - 1);

    if (structValueObj == NULL) {
        Tclh_ErrorInvalidValue(
            ip,
            structP->fields[fldIndex].nameObj,
            "No value supplied for dynamic field count.");
        return -1;

    }
    Tcl_Obj *countObj;
    if (Tcl_DictObjGet(
            ip, structValueObj, structP->fields[fldIndex].nameObj, &countObj)
            != TCL_OK
        || countObj == NULL) {
        return -1;
    }
    int count;
    if (Tcl_GetIntFromObj(ip, countObj, &count) != TCL_OK)
        return -1;
    if (count <= 0) {
        Tclh_ErrorInvalidValue(
            ip,
            countObj,
            "Variable length array size must be greater than 0.");
        return -1;
    }
    return count;
}

/* Function: CffiStructSizeVLACount
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
 * 
 * Returns:
 * Size of the struct or -1 if it could not be calculated.
 * The interp result holds an error message on failure.
 */
int
CffiStructSizeVLACount(CffiInterpCtx *ipCtxP,
                       CffiStruct *structP,
                       int vlaCount)
{
    if (!CffiStructIsVariableSize(structP))
        return structP->size;

    Tcl_Interp *ip = ipCtxP ? ipCtxP->interp : NULL;

    if (vlaCount <= 0) {
        return Tclh_ErrorInvalidValue(
            ip, NULL, "Variable length array size must be greater than 0.");
    }

    int size = structP->size; /* Base size - should include alignment */

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
        /* Align to field size */
        size = (size + elemAlignment - 1) & ~(elemAlignment - 1);
        size += vlaCount * elemSize;
    } else {
        /* Case 2 - vlaCount is meant for nested component */
        CFFI_ASSERT(typeP->baseType == CFFI_K_TYPE_STRUCT);
        CffiStruct *innerStructP = typeP->u.structP;
        int innerStructSize =
            CffiStructSizeVLACount(ipCtxP, innerStructP, vlaCount);
        if (innerStructSize < 0)
            return -1;
        /* Usual alignment as above */
        size = (size + innerStructP->alignment - 1)
             & ~(innerStructP->alignment - 1);
        size += innerStructSize;
    }
    /* Now ensure whole thing aligned to struct size */
    size = (size + structP->alignment - 1) & ~(structP->alignment - 1);
    return size;
}

/* Function: CffiStructSize
 * Get the number of bytes needed to store a struct.
 *
 * Parameters:
 * ipCtxP - interp context. Used for error messages. May be NULL.
 * structP - struct descriptor
 * structValueObj - struct value as a dictionary mapping field names to values.
 *   May be NULL for fixed size structs.
 * 
 * The function takes into account variable sized structs.
 *
 * Returns:
 * Size of the struct or -1 if it could not be calculated (e.g. the struct
 * is variable size and not enough field values were not provided). The
 * interp result holds an error message on failure.
 */
int
CffiStructSize(CffiInterpCtx *ipCtxP,
               const CffiStruct *structP,
               Tcl_Obj *structValueObj)
{
    if (!CffiStructIsVariableSize(structP))
        return structP->size;

    int size = structP->size; /* Base size - should include alignment */

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

        int count;
        count = CffiStructGetDynamicCountFromObj(ipCtxP, structP, structValueObj);
        if (count < 0)
            return -1;

        /* Align to field size */
        size = (size + elemAlignment - 1) & ~(elemAlignment - 1);
        size += count * elemSize;
    } else {
        /* Case 2 - recurse to get variable inner struct size and add */
        CFFI_ASSERT(typeP->baseType == CFFI_K_TYPE_STRUCT);
        CffiStruct *innerStructP = typeP->u.structP;
        Tcl_Obj *innerStructObj;
        if (Tcl_DictObjGet(ipCtxP->interp,
                           structValueObj,
                           structP->fields[lastFldIndex].nameObj,
                           &innerStructObj)
            != TCL_OK || innerStructObj == NULL) {
            return -1;
        }

        int innerStructSize =
            CffiStructSize(ipCtxP, innerStructP, innerStructObj);
        if (innerStructSize < 0)
            return -1;
        /* Usual alignment as above */
        size = (size + innerStructP->alignment - 1)
             & ~(innerStructP->alignment - 1);
        size += innerStructSize;
    }
    /* Now ensure whole thing aligned to struct size */
    size = (size + structP->alignment - 1) & ~(structP->alignment - 1);
    return size;
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
        fldArraySize =
            CffiStructGetDynamicCountNative(ipCtxP, structP, structAddr);
        if (fldArraySize <= 0)
            return TCL_ERROR;
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

    if (safe)
        CHECK(Tclh_PointerObjVerify(
            ip, ipCtxP->tclhCtxP, nativePointerObj, &structAddr, structP->name));
    else {
        /* TODO - check - is this correct. Won't below check for registration? */
        CHECK(Tclh_PointerUnwrapTagged(
            ip, ipCtxP->tclhCtxP, nativePointerObj, &structAddr, structP->name));
        if (structAddr == NULL) {
            Tcl_SetResult(ip, "Pointer is NULL.", TCL_STATIC);
            return TCL_ERROR;
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
            return CffiErrorVariableSizeStruct(ip, structP);
    } else {
        /* Not variable size struct. Indexing allowed. */
        structAddr = (structP->size * structIndex) + (char *)structAddr;
    }
    *structAddrP = structAddr;
    return TCL_OK;
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
    Tcl_Size i, j, nobjs, nfields;
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
    structP->dynamicCountFieldIndex = -1;
    structP->nFields = 0; /* Update as we go along */
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
        if (CffiTypeIsVariableSize(&structP->fields[j].fieldType.dataType)) {
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

        if (field_alignment > struct_alignment)
            struct_alignment = field_alignment;

        /* See if offset needs to be aligned for this field */
        offset = (offset + field_alignment - 1) & ~(field_alignment - 1);
        fieldP->offset = offset;
        fieldP->size   = field_size;
        if (field_size > 0) {
            /* Fixed size field */
            offset += field_size;
        }
    }

    /*
     * Calculate and cache the field (if any) holding dynamic count of
     * elements in the last field
     */
    CffiType *lastFldTypeP = &structP->fields[nfields - 1].fieldType.dataType;
    if (CffiTypeIsVariableSize(lastFldTypeP)) {
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
        Tcl_ObjPrintf("Struct %s nRefs=%d size=%d alignment=%d flags=%d nFields=%d",
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
            if (CffiTypeIsArray(&fieldP->fieldType.dataType))
                Tcl_AppendPrintfToObj(
                    objP, "[%d]", fieldP->fieldType.dataType.arraySize);
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
            if (CffiTypeIsArray(&fieldP->fieldType.dataType))
                Tcl_AppendPrintfToObj(
                    objP, "[%d]", fieldP->fieldType.dataType.arraySize);
            break;
        case CFFI_K_TYPE_UNISTRING:
        case CFFI_K_TYPE_UNICHAR_ARRAY:
#ifdef _WIN32
        case CFFI_K_TYPE_WINSTRING:
        case CFFI_K_TYPE_WINCHAR_ARRAY:
#endif
            Tcl_AppendPrintfToObj(objP, "\n  %s", cffiBaseTypes[baseType].token);
            Tcl_AppendPrintfToObj(objP, " %s", Tcl_GetString(fieldP->nameObj));
            if (CffiTypeIsArray(&fieldP->fieldType.dataType))
                Tcl_AppendPrintfToObj(
                    objP, "[%d]", fieldP->fieldType.dataType.arraySize);
            break;
        case CFFI_K_TYPE_STRUCT:
            Tcl_AppendPrintfToObj(
                objP,
                "\n  %s %s %s",
                cffiBaseTypes[baseType].token,
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
            if (CffiTypeIsVLA(&fieldP->fieldType.dataType)) {
                Tcl_AppendPrintfToObj(
                    objP, "[%s]", Tcl_GetString(fieldP->fieldType.dataType.countHolderObj));
            }
            else if (CffiTypeIsArray(&fieldP->fieldType.dataType)) {
                Tcl_AppendPrintfToObj(
                    objP, "[%d]", fieldP->fieldType.dataType.arraySize);

            }
            break;
        }
        Tcl_AppendPrintfToObj(
            objP, " offset=%d size=%d", fieldP->offset, fieldP->size);
    }

    Tcl_SetObjResult(ip, objP);

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

    objs[0] = Tcl_NewStringObj("Size", 4);
    objs[1] = Tcl_NewLongObj(structP->size);
    objs[2] = Tcl_NewStringObj("Alignment", 9);
    objs[3] = Tcl_NewLongObj(structP->alignment);
    objs[4] = Tcl_NewStringObj("Flags", 5);
    objs[5] = Tcl_NewLongObj(structP->flags);
    objs[6] = Tcl_NewStringObj("Fields", 6);
    objs[7] = Tcl_NewListObj(structP->nFields, NULL);

    for (i = 0; i < structP->nFields; ++i) {
        CffiField *fieldP = &structP->fields[i];
        Tcl_Obj *attrObjs[6];
        attrObjs[0] = Tcl_NewStringObj("Size", 4);
        attrObjs[1] = Tcl_NewLongObj(fieldP->size);
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
 * Constructs a C struct from a *Tcl_Obj* wrapper.
 *
 * Parameters:
 * ipCtxP - interpreter context
 * structP - pointer to the struct definition internal form
 * structValueObj - the *Tcl_Obj* containing the script level struct value
 * flags - if CFFI_F_PRESERVE_ON_ERROR is set, the target location will
 *   be preserved in case of errors.
 * structResultP - the location where the struct is to be constructed. Caller
 *           has to ensure size is large enough (as per *structP->size*)
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
    Tcl_Size structSize;

    structSize = CffiStructSize(ipCtxP, structP, structValueObj);
    if (structSize <= 0) {
        return TCL_ERROR;
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
        if (valueObj == NULL) {
            if (fieldP->fieldType.flags & CFFI_F_ATTR_STRUCTSIZE) {
                /* Fill in struct size */
                switch (fieldP->fieldType.dataType.baseType) {
                case CFFI_K_TYPE_SCHAR:
                    *(signed char *)fieldAddress = (signed char)structSize;
                    continue;
                case CFFI_K_TYPE_UCHAR:
                    *(unsigned char *)fieldAddress = (unsigned char)structSize;
                    continue;
                case CFFI_K_TYPE_SHORT:
                    *(short *)fieldAddress = (short)structSize;
                    continue;
                case CFFI_K_TYPE_USHORT:
                    *(unsigned short *)fieldAddress = (unsigned short)structSize;
                    continue;
                case CFFI_K_TYPE_INT:
                    *(int *)fieldAddress = (int)structSize;
                    continue;
                case CFFI_K_TYPE_UINT:
                    *(unsigned int *)fieldAddress = (unsigned int)structSize;
                    continue;
                case CFFI_K_TYPE_LONG:
                    *(long *)fieldAddress = (long)structSize;
                    continue;
                case CFFI_K_TYPE_ULONG:
                    *(unsigned long *)fieldAddress = (unsigned long)structSize;
                    continue;
                case CFFI_K_TYPE_LONGLONG:
                    *(long long *)fieldAddress = (long long)structSize;
                    continue;
                case CFFI_K_TYPE_ULONGLONG:
                    *(unsigned long long *)fieldAddress = (unsigned long long)structSize;
                    continue;
                default:
                    break; /* Just fall thru looking for default */
                }
            }

            valueObj = fieldP->fieldType.parseModeSpecificObj;/* Default */
        }
        if (valueObj == NULL) {
            /*
             * If still NULL, error unless the CLEAR bit is set which
             * indicates zeroing suffices
             */
            if (structP->flags & CFFI_F_STRUCT_CLEAR)
                continue;/* Move on to next field leaving this cleared */
            ret = Tclh_ErrorNotFound(
                ip,
                "Struct field",
                fieldP->nameObj,
                "Field missing in struct dictionary value.");
            break;
        }

        int realArraySize = 0;
        /* The last field may be a variable sized array */
        if (i == (structP->nFields - 1)
            && CffiTypeIsVLA(&fieldP->fieldType.dataType)) {
            realArraySize =
                CffiStructGetDynamicCountFromObj(ipCtxP, structP, structValueObj);
            if (realArraySize <= 0) {
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
        if (ret != TCL_OK)
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
 * Returns:
 *
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

/* Function: CffiErrorVariableSizeStruct
 * Stores an error message in the interpreter stating the operation is not
 * valid for a variable sized struct.
 *
 * Parameters:
 * ip - interpreter. May be NULL>
 * structP - struct descriptor
 *
 * Returns:
 * Always returns TCL_ERROR.
 */
CffiResult
CffiErrorVariableSizeStruct(Tcl_Interp *ip, CffiStruct *structP)
{
    return Tclh_ErrorInvalidValue(
        ip, NULL, "Operation not permitted on variable sized structs.");
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
    int vlaCount;
    CFFI_ASSERT(objc >= 2 && objc <=4);

    if (objc == 2)
        count = 1;
    else {
        CHECK(Tclh_ObjToInt(ip, objv[2], &count));
        if (count <= 0)
            goto invalid_array_size;
        if (objc > 3) {
            CHECK(Tclh_ObjToInt(ip, objv[3], &vlaCount));
            if (vlaCount <= 0)
                goto invalid_array_size;
        }
    }
    if (CffiStructIsVariableSize(structP)) {
        if (objc != 4) {
            return Tclh_ErrorNumArgs(
                ip,
                2,
                objv,
                "COUNT VLASIZE. The size of the variable length "
                "array component must be specified.");
        }
        if (count != 1) {
            return Tclh_ErrorInvalidValue(
                ip, NULL, "Struct is variable sized and arrays of variable size structs not allowed.");
        }
    }
    else {
        if (objc > 3) {
            return Tclh_ErrorNumArgs(ip, 2, objv, "?COUNT?. Fixed size structs do not take a VLASIZE argument.");
        }
    }

    Tcl_Obj *resultObj;
    void *resultP;
    Tcl_Size structSize =
        CffiStructSizeVLACount(structCtxP->ipCtxP, structP, vlaCount);
    if (structSize <= 0) {
        return TCL_ERROR;
    }
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

invalid_array_size:
    return Tclh_ErrorInvalidValue(
        ip, NULL, "Array size must be a positive integer.");
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
    Tcl_Size structSize;

    CFFI_ASSERT(objc == 2 || objc == 3);

    /* Note - this will fail for variable size structs when objc==2. TODO */
    structSize = CffiStructSize(ipCtxP, structP, objc == 2 ? NULL : objv[2]);
    if (structSize <= 0)
        return TCL_ERROR;

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
            ret = CffiStructComputeFieldAddress(ipCtxP,
                                                structP,
                                                structAddr,
                                                nameObj,
                                                &fldIndex,
                                                &fldAddr,
                                                &fldArraySize);
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

    resultObj = Tcl_NewByteArrayObj(NULL, structP->size);
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
 * scructCtxP - pointer to struct context
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
    int len;
    unsigned int offset;
    CffiResult ret;
    Tcl_Obj *resultObj;
    CffiStruct *structP = structCtxP->structP;

    valueP = Tcl_GetByteArrayFromObj(objv[2], &len);
    if (objc == 4)
        CHECK(Tclh_ObjToUInt(ip, objv[3], &offset));
    else
        offset = 0;

    if (len < (int) structP->size || (len - structP->size) < offset) {
        /* Do not pass objv[2] to error function -> binary values not printable */
        return Tclh_ErrorInvalidValue(
            ip, NULL, "Truncated structure binary value.");
    }
    ret = CffiStructToObj(
        structCtxP->ipCtxP, structP, offset + valueP, &resultObj);
    if (ret == TCL_OK)
        Tcl_SetObjResult(ip, resultObj);
    return ret;
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
        {"allocate", 0, 2, "?COUNT ?VLACOUNT??", CffiStructAllocateCmd},
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
        {"info", 0, 0, "", CffiStructInfoCmd},
        {"name", 0, 0, "", CffiStructNameCmd},
        {"new", 0, 1, "?INITIALIZER?", CffiStructNewCmd},
        {"setnative", 3, 4, "POINTER FIELD VALUE ?INDEX?", CffiStructSetNativeCmd},
        {"setnative!", 3, 4, "POINTER FIELD VALUE ?INDEX?", CffiStructSetNativeUnsafeCmd},
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
CffiStructInstanceDeleter(ClientData cdata)
{
    CffiStructCmdCtx *ctxP = (CffiStructCmdCtx *)cdata;
    if (ctxP->structP)
        CffiStructUnref(ctxP->structP);
    /* Note ctxP->ipCtxP is interp-wide and not to be freed here */
    ckfree(ctxP);
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
    int found;

    found = Tcl_GetCommandInfo(ip, nameP, &tci);
    if (found && tci.objProc == CffiStructInstanceCmd) {
        CffiStructCmdCtx *structCtxP;
        CFFI_ASSERT(tci.clientData);
        structCtxP = (CffiStructCmdCtx *)tci.objClientData;
        *structPP  = structCtxP->structP;
        return TCL_OK;
    }
    nameObj = Tcl_NewStringObj(nameP, -1);
    Tcl_IncrRefCount(nameObj);
    if (found) {
        (void)Tclh_ErrorInvalidValue(ip, nameObj, "Not a cffi::Struct.");
    }
    else {
        (void)Tclh_ErrorNotFound(ip, "Struct definition", nameObj, NULL);
    }
    Tcl_DecrRefCount(nameObj);
    return TCL_ERROR;
}


CffiResult
CffiStructObjCmd(ClientData cdata,
                   Tcl_Interp *ip,
                   int objc,
                   Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    CffiStruct *structP;
    CffiStructCmdCtx *structCtxP;
    static const Tclh_SubCommand subCommands[] = {
        {"new", 1, 2, "STRUCTDEF ?-clear?", NULL},
        {"create", 2, 3, "OBJNAME STRUCTDEF ?-clear?", NULL},
        {NULL}
    };
    Tcl_Obj *cmdNameObj;
    Tcl_Obj *defObj;
    int cmdIndex;
    int optIndex;
    int clear;
    int ret;
    static unsigned int name_generator; /* No worries about thread safety as
                                           generated names are interp-local */

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));

    if (cmdIndex == 0) {
        /* new */
        Tcl_Namespace *nsP = Tcl_GetCurrentNamespace(ip);
        const char *sep;
        sep        = strcmp(nsP->fullName, "::") ? "::" : "";
        cmdNameObj = Tcl_ObjPrintf(
            "%s%scffiStruct%u", nsP->fullName, sep, ++name_generator);
        Tcl_IncrRefCount(cmdNameObj);
        defObj  = objv[2];
        optIndex = 3;
    }
    else {
        /* create */
        if (Tcl_GetCharLength(objv[2]) == 0) {
            return Tclh_ErrorInvalidValue(
                ip, objv[2], "Empty string specified for structure name.");
        }
        cmdNameObj = Tclh_NsQualifyNameObj(ip, objv[2], NULL);
        Tcl_IncrRefCount(cmdNameObj);
        defObj  = objv[3];
        optIndex = 4;
    }

    /* See if an option is specified */
    clear = 0;
    if (optIndex < objc) {
        const char *optStr = Tcl_GetString(objv[optIndex]);
        if (strcmp("-clear", optStr)) {
            Tcl_DecrRefCount(cmdNameObj);
            Tcl_SetObjResult(
                ip, Tcl_ObjPrintf("bad option \"%s\": must be -clear", optStr));
            return TCL_ERROR;
        }
        clear = 1;
    }

    ret = CffiStructParse(
        ipCtxP, cmdNameObj, defObj, &structP);
    if (ret == TCL_OK) {
        if (clear)
            structP->flags |= CFFI_F_STRUCT_CLEAR;
        structCtxP          = ckalloc(sizeof(*structCtxP));
        structCtxP->ipCtxP  = ipCtxP;
        CffiStructRef(structP);
        structCtxP->structP = structP;

        Tcl_CreateObjCommand(ip,
                             Tcl_GetString(cmdNameObj),
                             CffiStructInstanceCmd,
                             structCtxP,
                             CffiStructInstanceDeleter);
        Tcl_SetObjResult(ip, cmdNameObj);
    }
    Tcl_DecrRefCount(cmdNameObj);
    return ret;
}

