#ifndef MEMLIFO_H
#define MEMLIFO_H

/*
 * Copyright (c) 2010-2023, Ashok P. Nadkarni
 * All rights reserved.
 *
 * See the file LICENSE for license
 */

#include <stdint.h>
#include "tclhBase.h"

#ifndef MEMLIFO_ASSERT
#define MEMLIFO_ASSERT(x) TCLH_ASSERT(x)
#endif

#define MEMLIFO_EXTERN

#ifndef MEMLIFO_INLINE
#ifdef _MSC_VER
# define MEMLIFO_INLINE __inline
#else
# define MEMLIFO_INLINE static inline
#endif
#endif

typedef size_t MemLifoUSizeT;

typedef struct _MemLifo MemLifo;
typedef struct _MemLifoMark MemLifoMark;
typedef MemLifoMark *MemLifoMarkHandle;

typedef void *MemLifoChunkAllocFn(size_t sz);
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
    MemLifoUSizeT	lifo_chunk_size;   /* Size of each chunk to allocate.
                                      Note this size might not be a multiple
                                      of the alignment size */
};

/* Error codes */
#define MEMLIFO_E_SUCCESS  0
#define MEMLIFO_E_NOMEMORY 1
#define MEMLIFO_E_INVALID_PARAM 2

/* Function: MemLifoInit
 * Create a Last-In-First-Out memory pool
 *
 * Parameters: 
 * lifoP - Memory pool to initialize
 * allocFunc - function to us to allocate memory. Must be callable at any
 *             time, including at initialization time and must return
 *             memory aligned at least to pointer size. If NULL, a default
 *             allocator is used.
 * freeFunc - function to free memory allocated by allocFunc. Can be NULL
 *            iff allocFunc is NULL.
 * chunkSz - Default size of usable space when allocating a chunk (does
 *           not include the chunk header). This is only a hint and may be
 *           adjusted to a minimum or maximum size.
 * flags - See MEMLIFO_F_* definitions
 * 
 * Initializes a memory pool from which memory can be allocated in
 * Last-In-First-Out fashion.
 *
 * Returns:
 * Returns MEMLIFO_E_SUCCESS or a MEMLIFO_E_* error code.
 */
MEMLIFO_EXTERN int MemLifoInit(MemLifo *lifoP,
                               MemLifoChunkAllocFn *allocFunc,
                               MemLifoChunkFreeFn *freeFunc,
                               MemLifoUSizeT chunkSz,
                               int flags);

/* Function: MemLifoClose
 * Frees up resources associated with a LIFO memory pool.
 *
 * Parameters:
 * lifoP - Memory pool to free up
 * 
 * Frees up various resources allocated for a LIFO memory pool. The pool
 * must not be used after the function is called. Note the function frees
 * the resources in use by the memory pool, not lifoP itself which is
 * owned by the caller.
*/
MEMLIFO_EXTERN void MemLifoClose(MemLifo  *lifoP);

/* Function: MemLifoAllocMin
 * Allocate a minimum amount of memory from LIFO memory pool
 *
 * Parameters:
 * lifoP - memory pool
 * minNumBytes - minimum number of bytes to allocate
 * allocatedP - location to store the actual number allocated. On
 *     success, this will be at least numBytes.
 *
 * Allocates memory from a LIFO memory pool and returns a pointer to it. If
 * allocatedP is not NULL and the current chunk has more than minNumBytes free,
 * all of it will be allocated.
 *
 * Returns:
 * Returns pointer to allocated memory on success. On failure returns 
 * a NULL pointer unless MEMLIFO_F_PANIC_ON_FAIL is set for the pool,
 * in which case it panics.
 */
MEMLIFO_EXTERN void *MemLifoAllocMin(
    MemLifo *lifoP,
    MemLifoUSizeT minNumBytes,
    MemLifoUSizeT *allocatedP
);

/* Function: MemLifoAlloc
 * Allocate memory from LIFO memory pool
 *
 * Parameters:
 * lifoP - memory pool
 * numBytes - number of bytes to allocate
 *
 * Allocates memory from a LIFO memory pool and returns a pointer to it.
 *
 * Returns:
 * Returns pointer to allocated memory on success. On failure returns 
 * a NULL pointer unless MEMLIFO_F_PANIC_ON_FAIL is set for the pool,
 * in which case it panics.
 */
MEMLIFO_INLINE void* MemLifoAlloc(MemLifo *l, MemLifoUSizeT numBytes) {
    return MemLifoAllocMin(l, numBytes, NULL);
}

/* Function: MemLifoPushFrameMin
 * Allocate a software stack frame in a LIFO memory pool
 *
 * Parameters:
 * lifoP - memory pool
 * minNumBytes - minimum number of bytes to allocate in the frame
 * allocatedP - location to store the actual number allocated. On
 *     success, this will be at least numBytes.
 * 
 * Allocates space in a LIFO memory pool being used as a software stack.
 * This provides a means of maintaining a software stack for temporary structures
 * that are too large to be allocated on the hardware stack.
 *
 * Both MemLifoMark and MemLifoPushFrame may be used on the same memory pool. The
 * latter function in effect creates a anonymous mark that is maintained
 * internally and which may be released (along with the associated user memory)
 * through the function MemLifoPopFrame which releases the last allocated mark,
 * irrespective of whether it was allocated through MemLifoMark or
 * MemLifoPushFrame. Alternatively, the mark and associated memory are also
 * freed when a previosly allocated mark is released.
 *
 * If allocatedP is not NULL, as much memory (at least minNumBytes) will be allocated
 * as possible from the current chunk.
 *
 * Returns:
 * Returns pointer to allocated memory on success.
 * If memory cannot be allocated, returns NULL unless MEMLIFO_F_PANIC_ON_FAIL
 * is set for the pool, in which case it panics.
*/
MEMLIFO_EXTERN void *MemLifoPushFrameMin(MemLifo *lifoP,
                                         MemLifoUSizeT minNumBytes,
                                         MemLifoUSizeT *allocatedP);

/* Function: MemLifoPushFrame
 * Allocate a software stack frame in a LIFO memory pool
 *
 * Parameters:
 * lifoP - memory pool
 * numBytes - number of bytes to allocate in the frame
 * 
 * Allocates space in a LIFO memory pool being used as a software stack.
 * This provides a means of maintaining a software stack for temporary structures
 * that are too large to be allocated on the hardware stack.
 *
 * Both MemLifoMark and MemLifoPushFrame may be used on the same memory pool. The
 * latter function in effect creates a anonymous mark that is maintained
 * internally and which may be released (along with the associated user memory)
 * through the function MemLifoPopFrame which releases the last allocated mark,
 * irrespective of whether it was allocated through MemLifoMark or
 * MemLifoPushFrame. Alternatively, the mark and associated memory are also
 * freed when a previosly allocated mark is released.
 *
 * Returns:
 * Returns pointer to allocated memory on success.
 * If memory cannot be allocated, returns NULL unless MEMLIFO_F_PANIC_ON_FAIL
 * is set for the pool, in which case it panics.
 */
MEMLIFO_INLINE void *
MemLifoPushFrame(MemLifo *lifoP, MemLifoUSizeT numBytes)
{
    return MemLifoPushFrameMin(lifoP, numBytes, NULL);
}

/* Function: MemLifoPushMark
 * Mark current state of a LIFO memory pool
 *
 * Parameters:
 * lifoP - memory pool
 *
 * Stores the current state of a LIFO memory pool on a stack. The state can be
 * restored later by calling MemLifoPopMark. Any number of marks may be
 * created but they must be popped in reverse order. However, not all marks
 * need be popped since popping a mark automatically pops all marks created
 * after it.
 *
 * Returns:
 * On success, returns a handle for the mark.
 * On failure, returns NULL unless the MEMLIFO_F_PANIC_ON_FAIL flag
 * is set for the pool, in which case the function panics.
 */
MEMLIFO_EXTERN MemLifoMarkHandle MemLifoPushMark(MemLifo *lifoP);

/* Function: MemLifoPopMark
 * Restore state of a LIFO memory pool
 *
 * Parameters:
 * lifoP - memory pool
 * 
 * Restores the state of a LIFO memory pool that was previously saved
 * using MemLifoPushMark or MemLifoPushFrame.  Memory allocated
 * from the pool between the push and this pop is freed up. Caller must
 * not subsequently call this routine with marks created between the
 * MemLifoMark and this MemLifoPopMark as they will have been
 * freed as well. The mark being passed to this routine is also freed
 * and hence must not be referenced afterward.
*/
MEMLIFO_EXTERN void MemLifoPopMark(MemLifoMarkHandle mark);

/* Function: MemLifoPopFrame
 * Release the topmost mark from the memory pool.
 *
 * Parameters:
 * lifoP - memory pool
 *
 * Releases the topmost (last allocated) mark from a MemLifo_t. The
 * mark may have been allocated using either MemLifoMark or
 * MemLifoPushFrame.
 */
MEMLIFO_INLINE void MemLifoPopFrame(MemLifo *lifoP) {
    MemLifoPopMark(lifoP->lifo_top_mark);
}

/* Function: MemLifoExpandLast
 * Expand the last memory block allocated from a LIFO memory pool
 *
 * Parameters:
 * lifoP - memory pool
 * incr - amount by which to expand the last allocated block
 * dontMove - if non-0, the expansion must happen in place, else the block
 *    may be moved in memory.
 * 
 * Expands the last memory block allocated from a LIFO memory pool. If no
 * memory was allocated in the pool since the last mark, just allocates
 * a new block of size incr.
 *
 * The function may move the block if necessary unless the dontMove parameter
 * is non-0 in which case the function will only attempt to expand the block in
 * place. When dontMove is 0, caller must be careful to update pointers that
 * point into the original block since it may have been moved on return.
 *
 * On success, the size of the block is guaranteed to be increased by at
 * least the requested increment. The function may fail if the last
 * allocated block cannot be expanded without moving and dontMove is set,
 * if it is not the last allocation from the LIFO memory pool, or if a mark
 * has been allocated after this block, or if there is insufficient memory.
 *
 * Returns:
 * On success, returns a pointer to the block position.
 * On failure, the function return a NULL pointer.
 * Note the function does not panic even if MEMLIFO_F_PANIC_ON_FAIL
 * is set for the pool.
 */
MEMLIFO_EXTERN void *
MemLifoExpandLast(MemLifo *lifoP, MemLifoUSizeT incr, int dontMove);

/* Function: MemLifoShrinkLast
 * Shrink the last memory block allocated from a LIFO memory pool
 * 
 * Parameters:
 * lifoP - memory pool
 * decr - amount by which to shrink the last allocated block
 * dontMove - if non-0, the shrinkage must happen in place, else the block
 *    may be moved in memory.
 * Shrinks the last memory block allocated from a LIFO memory pool. No marks
 * must have been allocated in the pool since the last memory allocation else
 * the function will fail.
 *
 * The function may move the block to compact memory unless the dontMove
 * parameter is non-0. When dontMove is 0, caller must be careful to update
 * pointers that point into the original block since it may have been moved
 * on return.
 *
 * On success, the size of the block is not guaranteed to have been decreased.
 *
 * Returns:
 * On success, returns a pointer to the block position.
 * On failure, the function return a NULL pointer. The original allocation
 * is still valid in this case.
 * Note the function does not panic even if MEMLIFO_F_PANIC_ON_FAIL
 * is set for the pool.
*/
MEMLIFO_EXTERN void *
MemLifoShrinkLast(MemLifo *lifoP, MemLifoUSizeT decr, int dontMove);

/* Function:
 * Resize the last memory block allocated from a LIFO memory pool
 * 
 * Parameters:
 * lifoP - memory pool
 * newSz - new size of the block
 * dontMove - if non-0, the resize must happen in place, else the block
 *    may be moved in memory.
 *
 * Resizes the last memory block allocated from a LIFO memory pool.  No marks
 * must have been allocated in the pool since the last memory allocation else
 * the function will fail.  The block may be moved if necessary unless the
 * dontMove parameter is non-0 in which the function will only attempt to resiz
 * e the block in place. When dontMove is 0, caller must be careful to update
 * pointers that point into the original block since it may have been moved on
 * return.

 * On success, the block is guaranteed to be at least as large as the requested
 * size. The function may fail if the block cannot be resized without moving
 * and dontMove is set, or if there is insufficient memory.
 *
 * Returns:
 * Returns pointer to new block position on success, else a NULL pointer.
 * Note the function does not panic even if MEMLIFO_F_PANIC_ON_FAIL
 * is set for the pool.
*/
MEMLIFO_EXTERN void *
MemLifoResizeLast(MemLifo *lifoP, MemLifoUSizeT newSz, int dontMove);

MEMLIFO_EXTERN int MemLifoValidate(MemLifo *l);

#endif
