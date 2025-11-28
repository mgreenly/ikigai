// External library wrappers for testing
// These provide link seams that tests can override to inject failures
//
// MOCKABLE functions are:
//   - weak symbols in debug/test builds (can be overridden)
//   - always_inline in release builds (zero overhead)

#ifndef IK_WRAPPER_H
#define IK_WRAPPER_H

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>
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

MOCKABLE yyjson_doc *yyjson_read_(const char *dat, size_t len, yyjson_read_flag flg)
{
    return yyjson_read(dat, len, flg);
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
                                       const yyjson_alc *allocator,
                                       yyjson_read_err *err);
MOCKABLE bool yyjson_mut_write_file_(const char *path,
                                     const yyjson_mut_doc *doc,
                                     yyjson_write_flag flg,
                                     const yyjson_alc *allocator,
                                     yyjson_write_err *err);
MOCKABLE yyjson_doc *yyjson_read_(const char *dat, size_t len, yyjson_read_flag flg);
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

// curl_easy_getinfo wrapper - variadic function
#define curl_easy_getinfo_(curl, info, ...) curl_easy_getinfo(curl, info, __VA_ARGS__)

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

// curl_easy_getinfo wrapper - variadic function
MOCKABLE CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...);

MOCKABLE CURLM *curl_multi_init_(void);
MOCKABLE CURLMcode curl_multi_cleanup_(CURLM *multi);
MOCKABLE CURLMcode curl_multi_add_handle_(CURLM *multi, CURL *easy);
MOCKABLE CURLMcode curl_multi_remove_handle_(CURLM *multi, CURL *easy);
MOCKABLE CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles);
MOCKABLE CURLMcode curl_multi_fdset_(CURLM *multi,
                                     fd_set *read_fd_set,
                                     fd_set *write_fd_set,
                                     fd_set *exc_fd_set,
                                     int *max_fd);
MOCKABLE CURLMcode curl_multi_timeout_(CURLM *multi, long *timeout);
MOCKABLE CURLMsg *curl_multi_info_read_(CURLM *multi, int *msgs_in_queue);
MOCKABLE const char *curl_multi_strerror_(CURLMcode code);
#endif

// ============================================================================
// PostgreSQL libpq wrappers
// ============================================================================

#include <libpq-fe.h>

#ifdef NDEBUG
// Release build: inline definitions for zero overhead
MOCKABLE char *PQgetvalue_(const PGresult *res, int row_number, int column_number)
{
    return PQgetvalue(res, row_number, column_number);
}

#else
// Debug/test build: weak symbol declarations
MOCKABLE char *PQgetvalue_(const PGresult *res, int row_number, int column_number);
#endif

// ============================================================================
// POSIX system call wrappers
// ============================================================================

#ifdef NDEBUG
// Release build: inline definitions for zero overhead
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
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

MOCKABLE size_t fread_(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fread(ptr, size, nmemb, stream);
}

MOCKABLE FILE *fopen_(const char *pathname, const char *mode)
{
    return fopen(pathname, mode);
}

MOCKABLE int fseek_(FILE *stream, long offset, int whence)
{
    return fseek(stream, offset, whence);
}

MOCKABLE long ftell_(FILE *stream)
{
    return ftell(stream);
}

MOCKABLE int fclose_(FILE *stream)
{
    return fclose(stream);
}

MOCKABLE DIR *opendir_(const char *name)
{
    return opendir(name);
}

#else
// Debug/test build: weak symbol declarations
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
MOCKABLE int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
MOCKABLE int posix_sigaction_(int signum, const struct sigaction *act, struct sigaction *oldact);
MOCKABLE int posix_pipe_(int pipefd[2]);
MOCKABLE int posix_fcntl_(int fd, int cmd, int arg);
MOCKABLE FILE *posix_fdopen_(int fd, const char *mode);
MOCKABLE size_t fread_(void *ptr, size_t size, size_t nmemb, FILE *stream);
MOCKABLE FILE *fopen_(const char *pathname, const char *mode);
MOCKABLE int fseek_(FILE *stream, long offset, int whence);
MOCKABLE long ftell_(FILE *stream);
MOCKABLE int fclose_(FILE *stream);
MOCKABLE DIR *opendir_(const char *name);
#endif

// ============================================================================
// PostgreSQL wrappers
// ============================================================================

#ifdef NDEBUG
// Release build: inline definitions for zero overhead
#include <libpq-fe.h>

MOCKABLE PGresult *pq_exec_(PGconn *conn, const char *command)
{
    return PQexec(conn, command);
}

MOCKABLE PGresult *pq_exec_params_(PGconn *conn,
                                   const char *command,
                                   int nParams,
                                   const Oid *paramTypes,
                                   const char *const *paramValues,
                                   const int *paramLengths,
                                   const int *paramFormats,
                                   int resultFormat)
{
    return PQexecParams(conn, command, nParams, paramTypes, paramValues, paramLengths, paramFormats, resultFormat);
}

#else
// Debug/test build: weak symbol declarations
#include <libpq-fe.h>

MOCKABLE PGresult *pq_exec_(PGconn *conn, const char *command);
MOCKABLE PGresult *pq_exec_params_(PGconn *conn,
                                   const char *command,
                                   int nParams,
                                   const Oid *paramTypes,
                                   const char *const *paramValues,
                                   const int *paramLengths,
                                   const int *paramFormats,
                                   int resultFormat);
#endif

// ============================================================================
// C standard library wrappers
// ============================================================================

#include <stdarg.h>
#include <time.h>

#ifdef NDEBUG
// Release build: inline definitions for zero overhead
MOCKABLE int vsnprintf_(char *str, size_t size, const char *format, va_list ap)
{
    return vsnprintf(str, size, format, ap);
}

MOCKABLE int snprintf_(char *str, size_t size, const char *format, ...)
{
    va_list ap; va_start(ap, format);
    int result = vsnprintf(str, size, format, ap);
    va_end(ap); return result;
}

MOCKABLE struct tm *gmtime_(const time_t *timep)
{
    return gmtime(timep);
}

MOCKABLE size_t strftime_(char *s, size_t max, const char *format, const struct tm *tm)
{
    return strftime(s, max, format, tm);
}

#else
// Debug/test build: weak symbol declarations
MOCKABLE int vsnprintf_(char *str, size_t size, const char *format, va_list ap);
MOCKABLE int snprintf_(char *str, size_t size, const char *format, ...);
MOCKABLE struct tm *gmtime_(const time_t *timep);
MOCKABLE size_t strftime_(char *s, size_t max, const char *format, const struct tm *tm);
#endif

// ============================================================================
// Internal ikigai function wrappers for testing
// ============================================================================

#ifdef NDEBUG
// Release build: Call through to actual functions
#include "db/connection.h"
#include "db/message.h"
#include "repl/session_restore.h"
#include "config.h"
#include "scrollback.h"

MOCKABLE res_t ik_db_init_(TALLOC_CTX *mem_ctx, const char *conn_str, ik_db_ctx_t **out_ctx)
{
    return ik_db_init(mem_ctx, conn_str, out_ctx);
}

MOCKABLE res_t ik_db_message_insert_(ik_db_ctx_t *db,
                                     int64_t session_id,
                                     const char *kind,
                                     const char *content,
                                     const char *data_json)
{
    return ik_db_message_insert(db, session_id, kind, content, data_json);
}

MOCKABLE res_t ik_repl_restore_session_(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx, void *cfg)
{
    return ik_repl_restore_session(repl, db_ctx, (ik_cfg_t *)cfg);
}

MOCKABLE res_t ik_scrollback_append_line_(void *scrollback, const char *text, size_t length)
{
    return ik_scrollback_append_line((ik_scrollback_t *)scrollback, text, length);
}

#else
// Debug/test build: weak symbol declarations
// Note: These use void* because the actual types are defined in headers that may
// not be included when wrapper.h is processed
#include "error.h"

MOCKABLE res_t ik_db_init_(TALLOC_CTX *mem_ctx, const char *conn_str, void **out_ctx);
MOCKABLE res_t ik_db_message_insert_(void *db,
                                     int64_t session_id,
                                     const char *kind,
                                     const char *content,
                                     const char *data_json);
MOCKABLE res_t ik_repl_restore_session_(void *repl, void *db_ctx, void *cfg);
MOCKABLE res_t ik_scrollback_append_line_(void *scrollback, const char *text, size_t length);
#endif

#endif // IK_WRAPPER_H
