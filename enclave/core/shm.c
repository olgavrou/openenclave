// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "shm.h"
#include <openenclave/bits/safemath.h>
#include <openenclave/internal/raise.h>
#include <openenclave/internal/thread.h>
#include <openenclave/internal/utils.h>
#include <string.h>

#define ALIGNMENT sizeof(uint64_t)

// the per-thread shared memory pool
__thread shared_memory_arena_t arena = {0};

// the global list of shared memory pools
shared_memory_arena_t* _arena_list = NULL;

static oe_spinlock_t _arena_list_lock = OE_SPINLOCK_INITIALIZER;

// default shared memory pool capacity is 1 mb
size_t capacity = 1024 * 1024;

size_t max_capacity = 1 << 30;

void* oe_reserve_arena(size_t capacity);
void oe_unreserve_arena(void* buffer);

bool oe_configure_arena_capacity(size_t cap)
{
    if (cap > max_capacity)
    {
        return false;
    }
    capacity = cap;
    return true;
}

void* oe_arena_malloc(size_t size)
{
    oe_result_t result = OE_UNEXPECTED;
    size_t total_size = 0;

    if (arena.buffer == NULL)
    {
        void* buffer = oe_reserve_arena(capacity);
        if (buffer == NULL)
            OE_RAISE(OE_OUT_OF_MEMORY);
        arena.buffer = (uint8_t*)buffer;
        arena.capacity = capacity;
        arena.used = 0;

        // add the newly created pool to the global list
        oe_spin_lock(&_arena_list_lock);
        arena.next = _arena_list;
        _arena_list = &arena;
        oe_spin_unlock(&_arena_list_lock);
    }

    // Round up to the nearest alignment size.
    total_size = oe_round_up_to_multiple(size, ALIGNMENT);

    // check for overflow
    OE_CHECK(total_size < size);

    // check for capacity
    size_t used_after;
    OE_CHECK(oe_safe_add_sizet(arena.used, total_size, &used_after));

    // Ok if the incoming malloc puts us below the capacity.
    if (used_after <= arena.capacity)
    {
        uint8_t* addr = arena.buffer + arena.used;
        arena.used = used_after;
        return addr;
    }
    else
        OE_RAISE(OE_OUT_OF_MEMORY);

done:
    return NULL;
}

void* oe_arena_calloc(size_t size)
{
    void* ptr = oe_arena_malloc(size);
    if (ptr != NULL)
    {
        memset(ptr, 0, size);
    }
    return ptr;
}

void oe_arena_clear()
{
    arena.used = 0;
}

// Free all shared memory pools in the global list
void oe_arena_destroy()
{
    shared_memory_arena_t* next = _arena_list;
    while (next != NULL)
    {
        oe_unreserve_arena(next->buffer);
        shared_memory_arena_t* current = next;
        next = next->next;
        memset(current, 0, sizeof(arena));
    }
    _arena_list = NULL;
}