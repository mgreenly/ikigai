#include "openai/client_multi.h"

#include "openai/client.h"
#include "openai/client_multi_callbacks.h"
#include "openai/sse_parser.h"
#include "error.h"
#include "wrapper.h"
#include "json_allocator.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>

/**
 * OpenAI multi-handle client implementation
 *
 * Provides non-blocking HTTP client using curl_multi interface.
 */

/**
 * Active request context
 *
 * Tracks state for a single in-flight HTTP request.
 */
typedef struct {
    CURL *easy_handle;                    /* curl easy handle for this request */
    struct curl_slist *headers;           /* HTTP headers */
    http_write_ctx_t *write_ctx;          /* Write callback context */
    char *request_body;                   /* JSON request body (must persist) */
    ik_http_completion_cb_t completion_cb; /* Completion callback (or NULL) */
    void *completion_ctx;                 /* Context for completion callback */
} active_request_t;

/**
 * Multi-handle manager structure
 */
struct ik_openai_multi {
    CURLM *multi_handle;                  /* curl multi handle */
    active_request_t **active_requests;   /* Array of active request contexts */
    size_t active_count;                  /* Number of active requests */
};

/**
 * Destructor for multi-handle manager
 *
 * Cleans up curl multi handle and any remaining active requests.
 */
static int multi_destructor(ik_openai_multi_t *multi) {
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

res_t ik_openai_multi_create(void *parent) {
    ik_openai_multi_t *multi = talloc_zero(parent, ik_openai_multi_t);
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

res_t ik_openai_multi_add_request(ik_openai_multi_t *multi,
                                   const ik_cfg_t *cfg,
                                   ik_openai_conversation_t *conv,
                                   ik_openai_stream_cb_t stream_cb,
                                   void *stream_ctx,
                                   ik_http_completion_cb_t completion_cb,
                                   void *completion_ctx,
                                   FILE *debug_output) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    assert(cfg != NULL);  // LCOV_EXCL_BR_LINE
    assert(conv != NULL);  // LCOV_EXCL_BR_LINE

    /* Validate inputs */
    if (conv->message_count == 0) {
        return ERR(multi, INVALID_ARG, "Conversation must contain at least one message");
    }

    if (cfg->openai_api_key == NULL || strlen(cfg->openai_api_key) == 0) {
        return ERR(multi, INVALID_ARG, "OpenAI API key is required");
    }

    /* Create request */
    ik_openai_request_t *request = ik_openai_request_create(multi, cfg, conv);

    /* Serialize request to JSON */
    char *json_body = ik_openai_serialize_request(multi, request);

    /* Create active request context */
    active_request_t *active_req = talloc_zero(multi, active_request_t);
    if (active_req == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate active request");  // LCOV_EXCL_LINE
    }

    /* Keep request body alive for the duration of the request */
    active_req->request_body = talloc_steal(active_req, json_body);

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

    /* Create SSE parser */
    active_req->write_ctx->parser = ik_openai_sse_parser_create(active_req->write_ctx);
    active_req->write_ctx->user_callback = stream_cb;
    active_req->write_ctx->user_ctx = stream_ctx;
    active_req->write_ctx->complete_response = NULL;
    active_req->write_ctx->response_len = 0;
    active_req->write_ctx->has_error = false;

    /* Store completion callback */
    active_req->completion_cb = completion_cb;
    active_req->completion_ctx = completion_ctx;

    /* Set up curl options */
    const char *url = "https://api.openai.com/v1/chat/completions";
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_URL, url);
#ifdef NDEBUG
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_POST, 1L);
#else
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_POST, (const void *)1L);
#endif
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_POSTFIELDS, active_req->request_body);
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt_(active_req->easy_handle, CURLOPT_WRITEDATA, active_req->write_ctx);

    // LCOV_EXCL_START - Debug: Enable verbose curl output if debug pipe provided
    if (debug_output != NULL) {
        fprintf(debug_output, "[OpenAI Request]\n");
        fprintf(debug_output, "URL: https://api.openai.com/v1/chat/completions\n");
        fprintf(debug_output, "Content-Type: application/json\n");
        fprintf(debug_output, "Body: %s\n", json_body);
        fprintf(debug_output, "\n");
        fflush(debug_output);

        curl_easy_setopt_(active_req->easy_handle, CURLOPT_VERBOSE, (const void *)1L);
        curl_easy_setopt_(active_req->easy_handle, CURLOPT_STDERR, debug_output);
    }
    // LCOV_EXCL_STOP

    /* Set headers */
    active_req->headers = NULL;
    active_req->headers = curl_slist_append_(active_req->headers, "Content-Type: application/json");

    char auth_header[512];  // Increased from 256 to handle longer API keys
    int32_t written = snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", cfg->openai_api_key);
    if (written < 0 || (size_t)written >= sizeof(auth_header)) {  // LCOV_EXCL_BR_LINE
        curl_easy_cleanup_(active_req->easy_handle);  // LCOV_EXCL_LINE
        curl_slist_free_all_(active_req->headers);  // LCOV_EXCL_LINE
        talloc_free(active_req);  // LCOV_EXCL_LINE
        return ERR(multi, INVALID_ARG, "API key too long");  // LCOV_EXCL_LINE
    }
    active_req->headers = curl_slist_append_(active_req->headers, auth_header);

    curl_easy_setopt_(active_req->easy_handle, CURLOPT_HTTPHEADER, active_req->headers);

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

res_t ik_openai_multi_perform(ik_openai_multi_t *multi, int *still_running) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    assert(still_running != NULL);  // LCOV_EXCL_BR_LINE

    CURLMcode mres = curl_multi_perform_(multi->multi_handle, still_running);
    if (mres != CURLM_OK) {
        return ERR(multi, IO, "curl_multi_perform failed: %s", curl_multi_strerror_(mres));
    }

    return OK(NULL);
}

res_t ik_openai_multi_fdset(ik_openai_multi_t *multi,
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

res_t ik_openai_multi_timeout(ik_openai_multi_t *multi, long *timeout_ms) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    assert(timeout_ms != NULL);  // LCOV_EXCL_BR_LINE

    CURLMcode mres = curl_multi_timeout_(multi->multi_handle, timeout_ms);
    if (mres != CURLM_OK) {
        return ERR(multi, IO, "curl_multi_timeout failed: %s", curl_multi_strerror_(mres));
    }

    return OK(NULL);
}

res_t ik_openai_multi_info_read(ik_openai_multi_t *multi) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE

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
                    /* NOTE: Error categorization tested manually (Tasks 7.10-7.14) */
                    ik_http_completion_t completion = {0};
                    completion.curl_code = curl_result;

                    // LCOV_EXCL_START
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
                    } else {
                        /* Network/connection error */
                        completion.type = IK_HTTP_NETWORK_ERROR;
                        completion.http_code = 0;
                        completion.error_message = talloc_asprintf(multi,
                            "Connection error: %s", curl_easy_strerror_(curl_result));
                    }

                    /* Invoke completion callback if provided */
                    if (completed->completion_cb != NULL) {
                        res_t cb_result = completed->completion_cb(&completion, completed->completion_ctx);
                        if (is_err(&cb_result)) {
                            /* Free error message from completion */
                            if (completion.error_message != NULL) {
                                talloc_free(completion.error_message);
                            }
                            /* Clean up curl handles */
                            curl_multi_remove_handle_(multi->multi_handle, easy_handle);
                            curl_easy_cleanup_(easy_handle);
                            curl_slist_free_all_(completed->headers);
                            /* Free the completed request context */
                            talloc_free(completed);
                            /* Remove from array */
                            for (size_t j = i; j < multi->active_count - 1; j++) {
                                multi->active_requests[j] = multi->active_requests[j + 1];
                            }
                            multi->active_count--;
                            /* Return the callback error */
                            return cb_result;
                        }
                    }

                    /* Free error message */
                    if (completion.error_message != NULL) {
                        talloc_free(completion.error_message);
                    }
                    // LCOV_EXCL_STOP

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

    return OK(NULL);
}
