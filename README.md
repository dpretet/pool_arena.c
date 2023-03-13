# Pool Arena

Welcome to the Pool Arena project! This project is a C language implementation
for learning purpose and usage for my [RISCV processor](https://github.com/dpretet/friscv).
The goal of the project is to create a memory allocator that manages a pool of
memory and allows fast allocation and deallocation of fixed-size blocks.

# Features

A pool arena is a memory management technique that allows a program to
efficiently allocate and deallocate small chunks of memory from a large
pre-allocated "pool" of memory.

In a pool arena, memory is divided into fixed-size blocks, and each block can
be allocated and deallocated independently. When a block is freed, it is not
immediately released back to the operating system, but rather added to a list
of available blocks that can be reused for future allocations. This reduces the
overhead of memory allocation and deallocation, since it avoids the need to
request new memory from the operating system each time a new block is needed.

This pool arena API is compliant with regular malloc/realloc/calloc/free calls.
It could be used in a regular C program, or as a malloc replacement in embedded 
system, compatible with 32 or 64 bits platform.

The project consists of the following files:

- `src/pool_arena.c`: Implementation of the pool arena functions
- `src/pool_arena.h`: Header file containing the pool arena API and data structures
- `test/test_pool_arena.c`: A test program that exercises the pool arena functions with [Unity](https://github.com/ThrowTheSwitch/Unity)

# Dependencies

The project has no external dependencies.

# Build and Run

To build the project, run the following command:

```bash
make
```

To run the test program, use the following command:

```bash
./test/testsuite
```

# License

Distributed under MIT license
