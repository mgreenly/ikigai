#include "wrapper_web.h"

#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <stdarg.h>

/* LCOV_EXCL_START */
#ifndef NDEBUG

// In debug builds, provide weak implementations that tests can override

MOCKABLE CURL *curl_easy_init_(void)
{
    return curl_easy_init();
}

MOCKABLE CURLcode curl_easy_perform_(CURL *curl)
{
    return curl_easy_perform(curl);
}

MOCKABLE void curl_easy_cleanup_(CURL *curl)
{
    curl_easy_cleanup(curl);
}

MOCKABLE CURLcode curl_easy_setopt_(CURL *curl, CURLoption option, ...)
{
    va_list args;
    va_start(args, option);
    CURLcode result = curl_easy_setopt(curl, option, va_arg(args, void *));
    va_end(args);
    return result;
}

MOCKABLE CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...)
{
    va_list args;
    va_start(args, info);
    CURLcode result = curl_easy_getinfo(curl, info, va_arg(args, void *));
    va_end(args);
    return result;
}

MOCKABLE const char *curl_easy_strerror_(CURLcode errornum)
{
    return curl_easy_strerror(errornum);
}

MOCKABLE CURLM *curl_multi_init_(void)
{
    return curl_multi_init();
}

MOCKABLE CURLMcode curl_multi_add_handle_(CURLM *multi_handle, CURL *easy_handle)
{
    return curl_multi_add_handle(multi_handle, easy_handle);
}

MOCKABLE CURLMcode curl_multi_perform_(CURLM *multi_handle, int *running_handles)
{
    return curl_multi_perform(multi_handle, running_handles);
}

MOCKABLE CURLMcode curl_multi_wait_(CURLM *multi_handle, struct curl_waitfd extra_fds[], unsigned int extra_nfds,
                                    int timeout_ms, int *numfds)
{
    return curl_multi_wait(multi_handle, extra_fds, extra_nfds, timeout_ms, numfds);
}

MOCKABLE CURLMcode curl_multi_remove_handle_(CURLM *multi_handle, CURL *easy_handle)
{
    return curl_multi_remove_handle(multi_handle, easy_handle);
}

MOCKABLE CURLMcode curl_multi_cleanup_(CURLM *multi_handle)
{
    return curl_multi_cleanup(multi_handle);
}

MOCKABLE htmlDocPtr htmlReadMemory_(const char *buffer, int size, const char *URL, const char *encoding, int options)
{
    return htmlReadMemory(buffer, size, URL, encoding, options);
}

MOCKABLE void xmlFreeDoc_(xmlDocPtr cur)
{
    xmlFreeDoc(cur);
}

#endif
/* LCOV_EXCL_STOP */
