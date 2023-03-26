// distributed under the mit license
// https://opensource.org/licenses/mit-license.php

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "unity.h"

#include "pool_arena.h"

#define NB_PT 16
#define ARENA_SIZE 16384


int reg_size = sizeof(void *);
void * arena;

void * blks_pt[NB_PT];
int blks_size[NB_PT];
int blks_sts[NB_PT];


void setUp(void) {
    // Pool arena size to manage
    int size = ARENA_SIZE;
    // Get first a space in system memory
    arena = malloc(size);
    // Initialize the blocks' pointer
    for (int i=0; i<NB_PT; i++) {
        blks_pt[i] = NULL;
		blks_sts[i] = 0;
	}
}


void tearDown(void) {
    // Release the pool arena
	memset(arena, 0, ARENA_SIZE);
    free(arena);
}


/*
 * Set of functions to stress out the data integrity while allocating
 * again and again in the pool arena
 */
void alloc_blk(int i, int size) {
	if (blks_sts[i] == 0) {
		blks_pt[i] = pool_malloc(size);
		if (blks_pt[i] != NULL)
			blks_sts[i] = 1;
		else 
			blks_sts[i] = 0;
	}
}

void alloc_blks(int size) {
	
	for (int i=0;i<NB_PT;i++) {
		alloc_blk(i, size);
	}
}

void fill_blk(int i, int size) {

	char * array;

	if (blks_sts[i]) {
		array = blks_pt[i];
		for (int j=0;j<size;j++)
			array[j] = i;
	}
}

void fill_blks(int size) {
	for (int i=0;i<NB_PT;i++) {
		fill_blk(i, size);
	}
}

void free_blk(int i) {
	if (blks_sts[i]) {
		int ret = pool_free(blks_pt[i]);
		TEST_ASSERT_EQUAL_INT(0, ret);
		blks_sts[i] = 0;
	}
}

void free_blks() {
	for (int i=0;i<NB_PT;i++) {
		free_blk(i);
	}
}

void check_blks(int size) {

	char * array;

	for (int i=0;i<NB_PT;i++) {
		if (blks_sts[i]) {
			array = blks_pt[i];
			printf("p:%p\n", (void*)array);
			for (int j=0;j<size;j++) {
				printf("i:%0x d:%0x\t", i, array[i]);
    			TEST_ASSERT_EQUAL_INT(1, array[j] == i);
			}
			printf("\n");
		}
	}
}

void print_blks(void) {
	void * end;
	printf("------------------------------------------------------------------------\n");
	printf("Allocated Blocks\n");
	printf("------------------------------------------------------------------------\n");
	for (int i=0;i<NB_PT;i++) {
		if (blks_pt[i] != NULL) {
			end = (char *)blks_pt[i] + pool_get_size(blks_pt[i]) - 1;
			printf("Addr: %p\t", blks_pt[i]);
			printf("End: %p\t", end);
			printf("Size: %d\t", pool_get_size(blks_pt[i]));
			printf("\n");
		}
	}
	printf("------------------------------------------------------------------------\n");
}


// Just try to create a pool arena
void test_pool_init(void) {
    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));
}


// Just try to create a pool arena but with a so small
// size a header can't fit into
void test_pool_init_too_small(void) {
    TEST_ASSERT_EQUAL_INT(-1, pool_init(arena, reg_size));
}


// Test to create a small chunk, smaller than a register
// register = 4 bytes for 32 bits arch
// register = 8 bytes for 64 bits arch
void test_micro_chunk(void) {

    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));
    blks_pt[0] = pool_malloc(reg_size-1);
    blks_pt[1] = pool_malloc(reg_size);
    TEST_ASSERT_NOT_NULL(blks_pt[0]);
    TEST_ASSERT_NOT_NULL(blks_pt[1]);
    TEST_ASSERT(blks_pt[0] > arena);
    TEST_ASSERT(blks_pt[1] > arena);
    TEST_ASSERT(blks_pt[1] > blks_pt[0]);
}


// Try to create a zero byte wide block
void test_zero_chunk(void) {

    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));
    TEST_ASSERT_NULL(pool_malloc(0));
}


// Test a chunk big as the arena
void test_giga_chunk_ok(void) {

    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));
    TEST_ASSERT_NOT_NULL(pool_malloc(ARENA_SIZE/2));
}


// Test a chunk big as the arena, spanning over the arena
void test_giga_chunk_ko(void) {

    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));
    TEST_ASSERT_NULL(pool_malloc(ARENA_SIZE));
}


// Test a complete cycle, alloc() then free()
void test_alloc_n_free(void) {

    int ret;
	int chunk_depth = reg_size*10;

    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));

    blks_pt[0] = pool_malloc(chunk_depth);
    TEST_ASSERT_NOT_NULL(blks_pt[0]);
    ret = pool_free(blks_pt[0]);
    TEST_ASSERT_EQUAL_INT(0, ret);
}


// Expect realloc is ok
void test_calloc(void) {

    int ret;
	int chunk_size = 10;
	int chunk_depth = reg_size*chunk_size;

    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));

	// alloc first
    blks_pt[0] = pool_calloc(chunk_depth);
    TEST_ASSERT_NOT_NULL(blks_pt[0]);

	// test data has been erased first
	int  *cmp = blks_pt[0];
	for (int i=0;i<chunk_size/reg_size; i++) {
		TEST_ASSERT_TRUE(*cmp==0);
		cmp++;
	}

	// free
    ret = pool_free(blks_pt[0]);
    TEST_ASSERT_EQUAL_INT(0, ret);
}


// Expect realloc is ok
void test_realloc_ok(void) {

    int ret;
	int chunk_size = 10;
	int chunk_depth = reg_size*chunk_size;

    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));
	// alloc first
    blks_pt[0] = pool_malloc(chunk_depth);
    TEST_ASSERT_NOT_NULL(blks_pt[0]);

	// realloc the block
    blks_pt[0] = pool_realloc(blks_pt[0], chunk_depth);
    TEST_ASSERT_NOT_NULL(blks_pt[0]);

	// free
    ret = pool_free(blks_pt[0]);
    TEST_ASSERT_EQUAL_INT(0, ret);
}


// Expect realloc is ko by reallocating a block too big
void test_realloc_ko(void) {

    int ret;
	int chunk_size = 10;
	int chunk_depth = reg_size*chunk_size;

    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));

	// alloc first
    blks_pt[0] = pool_malloc(chunk_depth);
    TEST_ASSERT_NOT_NULL(blks_pt[0]);

	// realloc the block
    TEST_ASSERT_NULL(pool_realloc(blks_pt[0], chunk_depth*1000));

	// free
    ret = pool_free(blks_pt[0]);
    TEST_ASSERT_EQUAL_INT(0, ret);
}



// Allocate blocks in the pool arena, fill them and checks the data
// integrity while freeing some blocks
void test_data_integrity(void) {

	int chunk_size;
	printf("arena: %p\n", arena);
    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));

	//-----------------------------------
	// Round 1: chunk size with 128 bytes
	//-----------------------------------

	chunk_size = 2048;
	// Create bunch of chunks in the pool arena
	alloc_blks(chunk_size);
	// Fill the chunk with data corresponding to their indexes
	fill_blks(chunk_size);
	// Check the blocks are correclty filled
	check_blks(chunk_size);
	// Display all chunks allocated across the arena
	print_blks();
    TEST_ASSERT_EQUAL_INT(0, pool_check());

	// Try to free some chunks to be sure it doesn't corrupt the arena
	free_blk(1);
	check_blks(chunk_size);
	free_blk(3);
	check_blks(chunk_size);
	free_blk(4);
	check_blks(chunk_size);
	free_blk(0);
	check_blks(chunk_size);
    TEST_ASSERT_EQUAL_INT(0, pool_check());

	// Restart again the allocation of freed blocks
	alloc_blks(chunk_size);
	fill_blks(chunk_size);
	check_blks(chunk_size);

	// Free all blocks
	free_blks();
    TEST_ASSERT_EQUAL_INT(0, pool_check());
	pool_log();
		
	/* memset(arena, 0, ARENA_SIZE); */

	// Round 2: chunk size 512 bytes
	chunk_size = 512;
	alloc_blks(chunk_size);
	fill_blks(chunk_size);
	check_blks(chunk_size);
    TEST_ASSERT_EQUAL_INT(0, pool_check());

	free_blk(0);
	check_blks(chunk_size);
	free_blk(3);
	check_blks(chunk_size);
	free_blk(5);
	check_blks(chunk_size);
	free_blk(1);
	check_blks(chunk_size);
	pool_log();
    TEST_ASSERT_EQUAL_INT(0, pool_check());

	// Free all blocks
	free_blks();
    TEST_ASSERT_EQUAL_INT(0, pool_check());
	memset(arena, 0, ARENA_SIZE);
		
	// Last round trip, blocks smaller than the minimum number of registers
	// to free a block
	chunk_size = 64;
	alloc_blks(chunk_size);
	fill_blks(chunk_size);
	check_blks(chunk_size);
    TEST_ASSERT_EQUAL_INT(0, pool_check());

	// Free all blocks
	free_blks();
    TEST_ASSERT_EQUAL_INT(0, pool_check());
}


void test_check(void) {

	int ret;

    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));
	pool_log();

	blks_pt[0] = pool_malloc(32);
	TEST_ASSERT_NOT_NULL(blks_pt[0]);
    TEST_ASSERT_EQUAL_INT(32, pool_get_size(blks_pt[0]));
    TEST_ASSERT_EQUAL_INT(0, pool_check());
	/* pool_log(); */

	blks_pt[1] = pool_malloc(256);
	TEST_ASSERT_NOT_NULL(blks_pt[1]);
    TEST_ASSERT_EQUAL_INT(256, pool_get_size(blks_pt[1]));
    TEST_ASSERT_EQUAL_INT(0, pool_check());
	/* pool_log(); */

	print_blks();

	ret = pool_free(blks_pt[0]);
    TEST_ASSERT_EQUAL_INT(0, ret);
	pool_log();
    TEST_ASSERT_EQUAL_INT(0, pool_check());

	ret = pool_free(blks_pt[1]);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(0, pool_check());
	/* pool_log(); */
}


int main(void) {

    UNITY_BEGIN();

    RUN_TEST(test_pool_init);
    RUN_TEST(test_pool_init_too_small);
    RUN_TEST(test_micro_chunk);
    RUN_TEST(test_zero_chunk);
    RUN_TEST(test_giga_chunk_ok);
    RUN_TEST(test_giga_chunk_ko);
    RUN_TEST(test_alloc_n_free);
    RUN_TEST(test_calloc);
    RUN_TEST(test_realloc_ok);
    RUN_TEST(test_realloc_ko);
    RUN_TEST(test_data_integrity);
    RUN_TEST(test_check);

    return UNITY_END();
}
