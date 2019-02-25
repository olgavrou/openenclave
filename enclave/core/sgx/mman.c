// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "../../../common/mman.c"
#include <openenclave/internal/globals.h>
#include <openenclave/internal/thread.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

OE_EXTERNC_BEGIN

oe_mman_t __oe_mman;

/* Initialize the __oe_mman object on the first call */
static void _init(void)
{
    static oe_spinlock_t _lock = OE_SPINLOCK_INITIALIZER;

    oe_spin_lock(&_lock);

    if (__oe_mman.initialized == false)
    {
        oe_mman_init(
            &__oe_mman, (uintptr_t)__oe_get_heap_base(), __oe_get_heap_size());
    }

    oe_spin_unlock(&_lock);
}

int oe_brk(uintptr_t addr)
{
    if (__oe_mman.initialized == false)
        _init();

    oe_result_t result = oe_mman_brk(&__oe_mman, addr);

    return result == OE_OK ? 0 : -1;
}

/*
** Enclave implementation of the standard Unix sbrk() system call.
**
** This function provides an enclave equivalent to the sbrk() system call.
** It increments the current end of the heap by **increment** bytes. Calling
** oe_sbrk() with an increment of 0, returns the current end of the heap.
**
** @param increment Number of bytes to increment the heap end by.
**
** @returns The old end of the heap (before the increment) or (void*)-1 if
** there are less than **increment** bytes left on the heap.
**
*/
void* oe_sbrk(ptrdiff_t increment)
{
    void* ptr;

    if (__oe_mman.initialized == false)
        _init();

    ptr = oe_mman_sbrk(&__oe_mman, increment);

    /* Map null to expected sbrk() error return type */
    if (!ptr)
        return (void*)-1;

    return ptr;
}

void* oe_mmap(void* addr, size_t length, int prot, int flags)
{
    if (__oe_mman.initialized == false)
        _init();

    void* ptr = oe_mman_map(&__oe_mman, addr, length, prot, flags);

    return ptr ? ptr : (void*)-1;
}

void* oe_mremap(void* addr, size_t old_size, size_t new_size, int flags)
{
    if (__oe_mman.initialized == false)
        _init();

    void* ptr = oe_mman_mremap(&__oe_mman, addr, old_size, new_size, flags);

    return ptr ? ptr : (void*)-1;
}

int oe_munmap(void* address, size_t size)
{
    if (__oe_mman.initialized == false)
        _init();

    oe_result_t result = oe_mman_munmap(&__oe_mman, address, size);

    return result == OE_OK ? 0 : -1;
}

OE_EXTERNC_END
