/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>

#define MIN_BLOCKSIZE 64
#define PREV_ALLOC_MASK 0x02
#define ALLOC_MASK 0x01

void init_freelist_heads();

size_t get_min_blocksize(size_t size);

size_t get_header_size();
size_t get_footer_size();
size_t get_links_size();

int get_freelist_index(size_t size);

void add_to_freelist(sf_block *block);
void remove_from_freelist(sf_block *block);

size_t get_blocksize(sf_block *block);
size_t get_prev_blocksize(sf_block *block);
int get_prev_alloc(sf_block *block);

void set_blocksize(sf_block *block, size_t size, int is_alloc, int is_prevalloc);
void set_footer(sf_footer *footer, size_t size, int is_alloc, int is_prevalloc);
void set_header(sf_header *header, size_t size, int is_alloc, int is_prevalloc);

void *search_freelist(int min_index, size_t min_blocksize);
void split_block(sf_block *block, size_t reqd_blocksize);

sf_block *get_next_block(sf_block *block);
sf_block *get_prev_block(sf_block *block);

void *coalesce_prev(sf_block *block);
void coalesce_next(sf_block *block);

int is_valid_ptr(void *block);

void set_block_allc(sf_block *block);
void set_block_free(sf_block *block);

void *sf_malloc(size_t size)
{

    if(size<=0) return NULL;

    void *start_of_heap = sf_mem_start();
    void *end_of_heap = sf_mem_end();

    if(start_of_heap==end_of_heap)
    {
        init_freelist_heads();
        sf_mem_grow();
        end_of_heap = sf_mem_end();
        // setup prologue
        sf_block *prologue = start_of_heap+MIN_BLOCKSIZE-(2*sizeof(sf_header));
        set_blocksize(prologue, MIN_BLOCKSIZE, 1, 1);
        // setup epilogue
        sf_footer *epilogue = end_of_heap-sizeof(sf_footer);
        set_footer(epilogue, 0, 1, 0);
        // Setup wilderness block
        sf_block *wilderness_block = (void *)prologue + get_blocksize(prologue);
        set_blocksize(wilderness_block, PAGE_SZ-(2*get_blocksize(prologue)), 0, 1);

        add_to_freelist(wilderness_block);
    }

    sf_block *freelist_block = search_freelist(get_freelist_index(size), get_min_blocksize(size));
    while(freelist_block==NULL)
    {
        //Ask for mem grow

        sf_block *new_block  = sf_mem_grow();
        if(new_block ==NULL)
        {
            sf_errno = ENOMEM;
            return NULL;
        }
        new_block  = (void *)new_block -(2*get_footer_size());
        set_blocksize( new_block , PAGE_SZ, 0, 1);
        end_of_heap = sf_mem_end();
        // set new epilogue
        sf_footer *epilogue = end_of_heap-sizeof(sf_footer);
        set_footer(epilogue, 0, 1, 0);
        sf_block *prev = coalesce_prev(new_block );
        add_to_freelist(prev);
        freelist_block = search_freelist(get_freelist_index(size), get_min_blocksize(size));
    }

    //free block exists but it is too big
    if(get_blocksize(freelist_block)>=(get_min_blocksize(size)+MIN_BLOCKSIZE))
    {
        // Split it
        split_block(freelist_block, get_min_blocksize(size));
    }

    //Cannot split block because it is going create an useless splinter block
    // debug("block: %p payload: %p", freelist_block, freelist_block->body.payload);
    set_block_allc(freelist_block);
    return freelist_block->body.payload;

}

void sf_free(void *pp) {
    sf_block *block = pp - (2*sizeof(sf_header));
    if(is_valid_ptr(pp))
    {
        if(!get_prev_alloc(block))
            block = coalesce_prev(block);
        if(((get_next_block(block)->header)&ALLOC_MASK)==0)
            coalesce_next(block);
        set_block_free(block);
        add_to_freelist(block);
    }
    else
        abort();
    return;
}

void *sf_realloc(void *pp, size_t rsize)
{

    sf_block *block = pp - (2*sizeof(sf_header));

    if(!is_valid_ptr(pp))
        abort();
    if(rsize==0)
        sf_free(pp);

    size_t current_size = get_blocksize(block);

    if(rsize>current_size)
    {
        void *new_payload = sf_malloc(rsize);
        if(new_payload==NULL) return NULL;
        memcpy(new_payload, pp, current_size-(2*sizeof(sf_header)));
        sf_free(pp);
        return new_payload;
    }
    if(rsize<current_size)
    {
        if(current_size>=(get_min_blocksize(rsize)+MIN_BLOCKSIZE))
        {
            // Split it
            split_block(block, get_min_blocksize(rsize));
            sf_block *free_block = get_next_block(block);
            sf_block *after_free = get_next_block(free_block);
            if(((after_free->header)&ALLOC_MASK)==0)
                coalesce_next(free_block);
            remove_from_freelist(free_block);
            add_to_freelist(free_block);
            return block->body.payload;
        }
        else
        {
            return pp;
        }
    }

    return pp;
}

void *sf_memalign(size_t size, size_t align) {

    int set_bits = 0;
    for (int i = 0; i < sizeof(align)*8; ++i)
    {
        if(((align>>i)&1)==1)
            ++set_bits;
    }
    if(set_bits!=1||align<MIN_BLOCKSIZE)
    {
        sf_errno = EINVAL;
        return NULL;
    }

    size_t total_size = size+align+MIN_BLOCKSIZE+get_header_size();
    void *pp = sf_malloc(total_size);
    // debug("pp: %p", pp);
    sf_block *first_block = pp - (2*sizeof(sf_header));
    sf_block *second_block;
    sf_block *third_block;
    sf_block *fourth_block;
    size_t initial_size = get_blocksize(first_block);
    // debug("block size: %ld",initial_size);

    if((uintptr_t)pp%align==0)
    {
        second_block = first_block;
        // return first_block->body.payload;
    }
    else
    {
        pp = pp +  align - (uintptr_t)pp%align;
        // debug("new pp: %p ", pp);
        second_block = pp - (2*sizeof(sf_header));
        // debug("leftover on top: %ld", (void *)second_block - (void *)first_block);
        set_blocksize(first_block, (void *)second_block - (void *)first_block, 0, get_prev_alloc(first_block));
        add_to_freelist(first_block);
        set_blocksize(second_block, initial_size-get_blocksize(first_block), 1, 0);

        // set_blocksize(block, size, (void *)second_block - (void *)block);
    }
    size_t unused_size = get_blocksize(second_block) - size;
    if(unused_size>=MIN_BLOCKSIZE)
    {
        split_block(second_block, get_min_blocksize(size));
    }

    third_block = get_next_block(second_block);
    fourth_block = get_next_block(third_block);
    if(((fourth_block->header)&ALLOC_MASK)==0)
        coalesce_next(third_block);
    remove_from_freelist(third_block);
    add_to_freelist(third_block);

    return second_block->body.payload;
}


/*************************
    Private methods(Not really)
*************************/

size_t get_min_blocksize(size_t size)
{
    size_t total = size + get_header_size();
    if(total<MIN_BLOCKSIZE) return MIN_BLOCKSIZE;
    total = ((total-1)|(MIN_BLOCKSIZE-1))+1;
    // debug("size: %ld size: %ld", size, total);
    return total;
}

size_t get_header_size()
{
    return sizeof(sf_header);
}

size_t get_footer_size()
{
    return sizeof(sf_footer);
}

size_t get_links_size()
{
    return 2*sizeof(sf_block *);
}

int get_freelist_index(size_t size)
{
    int fib[NUM_FREE_LISTS-1], i;

    fib[0]=1;
    fib[1]=2;
    for (i = 0; i < NUM_FREE_LISTS-2; ++i)
    {
        if(i>1)
            fib[i]=fib[i-1]+fib[i-2]; // calc fib only for 2 and higher but still check if size is within 1M and 2M
        if(size<=fib[i]*MIN_BLOCKSIZE) return i;
    }
    return i;
}

void add_to_freelist(sf_block *block)
{
    size_t blocksize = get_blocksize(block);
    sf_block *wilderness_sentinel = sf_free_list_heads+(NUM_FREE_LISTS-1);
    if(wilderness_sentinel->body.links.next==wilderness_sentinel)
    {
        wilderness_sentinel->body.links.next = block;
        wilderness_sentinel->body.links.prev = block;
        block->body.links.prev = wilderness_sentinel;
        block->body.links.next = wilderness_sentinel;
        return;
    }
    int freelist_index = get_freelist_index(blocksize);
    // debug("%d", freelist_index);
    sf_block *free_sentinel = sf_free_list_heads+(freelist_index);
    sf_block *next_block= free_sentinel->body.links.next;
    next_block->body.links.prev = block;
    free_sentinel->body.links.next = block;
    // free_sentinel->body.links.prev = block;
    block->body.links.prev = free_sentinel;
    block->body.links.next = next_block;
    return;
}

void remove_from_freelist(sf_block *block)
{
    sf_block *prev_block = block->body.links.prev;
    sf_block *next_block = block->body.links.next;
    prev_block->body.links.next = next_block;
    next_block->body.links.prev = prev_block;
}

size_t get_blocksize(sf_block* block)
{
    return block->header&BLOCK_SIZE_MASK;
}

size_t get_prev_blocksize(sf_block *block)
{
    return block->prev_footer&BLOCK_SIZE_MASK;
}


void init_freelist_heads()
{
    for (int i = 0; i < NUM_FREE_LISTS; ++i)
    {
        sf_free_list_heads[i].body.links.next = sf_free_list_heads+i;
        sf_free_list_heads[i].body.links.prev = sf_free_list_heads+i;
    }
}

void set_blocksize(sf_block *block, size_t size, int is_alloc, int is_prevalloc)
{
    set_header(&block->header, size, is_alloc, is_prevalloc);
    // debug("block: %p", block);
    sf_block *next_block = (void *)block+size;
    // debug("next_block: %p", next_block);
    set_footer(&next_block->prev_footer, size, is_alloc, is_prevalloc);
}

void set_header(sf_header *header, size_t size, int is_alloc, int is_prevalloc)
{
    *header=size;
    *header|=is_alloc;
    *header|=(is_prevalloc<<1);
    // debug("hd: %ld", *header);
}

void set_footer(sf_footer *footer, size_t size, int is_alloc, int is_prevalloc)
{
    // if(*footer<MIN_BLOCKSIZE) return;

   *footer = size;
   *footer|=is_alloc;
   *footer|=(is_prevalloc<<1);
}

// removes block from freelist?
void *search_freelist(int min_index, size_t min_blocksize)
{
    for (int i = min_index; i < NUM_FREE_LISTS; ++i)
    {
        sf_block *sentinel =  sf_free_list_heads+i;
        sf_block *curr_block = sentinel->body.links.next;
        while(curr_block!=sentinel)
        {
            sf_block *next = curr_block->body.links.next;
            if(get_blocksize(curr_block)>=min_blocksize)
            {
                sf_block *prev = curr_block->body.links.prev;
                next->body.links.prev = prev;
                prev->body.links.next = next;
                return curr_block;
            }
            curr_block = next;

        }
    }
    return NULL;
}

void split_block(sf_block *block, size_t reqd_blocksize)
{
    size_t remainder_blocksize = get_blocksize(block) - reqd_blocksize;
    set_blocksize(block, reqd_blocksize, 1,  get_prev_alloc(block));
    sf_block *free_block = get_next_block(block);
    set_blocksize(free_block, remainder_blocksize, 0, 1);
    add_to_freelist(free_block);
}

sf_block *get_next_block(sf_block *block)
{
    return (void *)block+get_blocksize(block);
}

sf_block *get_prev_block(sf_block *block)
{
    return (void *)block-get_prev_blocksize(block);
}

int get_prev_alloc(sf_block *block)
{
    sf_block *prev_block = get_prev_block(block);
    return (prev_block->header)&ALLOC_MASK;
}

void *coalesce_prev(sf_block *block)
{
    sf_block *prev_block = get_prev_block(block);
    remove_from_freelist(prev_block);
    size_t initial_size = get_blocksize(block) + get_blocksize(prev_block);
    set_blocksize(prev_block, initial_size, 0, get_prev_alloc(prev_block));//(prev_block->header)&PREV_ALLOC_MASK);
    return prev_block;
}

void coalesce_next(sf_block *block)
{
    sf_block *next_block = get_next_block(block);
    remove_from_freelist(next_block);
    size_t initial_size = get_blocksize(block) + get_blocksize(next_block);
    set_blocksize(block, initial_size, 0, get_prev_alloc(block));//(block->header)&PREV_ALLOC_MASK);
}

int is_valid_ptr(void *pp)
{

    sf_block *block = pp - (2*sizeof(sf_header));

    if(pp==NULL)
        return 0;

    if(((uintptr_t)pp)%MIN_BLOCKSIZE != 0)
        return 0;

    if((block->header&ALLOC_MASK)==0)
        return 0;

    sf_block *prologue = sf_mem_start() + MIN_BLOCKSIZE - sizeof(sf_footer);
    void *prologue_end = get_next_block(prologue);
    if((void *)block<prologue_end)
        return 0;

    void *block_footer = &(get_next_block(block)->prev_footer);
    void *epilogue = sf_mem_end()-sizeof(sf_footer);
    if(block_footer>=epilogue)
        return 0;

    return 1;
}

void set_block_free(sf_block *block)
{
    block->header&=~1;
    sf_block *next_block = get_next_block(block);
    next_block->prev_footer&=~1;
    next_block->header&=~2;
    sf_block *next_next_block = get_next_block(next_block);
    if(get_blocksize(next_next_block)!=0)
        next_next_block->prev_footer&=~2;
}

void set_block_allc(sf_block *block)
{
    block->header|=1;
    sf_block *next_block = get_next_block(block);
    next_block->prev_footer|=1;
    next_block->header|=2;
    sf_block *next_next_block = get_next_block(next_block);
    next_next_block->prev_footer|=2;
}
