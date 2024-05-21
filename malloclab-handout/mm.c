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
#include <stdio.h>


//implement the four functions -> also place and a heap checker
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Joe & Alby",
    /* First member's full name */
    "Joe Sharratt",
    /* First member's email address */
    "joesharratt29@gmail.com",
    /* Second member's full name (leave blank if none) */
    "albert sharratt",
    "albysharratt@woof.com"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define PACK(size, alloc) ((size) | (alloc))

#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET(p) (*(unsigned int *)(p))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

//#define PAGE_SIZE 4096
#define WORD_SIZE 4
#define D_SIZE 8
#define CHUNK_SIZE (1<<12)

#define MAX(x, y)  ((x) > (y)? (x): (y))

//Header and footer packing
#define HDPTR(bp) ((char *)(bp) - WORD_SIZE)
#define FTPTR(bp) ((char *)(bp) + GET_SIZE(HDPTR(bp)) - D_SIZE)

//get address of previous blocks
#define GET_NEXT(bp) ((void *)(bp) + GET_SIZE(HDPTR(bp)))
#define GET_PREV(bp) ((void *)(bp) - GET_SIZE((void *)(bp) - D_SIZE))


//macros for explicit list
#define PUT_PTR(p, val) (*(char**)(p) = (char *)(val))
#define UPD_PRV_PTR(bp, val) (PUT_PTR(bp, val))
#define UPD_NXT_PTR(bp, val)  (PUT_PTR(bp + WORD_SIZE, val))

#define GET_PRV_PTR(bp) (*(char**)(bp))
#define GET_NEXT_PTR(bp) (*(char**)(bp + WORD_SIZE))



char* expl_ptr = NULL;
char* heap_lp;


void pushNode(char* ptr)
{
    if (expl_ptr == NULL){
        expl_ptr = ptr;
        UPD_PRV_PTR(expl_ptr, NULL);
        UPD_NXT_PTR(expl_ptr, NULL);
        return; 
    }

    UPD_PRV_PTR(ptr, expl_ptr);
    UPD_NXT_PTR(ptr, NULL);
    UPD_NXT_PTR(expl_ptr, ptr);
    expl_ptr = ptr;
    return;
    
}



void deleteNode(char* ptr)
{    
    if((ptr == expl_ptr) && (GET_PRV_PTR(ptr) == NULL))
    {
        expl_ptr = NULL;
        return;
    }

    if(ptr == expl_ptr){
        expl_ptr = GET_PRV_PTR(ptr);
        UPD_NXT_PTR(expl_ptr, NULL);
        return;
    }
    
    char* temp = expl_ptr;
    char* above = NULL;
    char* prev = NULL;

    while((temp != NULL) && (temp != ptr))
    {
        temp = GET_PRV_PTR(temp);
        above = GET_NEXT_PTR(temp);
        prev = GET_PRV_PTR(temp);
    }

    UPD_PRV_PTR(above, prev);
    if (prev != NULL)
    {
        UPD_NXT_PTR(prev, above);
    }
    
}

static void *coalesce(char* ptr)
{
    size_t alloc_prev = GET_ALLOC(FTPTR(GET_PREV(ptr)));
    size_t alloc_next = GET_ALLOC(HDPTR(GET_NEXT(ptr)));
    size_t curr_size = GET_SIZE(HDPTR(ptr));


    if (alloc_next && alloc_prev)
    {
        pushNode(ptr);
        return ptr;
    } else if (alloc_next && !alloc_prev)
    {
        char* p_ptr = GET_PREV(ptr);
        size_t p_size = GET_SIZE(HDPTR(p_ptr));
        curr_size += p_size;
        PUT(HDPTR(p_ptr), PACK(curr_size, 0));
        PUT(FTPTR(ptr), PACK(curr_size, 0));
        //delete old version further down and then push it back on the list
        deleteNode(p_ptr);
        pushNode(p_ptr);
        return p_ptr;
    } else if(!alloc_next &&alloc_prev){
        char* n_ptr = GET_NEXT(ptr);
        size_t n_size = GET_SIZE(HDPTR(n_ptr));
        curr_size += n_size;
        PUT(HDPTR(ptr), PACK(curr_size, 0));
        PUT(FTPTR(n_ptr), PACK(curr_size, 0));
        deleteNode(n_ptr);
        pushNode(ptr);
        return ptr;
    } else {
            char* n_ptr = GET_NEXT(ptr);
            char* p_ptr = GET_PREV(ptr);

            curr_size += GET_SIZE(HDPTR(p_ptr)) + GET_SIZE(FTPTR(n_ptr));
            PUT(HDPTR(p_ptr), PACK(curr_size, 0));
            PUT(FTPTR(n_ptr), PACK(curr_size, 0));
            deleteNode(p_ptr);
            deleteNode(n_ptr);
            pushNode(p_ptr);
            return p_ptr;
        }

}


static void *extend_heap(size_t incr, int init)
{
    char *bp;
    size_t new_incr;

    new_incr = (incr % 2) ? (incr+1) * WORD_SIZE : incr * WORD_SIZE;

    if ((long)(bp = mem_sbrk(new_incr))==-1)
    {
        return NULL;
    }

    PUT(HDPTR(bp), PACK(new_incr, 0));
    PUT(FTPTR(bp), PACK(new_incr, 0));
    PUT(HDPTR(GET_NEXT(bp)), PACK(0, 1));

    return coalesce(bp);
}




int mm_init(void)
{
    int init = 1;
    if((heap_lp = mem_sbrk(4*WORD_SIZE)) == (void *)-1)
    {
        return -1;
    }
    
    PUT(heap_lp, 0);
    PUT(heap_lp+ (1*WORD_SIZE), PACK(D_SIZE, 1));  //PROLOGUE HEADER
    PUT(heap_lp+ (2*WORD_SIZE), PACK(D_SIZE, 1));  //PROLOGUE FOOTER
    PUT(heap_lp + (3*WORD_SIZE), PACK(0, 1)); //Epilgoue

    heap_lp += 2*WORD_SIZE;

    if(extend_heap(CHUNK_SIZE/WORD_SIZE, init)==NULL){
        return -1;
    };
    return 0;
}

static void place(char* ptr, size_t size)
{
    //update headers to denote allocation
    //add shortened block on to heap
    size_t orig_size = GET_SIZE(HDPTR(ptr));
    size_t shortened_size = orig_size - size;

    PUT(HDPTR(ptr), PACK(size, 1));
    PUT(FTPTR(ptr), PACK(size, 1));

    deleteNode(ptr);
    
    if (shortened_size == 0){
        return; 
    } else {
        char* n_ptr = GET_NEXT(ptr);
        PUT(HDPTR(n_ptr), PACK(shortened_size, 0));
        PUT(FTPTR(n_ptr), PACK(shortened_size, 0));
        pushNode(n_ptr);
        return;
    }

}

static void *find_fit(size_t asize)
{
    size_t block_size = 0;
    char* temp_ptr = expl_ptr;

    while(temp_ptr != NULL)
    {
        if((block_size = GET_SIZE(HDPTR(temp_ptr)))> (asize+8)){
            break;
        }
        temp_ptr = GET_PRV_PTR(temp_ptr);
    }

    if (temp_ptr == NULL){
        return NULL;
    }

    return temp_ptr;
    
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extend_size;
    char* bp;
    int init = 0;

    if (size == 0){
        return NULL;
    }

    if (size <= D_SIZE){
        asize = 2*D_SIZE;
    }
    else {
        asize = D_SIZE * ((size + (D_SIZE) + (D_SIZE-1)) / D_SIZE);
    }

    if((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    } 

    extend_size = MAX(asize, CHUNK_SIZE);

    if((bp = extend_heap(extend_size/WORD_SIZE, init)) == NULL){
        return NULL;
    }

    place(bp, asize);
    return bp;

}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDPTR(ptr));
    PUT(HDPTR(ptr), PACK(size, 0));
    PUT(FTPTR(ptr), PACK(size, 0));

    pushNode(ptr); 
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














