// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <openenclave/host.h>
#include <openenclave/internal/calls.h>
#include <openenclave/internal/raise.h>
#include <openenclave/internal/switchless.h>
#include "../calls.h"
#include "../hostthread.h"
#include "../ocalls.h"
#include "enclave.h"

/*
** The thread function that handles switchless ocalls
**
*/
static void* _switchless_ocall_worker(void* arg)
{
    host_worker_thread_context_t* context = (host_worker_thread_context_t*)arg;

    while (!context->is_stopping)
    {
        if (context->call_arg != NULL)
        {
            oe_handle_call_host_function(
                (uint64_t)context->call_arg, context->enclave);
            context->call_arg = NULL;
        }
    }
    return NULL;
}

static void oe_stop_worker_threads(oe_switchless_call_manager_t* manager)
{
    for (size_t i = 0; i < manager->num_host_workers; i++)
    {
        manager->host_worker_contexts[i].is_stopping = true;
    }

    for (size_t i = 0; i < manager->num_host_workers; i++)
    {
        if (manager->host_worker_threads[i] != (oe_thread_t)NULL)
            oe_thread_join(manager->host_worker_threads[i]);
    }
}

oe_result_t oe_start_switchless_manager(
    oe_enclave_t* enclave,
    size_t num_host_workers)
{
    oe_result_t result = OE_UNEXPECTED;
    uint64_t result_out = 0;

    if (num_host_workers < 1 || enclave == NULL)
        OE_RAISE(OE_INVALID_PARAMETER);

    if (enclave->switchless_manager != NULL)
        OE_RAISE(OE_UNEXPECTED);

    // Limit the number of host workers to the number of thread bindings
    // because the maximum parallelism is dictated by the latter for
    // synchronous ocalls. We may need to revisit this for asynchronous
    // calls later.
    if (num_host_workers > enclave->num_bindings)
        num_host_workers = (uint32_t)enclave->num_bindings;

    // Allocate memory for the manager and its arrays
    oe_switchless_call_manager_t* manager =
        calloc(1, sizeof(oe_switchless_call_manager_t));
    if (manager == NULL)
        OE_RAISE(OE_OUT_OF_MEMORY);

    host_worker_thread_context_t* contexts =
        calloc(num_host_workers, sizeof(host_worker_thread_context_t));
    if (contexts == NULL)
        OE_RAISE(OE_OUT_OF_MEMORY);

    oe_thread_t* threads = calloc(num_host_workers, sizeof(oe_thread_t));
    if (threads == NULL)
        OE_RAISE(OE_OUT_OF_MEMORY);

    manager->num_host_workers = num_host_workers;
    manager->host_worker_contexts = contexts;
    manager->host_worker_threads = threads;

    // Start the worker threads, and assign each one a private context.
    for (size_t i = 0; i < num_host_workers; i++)
    {
        manager->host_worker_contexts[i].enclave = enclave;
        if (oe_thread_create(
                &manager->host_worker_threads[i],
                _switchless_ocall_worker,
                &manager->host_worker_contexts[i]) != 0)
        {
            oe_stop_worker_threads(manager);
            OE_RAISE(OE_THREAD_CREATE_ERROR);
        }
    }

    // Each enclave has at most one switchless manager.
    enclave->switchless_manager = manager;

    // Inform the enclave about the switchless manager through an ECALL
    OE_CHECK(oe_ecall(
        enclave, OE_ECALL_INIT_SWITCHLESS, (uint64_t)manager, &result_out));
    OE_CHECK((oe_result_t)result_out);

    result = OE_OK;

done:
    return result;
}

void oe_stop_switchless_manager(oe_enclave_t* enclave)
{
    if (enclave != NULL && enclave->switchless_manager != NULL)
    {
        oe_stop_worker_threads(enclave->switchless_manager);
    }
}