// External library wrapper implementations
// Link seams that tests can override for failure injection
//
// In release builds (NDEBUG), these are defined as static inline in the header.
// In debug/test builds, these are compiled as weak symbols.

#include "wrapper.h"
#include <talloc.h>

#ifndef NDEBUG
// LCOV_EXCL_START

// ============================================================================
// talloc wrappers - debug/test builds only
// ============================================================================

MOCKABLE void *ik_talloc_zero_wrapper(TALLOC_CTX *ctx, size_t size)
{
    return talloc_zero_size(ctx, size);
}

MOCKABLE char *ik_talloc_strdup_wrapper(TALLOC_CTX *ctx, const char *str)
{
    return talloc_strdup(ctx, str);
}

MOCKABLE void *ik_talloc_array_wrapper(TALLOC_CTX *ctx, size_t el_size, size_t count)
{
    return talloc_zero_size(ctx, el_size * count);
}

MOCKABLE void *ik_talloc_realloc_wrapper(TALLOC_CTX *ctx, void *ptr, size_t size)
{
    return talloc_realloc_size(ctx, ptr, size);
}

MOCKABLE char *ik_talloc_asprintf_wrapper(TALLOC_CTX *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *result = talloc_vasprintf(ctx, fmt, ap);
    va_end(ap);
    return result;
}

// ============================================================================
// jansson wrappers - debug/test builds only
// ============================================================================

MOCKABLE json_t *ik_json_object_wrapper(void)
{
    return json_object();
}

MOCKABLE char *ik_json_dumps_wrapper(const json_t *json, size_t flags)
{
    return json_dumps(json, flags);
}

MOCKABLE int ik_json_is_object_wrapper(const json_t *json)
{
    return json_is_object(json);
}

MOCKABLE int ik_json_is_string_wrapper(const json_t *json)
{
    return json_is_string(json);
}

// ============================================================================
// POSIX system call wrappers - debug/test builds only
// ============================================================================

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

MOCKABLE int ik_open_wrapper(const char *pathname, int flags)
{
    return open(pathname, flags);
}

MOCKABLE int ik_close_wrapper(int fd)
{
    return close(fd);
}

MOCKABLE int ik_tcgetattr_wrapper(int fd, struct termios *termios_p)
{
    return tcgetattr(fd, termios_p);
}

MOCKABLE int ik_tcsetattr_wrapper(int fd, int optional_actions, const struct termios *termios_p)
{
    return tcsetattr(fd, optional_actions, termios_p);
}

MOCKABLE int ik_ioctl_wrapper(int fd, unsigned long request, void *argp)
{
    return ioctl(fd, request, argp);
}

MOCKABLE ssize_t ik_write_wrapper(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}

// LCOV_EXCL_STOP
#endif
