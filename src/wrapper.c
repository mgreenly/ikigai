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
// yyjson wrappers - debug/test builds only
// ============================================================================

MOCKABLE yyjson_doc *ik_yyjson_read_file_wrapper(const char *path, yyjson_read_flag flg,
                                                 const yyjson_alc *alc, yyjson_read_err *err)
{
    return yyjson_read_file(path, flg, alc, err);
}

MOCKABLE bool ik_yyjson_mut_write_file_wrapper(const char *path, const yyjson_mut_doc *doc,
                                               yyjson_write_flag flg, const yyjson_alc *alc,
                                               yyjson_write_err *err)
{
    return yyjson_mut_write_file(path, doc, flg, alc, err);
}

MOCKABLE yyjson_val *ik_yyjson_doc_get_root_wrapper(yyjson_doc *doc)
{
    return yyjson_doc_get_root(doc);
}

MOCKABLE yyjson_val *ik_yyjson_obj_get_wrapper(yyjson_val *obj, const char *key)
{
    return yyjson_obj_get(obj, key);
}

MOCKABLE int64_t ik_yyjson_get_sint_wrapper(yyjson_val *val)
{
    return yyjson_get_sint(val);
}

MOCKABLE const char *ik_yyjson_get_str_wrapper(yyjson_val *val)
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

MOCKABLE int ik_open_wrapper(const char *pathname, int flags)
{
    return open(pathname, flags);
}

MOCKABLE int ik_close_wrapper(int fd)
{
    return close(fd);
}

MOCKABLE int ik_stat_wrapper(const char *pathname, struct stat *statbuf)
{
    return stat(pathname, statbuf);
}

MOCKABLE int ik_mkdir_wrapper(const char *pathname, mode_t mode)
{
    return mkdir(pathname, mode);
}

MOCKABLE int ik_tcgetattr_wrapper(int fd, struct termios *termios_p)
{
    return tcgetattr(fd, termios_p);
}

MOCKABLE int ik_tcsetattr_wrapper(int fd, int optional_actions, const struct termios *termios_p)
{
    return tcsetattr(fd, optional_actions, termios_p);
}

MOCKABLE int ik_tcflush_wrapper(int fd, int queue_selector)
{
    return tcflush(fd, queue_selector);
}

MOCKABLE int ik_ioctl_wrapper(int fd, unsigned long request, void *argp)
{
    return ioctl(fd, request, argp);
}

MOCKABLE ssize_t ik_write_wrapper(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}

MOCKABLE ssize_t ik_read_wrapper(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}

// LCOV_EXCL_STOP
#endif
