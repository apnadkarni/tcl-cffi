/*
 * Copyright (c) 2021-2023, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include "tclCffiInt.h"

#define ALIGNMENT sizeof(double) /* TBD */
#define ALIGNMASK (~(intptr_t)(ALIGNMENT - 1))
/* Round up to alignment size */
#define ROUNDUP(x_) ((ALIGNMENT - 1 + (x_)) & ALIGNMASK)

typedef struct CffiArenaFrame {
    struct CffiArenaFrame *prevFrameP;
    struct CffiArenaAllocationLink *allocationsP; /* List of allocations in this arena */
} CffiArenaFrame;
#define ARENA_FRAME_HEADER_SIZE ROUNDUP(sizeof(CffiArenaFrame))

typedef struct CffiArenaAllocationLink {
    struct CffiArenaAllocationLink *prevAllocationP;
} CffiArenaAllocationLink;
#define ARENA_ALLOCATION_LINK_SIZE ROUNDUP(sizeof(CffiArenaAllocationLink))

static CffiResult CffiArenaPopFrame(CffiInterpCtx *ipCtxP);

CffiResult CffiArenaInit(CffiInterpCtx *ipCtxP)
{
    if (Tclh_LifoInit(
            &ipCtxP->arenaStore, NULL, NULL, 8000, 0)
        != 0) {
        return TCL_ERROR;
    }
    ipCtxP->arenaFrameP = NULL;
    return TCL_OK;
}

void CffiArenaFinit(CffiInterpCtx *ipCtxP)
{
    while (ipCtxP->arenaFrameP)
        CffiArenaPopFrame(ipCtxP);
    Tclh_LifoClose(&ipCtxP->arenaStore);
}

static CffiResult
CffiArenaPushFrame(CffiInterpCtx *ipCtxP, Tcl_Size size, void **allocationP)
{
    if (size < 0) {
memFail:
        return Tclh_ErrorAllocation(
            ipCtxP->interp, "Arena", "Could not allocate arena memory.");
    }

    Tcl_Size extra = ARENA_FRAME_HEADER_SIZE;
    if (size && allocationP)
        extra += ARENA_ALLOCATION_LINK_SIZE;

    if ((TCL_SIZE_MAX - extra) < size)
        goto memFail;

    CffiArenaFrame *arenaFrameP =
        Tclh_LifoPushFrame(&ipCtxP->arenaStore, size + extra);
    if (arenaFrameP == NULL)
        goto memFail;

    /* Link on to list of active arenas */
    arenaFrameP->prevFrameP = ipCtxP->arenaFrameP;
    ipCtxP->arenaFrameP     = arenaFrameP;
    arenaFrameP->allocationsP = NULL;

    if (size && allocationP) {
        CffiArenaAllocationLink *arenaLinkP;
        arenaLinkP   = (CffiArenaAllocationLink *) (ARENA_FRAME_HEADER_SIZE + (char *)arenaFrameP);
        arenaLinkP->prevAllocationP = NULL;
        arenaFrameP->allocationsP   = arenaLinkP;
        *allocationP = ARENA_ALLOCATION_LINK_SIZE + (char *)arenaLinkP;
        CFFI_ASSERT(*allocationP == (extra + (char *)arenaFrameP));
    }
    else {
        /* No allocation requested. */
        if (allocationP)
            *allocationP = NULL;
    }
    return TCL_OK;
}

CffiResult CffiArenaAllocate(CffiInterpCtx *ipCtxP, Tcl_Size size, void **allocationP)
{
    if (size <= 0) {
memFail:
        return Tclh_ErrorAllocation(
            ipCtxP->interp, "Arena", "Could not allocate arena memory.");
    }

    if (ipCtxP->arenaFrameP == NULL) {
        return Tclh_ErrorGeneric(
            ipCtxP->interp,
            NULL,
            "Internal error: attempt to allocate from an empty arena.");
    }

    if ((TCL_SIZE_MAX - ARENA_ALLOCATION_LINK_SIZE) < size)
        goto memFail;

    CffiArenaAllocationLink *arenaLinkP =
        Tclh_LifoPushFrame(&ipCtxP->arenaStore, size + ARENA_ALLOCATION_LINK_SIZE);
    if (arenaLinkP == NULL)
        goto memFail;

    arenaLinkP->prevAllocationP = ipCtxP->arenaFrameP->allocationsP;
    ipCtxP->arenaFrameP->allocationsP   = arenaLinkP;

    *allocationP = ARENA_ALLOCATION_LINK_SIZE + (char *)arenaLinkP;

    return TCL_OK;
}

static CffiResult
CffiArenaPopFrame(CffiInterpCtx *ipCtxP)
{
    CffiArenaFrame *arenaFrameP = ipCtxP->arenaFrameP;
    if (arenaFrameP == NULL) {
        return Tclh_ErrorGeneric(
            ipCtxP->interp,
            NULL,
            "Internal error: attempt to pop frame in empty arena.");
    }
    ipCtxP->arenaFrameP = arenaFrameP->prevFrameP;

    /* Unregister any pointers that might have been registered */
    CffiArenaAllocationLink *arenaLinkP;
    for (arenaLinkP = arenaFrameP->allocationsP; arenaLinkP;
         arenaLinkP = arenaLinkP->prevAllocationP) {
        void *p = ARENA_ALLOCATION_LINK_SIZE + (char *)arenaLinkP;
        (void)Tclh_PointerUnregister(ipCtxP->interp, ipCtxP->tclhCtxP, p, NULL);
    }
    Tclh_LifoPopFrame(&ipCtxP->arenaStore);
    return TCL_OK;
}

static CffiResult
CffiArenaValidate(CffiInterpCtx *ipCtxP)
{
    int invalid = Tclh_LifoValidate(&ipCtxP->arenaStore);
    if (invalid) {
        Tcl_SetObjResult(
            ipCtxP->interp,
            Tcl_ObjPrintf("Arena memlifo validation failed with error code %d.",
                          invalid));
        return TCL_ERROR;
    }

    CffiArenaFrame *frameP;
    for (frameP = ipCtxP->arenaFrameP; frameP; frameP = frameP->prevFrameP) {
        CffiArenaAllocationLink *linkP;
        for (linkP = frameP->allocationsP; linkP;
             linkP = linkP->prevAllocationP) {
            void *p = ARENA_ALLOCATION_LINK_SIZE + (char *)linkP;
            CHECK(
                Tclh_PointerVerify(ipCtxP->interp, ipCtxP->tclhCtxP, p, NULL));
        }
    }
    return TCL_OK;
}

CffiResult
CffiArenaObjCmd(ClientData cdata,
                Tcl_Interp *ip,
                int objc,
                Tcl_Obj *const objv[])
{
    CffiInterpCtx *ipCtxP = (CffiInterpCtx *)cdata;
    enum cmds { ALLOCATE, NEW, POPFRAME, PUSHFRAME, VALIDATE };
    int cmdIndex;
    static Tclh_SubCommand subCommands[] = {
        {"allocate", 1, 2, "SIZE ?TAG?", NULL},
        {"new", 2, 3, "TYPE INITIALIZER ?TAG?", NULL},
        {"popframe", 0, 0, "", NULL},
        {"pushframe", 0, 2, "?SIZE ?TAG??", NULL},
        {"validate", 0, 0, "", NULL},
        {NULL}};
    void *pv;
    Tcl_Obj *resultObj = NULL;
    Tcl_Size size = 0;
    CffiResult ret = TCL_OK;
    CffiTypeAndAttrs typeAttrs;

    CHECK(Tclh_SubCommandLookup(ip, subCommands, objc, objv, &cmdIndex));
    switch (cmdIndex) {
    case ALLOCATE:
        CHECK(CffiParseAllocationSize(ipCtxP, objv[2], &size));
        CHECK(CffiArenaAllocate(ipCtxP, size, &pv));
        /* Note nothing to free if pointer obj creation fails */
        ret = CffiMakePointerObj(
            ipCtxP, pv, objc > 3 ? objv[3] : NULL, 0, &resultObj);
        break;

    case NEW:
        CHECK(
            CffiTypeSizeForValue(ipCtxP, objv[2], objv[3], &typeAttrs, &size));
        CHECK(CffiArenaAllocate(ipCtxP, size, &pv));
        /* Note nothing to free on failure */
        CHECK(CffiNativeValueFromObj(
            ipCtxP, &typeAttrs, 0, objv[3], 0, pv, 0, NULL));
        ret = CffiMakePointerObj(
            ipCtxP, pv, objc > 4 ? objv[4] : NULL, 0, &resultObj);
        break;

    case PUSHFRAME:
        if (objc > 2) {
            CHECK(CffiParseAllocationSize(ipCtxP, objv[2], &size));
        }
        CHECK(CffiArenaPushFrame(ipCtxP, size, &pv));
        if (size) {
            CFFI_ASSERT(pv);
            ret = CffiMakePointerObj(
                ipCtxP, pv, objc > 3 ? objv[3] : NULL, 0, &resultObj);
            if (ret != TCL_OK) {
                CffiArenaPopFrame(ipCtxP); /* Pop frame we just created */
            }
        }
        break;

    case POPFRAME:
        ret = CffiArenaPopFrame(ipCtxP);
        break;

    case VALIDATE:
        ret = CffiArenaValidate(ipCtxP);
        break;
    }

    if (ret == TCL_OK && resultObj != NULL)
        Tcl_SetObjResult(ip, resultObj);
    return ret;
}