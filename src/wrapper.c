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

MOCKABLE void *talloc_zero_(TALLOC_CTX *ctx, size_t size)
{
    return talloc_zero_size(ctx, size);
}

MOCKABLE char *talloc_strdup_(TALLOC_CTX *ctx, const char *str)
{
    return talloc_strdup(ctx, str);
}

MOCKABLE void *talloc_array_(TALLOC_CTX *ctx, size_t el_size, size_t count)
{
    return talloc_zero_size(ctx, el_size * count);
}

MOCKABLE void *talloc_realloc_(TALLOC_CTX *ctx, void *ptr, size_t size)
{
    return talloc_realloc_size(ctx, ptr, size);
}

MOCKABLE char *talloc_asprintf_(TALLOC_CTX *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *result = talloc_vasprintf(ctx, fmt, ap);
    va_end(ap);
    return result;
}

// ============================================================================
// yyjson wrappers - debug/test builds only
// ============================================================================

MOCKABLE yyjson_doc *yyjson_read_file_(const char *path, yyjson_read_flag flg,
                                       const yyjson_alc *alc, yyjson_read_err *err)
{
    return yyjson_read_file(path, flg, alc, err);
}

MOCKABLE bool yyjson_mut_write_file_(const char *path, const yyjson_mut_doc *doc,
                                     yyjson_write_flag flg, const yyjson_alc *alc,
                                     yyjson_write_err *err)
{
    return yyjson_mut_write_file(path, doc, flg, alc, err);
}

MOCKABLE yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc)
{
    return yyjson_doc_get_root(doc);
}

MOCKABLE yyjson_val *yyjson_obj_get_(yyjson_val *obj, const char *key)
{
    return yyjson_obj_get(obj, key);
}

MOCKABLE int64_t yyjson_get_sint_(yyjson_val *val)
{
    return yyjson_get_sint(val);
}

MOCKABLE const char *yyjson_get_str_(yyjson_val *val)
{
    return yyjson_get_str(val);
}

// ============================================================================
// POSIX system call wrappers - debug/test builds only
// ============================================================================

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

MOCKABLE int posix_open_(const char *pathname, int flags)
{
    return open(pathname, flags);
}

MOCKABLE int posix_close_(int fd)
{
    return close(fd);
}

MOCKABLE int posix_stat_(const char *pathname, struct stat *statbuf)
{
    return stat(pathname, statbuf);
}

MOCKABLE int posix_mkdir_(const char *pathname, mode_t mode)
{
    return mkdir(pathname, mode);
}

MOCKABLE int posix_tcgetattr_(int fd, struct termios *termios_p)
{
    return tcgetattr(fd, termios_p);
}

MOCKABLE int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p)
{
    return tcsetattr(fd, optional_actions, termios_p);
}

MOCKABLE int posix_tcflush_(int fd, int queue_selector)
{
    return tcflush(fd, queue_selector);
}

MOCKABLE int posix_ioctl_(int fd, unsigned long request, void *argp)
{
    return ioctl(fd, request, argp);
}

MOCKABLE ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}

MOCKABLE ssize_t posix_read_(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}

// LCOV_EXCL_STOP
#endif
