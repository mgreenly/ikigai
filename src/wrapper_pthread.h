// Pthread wrappers for testing
#ifndef IK_WRAPPER_PTHREAD_H
#define IK_WRAPPER_PTHREAD_H

#include <pthread.h>
#include "wrapper_base.h"

#ifdef NDEBUG

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

#else

MOCKABLE int pthread_mutex_init_(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
MOCKABLE int pthread_mutex_destroy_(pthread_mutex_t *mutex);
MOCKABLE int pthread_mutex_lock_(pthread_mutex_t *mutex);
MOCKABLE int pthread_mutex_unlock_(pthread_mutex_t *mutex);
MOCKABLE int pthread_create_(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
MOCKABLE int pthread_join_(pthread_t thread, void **retval);

#endif

#endif // IK_WRAPPER_PTHREAD_H
