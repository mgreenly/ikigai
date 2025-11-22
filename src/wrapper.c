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
                                       const yyjson_alc *allocator, yyjson_read_err *err)
{
    return yyjson_read_file(path, flg, allocator, err);
}

MOCKABLE bool yyjson_mut_write_file_(const char *path, const yyjson_mut_doc *doc,
                                     yyjson_write_flag flg, const yyjson_alc *allocator,
                                     yyjson_write_err *err)
{
    return yyjson_mut_write_file(path, doc, flg, allocator, err);
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

MOCKABLE int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    return select(nfds, readfds, writefds, exceptfds, timeout);
}

MOCKABLE int posix_sigaction_(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    return sigaction(signum, act, oldact);
}

MOCKABLE int posix_pipe_(int pipefd[2])
{
    return pipe(pipefd);
}

MOCKABLE int posix_fcntl_(int fd, int cmd, int arg)
{
    return fcntl(fd, cmd, arg);
}

MOCKABLE FILE *posix_fdopen_(int fd, const char *mode)
{
    return fdopen(fd, mode);
}

// ============================================================================
// libcurl wrappers - debug/test builds only
// ============================================================================

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

MOCKABLE CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val)
{
    return curl_easy_setopt(curl, opt, val);
}

MOCKABLE CURLM *curl_multi_init_(void)
{
    return curl_multi_init();
}

MOCKABLE CURLMcode curl_multi_cleanup_(CURLM *multi)
{
    return curl_multi_cleanup(multi);
}

MOCKABLE CURLMcode curl_multi_add_handle_(CURLM *multi, CURL *easy)
{
    return curl_multi_add_handle(multi, easy);
}

MOCKABLE CURLMcode curl_multi_remove_handle_(CURLM *multi, CURL *easy)
{
    return curl_multi_remove_handle(multi, easy);
}

MOCKABLE CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles)
{
    return curl_multi_perform(multi, running_handles);
}

MOCKABLE CURLMcode curl_multi_fdset_(CURLM *multi, fd_set *read_fd_set,
                                     fd_set *write_fd_set, fd_set *exc_fd_set,
                                     int *max_fd)
{
    return curl_multi_fdset(multi, read_fd_set, write_fd_set, exc_fd_set, max_fd);
}

MOCKABLE CURLMcode curl_multi_timeout_(CURLM *multi, long *timeout)
{
    return curl_multi_timeout(multi, timeout);
}

MOCKABLE CURLMsg *curl_multi_info_read_(CURLM *multi, int *msgs_in_queue)
{
    return curl_multi_info_read(multi, msgs_in_queue);
}

MOCKABLE const char *curl_multi_strerror_(CURLMcode code)
{
    return curl_multi_strerror(code);
}

// LCOV_EXCL_STOP
#endif
