// distributed under the mit license
// https://opensource.org/licenses/mit-license.php

#ifndef MALLOC_INCLUDE
#define MALLOC_INCLUDE

// Called by the environment to setup the heap start address
// To call once when the system boots up or when creating
// a new pool arena
int heap_init(void * addr, int size);

// Pool arena allocator, to substitute to malloc
// Use first-fit, but could use best-fit
void * _malloc(int size);

// Pool arena deallocater, to substitute to free
int _free(void * addr);

#endif // MALLOC_INCLUDE

