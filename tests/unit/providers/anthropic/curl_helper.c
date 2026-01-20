/**
 * @file curl_mocks.c
 * @brief Mock implementations of curl functions for testing
 */

#include <stdarg.h>
#include <curl/curl.h>

/* Curl mock implementations that delegate to real curl functions */

CURLM *curl_multi_init_(void)
{
    return curl_multi_init();
}

CURLMcode curl_multi_perform_(CURLM *multi_handle, int *running_handles)
{
    return curl_multi_perform(multi_handle, running_handles);
}

CURLMcode curl_multi_fdset_(CURLM *multi_handle, fd_set *read_fd_set,
                            fd_set *write_fd_set, fd_set *exc_fd_set,
                            int *max_fd)
{
    return curl_multi_fdset(multi_handle, read_fd_set, write_fd_set, exc_fd_set, max_fd);
}

CURLMcode curl_multi_timeout_(CURLM *multi_handle, long *timeout)
{
    return curl_multi_timeout(multi_handle, timeout);
}

CURLMcode curl_multi_cleanup_(CURLM *multi_handle)
{
    return curl_multi_cleanup(multi_handle);
}

CURL *curl_easy_init_(void)
{
    return curl_easy_init();
}

void curl_easy_cleanup_(CURL *curl)
{
    curl_easy_cleanup(curl);
}

CURLcode curl_easy_setopt_(CURL *curl, CURLoption option, const void *val)
{
    return curl_easy_setopt(curl, option, val);
}

CURLMcode curl_multi_add_handle_(CURLM *multi_handle, CURL *curl_handle)
{
    return curl_multi_add_handle(multi_handle, curl_handle);
}

CURLMcode curl_multi_remove_handle_(CURLM *multi_handle, CURL *curl_handle)
{
    return curl_multi_remove_handle(multi_handle, curl_handle);
}

CURLMsg *curl_multi_info_read_(CURLM *multi_handle, int *msgs_in_queue)
{
    return curl_multi_info_read(multi_handle, msgs_in_queue);
}

const char *curl_multi_strerror_(CURLMcode code)
{
    return curl_multi_strerror(code);
}

const char *curl_easy_strerror_(CURLcode code)
{
    return curl_easy_strerror(code);
}

CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...)
{
    va_list args;
    va_start(args, info);
    void *param = va_arg(args, void *);
    va_end(args);
    return curl_easy_getinfo(curl, info, param);
}

struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string)
{
    return curl_slist_append(list, string);
}

void curl_slist_free_all_(struct curl_slist *list)
{
    curl_slist_free_all(list);
}
