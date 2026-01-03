#include "providers/common/http_multi.h"

#include "providers/common/http_multi_internal.h"
#include "wrapper.h"

#include <assert.h>
#include <inttypes.h>
#include <talloc.h>

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
