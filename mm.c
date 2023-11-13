/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include "mm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE     sizeof(void *)
#define DSIZE     (2 * WSIZE)
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((unsigned char *)(bp)-WSIZE)
#define FTRP(bp) ((unsigned char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) \
    ((unsigned char *)(bp) + GET_SIZE(((unsigned char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) \
    ((unsigned char *)(bp)-GET_SIZE(((unsigned char *)(bp)-DSIZE)))

#define PRED(bp) (*(unsigned char **)(bp))
#define SUCC(bp) (*(unsigned char **)((bp) + WSIZE))

typedef enum { ZERO_BLK = 0, FREE_BLK = 0, ALLOC_BLK = 1 } block_status_t;

static void *extend_heap(size_t);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    // Create the initial emtpy heap
    void *heap_pt;
    if ((heap_pt) = mem_sbrk(4 * WSIZE) == (void *)-1) {
        return -1;
    }

    PUT(heap_pt, ZERO_BLK);                              // Alignment padding
    PUT(heap_pt + (1 * WSIZE), PACK(DSIZE, ALLOC_BLK));  // Prologue header
    PUT(heap_pt + (2 * WSIZE), PACK(DSIZE, ALLOC_BLK));  // Prologue footer
    PUT(heap_pt + (3 * WSIZE), PACK((int)NULL, ALLOC_BLK));  // Epilogue header

    heap_pt = heap_pt + (2 * WSIZE);

    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
        return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize) copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/*
 * extend_heap - Extend the heap by allocating a new free block.
 */
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    // Allocate an even number of words to maintain alignment
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    // Initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size, FREE_BLK));           // Free block header
    PUT(FTRP(bp), PACK(size, FREE_BLK));           // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, ALLOC_BLK));  // New epilogue header

    // Coalesce if the previous block was free
    return coalesce(bp);
}