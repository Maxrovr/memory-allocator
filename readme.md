# Usage
- You will be able to use void `*sf_malloc(size_t size)`, `void* sf_realloc(void *ptr, size_t size)`, `void sf_free(void *ptr)`, `void *sf_memalign(size_t size, size_t align)` to manipulate memory requirements. 
- You can start by just testing out various scenarios on the `sfmm_test.c` file or directly calling these functions from the `main.c` file.


# Allocator Features
- Free lists segregated by size class, using first-fit policy within each size class.
- Immediate coalescing of blocks on free with adjacent free blocks.
- Boundary tags to support efficient coalescing.
- Block splitting without creating splinters.
- Allocated blocks with client-specified alignment.
- Free lists maintained using **last in first out (LIFO)** discipline.

## Free List Management Policy

Free blocks are stored in a fixed array of `NUM_FREE_LISTS` free lists,
segregated by size class.
Each individual free list will be organized as a **circular, doubly linked list**.
The size classes are based on a Fibonacci sequence (1, 2, 3, 5, 8, 13, ...),
according to the following scheme:
The first free list (at index 0) holds blocks of the minimum size `M`
(where `M = 64` for this assignment).
The second list (at index 1) holds blocks of size `2M`.
The third list (at index 2) holds blocks of size `3M`.
The fourth list holds blocks whose size is in the interval `(3M, 5M]`.
The fifth list holds blocks whose size is in the interval `(5M, 8M]`,
and so on.
This pattern continues up to the interval `(21M, 34M]`,
and then the second-to-last list (at index `NUM_FREE_LISTS-2`; *i.e.* 8)
holds blocks of size greater than `34M`.
The last list (at index `NUM_FREE_LISTS-1`; *i.e.* 9) is only used to contain
the so-called "wilderness block", which is the free block at the end of the heap
that will be extended when the heap is grown.
Allocation requests will be satisfied by searching the free lists in increasing
order of size class; considering the last list with the wilderness block only
if no suitable block has been found in the earlier lists.

> This policy means that the wilderness block will effectively be treated
> as larger than any other free block, regardless of its actual size.
> This "wilderness preservation" heuristic tends to prevent the heap from growing
> unnecessarily.

# Getting Started


## Directory Structure

<pre>
.
├── include
│   ├── debug.h
│   └── sfmm.h
├── lib
│   └── sfutil.o
├── Makefile
├── src
│   ├── main.c
│   └── sfmm.c
└── tests
    └── sfmm_tests.c
</pre>

The provided `Makefile` creates object files from the `.c` files in the `src`
directory, places the object files inside the `build` directory, and then links
the object files together, including `lib/sfutil.o`, to make executables that
are stored to the `bin` directory.

**Note:** `make clean` will not delete `sfutil.o` or the `lib` folder, but it
will delete all other contained `.o` files.

The program in `src/main.c` contains a basic example of using the initialization
and allocation functions together. Running `make` will create a `sfmm`
executable in the `bin` directory. This can be run using the command `bin/sfmm`.


# Allocation Functions

```
/*
 * It acquires uninitialized memory that
 * is aligned and padded properly for the underlying system.
 *
 * @param size The number of bytes requested to be allocated.
 *
 * @return If size is 0, then NULL is returned without setting sf_errno.
 * If size is nonzero, then if the allocation is successful a pointer to a valid region of
 * memory of the requested size is returned.  If the allocation is not successful, then
 * NULL is returned and sf_errno is set to ENOMEM.
 */
void *sf_malloc(size_t size);

/*
 * Resizes the memory pointed to by ptr to size bytes.
 *
 * @param ptr Address of the memory region to resize.
 * @param size The minimum size to resize the memory to.
 *
 * @return If successful, the pointer to a valid region of memory is
 * returned, else NULL is returned and sf_errno is set appropriately.
 *
 *   If sf_realloc is called with an invalid pointer sf_errno should be set to EINVAL.
 *   If there is no memory available sf_realloc should set sf_errno to ENOMEM.
 *
 * If sf_realloc is called with a valid pointer and a size of 0 it should free
 * the allocated block and return NULL without setting sf_errno.
 */
void* sf_realloc(void *ptr, size_t size);

/*
 * Marks a dynamically allocated region as no longer in use.
 * Adds the newly freed block to the free list.
 *
 * @param ptr Address of memory returned by the function sf_malloc.
 *
 * If ptr is invalid, the function calls abort() to exit the program.
 */
void sf_free(void *ptr);

/*
 * Allocates a block of memory with a specified alignment.
 *
 * @param align The alignment required of the returned pointer.
 * @param size The number of bytes requested to be allocated.
 *
 * @return If align is not a power of two or is less than the minimum block size,
 * then NULL is returned and sf_errno is set to EINVAL.
 * If size is 0, then NULL is returned without setting sf_errno.
 * Otherwise, if the allocation is successful a pointer to a valid region of memory
 * of the requested size and with the requested alignment is returned.
 * If the allocation is not successful, then NULL is returned and sf_errno is set
 * to ENOMEM.
 */
void *sf_memalign(size_t size, size_t align);
```
# Unit Testing

The Criterion framework alongside the provided helper functions can be used to
ensure the allocator works exactly as specified.

In the `tests/sfmm_tests.c` file, there are ten unit test examples. These tests
check for the correctness of `sf_malloc`, `sf_realloc`, and `sf_free`.
I have provided some basic assertions, but by no means are they exhaustive. There are certainly a few cases that this implemntation fails at.

## Compiling and Running Tests

When you compile your program with `make`, a `sfmm_tests` executable will be
created in the `bin` folder alongside the `main` executable. This can be run
with `bin/sfmm_tests`. To obtain more information about each test run, you can
use the verbose print option: `bin/sfmm_tests --verbose=0`.

For System Fundamentals II - Memory Allocator
