# Pool Arena

Welcome to the Pool Arena project! This project is a C language implementation
for learning purpose and usage for my RISCV processor. The goal of the project is to
create a memory allocator that manages a pool of memory and allows fast
allocation and deallocation of fixed-size blocks.

# Features

Fast allocation and deallocation of fixed-size blocks using a free list
Memory pool is managed as a single contiguous block to minimize fragmentation
Supports multiple memory pools, each with its own configuration
File Structure

The project consists of the following files:

pool_arena.c: Implementation of the pool arena functions
pool_arena.h: Header file containing the pool arena API and data structures
test.c: A test program that exercises the pool arena functions

# Dependencies

The project has no external dependencies.

# Building and Running

To build the project, run the following command:

```bash
make
```

To run the test program, use the following command:

```bash
./testsuite
```

# Contributing

As I continue to work on this project, I welcome any feedback or contributions
that can help me improve my understanding of C and my programming skills. If
you find a bug or have an idea for a new feature, please open an issue in the
repository. If you'd like to submit a fix or implement a new feature, please
open a pull request. Before submitting your changes, please make sure to run
the test program and ensure that it passes.

Thank you for checking out the Pool Arena project!
