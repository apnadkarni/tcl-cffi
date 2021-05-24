#ifndef MEMLIFO_H
#define MEMLIFO_H

/*
 * Copyright (c) 2010-2016, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

/* TBD - what if allocation of size 0 is requested ? */

#include <stdint.h>
#include "tclhBase.h"

#define MEMLIFO_ASSERT(x) TCLH_ASSERT(x)
#define MEMLIFO_EXTERN

typedef Tclh_USizeT MemlifoUSizeT;

typedef struct _MemLifo MemLifo;
typedef struct _MemLifoMark MemLifoMark;
typedef MemLifoMark *MemLifoMarkHandle;

typedef void *MemLifoChunkAllocFn(MemlifoUSizeT sz);
typedef void MemLifoChunkFreeFn(void *p);

struct _MemLifo {
#define MEMLIFO_F_PANIC_ON_FAIL 0x1
    int32_t		lifo_magic;	/* Only used in debug mode */
#define MEMLIFO_MAGIC 0xb92c610a
    int                 lifo_flags;
    MemLifoChunkAllocFn *lifo_allocFn;
    MemLifoChunkFreeFn *lifo_freeFn;
    MemLifoMarkHandle lifo_top_mark;  /* Topmost mark */
    MemLifoMarkHandle lifo_bot_mark; /* Bottommost mark */
    MemlifoUSizeT	lifo_chunk_size;   /* Size of each chunk to allocate.
                                      Note this size might not be a multiple
                                      of the alignment size */
};

#define MEMLIFO_E_SUCCESS  0
#define MEMLIFO_E_NOMEMORY 1

/*f
Create a Last-In-First-Out memory pool

Creates a memory pool from which memory can be allocated in Last-In-First-Out
fashion.

On success, returns a handle for the LIFO memory pool. On failure, returns 0.
In this case, the caller can obtain the error code through APNgetError().

See also MemLifoAlloc, MemLifoMark, MemLifoPopMark,
MemLifoReset

Returns MEMLIFO_E_SUCCESS or a MEMLIFO error code.
*/
MEMLIFO_EXTERN int MemLifoInit
(
    MemLifo *lifoP,
    MemLifoChunkAllocFn *allocFunc,
                                /* Pointer to routine to be used to allocate
				   memory. Must return aligned memory.
				   The parameter indicates the amount of
				   memory to allocate. Note this function
				   must be callable at ANY time including
				   during the MemLifoInit call itself.
				   Naturally, this function must not make a
				   call to LIFO memory allocation routines
				   from the same pool. This parameter may be
				   indicated as 0, in which case allocation
				   will be done using an internal default. */
    MemLifoChunkFreeFn *freeFunc,
				/* Pointer to routine to be used for freeing
				   memory allocated through allocFunc. The
				   parameter points to the memory to be
				   freed. This routine must be specified
				   unless allocFunc is 0. If allocFunc is 0,
				   the value of this parameter is ignored. */
    MemlifoUSizeT chunkSz,             /* Default unit size for allocating memory
				   from (*allocFunc) as needed. This does
				   not include space for descriptor at start
				   of each allocation. The implementation
				   has a minimum default which will be used
				   if chunkSz is too small. */
    int    flags                /* See MEMLIFO_F_* definitions */
     );


/*f
Free up resources associated with a LIFO memory pool

Frees up various resources allocated for a LIFO memory pool. The pool must not
be used after the function is called.
*/
MEMLIFO_EXTERN void MemLifoClose(MemLifo  *lifoP);

/*f
Allocate memory from LIFO memory pool

Allocates memory from a LIFO memory pool and returns a pointer to it.
If memory cannot be allocated, returns NULL unless MEMLIFO_F_PANIC_ON_FAIL
is set for the pool, in which case it panics.

Returns pointer to allocated memory on success, a null pointer on failure.
*/
MEMLIFO_EXTERN void* MemLifoAlloc
    (
     MemLifo *lifoP,    /* LIFO pool to allocate from */
     MemlifoUSizeT sz   /* Number of bytes to allocate */
    );

/*f
Allocate memory from LIFO memory pool and copies data to it

Allocates memory from a LIFO memory pool and returns a pointer to it
after copying the data pointed to by srcP.
If memory cannot be allocated, returns NULL unless MEMLIFO_F_PANIC_ON_FAIL
is set for the pool, in which case it panics.

Returns pointer to allocated memory on success, a null pointer on failure.
*/
MEMLIFO_EXTERN void* MemLifoCopy
    (
     MemLifo *lifoP,    /* LIFO pool to allocate from */
     void *srcP,        /* Source to copy from */
     MemlifoUSizeT nbytes      /* Number of bytes to copy */
    );


/*f
Allocate zeroed memory from LIFO memory pool

Allocates zeroed memory from a LIFO memory pool and returns a pointer to it.
If memory cannot be allocated, returns NULL unless MEMLIFO_F_PANIC_ON_FAIL
is set for the pool, in which case it panics.

Returns pointer to allocated memory on success, a null pointer on failure.
*/
MEMLIFO_EXTERN void* MemLifoZeroes(MemLifo *l, MemlifoUSizeT nbytes);

/*f
Allocate a software stack frame in a LIFO memory pool

Allocates space in a LIFO memory pool being used as a software stack.
This provides a means of maintaining a software stack for temporary structures
that are too large to be allocated on the hardware stack.

Both MemLifoMark and MemLifoPushFrame may be used on the same
MemLifo_t. The latter function in effect creates a anonymous mark
that is maintained internally and which may be released (along with
the associated user memory) through the function MemLifoPopFrame
which releases the last allocated mark, irrespective of whether it was
allocated through MemLifoMark or MemLifoPushFrame.
Alternatively, the mark and associated memory are also freed when a
previosly allocated mark is released.

If memory cannot be allocated, returns NULL unless MEMLIFO_F_PANIC_ON_FAIL
is set for the pool, in which case it panics.

Returns pointer to allocated memory on success, a null pointer on failure.
*/
MEMLIFO_EXTERN void *MemLifoPushFrame
(
    MemLifo *lifoP,		/* LIFO pool to allocate from */
    MemlifoUSizeT sz			/* Number of bytes to allocate */
    );

/*
Release the topmost mark from a MemLifo_t

Releases the topmost (last allocated) mark from a MemLifo_t. The
mark may have been allocated using either MemLifoMark or
MemLifoPushFrame.

Returns ERROR_SUCCESS or Win32 status code on error
*/
#define MemLifoPopFrame(l_) MemLifoPopMark((l_)->lifo_top_mark)

/*
Mark current state of a LIFO memory pool

Stores the current state of a LIFO memory pool on a stack. The state can be
restored later by calling MemLifoPopMark. Any number of marks may be
created but they must be popped in reverse order. However, not all marks
need be popped since popping a mark automatically pops all marks created
after it.

If memory cannot be allocated, returns NULL unless MEMLIFO_F_PANIC_ON_FAIL
is set for the pool, in which case it panics.

Returns a handle for the mark if successful, 0 on failure.
*/
MEMLIFO_EXTERN MemLifoMarkHandle MemLifoPushMark(MemLifo *lifoP);


/*f
Restore state of a LIFO memory pool

Restores the state of a LIFO memory pool that was previously saved
using MemLifoMark or MemLifoMarkedAlloc.  Memory allocated
from the pool between the push and this pop is freed up. Caller must
not subsequently call this routine with marks created between the
MemLifoMark and this MemLifoPopMark as they will have been
freed as well. The mark being passed to this routine is freed as well
and hence must not be reused.

Returns 0 on success, 1 on failure
*/
MEMLIFO_EXTERN void MemLifoPopMark(MemLifoMarkHandle mark);


/*f
Expand the last memory block allocated from a LIFO memory pool

Expands the last memory block allocated from a LIFO memory pool. If no
memory was allocated in the pool since the last mark, just allocates
a new block of size incr.

The function may move the block if necessary unless the dontMove parameter
is non-0 in which case the function will only attempt to expand the block in
place. When dontMove is 0, caller must be careful to update pointers that
point into the original block since it may have been moved on return.

On success, the size of the block is guaranteed to be increased by at
least the requested increment. The function may fail if the last
allocated block cannot be expanded without moving and dontMove is set,
if it is not the last allocation from the LIFO memory pool, or if a mark
has been allocated after this block, or if there is insufficient memory.

On failure, the function return a NULL pointer. 
Note the function does not panic even if MEMLIFO_F_PANIC_ON_FAIL
is set for the pool.

Returns pointer to new block position on success, else a NULL pointer.
*/
MEMLIFO_EXTERN void * MemLifoExpandLast
    (
     MemLifo *lifoP,		/* Lifo pool from which alllocation
					   was made */
     MemlifoUSizeT incr,			/* The amount by which the
					   block is to be expanded. */
     int dontMove			/* If 0, block may be moved if
					   necessary to expand. If non-0, the
					   function will fail if the block
					   cannot be expanded in place. */
    );



/*f
Shrink the last memory block allocated from a LIFO memory pool

Shrinks the last memory block allocated from a LIFO memory pool. No marks
must have been allocated in the pool since the last memory allocation else
the function will fail.

The function may move the block to compact memory unless the dontMove parameter
is non-0. When dontMove is 0, caller must be careful to update pointers that
point into the original block since it may have been moved on return.

On success, the size of the block is not guaranteed to have been decreased.

Returns pointer to address of relocated block or a null pointer if a mark
was allocated after the last allocation.
Note the function does not panic even if MEMLIFO_F_PANIC_ON_FAIL
is set for the pool.
*/
MEMLIFO_EXTERN void *MemLifoShrinkLast
    (
     MemLifo *lifoP,       /* Lifo pool from which alllocation
                                   was made */
     MemlifoUSizeT decr,               /* The amount by which the
                                   block is to be shrunk. */
     int dontMove              /* If 0, block may be moved in order
                                   to reduce memory fragmentation.
                                   If non-0, the the block will be
                                   shrunk in place or be left as is. */
    );


/*f
Resize the last memory block allocated from a LIFO memory pool

Resizes the last memory block allocated from a LIFO memory pool.  No marks
must have been allocated in the pool since the last memory allocation else
the function will fail.  The block may be moved if necessary unless the
dontMove parameter is non-0 in which the function will only attempt to resiz
e the block in place. When dontMove is 0, caller must be careful to update
pointers that point into the original block since it may have been moved on
return.

On success, the block is guaranteed to be at least as large as the requested
size. The function may fail if the block cannot be resized without moving
and dontMove is set, or if there is insufficient memory.

Returns pointer to new block position on success, else a NULL pointer.
Note the function does not panic even if MEMLIFO_F_PANIC_ON_FAIL
is set for the pool.
*/
MEMLIFO_EXTERN void * MemLifoResizeLast
(
    MemLifo *lifoP,        /* Lifo pool from which allocation was made */
    MemlifoUSizeT newSz,		/* New size of the block */
    int dontMove                /* If 0, block may be moved if
                                   necessary to expand. If non-0, the
                                   function will fail if the block
                                   cannot be expanded in place. */
    );

MEMLIFO_EXTERN int MemLifoValidate(MemLifo *l);

#endif
