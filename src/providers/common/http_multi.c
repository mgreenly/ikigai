#include "providers/common/http_multi.h"

#include "error.h"
#include "logger.h"
#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <curl/curl.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>

/**
 * Shared HTTP multi-handle client implementation
 *
 * Provides generic async HTTP layer for all provider adapters.
 * Pattern based on src/openai/client_multi.c but generalized.
 */

/**
 * Write callback context
 *
 * Accumulates response data as it arrives.
 */
typedef struct {
    ik_http_write_cb_t user_callback;  /* User's streaming callback (or NULL) */
    void *user_ctx;                    /* Context for user callback */
    char *response_buffer;             /* Accumulated response data */
    size_t response_len;               /* Current response length */
    size_t buffer_capacity;            /* Allocated buffer size */
} http_write_ctx_t;

/**
 * Active request context
 *
 * Tracks state for a single in-flight HTTP request.
 */
typedef struct {
    CURL *easy_handle;                      /* curl easy handle for this request */
    struct curl_slist *headers;             /* HTTP headers */
    http_write_ctx_t *write_ctx;            /* Write callback context */
    char *request_body;                     /* Request body (must persist) */
    ik_http_completion_cb_t completion_cb;  /* Completion callback (or NULL) */
    void *completion_ctx;                   /* Context for completion callback */
} active_request_t;

/**
 * Multi-handle manager structure
 */
struct ik_http_multi {
    CURLM *multi_handle;                /* curl multi handle */
    active_request_t **active_requests; /* Array of active request contexts */
    size_t active_count;                /* Number of active requests */
};

/**
 * Write callback for curl
 *
 * Called by curl as response data arrives.
 */
static size_t http_write_callback(const char *data, size_t size, size_t nmemb, void *userdata) {
    http_write_ctx_t *ctx = (http_write_ctx_t *)userdata;
    size_t total_size = size * nmemb;

    /* Call user's streaming callback if provided */
    if (ctx->user_callback != NULL) {
        size_t processed = ctx->user_callback(data, total_size, ctx->user_ctx);
        if (processed != total_size) {
            /* User callback indicated error by not consuming all data */
            return 0;  /* Signal error to curl */
        }
    }

    /* Accumulate response data for completion callback */
    size_t new_len = ctx->response_len + total_size;
    if (new_len + 1 > ctx->buffer_capacity) {
        /* Grow buffer (double size or fit new data, whichever is larger) */
        size_t new_capacity = ctx->buffer_capacity * 2;
        if (new_capacity < new_len + 1) {
            new_capacity = new_len + 1;
        }

        char *new_buffer = talloc_realloc_(ctx, ctx->response_buffer, new_capacity);
        if (new_buffer == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
        ctx->response_buffer = new_buffer;
        ctx->buffer_capacity = new_capacity;
    }

    /* Append data to buffer */
    memcpy(ctx->response_buffer + ctx->response_len, data, total_size);
    ctx->response_len = new_len;
    ctx->response_buffer[ctx->response_len] = '\0';  /* Null terminate */

    return total_size;
}

/**
 * Destructor for multi-handle manager
 *
 * Cleans up curl multi handle and any remaining active requests.
 */
static int multi_destructor(ik_http_multi_t *multi) {
    /* Clean up any remaining active requests */
    for (size_t i = 0; i < multi->active_count; i++) {
        active_request_t *req = multi->active_requests[i];
        curl_multi_remove_handle_(multi->multi_handle, req->easy_handle);
        curl_easy_cleanup_(req->easy_handle);
        curl_slist_free_all_(req->headers);
        /* talloc will free req and its children */
    }

    /* Clean up multi handle */
    if (multi->multi_handle != NULL) {  // LCOV_EXCL_BR_LINE
        curl_multi_cleanup_(multi->multi_handle);
    }

    return 0;  /* Success */
}

res_t ik_http_multi_create(void *parent) {
    ik_http_multi_t *multi = talloc_zero(parent, ik_http_multi_t);
    if (multi == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate multi-handle manager");  // LCOV_EXCL_LINE
    }

    multi->multi_handle = curl_multi_init_();
    if (multi->multi_handle == NULL) {
        talloc_free(multi);
        return ERR(parent, IO, "Failed to initialize curl multi handle");
    }

    multi->active_requests = NULL;
    multi->active_count = 0;

    /* Register destructor */
    talloc_set_destructor(multi, multi_destructor);

    return OK(multi);
}

res_t ik_http_multi_perform(ik_http_multi_t *multi, int *still_running) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    assert(still_running != NULL);  // LCOV_EXCL_BR_LINE

    CURLMcode mres = curl_multi_perform_(multi->multi_handle, still_running);
    if (mres != CURLM_OK) {
        return ERR(multi, IO, "curl_multi_perform failed: %s", curl_multi_strerror_(mres));
    }

    return OK(NULL);
}

res_t ik_http_multi_fdset(ik_http_multi_t *multi,
                           fd_set *read_fds, fd_set *write_fds,
                           fd_set *exc_fds, int *max_fd) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    assert(read_fds != NULL);  // LCOV_EXCL_BR_LINE
    assert(write_fds != NULL);  // LCOV_EXCL_BR_LINE
    assert(exc_fds != NULL);  // LCOV_EXCL_BR_LINE
    assert(max_fd != NULL);  // LCOV_EXCL_BR_LINE

    CURLMcode mres = curl_multi_fdset_(multi->multi_handle, read_fds, write_fds, exc_fds, max_fd);
    if (mres != CURLM_OK) {
        return ERR(multi, IO, "curl_multi_fdset failed: %s", curl_multi_strerror_(mres));
    }

    return OK(NULL);
}

res_t ik_http_multi_timeout(ik_http_multi_t *multi, long *timeout_ms) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    assert(timeout_ms != NULL);  // LCOV_EXCL_BR_LINE

    CURLMcode mres = curl_multi_timeout_(multi->multi_handle, timeout_ms);
    if (mres != CURLM_OK) {
        return ERR(multi, IO, "curl_multi_timeout failed: %s", curl_multi_strerror_(mres));
    }

    return OK(NULL);
}

void ik_http_multi_info_read(ik_http_multi_t *multi, ik_logger_t *logger) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    (void)logger;  /* May be NULL, used for future logging */

    int msgs_left;
    CURLMsg *msg;

    while ((msg = curl_multi_info_read_(multi->multi_handle, &msgs_left)) != NULL) {
        if (msg->msg == CURLMSG_DONE) {
            CURL *easy_handle = msg->easy_handle;
            CURLcode curl_result = msg->data.result;

            /* Find and remove the completed request from active_requests array */
            for (size_t i = 0; i < multi->active_count; i++) {
                if (multi->active_requests[i]->easy_handle == easy_handle) {
                    active_request_t *completed = multi->active_requests[i];

                    /* Build completion information */
                    ik_http_completion_t completion = {0};
                    completion.curl_code = curl_result;
                    completion.response_body = NULL;
                    completion.response_len = 0;

                    if (curl_result == CURLE_OK) {
                        /* Get HTTP response code */
                        long response_code = 0;
                        curl_easy_getinfo_(easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
                        completion.http_code = (int32_t)response_code;

                        /* Categorize response */
                        if (response_code >= 200 && response_code < 300) {
                            completion.type = IK_HTTP_SUCCESS;
                            completion.error_message = NULL;
                        } else if (response_code >= 400 && response_code < 500) {
                            completion.type = IK_HTTP_CLIENT_ERROR;
                            completion.error_message = talloc_asprintf(multi,
                                "HTTP %ld error", response_code);
                        } else if (response_code >= 500 && response_code < 600) {
                            completion.type = IK_HTTP_SERVER_ERROR;
                            completion.error_message = talloc_asprintf(multi,
                                "HTTP %ld server error", response_code);
                        } else {
                            /* Unexpected response code */
                            completion.type = IK_HTTP_NETWORK_ERROR;
                            completion.error_message = talloc_asprintf(multi,
                                "Unexpected HTTP response code: %ld", response_code);
                        }

                        /* Transfer response body to completion */
                        if (completed->write_ctx->response_buffer != NULL) {
                            completion.response_body = talloc_steal(multi, completed->write_ctx->response_buffer);
                            completion.response_len = completed->write_ctx->response_len;
                            completed->write_ctx->response_buffer = NULL;
                        }
                    } else {
                        /* Network/connection error */
                        completion.type = IK_HTTP_NETWORK_ERROR;
                        completion.http_code = 0;
                        completion.error_message = talloc_asprintf(multi,
                            "Connection error: %s", curl_easy_strerror_(curl_result));
                    }

                    /* Invoke completion callback if provided */
                    if (completed->completion_cb != NULL) {
                        completed->completion_cb(&completion, completed->completion_ctx);
                    }

                    /* Free error message and response body */
                    if (completion.error_message != NULL) {
                        talloc_free(completion.error_message);
                    }
                    if (completion.response_body != NULL) {
                        talloc_free(completion.response_body);
                    }

                    /* Clean up curl handles */
                    curl_multi_remove_handle_(multi->multi_handle, easy_handle);
                    curl_easy_cleanup_(easy_handle);
                    curl_slist_free_all_(completed->headers);

                    /* Free the completed request context (talloc will clean up children) */
                    talloc_free(completed);

                    /* Remove from array by shifting remaining elements */
                    for (size_t j = i; j < multi->active_count - 1; j++) {
                        multi->active_requests[j] = multi->active_requests[j + 1];
                    }
                    multi->active_count--;

                    break;
                }
            }
        }
    }
}

res_t ik_http_multi_add_request(ik_http_multi_t *multi,
                                 const ik_http_request_t *req,
                                 ik_http_write_cb_t write_cb,
                                 void *write_ctx,
                                 ik_http_completion_cb_t completion_cb,
                                 void *completion_ctx) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE
    assert(req->url != NULL);  // LCOV_EXCL_BR_LINE

    /* Create active request context */
    active_request_t *active_req = talloc_zero(multi, active_request_t);
    if (active_req == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate active request");  // LCOV_EXCL_LINE
    }

    /* Initialize libcurl easy handle */
    active_req->easy_handle = curl_easy_init_();
    if (active_req->easy_handle == NULL) {
        talloc_free(active_req);
        return ERR(multi, IO, "Failed to initialize curl easy handle");
    }

    /* Create write callback context */
    active_req->write_ctx = talloc_zero(active_req, http_write_ctx_t);
    if (active_req->write_ctx == NULL) {  // LCOV_EXCL_BR_LINE
        curl_easy_cleanup_(active_req->easy_handle);  // LCOV_EXCL_LINE
        talloc_free(active_req);  // LCOV_EXCL_LINE
        PANIC("Failed to allocate write context");  // LCOV_EXCL_LINE
    }

    /* Initialize write context */
    active_req->write_ctx->user_callback = write_cb;
    active_req->write_ctx->user_ctx = write_ctx;
    active_req->write_ctx->response_buffer = talloc_array(active_req->write_ctx, char, 4096);
    if (active_req->write_ctx->response_buffer == NULL) {  // LCOV_EXCL_BR_LINE
        curl_easy_cleanup_(active_req->easy_handle);  // LCOV_EXCL_LINE
        talloc_free(active_req);  // LCOV_EXCL_LINE
        PANIC("Failed to allocate response buffer");  // LCOV_EXCL_LINE
    }
    active_req->write_ctx->response_len = 0;
    active_req->write_ctx->buffer_capacity = 4096;
    active_req->write_ctx->response_buffer[0] = '\0';

    /* Store completion callback */
    active_req->completion_cb = completion_cb;
    active_req->completion_ctx = completion_ctx;

    /* Set URL */
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_URL, req->url);

    /* Set method (default to GET if not specified) */
    if (req->method != NULL) {
        if (strcmp(req->method, "POST") == 0) {
#ifdef NDEBUG
            curl_easy_setopt_(active_req->easy_handle, CURLOPT_POST, 1L);
#else
            curl_easy_setopt_(active_req->easy_handle, CURLOPT_POST, (const void *)1L);
#endif
        } else if (strcmp(req->method, "GET") == 0) {
            /* GET is default, no action needed */
        } else {
            /* Custom method */
            curl_easy_setopt_(active_req->easy_handle, CURLOPT_CUSTOMREQUEST, req->method);
        }
    }

    /* Set request body if provided */
    if (req->body != NULL && req->body_len > 0) {
        /* Keep request body alive for duration of request */
        active_req->request_body = talloc_memdup(active_req, req->body, req->body_len + 1);
        if (active_req->request_body == NULL) {  // LCOV_EXCL_BR_LINE
            curl_easy_cleanup_(active_req->easy_handle);  // LCOV_EXCL_LINE
            talloc_free(active_req);  // LCOV_EXCL_LINE
            PANIC("Failed to copy request body");  // LCOV_EXCL_LINE
        }
        curl_easy_setopt_(active_req->easy_handle, CURLOPT_POSTFIELDS, active_req->request_body);
    }

    /* Set write callback */
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_WRITEDATA, active_req->write_ctx);

    /* Set headers if provided */
    active_req->headers = NULL;
    if (req->headers != NULL) {
        for (size_t i = 0; req->headers[i] != NULL; i++) {
            active_req->headers = curl_slist_append_(active_req->headers, req->headers[i]);
        }
        curl_easy_setopt_(active_req->easy_handle, CURLOPT_HTTPHEADER, active_req->headers);
    }

    /* Add to multi handle */
    CURLMcode mres = curl_multi_add_handle_(multi->multi_handle, active_req->easy_handle);
    if (mres != CURLM_OK) {
        curl_easy_cleanup_(active_req->easy_handle);
        curl_slist_free_all_(active_req->headers);
        talloc_free(active_req);
        return ERR(multi, IO, "Failed to add handle to multi: %s", curl_multi_strerror_(mres));
    }

    /* Add to active requests array */
    active_request_t **new_array = talloc_realloc_(multi, multi->active_requests,
                                                    sizeof(active_request_t *) * (multi->active_count + 1));
    if (new_array == NULL) {  // LCOV_EXCL_BR_LINE
        curl_multi_remove_handle_(multi->multi_handle, active_req->easy_handle);  // LCOV_EXCL_LINE
        curl_easy_cleanup_(active_req->easy_handle);  // LCOV_EXCL_LINE
        curl_slist_free_all_(active_req->headers);  // LCOV_EXCL_LINE
        talloc_free(active_req);  // LCOV_EXCL_LINE
        PANIC("Failed to resize active requests array");  // LCOV_EXCL_LINE
    }

    multi->active_requests = new_array;
    multi->active_requests[multi->active_count] = active_req;
    multi->active_count++;

    return OK(NULL);
}
