#include <stdio.h>
#include <stdlib.h>

void lauf_panic(const char* msg)
{
    fprintf(stderr, "[lauf] panic: %s\n", msg);
    abort();
}

void* lauf_heap_alloc(size_t size, size_t alignment)
{
    return aligned_alloc(alignment, size);
}

void* lauf_heap_alloc_array(size_t count, size_t size, size_t alignment)
{
    // See lauf::round_to_multiple_of_alignment().
    size_t rounded_size = (size + alignment - 1) & ~(alignment - 1);
    return lauf_heap_alloc(count * rounded_size, alignment);
}

void lauf_heap_free(void* ptr)
{
    free(ptr);
}

