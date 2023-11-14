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

#define WSIZE        sizeof(void *)
#define DSIZE        (2 * WSIZE)
#define CHUNKSIZE    (1 << 12) /* Extend heap by this amount (bytes) */
#define SEG_LIST_LEN 20

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

/* Declarations */
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *extend_heap(size_t);
static void *coalesce(void *);
static void *attach_free_list(void *bp, size_t asize);
static void *detach_free_list(void *bp);
static size_t asize_to_index(size_t asize);

/* Heap list */
static void *heap_listp = NULL;
static void *free_listp[SEG_LIST_LEN] = {NULL};

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    // Create the initial emtpy heap
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }

    PUT(heap_listp, 0);                                     // Alignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, ALLOC_BLK));  // Prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, ALLOC_BLK));  // Prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, ALLOC_BLK));      // Epilogue header

    heap_listp = heap_listp + (2 * WSIZE);
    for (size_t i = 0; i < SEG_LIST_LEN; i++) {
        free_listp[i] = NULL;
    }

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
    size_t asize;
    size_t extend_size;
    unsigned char *bp;

    if (size == 0) {
        return NULL;
    }

    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extend_size = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extend_size / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, FREE_BLK));
    PUT(FTRP(ptr), PACK(size, FREE_BLK));
    coalesce(ptr);
}

static void *coalesce(void *ptr) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    // Update free list and header, footer
    if (prev_alloc && !next_alloc) {
        detach_free_list(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, FREE_BLK));
        PUT(FTRP(ptr), PACK(size, FREE_BLK));
    } else if (!prev_alloc && next_alloc) {
        detach_free_list(PREV_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(FTRP(ptr), PACK(size, FREE_BLK));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, FREE_BLK));
        ptr = PREV_BLKP(ptr);
    } else if (!prev_alloc && !next_alloc) {
        detach_free_list(PREV_BLKP(ptr));
        detach_free_list(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(FTRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, FREE_BLK));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, FREE_BLK));
        ptr = PREV_BLKP(ptr);
    }

    attach_free_list(ptr, size);
    return ptr;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    // if (size < copySize)
    copySize = size;
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

static void *find_fit(size_t asize) {
    size_t start_index = asize_to_index(asize);
    for (size_t i = start_index; i < SEG_LIST_LEN; i++) {
        if (free_listp[i] == NULL) {
            continue;
        }

        for (void *bp = free_listp[i]; bp != NULL; bp = SUCC(bp)) {
            if (GET_SIZE(HDRP(bp)) >= asize) {
                return bp;
            }
        }
    }

    return NULL;
}

static void *find_fit(size_t asize) {
    void *bp;
    size_t index = asize_to_index(asize);

    for (bp = free_listp[index]; bp != NULL; bp = SUCC(bp)) {
        if (GET_SIZE(HDRP(bp)) >= asize) {
            return bp;
        }
    }

    return NULL;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    detach_free_list(bp);
    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, ALLOC_BLK));
        PUT(FTRP(bp), PACK(asize, ALLOC_BLK));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, FREE_BLK));
        PUT(FTRP(bp), PACK(csize - asize, FREE_BLK));
        coalesce(bp);
    } else {
        PUT(HDRP(bp), PACK(csize, ALLOC_BLK));
        PUT(FTRP(bp), PACK(csize, ALLOC_BLK));
    }
}

static void *attach_free_list(void *bp, size_t asize) {
    int index = 0;
    void *current;
    void *tmp = NULL;

    while((index < SEG_LIST_LEN - 1) && (size > 1)){
        size >>= 1;
        index += 1;
    }
    current = free_listp[index];
    while (( current != NULL ) && ( size > GET_SIZE(HDRP(current)))) {
        tmp = current;
        current = SUCC(current);
    }
    if (current != NULL){
        if (tmp != NULL){
            SUCC(bp) = current;
            PRED(bp) = tmp;
            PRED(current) = bp;
            SUCC(tmp) = bp;
        }else{
            SUCC(bp) = current;
            PRED(bp) = NULL;
            PRED(current) = bp;
            free_listp[index] = bp;
        }
    }else{
        if(tmp != NULL){
            SUCC(bp) = NULL;
            PRED(bp) = tmp;
            SUCC(tmp) = bp;
        }else{
            SUCC(bp) = NULL;
            PRED(bp) = NULL;
            free_list_p[index] = bp;
        }
    }

    return bp;
}

static void *detach_free_list(void *bp) {
    size_t asize = GET_SIZE(HDRP(bp));
    size_t index = asize_to_index(asize);
    // void *curr_bp = free_listp[index];
    if (bp == free_listp[index]) {
        free_listp[index] = SUCC(bp);
    } else if (SUCC(bp) == NULL) {
        SUCC(PRED(bp)) = NULL;
    } else if (SUCC(bp) != NULL) {
        SUCC(PRED(bp)) = SUCC(bp);
        PRED(SUCC(bp)) = PRED(bp);
    }
    return bp;
}

static size_t asize_to_index(size_t asize) {
    size_t power = 1;
    size_t index = 0;
    if (asize == 0) {
        return 0;
    }
    while (power <= asize) {
        power <<= 1;
        index += 1;
    }

    if (index >= SEG_LIST_LEN) {
        index = SEG_LIST_LEN - 1;
    }
    return index;
}