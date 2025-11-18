// External library wrappers for testing
// These provide link seams that tests can override to inject failures
//
// MOCKABLE functions are:
//   - weak symbols in debug/test builds (can be overridden)
//   - always_inline in release builds (zero overhead)

#ifndef IK_WRAPPER_H
#define IK_WRAPPER_H

#include <talloc.h>
#include <stddef.h>
#include <stdarg.h>
#include "vendor/yyjson/yyjson.h"

// MOCKABLE: Weak symbols for testing in debug builds,
// inline with definitions in header for zero overhead in release builds
#ifdef NDEBUG
#define MOCKABLE static inline
#else
#define MOCKABLE __attribute__((weak))
#endif

// ============================================================================
// talloc wrappers
// ============================================================================

#ifdef NDEBUG
// Release build: inline definitions for zero overhead
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

#else
// Debug/test build: weak symbol declarations
MOCKABLE void *talloc_zero_(TALLOC_CTX *ctx, size_t size);
MOCKABLE char *talloc_strdup_(TALLOC_CTX *ctx, const char *str);
MOCKABLE void *talloc_array_(TALLOC_CTX *ctx, size_t el_size, size_t count);
MOCKABLE void *talloc_realloc_(TALLOC_CTX *ctx, void *ptr, size_t size);
MOCKABLE char *talloc_asprintf_(TALLOC_CTX *ctx, const char *fmt, ...);
#endif

// ============================================================================
// yyjson wrappers
// ============================================================================

#ifdef NDEBUG
// Release build: inline definitions for zero overhead
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

#else
// Debug/test build: weak symbol declarations
MOCKABLE yyjson_doc *yyjson_read_file_(const char *path,
                                       yyjson_read_flag flg,
                                       const yyjson_alc *alc,
                                       yyjson_read_err *err);
MOCKABLE bool yyjson_mut_write_file_(const char *path,
                                     const yyjson_mut_doc *doc,
                                     yyjson_write_flag flg,
                                     const yyjson_alc *alc,
                                     yyjson_write_err *err);
MOCKABLE yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc);
MOCKABLE yyjson_val *yyjson_obj_get_(yyjson_val *obj, const char *key);
MOCKABLE int64_t yyjson_get_sint_(yyjson_val *val);
MOCKABLE const char *yyjson_get_str_(yyjson_val *val);
#endif

// ============================================================================
// libcurl wrappers
// ============================================================================

#ifdef NDEBUG
// Release build: inline definitions for zero overhead
#include <curl/curl.h>

MOCKABLE CURL *curl_easy_init_(void)
{
    return curl_easy_init();
}

MOCKABLE void curl_easy_cleanup_(CURL *curl)
{
    curl_easy_cleanup(curl);
}

MOCKABLE CURLcode curl_easy_perform_(CURL *curl)
{
    return curl_easy_perform(curl);
}

MOCKABLE const char *curl_easy_strerror_(CURLcode code)
{
    return curl_easy_strerror(code);
}

MOCKABLE struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string)
{
    return curl_slist_append(list, string);
}

MOCKABLE void curl_slist_free_all_(struct curl_slist *list)
{
    curl_slist_free_all(list);
}

// curl_easy_setopt wrapper - single void* version for simplicity
#define curl_easy_setopt_(curl, opt, val) curl_easy_setopt(curl, opt, val)

#else
// Debug/test build: weak symbol declarations
#include <curl/curl.h>

MOCKABLE CURL *curl_easy_init_(void);
MOCKABLE void curl_easy_cleanup_(CURL *curl);
MOCKABLE CURLcode curl_easy_perform_(CURL *curl);
MOCKABLE const char *curl_easy_strerror_(CURLcode code);
MOCKABLE struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string);
MOCKABLE void curl_slist_free_all_(struct curl_slist *list);

// curl_easy_setopt wrapper - const void* to preserve const-correctness
MOCKABLE CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val);
#endif

// ============================================================================
// POSIX system call wrappers
// ============================================================================

#ifdef NDEBUG
// Release build: inline definitions for zero overhead
#include <fcntl.h>
#include <termios.h>
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

#else
// Debug/test build: weak symbol declarations
#include <termios.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>

MOCKABLE int posix_open_(const char *pathname, int flags);
MOCKABLE int posix_close_(int fd);
MOCKABLE int posix_stat_(const char *pathname, struct stat *statbuf);
MOCKABLE int posix_mkdir_(const char *pathname, mode_t mode);
MOCKABLE int posix_tcgetattr_(int fd, struct termios *termios_p);
MOCKABLE int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
MOCKABLE int posix_tcflush_(int fd, int queue_selector);
MOCKABLE int posix_ioctl_(int fd, unsigned long request, void *argp);
MOCKABLE ssize_t posix_write_(int fd, const void *buf, size_t count);
MOCKABLE ssize_t posix_read_(int fd, void *buf, size_t count);
#endif

#endif // IK_WRAPPER_H
