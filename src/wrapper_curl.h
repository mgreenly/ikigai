// libcurl wrappers for testing
#ifndef IK_WRAPPER_CURL_H
#define IK_WRAPPER_CURL_H

#include <curl/curl.h>
#include "wrapper_base.h"

#ifdef NDEBUG

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

MOCKABLE CURLMcode curl_multi_wait_(CURLM *multi, struct curl_waitfd *extra_fds,
                                    unsigned int extra_nfds, int timeout_ms, int *numfds)
{
    return curl_multi_wait(multi, extra_fds, extra_nfds, timeout_ms, numfds);
}

#else

MOCKABLE CURL *curl_easy_init_(void);
MOCKABLE void curl_easy_cleanup_(CURL *curl);
MOCKABLE CURLcode curl_easy_perform_(CURL *curl);
MOCKABLE const char *curl_easy_strerror_(CURLcode code);
MOCKABLE struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string);
MOCKABLE void curl_slist_free_all_(struct curl_slist *list);

// curl_easy_setopt wrapper - variadic to match curl API
MOCKABLE CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, ...);

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
MOCKABLE CURLMcode curl_multi_wait_(CURLM *multi,
                                    struct curl_waitfd *extra_fds,
                                    unsigned int extra_nfds,
                                    int timeout_ms,
                                    int *numfds);
#endif

#endif // IK_WRAPPER_CURL_H
