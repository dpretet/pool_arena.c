// distributed under the mit license
// https://opensource.org/licenses/mit-license.php

#include <stdio.h>

// -----------------------------------------------------------------------------------------------
// Local declarations
// -----------------------------------------------------------------------------------------------

// The basic data structure describing a arena free space element
struct blk {
    // Size of the data payload
    int size;
    // Pointer to the previous block. 0 means not assigned
    struct blk * prv;
    // Pointer to the next block. 0 means not assigned
    struct blk * nxt;
};

typedef struct blk blk_t;

// Size of a memory element, 32 or 64 bits
const static int reg_size = sizeof(void *);
const static int log2_reg_size = (reg_size == 4) ? 2 : 3;
// Size of a block header
const static int header_size = 3 * reg_size;

// Current block of the arena
static blk_t * current;
// temporary struct used when forking/merging blocks
static blk_t * tmp_blk;
// Used to store the original block address before parsing the free space blocks
static void * tmp_pt;

// Used to track arena status during usage and check if no leaks occcur
static int pool_size;
static int pool_allocated;
static int pool_free_space;


// -----------------------------------------------------------------------------------------------
// Called by the environment to setup the arena start address
// To call once when the system boots up or when creating
// a new pool arena.
//
// Arguments:
//  - addr: address of the arena's first byte
//  - size: size in byte available for the arena
// Returns:
//  - -1 if size is too small to contain at least 1 byte, otherwise 0
// -----------------------------------------------------------------------------------------------
int pool_init(void * addr, int size) {

	// addr is not a valid memory address
	if (addr == NULL) {
		return -1;
	}

	// size is too small, can't even store a header
    if (size <= header_size) {
        return -1;
	}

    tmp_blk = 0;
    tmp_pt = 0;

	pool_size = size - reg_size;
    pool_free_space = size - reg_size;
    pool_allocated = 0;

    current = (blk_t *)addr;
    current->size = pool_free_space;
    current->prv = NULL;
    current->nxt = NULL;

    /* #ifdef POOL_ARENA_DEBUG */
    printf("Architecture/Library Setup:\n");
    printf("  - register size: %d bytes\n", reg_size);
    printf("  - header size: %d bytes\n", header_size);
    printf("  - pool size: %d bytes\n", size);
    printf("\n");

    printf("Init pool arena:\n");
    printf("  - addr: %p\n", addr);
    printf("  - addr: %p\n", (void *)current);
    printf("  - size: %d\n", current->size);
    printf("  - prv: %p\n", (void *)current->prv);
    printf("  - nxt: %p\n", (void *)current->nxt);
    printf("\n");
    /* #endif */

    return 0;
}


// -----------------------------------------------------------------------------------------------
// To round up a size to the next multiple of 32 or 64 bits size
//
// Argument:
//  - size: the number of bytes the block needs to own
// Returns:
//  - the number of bytes rounds up to the architcture width
// -----------------------------------------------------------------------------------------------
static inline int round_up(int * x) {
    return ((*x + 7) >> log2_reg_size) << log2_reg_size;
}

// -----------------------------------------------------------------------------------------------
// Allocates in the arena a buffer of _size_ bytes. Memory blocked reserved in memory are always
// boundary aligned with the hw architecture, so 4 bytes for 32 bits architecture, or 8 bytes
// for 64 bits architecture.
//
// Argument:
//  - size: the number of bytes the block needs to own
// Returns:
//  - the address of the buffer's first byte, otherwise -1 if failed
// -----------------------------------------------------------------------------------------------
void * pool_malloc(int size) {

    // Return -1 if failed to allocate a buffer
    if (current->size <= (size + header_size)) {
		#ifdef POOL_ARENA_DEBUG
        printf("ERROR: Failed to allocate the chunk\n");
		printf("  - current free space: %d\n", current->size);
		#endif
        return (void *)(-1);
    }

    // TODO: Parse the blocks if current is not usable

    int _size;
    void * ret;
    void * new_space;
    void * prv_pt;
    void * nxt_pt;
    int new_size;

    // Round up the size up to the arch width. Ensure the size is at minimum a register size and
    // a multiple of that register. So if use 64 bits arch, a 4 bytes allocation is round up
    // to 8 bytes, and 28 bytes is round up to 32 bytes, 46 to 48 bytes, 118 to 120 bytes.
    _size = round_up(&size);

    // Update monitoring
    pool_allocated += _size + reg_size;

    // Payload's address the application can use
    ret = (char *)current + reg_size;

    #ifdef POOL_ARENA_DEBUG
    printf("malloc():\n");
    printf("  - actual free space address: %p\n", (void *)current);
    printf("  - actual free space size: %d\n", current->size);
    printf("  - chunk size: %d\n", _size);
    printf("  - allocated addr = %p\n", ret);
    #endif

    // Store current's info before moving its head
    nxt_pt = current->nxt;
    prv_pt = current->prv;
    new_size = current->size - reg_size - _size;

    // Adjust free space of current block and update its meta-data
    new_space = (char *)current + reg_size + _size;
    current = new_space;

    if (new_size<0) {
		#ifdef POOL_ARENA_DEBUG
		printf("ERROR: computed a negative free space size\n");
		#endif
        current->size = 0;
        return (void *)(-1);
	} else {
        current->size = new_size;
	}

    #ifdef POOL_ARENA_DEBUG
    printf("  - new free space address: %p\n", (void *)current);
	printf("  - new free space size: %d\n", current->size);
    #endif

    current->prv = prv_pt;
    current->nxt = nxt_pt;

    // Update previous block to link current
    if (current->prv) {
        tmp_blk = prv_pt;
        tmp_blk->nxt = current;
    }
    // Update next block to link current, only if exists
    if (current->nxt) {
        tmp_blk = nxt_pt;
        tmp_blk->prv = current;
    }

    return ret;
}


// -----------------------------------------------------------------------------------------------
// Parses the free blocks to find the place to set the one under release
// Useful to update the linked list correctly and fast its parsing.
//
// Follows the different cases to handle:
//
// ┌───────┬────────────────────────────────────────────┐          
// │Block 0│          ~~~~~~~~ Free ~~~~~~~~~           │          
// └───────┴────────────────────────────────────────────┘          
// ┌────────────────────────────────────────────┬───────┐          
// │          ~~~~~~~~ Free ~~~~~~~~~           │Block 0│          
// └────────────────────────────────────────────┴───────┘          
// ┌───────────────┬───────┬───────────────────────────┐          
// │ ~~~ Free ~~~  │Block 0│  ~~~~~~~~ Free ~~~~~~~~   │          
// └───────────────┴───────┴───────────────────────────┘          
// ┌───────┬────────────────────────────────────┬───────┐          
// │Block 0│      ~~~~~~~~ Free ~~~~~~~~~       │Block 1│          
// └───────┴────────────────────────────────────┴───────┘          
// ┌───────┬───────┬────────────────────┬───────┬────────┬───────┬───────┐
// │Block 0│Block 1│   ~~~ Freee ~~~    │Block 2│~ Free ~│Block 3│Block 4│
// └───────┴───────┴────────────────────┴───────┴────────┴───────┴───────┘
// ┌────────┬───────┬───────┬────────────────────┬───────┬────────┐
// │~ Free ~│Block 0│Block 1│   ~~~ Freee ~~~    │Block 2│~ Free ~│
// └────────┴───────┴───────┴────────────────────┴───────┴────────┘
//
// Argument:
//  - addr: pointer to an address to release
// Returns:
//  - a pointer to the location where to place the block to release. The place to use can be on the
//    left or on the right of address passed
//
// -----------------------------------------------------------------------------------------------
static inline void * get_loc_to_free(void * addr) {

	// In case the free block is monolithic, just return its address
	if (current->prv == NULL && current->nxt == NULL)
		return (void *)(&current);

    // The current block of free space manipulated by the library
    tmp_pt = (blk_t *)(&current);
    tmp_blk = tmp_pt;

	// Location found to place the bloc under release
    void * loc = NULL;

    // The list is ordered by address, so we can divide the parsing to select
    // directly the right direction
    if (addr < tmp_pt) {
        while (1) {
			loc = (blk_t *)(&tmp_blk);
			// No more free space on smaller address range, so when
			// can place this block on left of the current tmp / current free space
			if (tmp_blk->prv == NULL) {
				break;
			}
			// Next free block has a smaller address, so we are place
			// between two blocks: free.prv < data block < tmp / currrent free space
			else if (addr > (void *)tmp_blk->prv) {
				break;
			}
			tmp_blk = tmp_blk->prv;
        }
    } else {
        while (1) {
			loc = (blk_t *)(&tmp_blk);
			// No more free space on higher address range, so when
			// can place this block on right of the current tmp / current free space
			if (tmp_blk->nxt == NULL) {
				break;
			}
			// Next free block has a higher address, so we are place
			// between two blocks: free.prv < data block < tmp / currrent free space
			else if (addr < (void *)tmp_blk->nxt) {
				break;
			}
			tmp_blk = tmp_blk->nxt;
        }
    }

    return loc;
}


// -----------------------------------------------------------------------------------------------
// Releases a block and make it available again for future use.
//
// Arguments:
//  - addr: the address of the data block
// Returns:
//  - 0 if block has been found (and so was a block), anything otherwise if failed
// -----------------------------------------------------------------------------------------------
int pool_free(void * addr) {

    // Get block info
    void * blk_pt = (char *)addr - reg_size;
    blk_t * blk = blk_pt;
    // Update pool arena statistics
    pool_free_space += blk->size;

	// Region is used to check if the block to release is adjacent to a free space
	void * region;

	// Free space zone to connect or merge with the block to release. Multiple
	// free blocks are suitable to connect, ensuring we'll parse fastly the linked list
    void * free_pt = get_loc_to_free(blk_pt);
    blk_t * free_blk = (blk_t *)free_pt;

	// 1. Check the block can be merged with the current free space selected
	if (blk_pt<free_pt) {
		// If block space matches free space start address, merge
		region = (char *)blk_pt + blk->size + reg_size;
		if (region == free_pt) {
			blk->prv = free_blk->prv;
			blk->nxt = free_blk->nxt;
			blk->size += free_blk->size + header_size;
			pool_free_space += reg_size;
           	// If a next block exists, connect it 
			if (blk->nxt != NULL) {
				tmp_blk = blk->nxt;
				tmp_blk->prv = blk_pt;
			}
			// If a previsou block exists, connect it 
			if (blk->prv != NULL) {
				tmp_blk = blk->prv;
				tmp_blk->nxt = blk_pt;
			}
		}
	} else {
		// if free space range macthes the block start address, merge
		// in this case, the free space becomes the block to release
		region = (char *)free_pt + free_blk->size + header_size;
		if (region == blk_pt) {
			free_blk->size += blk->size + reg_size;
			blk_pt = free_pt;
			blk = blk_pt;
			pool_free_space += reg_size;
		}
	}

    // 2. Try to merge with next block if exists
    if (blk->nxt != NULL) {
        // if next block is contiguous the one to free, merge them
        region = (char *)blk_pt + blk->size + reg_size;
        if (region == blk->nxt) {
            // extend block size with nxt size
            tmp_blk = blk->nxt;
            blk->size += tmp_blk->size + reg_size;
            // link nxt->nxt block with current block
            tmp_blk = tmp_blk->nxt;
            tmp_blk->prv = blk_pt;
			// Update pool's statistics
			pool_free_space += reg_size;
        }
    }

    // 3. Try to merge with previous block if exists
    if (blk->prv != NULL) {
        // if previous block is contiguous the one to free, merge them
        tmp_blk = blk->prv;
        region = (char *)blk->prv + tmp_blk->size + header_size;
        if (region==blk_pt) {
            // Update previous block by extending its size with blk (to free)
            tmp_blk->size += reg_size + blk->size;
            // Link blk-1 and blk+1 together
            tmp_blk->nxt = blk->nxt;
            // Current block's prv becomes the new current block
            blk = blk->prv;
            // blk+1's prv is now linked to orignal blk-1
            tmp_blk = blk->nxt;
            tmp_blk->prv = (void *)blk;
			// Update pool's statistics
			pool_free_space += reg_size;
        }
    }

    return 0;
}

int pool_check_free_space(int used) {

	// Size in bytes available in the pool
	int size = 0;
	// Number of free blocks available
	int nb_fb = 0;

	tmp_pt = current;
	tmp_blk = tmp_pt;

	printf("Parse prv(s)\n");
	while (1) {

		printf("Free bloc Info:\n");
		printf("	- addr: %p\n", tmp_pt);
		printf("	- free space: %d\n", tmp_blk->size);
		printf("	- prv: %p\n", (void *)tmp_blk->prv);
		printf("	- nxt: %p\n", (void *)tmp_blk->nxt);

		size += tmp_blk->size;
		nb_fb += 1;
		if (tmp_blk->prv != NULL) {
			tmp_blk = tmp_blk->prv;
		} else {
			break;
		}
	}

	// Go back current for nxt parsing, we remove the size and decrement
	// block count while we will count current again
	tmp_pt = current;
	tmp_blk = tmp_pt;

	size -= tmp_blk->size;
	nb_fb -= 1;	

	printf("Parse nxt(s)\n");
	while (1) {

		printf("Free bloc Info:\n");
		printf("	- addr: %p\n", tmp_pt);
		printf("	- free space: %d\n", tmp_blk->size);
		printf("	- prv: %p\n", (void *)tmp_blk->prv);
		printf("	- nxt: %p\n", (void *)tmp_blk->nxt);

		size += tmp_blk->size;
		nb_fb += 1;	

		if (tmp_blk->nxt != NULL) {
			tmp_blk = tmp_blk->nxt;
		} else {
			break;
		}
	}
	
	size += used;

	if (size != pool_size) {
		/* #ifdef POOL_ARENA_DEBUG */
		printf("ERROR: Free space check failed:\n");
		printf("	- Nb of block: %d\n", nb_fb);
		printf("	- Total free space: %d\n", size);
		printf("	- Expected free space: %d\n", pool_size);
		/* #endif */
		return 1;
	}

	return 0;
}
