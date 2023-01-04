// distributed under the mit license
// https://opensource.org/licenses/mit-license.php

/*

# OVERVIEW

A basic malloc/calloc/free implementation, targeting first RISCV 32 bits platform but that could be
used to other targets and needs like a pool arena to avoid calls to a kernel. The memory pool is
divided into chunks, each storing meta-data to store its size and the links to the previous and next
free blocks.

Blocks can be allocated with malloc() (or calloc()=malloc()+erase()). Blocks are released with free(),
merging the block with the previous/next ones if also available (can be erase() on free()).

   ┌───────┬───────┬────────────────────┬───────┬───────┬───────┬─────────────────────────────┐
   │Block 0│Block 1│     Free Space     │Block 3│ Free  │Block 4│   ......................    │
   └───────┴───────┴────────────────────┴───────┴───────┴───────┴─────────────────────────────┘

malloc()/free() combo only manipulates the free space, forking it to allocate a chunk, merging it to
release a chunk.


# ALGORITHM

An allocated block is composed first by a register of 32 or 64 bits wide based on the CPU arch,
describing the size in bytes, then by the payload.

A free block is composed first by a register describing the block size, then by two pointers. So
the free space is described by a linked list to parse and manipulate it fastly.


                             Free block                  In-use block

                         ┌────────────────┐           ┌────────────────┐
                         │      Size      │           │      Size      │
                         ├────────────────┤           ├────────────────┤
                         │ Next Block Ptr │           │                │
                         ├────────────────┤           │                │
                         │Prev. Block Ptr │           │    Payload     │
                         ├────────────────┤           │                │
                         │                │           │                │
                         │    ........    │           │                │
                         │                │           │                │
                         └────────────────┘           └────────────────┘


Free blocks store the links to the next and previous free block, so, malloc() and free() can use
these links to parse the pool, allocate a chunk or release it. On allocation, if no region is able
to store the requested payload size, returns -1.

## malloc()

Parse the free space to find a chunk. Check if current block can contain the requested payload:

1. If yes, fork the block:
     - If not consume the whole space, set the first word as the register as the size allocated.
       The new allocated block is placed on the tail of the free space. Adjust the size of the
       free block.
     - If yes, erase the prv/nxt pointers and return the payload address
     - Erase payload if use secure alloc()
     - Return pyaload address

2. If no, parse the linked list
     - Store the original address
     - Move thru the previous block and check if some can be used
     - If no block is big enough, move to the original address then parse the next blocks
     - If a block is found, apply (1)
     - If not, return -1

## free()

If next block is contiguous, merge it.
  - Erase next block info if existing
  - Add current block size with next block size

If previous block is contiguous, merge it:
  - update the size of the previous block by adding the chunk size

TODO:
- support small array size, like one int (4 bytes), so nxt/prv can't fit into.
  needs to allocate at least 3 * registers
- compute the size with address, so gather small blocks if smaller than 4 registers
- how to handle release of blocks, fragmented, small space still available between 2 blocks
- free can erase a block (secure erase)
- add calloc(), malloc() + erase()
- always align on architecture boundary
- use a binary tree to search for fastly free space on allocation?
    - needs parent reg + 2 children regs, 1 more than linked list
    - could speed very much parsing of the free blocks
    - can ease defragmentation process (if needed)
- function to defragment the heap?
- function to check the heap usage
- monitor / record used vs free space on alloc/free calls
- store biggest free space address
- store smallest free space address

*/

#include <stdio.h>

// The basic data structure describing a heap free space element
struct blk {
    // Size of the data payload
    int size;
    // Pointer to the previous block. 0 means not assigned
    void * prv;
    // Pointer to the next block. 0 means not assigned
    void * nxt;
};

typedef struct blk blk_t;

// Size of a memory element
const static int reg_size = sizeof(void *);
// Size of a block header
const static int header_size = 3 * reg_size;

// Current block of the heap
static blk_t * current;
static blk_t * tmp;

// Used to store the original block address before parsing the free space blocks
static int org;
// Used to remember the biggest and smallest blocks' address, used to parse the heap list
static void * biggest;
static void * smallest;

// Used to track heap status during usage and check if no leaks occcur
static int heap_size;
static int heap_allocated;
static int heap_free;


// -----------------------------------------------------------------------------------------------
// Allocate in the heap a buffer of _size_ bytes. Memory blocked reserved in memory are always
// boundary aligned with the hw architecture, so 4 bytes for 32 bits architecture, or 8 bytes
// for 64 bits architecture.
//
// Argument:
//  - size: the number of bytes the block needs to own
// Returns:
//  - the address of the buffer's first byte, otherwise -1 if failed
// -----------------------------------------------------------------------------------------------
void * _malloc(int size) {

    // Read the current block info to check if can store the payload
    //  - if not, parse the blocks until finding one
    //  - if don't find a block, return -1
    // Fork the space and create a new block
    //  - in original block, store in next the new block address in nxt
    //  - in new block, store the original block address in prv
    // Move curr pointer after the new block
    //  - init the size to 0
    // To check when manipulating the block:
    //  - prv = 0, meaning we are at the beginning of the pool
    //  - nxt = 0, meaning we reach the end of the pool
    //  - prv = -1, meaning previous block is allocated (pool beginning and first block allocated)
    //  - nxt = -1, meaning next block is allocated (pool end and last block is allocated)

    // Return -1 if failed to allocate a buffer
    if (current->size <= (size + header_size))
        return (void *)(-1);

    int _size;
    void * ret;
    void * prv_pt;
    void * nxt_pt;
    int new_size;

    // Ensure the requested size is not lower than a register
    // So stay boundary align and avoid misalignment
    // TODO: round up next boundary, i.e. 24 bytes >> 32 bytes
    _size = size;
    if (size<reg_size)
        _size = reg_size;

    // Payload address the application can use
    ret = (char *)current + header_size;
    printf("Malloc:\n");
    printf("  - current: %p\n", current);
    printf("  - header_size: %x\n", header_size);
    printf("  - blk size: %x\n", _size);
    printf("  - pt = %p\n", ret);

    // Update monitoring
    heap_allocated += _size;

    // Store current info before moving its head
    nxt_pt = current->nxt;
    prv_pt = current->prv;
    new_size = current->size - header_size - _size;

    // Adjust free space of current block and update its meta-data
    current += header_size + size;
    current->size = new_size;
    current->prv = prv_pt;
    current->nxt = nxt_pt;

    // Update previous block to link current
    if (current->prv) {
        tmp = prv_pt;
        tmp->nxt = current;
    }
    // Update next block to link current, only if exists
    if (current->nxt) {
        tmp = nxt_pt;
        tmp->prv = current;
    }

    return ret;
}

// -----------------------------------------------------------------------------------------------
// Free a block and make it available again for future use.
//
// Arguments:
//  - addr: the address of the data block
// Returns:
//  - 0 if block has been found (and so was a block), anything otherwise if failed
// -----------------------------------------------------------------------------------------------
int _free(/*void * addr*/) {

    // blk_t * blk;
    // blk = (char *)addr - header_size;

    return 0;
}

// -----------------------------------------------------------------------------------------------
// Called by the environment to setup the heap start address
// To call once when the system boots up or when creating
// a new pool arena.
//
// Arguments:
//  - addr: address of the heap's first byte
//  - size: size in byte available for the heap
// Returns:
//  - -1 if size is too small to contain at least 1 byte, otherwise 0
// -----------------------------------------------------------------------------------------------
int heap_init(void * addr, int size) {

    smallest = 0;
    biggest = 0;
    org = 0;

    heap_size = size;
    heap_free = size;
    heap_allocated = 0;

    current = addr;
    current->size = size;
    current->prv = 0;
    current->nxt = 0;

    if (size <= header_size)
        return -1;

    printf("Architecture/Library Setup:\n");
    printf("  - reg_size: %d bytes\n", reg_size);
    printf("  - header_size: %d bytes\n", header_size);

    printf("Init pool arena:\n");
    printf("  - addr: %p\n", addr);
    printf("  - size: %d\n", current->size);
    printf("  - prv: %p\n", current->prv);
    printf("  - nxt: %p\n", current->nxt);
    return 0;
}
