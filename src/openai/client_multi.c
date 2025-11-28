#include "openai/client_multi_internal.h"

#include "error.h"
#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <curl/curl.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>

/**
 * OpenAI multi-handle client core implementation
 *
 * Provides lifecycle management and event loop operations.
 * Request management is in client_multi_request.c
 */

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
                    ik_http_completion_t completion = {0};
                    completion.curl_code = curl_result;
                    completion.model = NULL;
                    completion.finish_reason = NULL;
                    completion.completion_tokens = 0;

                    if (curl_result == CURLE_OK) {
                        /* Get HTTP response code */
                        long response_code = 0;
                        curl_easy_getinfo_(easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
                        completion.http_code = (int32_t)response_code;

                        /* Categorize response */
                        if (response_code >= 200 && response_code < 300) {
                            completion.type = IK_HTTP_SUCCESS;
                            completion.error_message = NULL;

                            /* Transfer metadata from write context */
                            if (completed->write_ctx->model != NULL) {
                                completion.model = talloc_steal(multi, completed->write_ctx->model);
                            }
                            if (completed->write_ctx->finish_reason != NULL) {
                                completion.finish_reason = talloc_steal(multi, completed->write_ctx->finish_reason);
                            }
                            completion.completion_tokens = completed->write_ctx->completion_tokens;
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
                            /* Free error message and metadata from completion */
                            if (completion.error_message != NULL) {
                                talloc_free(completion.error_message);
                            }
                            if (completion.model != NULL) {
                                talloc_free(completion.model);
                            }
                            if (completion.finish_reason != NULL) {
                                talloc_free(completion.finish_reason);
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

                    /* Free error message and metadata */
                    if (completion.error_message != NULL) {
                        talloc_free(completion.error_message);
                    }
                    if (completion.model != NULL) {
                        talloc_free(completion.model);
                    }
                    if (completion.finish_reason != NULL) {
                        talloc_free(completion.finish_reason);
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

    return OK(NULL);
}
