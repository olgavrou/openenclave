// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef _OE_SHM_H
#define _OE_SHM_H

#include <openenclave/bits/types.h>

typedef struct _shared_memory_arena_t
{
    /* Buffer holding the shared memory pool */
    uint8_t* buffer;
    size_t capacity;
    size_t used;
    struct _shared_memory_arena_t* next;
} shared_memory_arena_t;

bool oe_configure_shm_capacity(size_t cap);

void* oe_arena_malloc(size_t size);

void* oe_arena_calloc(size_t size);

void oe_arena_clear();

void oe_arena_destroy();

#endif /* _OE_SHM_H */