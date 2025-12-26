// libcurl wrapper implementations
// Link seams that tests can override for failure injection
//
// In release builds (NDEBUG), these are defined as static inline in the header.
// In debug/test builds, these are compiled as weak symbols.

#include "wrapper_curl.h"

#ifndef NDEBUG
// LCOV_EXCL_START

#include <curl/curl.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../tests/helpers/vcr.h"

// ============================================================================
// libcurl wrappers - debug/test builds only
// ============================================================================

// VCR integration state for tracking curl callbacks
typedef struct vcr_curl_state {
    CURL *easy_handle;
    curl_write_callback user_callback;
    void *user_context;
    long http_status;
    struct vcr_curl_state *next;
} vcr_curl_state_t;

static vcr_curl_state_t *g_vcr_curl_state_list = NULL;

// Helper functions for VCR integration
static vcr_curl_state_t *vcr_find_or_create_state(CURL *handle)
{
    // Find existing state
    for (vcr_curl_state_t *state = g_vcr_curl_state_list; state; state = state->next) {
        if (state->easy_handle == handle) {
            return state;
        }
    }

    // Create new state
    vcr_curl_state_t *state = calloc(1, sizeof(vcr_curl_state_t));
    state->easy_handle = handle;
    state->next = g_vcr_curl_state_list;
    g_vcr_curl_state_list = state;
    return state;
}

static vcr_curl_state_t *vcr_find_state(CURL *handle)
{
    for (vcr_curl_state_t *state = g_vcr_curl_state_list; state; state = state->next) {
        if (state->easy_handle == handle) {
            return state;
        }
    }
    return NULL;
}

static void vcr_remove_state(CURL *handle)
{
    vcr_curl_state_t **prev_ptr = &g_vcr_curl_state_list;
    for (vcr_curl_state_t *state = g_vcr_curl_state_list; state; state = state->next) {
        if (state->easy_handle == handle) {
            *prev_ptr = state->next;
            free(state);
            return;
        }
        prev_ptr = &state->next;
    }
}

// VCR write callback wrapper for record mode
static size_t vcr_write_callback_wrapper(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t realsize = size * nmemb;

    // In record mode, capture this chunk
    if (vcr_is_recording()) {
        vcr_record_chunk(ptr, realsize);
    }

    // Call user's original callback if set
    vcr_curl_state_t *state = (vcr_curl_state_t *)userdata;
    if (state && state->user_callback) {
        return state->user_callback(ptr, size, nmemb, state->user_context);
    }

    return realsize;
}

// Deliver VCR chunk to write callback in playback mode
// Note: curl write callbacks take non-const char*, but we know they won't modify the data
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
static void vcr_deliver_chunk_to_callback(CURL *handle, const char *chunk_data, size_t chunk_len)
{
    vcr_curl_state_t *state = vcr_find_state(handle);
    if (state && state->user_callback) {
        state->user_callback((char *)chunk_data, chunk_len, 1, state->user_context);
    }
}
#pragma GCC diagnostic pop

MOCKABLE CURL *curl_easy_init_(void)
{
    return curl_easy_init();
}

MOCKABLE void curl_easy_cleanup_(CURL *curl)
{
    // Clean up VCR state if present
    vcr_remove_state(curl);
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

// Suppress cast-qual warning for setopt - curl API requires non-const void*
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
MOCKABLE CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val)
{
    // Track write callbacks in VCR mode (both record and playback)
    if (vcr_is_active()) {
        if (opt == CURLOPT_WRITEFUNCTION) {
            vcr_curl_state_t *state = vcr_find_or_create_state(curl);
            state->user_callback = (curl_write_callback)val;

            // In record mode, wrap the callback to capture data
            if (vcr_is_recording()) {
                return curl_easy_setopt(curl, opt, vcr_write_callback_wrapper);
            }
            // In playback mode, just track it for later delivery
            return CURLE_OK;
        } else if (opt == CURLOPT_WRITEDATA) {
            vcr_curl_state_t *state = vcr_find_or_create_state(curl);
            state->user_context = (void *)val;

            // In record mode, pass our state to the wrapper
            if (vcr_is_recording()) {
                return curl_easy_setopt(curl, opt, state);
            }
            // In playback mode, just track it
            return CURLE_OK;
        }
    }

    return curl_easy_setopt(curl, opt, val);
}
#pragma GCC diagnostic pop

MOCKABLE CURLcode curl_easy_getinfo_(CURL *curl, CURLINFO info, ...)
{
    va_list args;
    va_start(args, info);
    void *param = va_arg(args, void *);
    va_end(args);

    // In VCR playback mode, return status from fixture
    if (vcr_is_active() && !vcr_is_recording() && info == CURLINFO_RESPONSE_CODE) {
        vcr_curl_state_t *state = vcr_find_state(curl);
        if (state) {
            long *status_ptr = (long *)param;
            *status_ptr = state->http_status;
            return CURLE_OK;
        }
    }

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

// VCR message tracking - forward declaration of variables defined later
static CURLMsg g_vcr_curl_msg;
static bool g_vcr_msg_delivered;

MOCKABLE CURLMcode curl_multi_add_handle_(CURLM *multi, CURL *easy)
{
    // In VCR playback mode, populate HTTP status from fixture
    if (vcr_is_active() && !vcr_is_recording()) {
        vcr_curl_state_t *state = vcr_find_or_create_state(easy);
        state->http_status = vcr_get_response_status();
        // Reset the message delivered flag for new requests
        g_vcr_msg_delivered = false;
    }

    return curl_multi_add_handle(multi, easy);
}

MOCKABLE CURLMcode curl_multi_remove_handle_(CURLM *multi, CURL *easy)
{
    return curl_multi_remove_handle(multi, easy);
}

MOCKABLE CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles)
{
    // VCR playback mode: deliver chunks from fixture
    if (vcr_is_active() && !vcr_is_recording()) {
        const char *chunk_data;
        size_t chunk_len;

        if (vcr_next_chunk(&chunk_data, &chunk_len)) {
            // Find the easy handle associated with this multi handle
            // For now, assume single handle per multi (common case)
            // Deliver chunk to the first state's callback
            for (vcr_curl_state_t *state = g_vcr_curl_state_list; state; state = state->next) {
                if (state->user_callback) {
                    vcr_deliver_chunk_to_callback(state->easy_handle, chunk_data, chunk_len);
                    break;  // Only deliver to first active handle
                }
            }

            // Update running handles based on whether more data available
            *running_handles = vcr_has_more() ? 1 : 0;
        } else {
            // No more data
            *running_handles = 0;
        }

        return CURLM_OK;
    }

    // Normal operation (real curl or record mode)
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
    // VCR playback mode: return synthetic CURLMSG_DONE when no more chunks
    if (vcr_is_active() && !vcr_is_recording()) {
        // Check if we've already delivered the message
        if (g_vcr_msg_delivered) {
            *msgs_in_queue = 0;
            return NULL;
        }

        // If there's no more data and we have an active curl handle, signal completion
        if (!vcr_has_more()) {
            for (vcr_curl_state_t *state = g_vcr_curl_state_list; state; state = state->next) {
                if (state->easy_handle) {
                    g_vcr_curl_msg.msg = CURLMSG_DONE;
                    g_vcr_curl_msg.easy_handle = state->easy_handle;
                    g_vcr_curl_msg.data.result = CURLE_OK;
                    g_vcr_msg_delivered = true;
                    *msgs_in_queue = 0;
                    return &g_vcr_curl_msg;
                }
            }
        }

        *msgs_in_queue = 0;
        return NULL;
    }

    return curl_multi_info_read(multi, msgs_in_queue);
}

MOCKABLE const char *curl_multi_strerror_(CURLMcode code)
{
    return curl_multi_strerror(code);
}

// LCOV_EXCL_STOP
#endif
