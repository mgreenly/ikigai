// pthread wrapper implementations
// Link seams that tests can override for failure injection
//
// In release builds (NDEBUG), these are defined as static inline in the header.
// In debug/test builds, these are compiled as weak symbols.

#include "wrapper_pthread.h"

#ifndef NDEBUG
// LCOV_EXCL_START

#include <pthread.h>

// ============================================================================
// Pthread wrappers - debug/test builds only
// ============================================================================

MOCKABLE int pthread_mutex_init_(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    return pthread_mutex_init(mutex, attr);
}

MOCKABLE int pthread_mutex_destroy_(pthread_mutex_t *mutex)
{
    return pthread_mutex_destroy(mutex);
}

MOCKABLE int pthread_mutex_lock_(pthread_mutex_t *mutex)
{
    return pthread_mutex_lock(mutex);
}

MOCKABLE int pthread_mutex_unlock_(pthread_mutex_t *mutex)
{
    return pthread_mutex_unlock(mutex);
}

MOCKABLE int pthread_create_(pthread_t *thread, const pthread_attr_t *attr,
                             void *(*start_routine)(void *), void *arg)
{
    return pthread_create(thread, attr, start_routine, arg);
}

MOCKABLE int pthread_join_(pthread_t thread, void **retval)
{
    return pthread_join(thread, retval);
}

// LCOV_EXCL_STOP
#endif
