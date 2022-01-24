/*
 * Copyright (c) 2021, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#define TCLH_SHORTNAMES
#include "tclCffiInt.h"

static CffiStruct *CffiStructCkalloc(int nfields)
{
    int         sz;
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
                                  &structP->fields[j].fieldType)
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
        CFFI_ASSERT(fieldP->fieldType.dataType.baseType != CFFI_K_TYPE_VOID);
        CffiTypeLayoutInfo(
            &fieldP->fieldType.dataType, NULL, &field_size, &field_alignment);

        if (field_size <= 0) {
            /* Dynamic size field. Should have been caught in earlier pass */
            return Tclh_ErrorGeneric(
                ip, NULL, "Internal error: struct field is not fixed size.");
        }
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
#ifdef CFFI_USE_LIBFFI
    structP->libffiTypes = NULL;
#endif
    structP->nRefs     = 0;
    structP->alignment = struct_alignment;
    structP->size      = (offset + struct_alignment - 1) & ~(struct_alignment - 1);
    structP->flags     = 0;
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
        case CFFI_K_TYPE_ASTRING:
        case CFFI_K_TYPE_UNISTRING:
        case CFFI_K_TYPE_CHAR_ARRAY:
        case CFFI_K_TYPE_UNICHAR_ARRAY:
            Tcl_AppendPrintfToObj(objP, "\n  %s", cffiBaseTypes[baseType].token);
            if (fieldP->fieldType.dataType.u.tagObj)
                Tcl_AppendPrintfToObj(
                    objP,
                    ".%s",
                    Tcl_GetString(fieldP->fieldType.dataType.u.tagObj));
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
            if (CffiTypeIsArray(&fieldP->fieldType.dataType))
                Tcl_AppendPrintfToObj(
                    objP, "[%d]", fieldP->fieldType.dataType.arraySize);
            break;

        default:
            Tcl_AppendPrintfToObj(objP,
                                  "\n  %s %s",
                                  cffiBaseTypes[baseType].token,
                                  Tcl_GetString(fieldP->nameObj));
            if (CffiTypeIsArray(&fieldP->fieldType.dataType))
                Tcl_AppendPrintfToObj(
                    objP, "[%d]", fieldP->fieldType.dataType.arraySize);
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

    objs[0] = Tcl_NewStringObj("size", 4);
    objs[1] = Tcl_NewLongObj(structP->size);
    objs[2] = Tcl_NewStringObj("alignment", 9);
    objs[3] = Tcl_NewLongObj(structP->alignment);
    objs[4] = Tcl_NewStringObj("flags", 5);
    objs[5] = Tcl_NewLongObj(structP->flags);
    objs[6] = Tcl_NewStringObj("fields", 6);
    objs[7] = Tcl_NewListObj(structP->nFields, NULL);

    for (i = 0; i < structP->nFields; ++i) {
        CffiField *fieldP = &structP->fields[i];
        Tcl_Obj *attrObjs[6];
        attrObjs[0] = Tcl_NewStringObj("size", 4);
        attrObjs[1] = Tcl_NewLongObj(fieldP->size);
        attrObjs[2] = Tcl_NewStringObj("offset", 6);
        attrObjs[3] = Tcl_NewLongObj(fieldP->offset);
        attrObjs[4] = Tcl_NewStringObj("definition", 10);
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
 * ipCtxP - Interpreter context
 * structP - pointer to the struct definition internal form
 * structValueObj - the *Tcl_Obj* containing the script level struct value
 * resultP - the location where the struct is to be constructed. Caller
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
                  void *resultP,
                  MemLifo *memlifoP)
{
    int i;
    void *pv;
    CffiResult ret;
    Tcl_Interp *ip = ipCtxP->interp;

#define STOREFIELD(objfn_, type_)                                              \
    do {                                                                       \
        int indx;                                                              \
        type_ *valueP                      = (type_ *)fieldResultP;            \
        const CffiTypeAndAttrs *typeAttrsP = &fieldP->fieldType;               \
        CFFI_ASSERT(count != 0); /* -1 for scalar, > 0 for arrays */           \
        if (CffiTypeIsNotArray(&typeAttrsP->dataType)) {                       \
            if (typeAttrsP->flags                                              \
                & (CFFI_F_ATTR_BITMASK | CFFI_F_ATTR_ENUM)) {                  \
                Tcl_WideInt wide;                                              \
                CHECK(                                                         \
                    CffiIntValueFromObj(ipCtxP, typeAttrsP, valueObj, &wide)); \
                *valueP = (type_)wide;                                         \
            }                                                                  \
            else {                                                             \
                CHECK(objfn_(ip, valueObj, valueP));                           \
            }                                                                  \
        }                                                                      \
        else {                                                                 \
            Tcl_Obj **valueObjList;                                            \
            int nvalues;                                                       \
            if (Tcl_ListObjGetElements(ip, valueObj, &nvalues, &valueObjList)  \
                != TCL_OK)                                                     \
                return TCL_ERROR;                                              \
            /* Note - if caller has specified too few values, it's ok          \
             * because perhaps the actual count is specified in another        \
             * parameter. If too many, only up to array size */                \
            if (nvalues > count)                                               \
                nvalues = count;                                               \
            if (typeAttrsP->flags                                              \
                & (CFFI_F_ATTR_BITMASK | CFFI_F_ATTR_ENUM)) {                  \
                for (indx = 0; indx < nvalues; ++indx) {                       \
                    Tcl_WideInt wide;                                          \
                    CHECK(CffiIntValueFromObj(                                 \
                        ipCtxP, typeAttrsP, valueObjList[indx], &wide));       \
                    valueP[indx] = (type_)wide;                                \
                }                                                              \
            }                                                                  \
            else {                                                             \
                for (indx = 0; indx < nvalues; ++indx) {                       \
                    CHECK(objfn_(ip, valueObjList[indx], &valueP[indx]));      \
                    if (ret != TCL_OK)                                         \
                        return ret;                                            \
                }                                                              \
            }                                                                  \
            /* Fill additional unspecified elements with 0 */                  \
            for (indx = nvalues; indx < count; ++indx)                         \
                valueP[indx] = 0;                                              \
        }                                                                      \
    } while (0)

    if (structP->flags & CFFI_F_STRUCT_CLEAR)
        memset(resultP, 0, structP->size);

    /*
     * NOTE: On failure, we can just return as there are no allocations
     * for any fields that need to be cleaned up.
     */
    for (i = 0; i < structP->nFields; ++i) {
        Tcl_Obj *valueObj;
        const CffiField *fieldP = &structP->fields[i];
        void *fieldResultP      = fieldP->offset + (char *)resultP;
        int count               = fieldP->fieldType.dataType.arraySize;

        CFFI_ASSERT(count != 0);/* No dynamic size arrays in structs */

        if (Tcl_DictObjGet(ip,structValueObj, fieldP->nameObj, &valueObj) != TCL_OK)
            return TCL_ERROR; /* Invalid dictionary. Note TCL_OK does not mean found */
        if (valueObj == NULL) {
            if (fieldP->fieldType.flags & CFFI_F_ATTR_STRUCTSIZE) {
                /* Fill in struct size */
                switch (fieldP->fieldType.dataType.baseType) {
                case CFFI_K_TYPE_SCHAR:
                    *(signed char *)fieldResultP = (signed char)structP->size;
                    continue;
                case CFFI_K_TYPE_UCHAR:
                    *(unsigned char *)fieldResultP = (unsigned char)structP->size;
                    continue;
                case CFFI_K_TYPE_SHORT:
                    *(short *)fieldResultP = (short)structP->size;
                    continue;
                case CFFI_K_TYPE_USHORT:
                    *(unsigned short *)fieldResultP = (unsigned short)structP->size;
                    continue;
                case CFFI_K_TYPE_INT:
                    *(int *)fieldResultP = (int)structP->size;
                    continue;
                case CFFI_K_TYPE_UINT:
                    *(unsigned int *)fieldResultP = (unsigned int)structP->size;
                    continue;
                case CFFI_K_TYPE_LONG:
                    *(long *)fieldResultP = (long)structP->size;
                    continue;
                case CFFI_K_TYPE_ULONG:
                    *(unsigned long *)fieldResultP = (unsigned long)structP->size;
                    continue;
                case CFFI_K_TYPE_LONGLONG:
                    *(long long *)fieldResultP = (long long)structP->size;
                    continue;
                case CFFI_K_TYPE_ULONGLONG:
                    *(unsigned long long *)fieldResultP = (unsigned long long)structP->size;
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
            return Tclh_ErrorNotFound(
                ip,
                "Struct field",
                fieldP->nameObj,
                "Field missing in struct dictionary value.");
        }
        switch (fieldP->fieldType.dataType.baseType) {
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
            if (CffiTypeIsNotArray(&fieldP->fieldType.dataType)) {
                ret = CffiStructFromObj(ipCtxP,
                                        fieldP->fieldType.dataType.u.structP,
                                        valueObj,
                                        fieldResultP,
                                        memlifoP);
            }
            else {
                /* Nested array of structs */
                char *valueP;
                Tcl_Obj **valueObjList;
                int nvalues;
                int indx;
                int struct_size = fieldP->fieldType.dataType.u.structP->size;
                if (Tcl_ListObjGetElements(
                        ip, valueObj, &nvalues, &valueObjList)
                    != TCL_OK)
                    return TCL_ERROR;
                valueP = (char *)fieldResultP;
                if (nvalues > count)
                    nvalues = count;
                for (indx = 0; indx < nvalues; ++indx, valueP += struct_size) {
                    ret = CffiStructFromObj(ipCtxP,
                                            fieldP->fieldType.dataType.u.structP,
                                            valueObjList[indx],
                                            valueP, memlifoP);
                    if (ret != TCL_OK)
                        return ret;
                }
                /* Fill additional unspecified elements with 0 */
                if (indx < count) {
                    memset(valueP, 0, struct_size * (count - indx));
                }
            }
            if (ret != TCL_OK)
                return ret;
            break;
        case CFFI_K_TYPE_POINTER:
            if (count < 0) {
                ret = CffiPointerFromObj(
                    ip, &fieldP->fieldType, valueObj, &pv);
                if (ret != TCL_OK)
                    return ret;
                *(void **)fieldResultP = pv;
            }
            else {
                void **valueP;
                Tcl_Obj **valueObjList;
                int nvalues;
                int indx;
                if (Tcl_ListObjGetElements(
                        ip, valueObj, &nvalues, &valueObjList)
                    != TCL_OK)
                    return TCL_ERROR;
                valueP = (void **)fieldResultP;
                if (nvalues > count)
                    nvalues = count;
                for (indx = 0; indx < nvalues; ++indx) {
                    ret = CffiPointerFromObj(ip,
                                             &fieldP->fieldType,
                                             valueObjList[indx],
                                             &valueP[indx]);
                    if (ret != TCL_OK)
                        return ret;
                }
                /* Fill additional unspecified elements with 0 */
                for (indx = nvalues; indx < count; ++indx)
                    valueP[indx] = NULL;
            }
            break;
        case CFFI_K_TYPE_CHAR_ARRAY:
            CFFI_ASSERT(count > 0);
            ret =
                CffiCharsFromObj(ip,
                                 fieldP->fieldType.dataType.u.tagObj,
                                 valueObj,
                                 (char *)fieldResultP,
                                 count);
            if (ret != TCL_OK)
                return ret;
            break;
        case CFFI_K_TYPE_UNICHAR_ARRAY:
            CFFI_ASSERT(count > 0);
            ret = CffiUniCharsFromObj(
                ip, valueObj, (Tcl_UniChar *)fieldResultP, count);
            if (ret != TCL_OK)
                return ret;
            break;
        case CFFI_K_TYPE_BYTE_ARRAY:
            CFFI_ASSERT(count > 0);
            ret = CffiBytesFromObj(
                ip, valueObj, fieldResultP, count);
            if (ret != TCL_OK)
                return ret;
            break;
        case CFFI_K_TYPE_ASTRING:
            if (memlifoP == NULL) {
                return ErrorInvalidValue(
                    ip,
                    NULL,
                    "string type not supported in this struct context.");
            }
            ret = CffiCharsInMemlifoFromObj(
                ip,
                fieldP->fieldType.dataType.u.tagObj,
                valueObj,
                memlifoP,
                (char **)fieldResultP);
            if (ret != TCL_OK)
                return ret;
            if ((fieldP->fieldType.flags & CFFI_F_ATTR_NULLIFEMPTY)
                && (*(*(char **)fieldResultP) == 0)) {
                *(char **)fieldResultP = 0;
            }
            break;
        case CFFI_K_TYPE_UNISTRING:
            if (memlifoP) {
                int space;
                Tcl_UniChar *fromP = Tcl_GetUnicodeFromObj(valueObj, &space);
                Tcl_UniChar *toP;
                if (space == 0
                    && (fieldP->fieldType.flags
                        & CFFI_F_ATTR_NULLIFEMPTY)) {
                    *(Tcl_UniChar **)fieldResultP = NULL;
                }
                else {
                    space = sizeof(Tcl_UniChar) * (space + 1);
                    toP   = MemLifoAlloc(memlifoP, space);
                    memcpy(toP, fromP, space);
                    *(Tcl_UniChar **)fieldResultP = toP;
                }
            }
            else {
                return ErrorInvalidValue(ip, NULL, "unistring type not supported in this struct context.");
            }
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
        /* fields within structs cannot be dynamic size */
        CFFI_ASSERT(! CffiTypeIsVariableSizeArray(&fieldP->fieldType.dataType));
        ret = CffiNativeValueToObj(ip,
                                   &fieldP->fieldType,
                                   fieldP->offset + (char *)valueP,
                                   fieldP->fieldType.dataType.arraySize,
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


/* Function: CffiStructAllocateCmd
 * Allocates memory for one or more structures.
 *
 * Parameters:
 * ip - Interpreter
 * objc - number of arguments in objv[].
 * objv - argument array. This includes the command and subcommand provided
 *   at the script level, and an optional count
 * scructCtxP - pointer to struct context
 *
 * The single optional argument specifies the number of structures to
 * allocate. By default, space for a single structure is allocated.
 * The allocated space is not initialized.
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
    Tcl_Obj *resultObj;
    void *resultP;
    int count;

    if (objc == 2)
        count = 1;
    else {
        CHECK(Tclh_ObjToInt(ip, objv[2], &count));
        if (count <= 0)
            return Tclh_ErrorInvalidValue(
                ip, NULL, "Array size must be a positive integer.");
    }
    resultP = ckalloc(count * structP->size);

    if (Tclh_PointerRegister(ip, resultP, structP->name, &resultObj) != TCL_OK) {
        ckfree(resultP);
        return TCL_ERROR;
    }
    Tcl_SetObjResult(ip, resultObj);
    return TCL_OK;
}


static CffiResult
CffiStructFromNativePointer(Tcl_Interp *ip,
                        int objc,
                        Tcl_Obj *const objv[],
                        CffiStructCmdCtx *structCtxP,
                        int safe
                        )
{
    CffiStruct *structP = structCtxP->structP;
    int index;
    void *valueP;
    Tcl_Obj *resultObj;

    if (safe)
        CHECK(Tclh_PointerObjVerify(ip, objv[2], &valueP, structP->name));
    else
        CHECK(Tclh_PointerUnwrap(ip, objv[2], &valueP, structP->name));

    /* TBD - check alignment of valueP */

    if (objc == 4) {
        Tcl_WideInt wide;
        CHECK(Tclh_ObjToRangedInt(ip, objv[3], 0, INT_MAX, &wide));
        index = (int) wide;
    }
    else
        index = 0;

    /* TBD - check addition does not cause valueP to overflow */
    CHECK(CffiStructToObj(
        ip, structP, (index * structP->size) + (char *)valueP, &resultObj));

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
 * scructCtxP - pointer to struct context
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
 * objv[2] - pointer to memory
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
    CffiStruct *structP = structCtxP->structP;
    void *valueP;
    int index;

    CHECK(Tclh_PointerObjVerify(ip, objv[2], &valueP, structP->name));

    /* TBD - check alignment of valueP */

    if (objc == 5) {
        Tcl_WideInt wide;
        CHECK(Tclh_ObjToRangedInt(ip, objv[4], 0, INT_MAX, &wide));
        index = (int) wide;
    }
    else
        index = 0;

    /* TBD - check addition does not cause valueP to overflow */
    CHECK(CffiStructFromObj(structCtxP->ipCtxP,
                            structP,
                            objv[3],
                            (index * structP->size) + (char *)valueP,
                            NULL));

    return TCL_OK;
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
 * scructCtxP - pointer to struct context
 *
 * The **objv** contains the following arguments:
 * objv[2] - pointer to memory
 * objv[3] - field name
 * objv[4] - optional, index into array of structs pointed to by objv[2]
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
    CffiStruct *structP = structCtxP->structP;
    void *pv;
    char *fieldAddr;
    int fieldIndex;

    /* S fieldpointer POINTER FIELDNAME ?TAG? ?INDEX? */
    CFFI_ASSERT(objc > 4);

    fieldIndex = CffiStructFindField(ip, structP, Tcl_GetString(objv[3]));
    if (fieldIndex < 0)
        return TCL_ERROR;

    CHECK(Tclh_PointerObjVerify(ip, objv[2], &pv, structP->name));
    fieldAddr = pv;

    /* TBD - check alignment of fieldP for the struct */
    if (objc == 6) {
        Tcl_WideInt wide;
        CHECK(Tclh_ObjToRangedInt(ip, objv[5], 0, INT_MAX, &wide));
        fieldAddr += structP->size * (int)wide;
    }
    fieldAddr += structP->fields[fieldIndex].offset;
    Tcl_SetObjResult(ip,
                     Tclh_PointerWrap(fieldAddr, objc > 4 ? objv[4] : NULL));

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

    ret = Tclh_PointerObjUnregister(
        ip, objv[2], &valueP, structCtxP->structP->name);
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
    valueP    = Tcl_GetByteArrayFromObj(resultObj, NULL);
    ret = CffiStructFromObj(structCtxP->ipCtxP, structP, objv[2], valueP, NULL);
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
    ret = CffiStructToObj(ip, structP, offset + valueP, &resultObj);
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
        {"allocate", 0, 1, "?COUNT?", CffiStructAllocateCmd}, /* Same command as Encode */
        {"destroy", 0, 0, "", CffiStructDestroyCmd},
        {"fromnative", 1, 2, "POINTER ?INDEX?", CffiStructFromNativeCmd},
        {"fromnative!", 1, 2, "POINTER ?INDEX?", CffiStructFromNativeUnsafeCmd},
        {"describe", 0, 0, "", CffiStructDescribeCmd},
        {"tonative", 2, 3, "POINTER INITIALIZER ?INDEX?", CffiStructToNativeCmd},
        {"free", 1, 1, "POINTER", CffiStructFreeCmd},
        {"frombinary", 1, 1, "BINARY", CffiStructFromBinaryCmd},
        {"info", 0, 0, "", CffiStructInfoCmd},
        {"name", 0, 0, "", CffiStructNameCmd},
        {"tobinary", 1, 1, "DICTIONARY", CffiStructToBinaryCmd},
        {"fieldpointer", 2, 4, "POINTER FIELD ?TAG? ?INDEX?", CffiStructFieldPointerCmd},
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

