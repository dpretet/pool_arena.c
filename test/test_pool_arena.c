// distributed under the mit license
// https://opensource.org/licenses/mit-license.php

#include <stdlib.h>
#include <stdio.h>
#include "unity.h"

#include "pool_arena.h"

#define NB_PT 16
#define ARENA_SIZE 1024
void * arena;
void * blks[NB_PT];
int reg_size = sizeof(void *);


void setUp(void) {
    // Pool arena size to manage
    int size = ARENA_SIZE * sizeof(uint32_t);
    // Get first a space in system memory
    arena = malloc(size);
    // Initialize the blocks' pointer
    for (int i=0; i<NB_PT; i++)
        blks[i] = NULL;
}


void tearDown(void) {
    // Release the pool arena
    free(arena);
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
    blks[0] = pool_malloc(reg_size-1);
    blks[1] = pool_malloc(reg_size);
    TEST_ASSERT_NOT_NULL(blks[0]);
    TEST_ASSERT_NOT_NULL(blks[1]);
    TEST_ASSERT(blks[0] > arena);
    TEST_ASSERT(blks[1] > arena);
    TEST_ASSERT(blks[1] > blks[0]);
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

    blks[0] = pool_malloc(chunk_depth);
    TEST_ASSERT_NOT_NULL(blks[0]);
    ret = pool_free(blks[0]);
    TEST_ASSERT_EQUAL_INT(0, ret);
}


// Expect realloc is ok
void test_calloc(void) {

    int ret;
	int chunk_size = 10;
	int chunk_depth = reg_size*chunk_size;

    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));

	// alloc first
    blks[0] = pool_calloc(chunk_depth);
    TEST_ASSERT_NOT_NULL(blks[0]);

	int  *cmp = blks[0];
	for (int i=0;i<chunk_size/reg_size; i++) {
		TEST_ASSERT_TRUE(*cmp==0);
		cmp++;
	}

	// free
    ret = pool_free(blks[0]);
    TEST_ASSERT_EQUAL_INT(0, ret);
}


// Expect realloc is ok
void test_realloc_ok(void) {

    int ret;
	int chunk_size = 10;
	int chunk_depth = reg_size*chunk_size;

    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));
	// alloc first
    blks[0] = pool_malloc(chunk_depth);
    TEST_ASSERT_NOT_NULL(blks[0]);

	// realloc the block
    blks[0] = pool_realloc(blks[0], chunk_depth);
    TEST_ASSERT_NOT_NULL(blks[0]);

	// free
    ret = pool_free(blks[0]);
    TEST_ASSERT_EQUAL_INT(0, ret);
}


void test_realloc_ko(void) {

    int ret;
	int chunk_size = 10;
	int chunk_depth = reg_size*chunk_size;

    TEST_ASSERT_EQUAL_INT(0, pool_init(arena, ARENA_SIZE));

	// alloc first
    blks[0] = pool_malloc(chunk_depth);
    TEST_ASSERT_NOT_NULL(blks[0]);

	// realloc the block
    TEST_ASSERT_NULL(pool_realloc(blks[0], chunk_depth*1000));

	// free
    ret = pool_free(blks[0]);
    TEST_ASSERT_EQUAL_INT(0, ret);
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

    return UNITY_END();
}
