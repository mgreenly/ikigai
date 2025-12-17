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

// ============================================================================
// Pthread wrappers - debug/test builds only
// ============================================================================

#include <pthread.h>

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

MOCKABLE size_t fread_(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fread(ptr, size, nmemb, stream);
}

MOCKABLE size_t fwrite_(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
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

MOCKABLE FILE *popen_(const char *command, const char *mode)
{
    return popen(command, mode);
}

MOCKABLE int pclose_(FILE *stream)
{
    return pclose(stream);
}

MOCKABLE DIR *opendir_(const char *name)
{
    return opendir(name);
}

MOCKABLE int posix_access_(const char *pathname, int mode)
{
    return access(pathname, mode);
}

MOCKABLE int posix_rename_(const char *oldpath, const char *newpath)
{
    return rename(oldpath, newpath);
}

MOCKABLE char *posix_getcwd_(char *buf, size_t size)
{
    return getcwd(buf, size);
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

MOCKABLE CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...)
{
    va_list args;
    va_start(args, info);
    void *param = va_arg(args, void *);
    va_end(args);
    return curl_easy_getinfo(curl, info, param);
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

// ============================================================================
// PostgreSQL wrappers - debug/test builds only
// ============================================================================

#include <libpq-fe.h>

MOCKABLE PGresult *pq_exec_(PGconn *conn, const char *command)
{
    return PQexec(conn, command);
}

MOCKABLE char *PQgetvalue_(const PGresult *res, int row_number, int column_number)
{
    return PQgetvalue(res, row_number, column_number);
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

// ============================================================================
// C standard library wrappers - debug/test builds only
// ============================================================================

#include <stdarg.h>
#include <time.h>

MOCKABLE int vsnprintf_(char *str, size_t size, const char *format, va_list ap)
{
    return vsnprintf(str, size, format, ap);
}

MOCKABLE int snprintf_(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int result = vsnprintf(str, size, format, ap);
    va_end(ap);
    return result;
}

MOCKABLE struct tm *gmtime_(const time_t *timep)
{
    return gmtime(timep);
}

MOCKABLE size_t strftime_(char *s, size_t max, const char *format, const struct tm *tm)
{
    // Suppress format-nonliteral warning - format string comes from caller
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    return strftime(s, max, format, tm);
#pragma GCC diagnostic pop
}

// ============================================================================
// Internal ikigai function wrappers for testing - debug/test builds only
// ============================================================================

#include "db/connection.h"
#include "db/message.h"
#include "repl/session_restore.h"
#include "config.h"
#include "scrollback.h"
#include "msg.h"
#include "openai/client.h"

MOCKABLE res_t ik_db_init_(TALLOC_CTX *mem_ctx, const char *conn_str, void **out_ctx)
{
    return ik_db_init(mem_ctx, conn_str, (ik_db_ctx_t **)out_ctx);
}

MOCKABLE res_t ik_db_message_insert_(void *db,
                                     int64_t session_id,
                                     const char *agent_uuid,
                                     const char *kind,
                                     const char *content,
                                     const char *data_json)
{
    return ik_db_message_insert((ik_db_ctx_t *)db, session_id, agent_uuid, kind, content, data_json);
}

MOCKABLE res_t ik_repl_restore_session_(void *repl, void *db_ctx, void *cfg)
{
    return ik_repl_restore_session((ik_repl_ctx_t *)repl, (ik_db_ctx_t *)db_ctx, (ik_cfg_t *)cfg);
}

MOCKABLE res_t ik_scrollback_append_line_(void *scrollback, const char *text, size_t length)
{
    return ik_scrollback_append_line((ik_scrollback_t *)scrollback, text, length);
}

MOCKABLE res_t ik_openai_conversation_add_msg_(void *conv, void *msg)
{
    return ik_openai_conversation_add_msg((ik_openai_conversation_t *)conv, (ik_msg_t *)msg);
}

// LCOV_EXCL_STOP
#endif
