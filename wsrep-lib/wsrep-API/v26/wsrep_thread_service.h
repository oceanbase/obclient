/*
 * Copyright (C) 2019 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-API.
 *
 * Wsrep-API is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-API is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-API.  If not, see <https://www.gnu.org/licenses/>.
 */

/** @file wsrep_thread_service.h
 *
 * Service interface for threads, mutexes and condition variables.
 * The application which may provide callbacks to routines which will
 * be used to manage lifetime and use threads and sycnronization primitives.
 *
 * The type tags and interface methods are loosely modeled after POSIX
 * threading interface.
 *
 * The application must either none or all of the callbacks defined in
 * wsrep_thread_service structure which is defined below.
 *
 * The error codes returned by the callbacks are generally assumed to
 * the system error numbers defined in errno.h unless stated otherwise.
 *
 * The provider must implement and export the following functions
 * to provide initialization point for the service implementation:
 *
 * Version 1:
 *   int wsrep_init_thread_service_v1(wsrep_thread_service_v1_t*)
 *
 * The application defined implementation must be initialized before
 * calling the initialization function and must stay functional
 * until the provider is unloaded.
 */

#ifndef WSREP_THREAD_SERVICE_H
#define WSREP_THREAD_SERVICE_H

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /* Forward declarations */
    struct timespec;
    struct sched_param;

    /** Thread type tags */
    typedef struct wsrep_thread_key_st wsrep_thread_key_t;
    typedef struct wsrep_thread_st wsrep_thread_t;
    /** Mutex type tags */
    typedef struct wsrep_mutex_key_st wsrep_mutex_key_t;
    typedef struct wsrep_mutex_st wsrep_mutex_t;
    /** Condition variable tags */
    typedef struct wsrep_cond_key_st wsrep_cond_key_t;
    typedef struct wsrep_cond_st wsrep_cond_t;

    /**
     * Create key for a thread with a name. This key object will be passed
     * to thread creation and destrunction notification callbacks.
     *
     * @param name Name of the thread.
     */
    typedef const wsrep_thread_key_t* (*wsrep_thread_key_create_cb_t)(
        const char* name);

    /**
     * Create a new thread.
     *
     * @param[out] thread Newly allocated thread.
     * @param key Key created by wsrep_thread_key_create_cb_t
     * @param start_fn Pointer to start routine
     * @param arg Argument for start_fn
     *
     * @return Zero in case of success, non-zero error code in case of failure.
     */
    typedef int (*wsrep_thread_create_cb_t)(const wsrep_thread_key_t* key,
                                            wsrep_thread_t** thread,
                                            void* (*start_fn)(void*),
                                            void* arg);

    /**
     * Detach a thread.
     *
     * @param thread Thread to be detached.
     *
     * @return Zero in case of error, non-zero error code in case of failure.
     */
    typedef int (*wsrep_thread_detach_cb_t)(wsrep_thread_t* thread);

    /**
     * Compare two threads for equality.
     *
     * @params t1, t2 Threads to be compared.
     *
     * @return Non-zero value if threads are equal, zero otherwise.
     */
    typedef int (*wsrep_thread_equal_cb_t)(wsrep_thread_t* t1,
                                           wsrep_thread_t* t2);

    /**
     * Terminate the calling thread.
     *
     * @param thread Pointer to thread.
     * @param retval Pointer to return value.
     *
     * This function does not return.
     */
    typedef void __attribute__((noreturn)) (*wsrep_thread_exit_cb_t)(
        wsrep_thread_t* thread, void* retval);

    /**
     * Join a thread. Trying to join detached thread may cause undefined
     * behavior.
     *
     * @param thread Thread to be joined.
     * @param[out] retval Return value from the thread wthat was joined.
     *
     * @return Zero in case of success, non-zero error code in case of error.
     */
    typedef int (*wsrep_thread_join_cb_t)(wsrep_thread_t* thread,
                                          void** retval);

    /**
     * Return a pointer to the wsrep_thread_t of the calling thread.
     *
     * @return Pointer to wsrep_thread_t associated with current thread.
     */
    typedef wsrep_thread_t* (*wsrep_thread_self_cb_t)(void);

    /**
     * Set the scheduling policy for the thread.
     *
     * @param thread Thread for which sceduing policy should be changed.
     * @param policy New scheduling policy for the thread.
     * @param param New scheduling parameters for the thread.
     */
    typedef int (*wsrep_thread_setschedparam_cb_t)(
        wsrep_thread_t* thread, int policy, const struct sched_param* param);

    /**
     * Get the current scheduling policy for the thread.
     *
     * @param thread Thread.
     * @param policy Pointer to location where the scheduling policy will
     *               will be stored in.
     * @param Param Pointer to location where the current scheduling
     *              parameters will be stored.
     */
    typedef int (*wsrep_thread_getschedparam_cb_t)(wsrep_thread_t* thread,
                                                   int* policy,
                                                   struct sched_param* param);
    /**
     * Create key for a mutex with a name. This key object must be passed
     * to mutex creation callback.
     *
     * @param name Name of the mutex.
     *
     * @return Const pointer to mutex key.
     */
    typedef const wsrep_mutex_key_t* (*wsrep_mutex_key_create_cb_t)(
        const char* name);

    /**
     * Create a mutex.
     *
     * @param key Mutex key obtained via wsrep_mutex_key_create_cb call.
     * @param memblock Optional memory block allocated by the provider
     *                 which can be used by the implementation to store
     *                 the mutex.
     * @param memblock_size Size of the optional memory block.
     *
     * @return Pointer to wsrep_mutex_t object or NULL in case of failure.
     */
    typedef wsrep_mutex_t* (*wsrep_mutex_init_cb_t)(
        const wsrep_mutex_key_t* key, void* memblock, size_t memblock_size);

    /**
     * Destroy a mutex. This call must consume the mutex object.
     *
     * @param mutex Mutex to be destroyed.
     */
    typedef int (*wsrep_mutex_destroy_cb_t)(wsrep_mutex_t* mutex);

    /**
     * Lock a mutex.
     *
     * @param mutex Mutex to be locked.
     *
     * @return Zero on success, non-zero error code on error.
     */
    typedef int (*wsrep_mutex_lock_cb_t)(wsrep_mutex_t* mutex);

    /**
     * Try to lock a mutex.
     *
     * @param Mutex to be locked.
     *
     * @return Zero if mutex was succesfully locked.
     * @return EBUSY if the mutex could not be acquired because it was already
     *         locked.
     * @return Non-zero error code on any other error.
     */
    typedef int (*wsrep_mutex_trylock_cb_t)(wsrep_mutex_t* mutex);

    /**
     * Unlock a mutex.
     *
     * @param mutex Mutex to be unlocked.
     *
     * @return Zero on success, non-zero on error.
     */
    typedef int (*wsrep_mutex_unlock_cb_t)(wsrep_mutex_t* mutex);

    /**
     * Create key for a condition variable with a name. This key
     * must be passed to wsrep_cond_create_cb when creating a new
     * condition variable.
     *
     * @param name Name of the condition variable.
     *
     * @return Allocated key object.
     */
    typedef const wsrep_cond_key_t* (*wsrep_cond_key_create_cb_t)(
        const char* name);

    /**
     * Create a new condition variable.
     *
     * @param key Const pointer to key object created by
     * wsrep_cond_key_create_cb.
     * @param memblock Optional memory block allocated by the provider
     *                 which can be used by the implementation to store
     *                 the mutex.
     * @param memblock_size Size of the optional memory block.
     *
     * @return Pointer to new condition variable.
     */
    typedef wsrep_cond_t* (*wsrep_cond_init_cb_t)(const wsrep_cond_key_t* key,
                                                  void* memblock,
                                                  size_t memblock_size);

    /**
     * Destroy a condition variable. This call must consume the condition
     * variable object.
     *
     * @param cond Condition variable to be destroyed.
     *
     * @return Zero on success, non-zero on error.
     */
    typedef int (*wsrep_cond_destroy_cb_t)(wsrep_cond_t* cond);

    /**
     * Wait for condition.
     *
     * @param cond Condition variable to wait for.
     * @param mutex Mutex associated to the condition variable. The mutex
     *              may be unlocked for the duration of the wait.
     *
     * @return Zero on success, non-zero on error.
     */
    typedef int (*wsrep_cond_wait_cb_t)(wsrep_cond_t* cond,
                                        wsrep_mutex_t* mutex);

    /**
     * Perform timed wait on condition.
     *
     * @param cond Condition to wait for.
     * @param mutex Mutex associated to the condition variable. The mutex
     *              may be unlocked for the duration of the wait.
     * @param wait_until System time to wait until before returning from the
     *                   the timed wait.
     *
     * @return Zero on success.
     * @return ETIMEDOUT if the time specified by wait_until has passed.
     * @return Non-zero error code on other error.
     */
    typedef int (*wsrep_cond_timedwait_cb_t)(wsrep_cond_t* cond,
                                             wsrep_mutex_t* mutex,
                                             const struct timespec* wait_until);

    /**
     * Signal a condition variable. This will wake up at least one of
     * the threads which is waiting for the condition.
     *
     * @param cond Condition variable to signal.
     *
     * @return Zero on success, non-zero on failure.
     */
    typedef int (*wsrep_cond_signal_cb_t)(wsrep_cond_t* cond);

    /**
     * Broadcast a signal to condition variable. This will wake up
     * all the threads which are currently waiting on condition variable.
     *
     * @param cond Condition variable to broadcast the signal to.
     *
     * @return Zero on success, non-zero on failure.
     */
    typedef int (*wsrep_cond_broadcast_cb_t)(wsrep_cond_t* cond);

    typedef struct wsrep_thread_service_v1_st
    {
        /* Threads */
        wsrep_thread_key_create_cb_t thread_key_create_cb;
        wsrep_thread_create_cb_t thread_create_cb;
        wsrep_thread_detach_cb_t thread_detach_cb;
        wsrep_thread_equal_cb_t thread_equal_cb;
        wsrep_thread_exit_cb_t thread_exit_cb;
        wsrep_thread_join_cb_t thread_join_cb;
        wsrep_thread_self_cb_t thread_self_cb;
        wsrep_thread_setschedparam_cb_t thread_setschedparam_cb;
        wsrep_thread_getschedparam_cb_t thread_getschedparam_cb;
        /* Mutexes */
        wsrep_mutex_key_create_cb_t mutex_key_create_cb;
        wsrep_mutex_init_cb_t mutex_init_cb;
        wsrep_mutex_destroy_cb_t mutex_destroy_cb;
        wsrep_mutex_lock_cb_t mutex_lock_cb;
        wsrep_mutex_trylock_cb_t mutex_trylock_cb;
        wsrep_mutex_unlock_cb_t mutex_unlock_cb;
        /* Condition variables */
        wsrep_cond_key_create_cb_t cond_key_create_cb;
        wsrep_cond_init_cb_t cond_init_cb;
        wsrep_cond_destroy_cb_t cond_destroy_cb;
        wsrep_cond_wait_cb_t cond_wait_cb;
        wsrep_cond_timedwait_cb_t cond_timedwait_cb;
        wsrep_cond_signal_cb_t cond_signal_cb;
        wsrep_cond_broadcast_cb_t cond_broadcast_cb;
    } wsrep_thread_service_v1_t;

#ifdef __cplusplus
}

#define WSREP_THREAD_SERVICE_INIT_FUNC "wsrep_init_thread_service_v1"

#endif /* __cplusplus */
#endif /* WSREP_THREAD_SERVICE_H */
