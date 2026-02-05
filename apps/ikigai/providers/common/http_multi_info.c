#include "apps/ikigai/providers/common/http_multi.h"

#include "apps/ikigai/providers/common/http_multi_internal.h"
#include "shared/wrapper.h"

#include <assert.h>
#include <inttypes.h>
#include <talloc.h>


#include "shared/poison.h"
static void categorize_http_response(ik_http_multi_t *multi, long response_code,
                                      ik_http_completion_t *completion)
{
    if (response_code >= 200 && response_code < 300) {
        completion->type = IK_HTTP_SUCCESS;
        completion->error_message = NULL;
    } else if (response_code >= 400 && response_code < 500) {
        completion->type = IK_HTTP_CLIENT_ERROR;
        completion->error_message = talloc_asprintf(multi, "HTTP %ld error", response_code);
    } else if (response_code >= 500 && response_code < 600) {
        completion->type = IK_HTTP_SERVER_ERROR;
        completion->error_message = talloc_asprintf(multi, "HTTP %ld server error", response_code);
    } else {
        completion->type = IK_HTTP_NETWORK_ERROR;
        completion->error_message = talloc_asprintf(multi, "Unexpected HTTP response code: %ld",
                                                    response_code);
    }
}

static void build_completion_for_success(ik_http_multi_t *multi, CURL *easy_handle,
                                          active_request_t *completed,
                                          ik_http_completion_t *completion)
{
    long response_code = 0;
    curl_easy_getinfo_(easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
    completion->http_code = (int32_t)response_code;

    categorize_http_response(multi, response_code, completion);

    if (completed->write_ctx->response_buffer != NULL) {
        completion->response_body = talloc_steal(multi, completed->write_ctx->response_buffer);
        completion->response_len = completed->write_ctx->response_len;
        completed->write_ctx->response_buffer = NULL;
    }
}

static void build_completion_for_error(ik_http_multi_t *multi, CURLcode curl_result,
                                        ik_http_completion_t *completion)
{
    completion->type = IK_HTTP_NETWORK_ERROR;
    completion->http_code = 0;
    completion->error_message = talloc_asprintf(multi, "Connection error: %s",
                                                curl_easy_strerror_(curl_result));
}

static void cleanup_completed_request(ik_http_multi_t *multi, CURL *easy_handle,
                                       active_request_t *completed)
{
    curl_multi_remove_handle_(multi->multi_handle, easy_handle);
    curl_easy_cleanup_(easy_handle);
    curl_slist_free_all_(completed->headers);
    talloc_free(completed);
}

static void remove_from_active_array(ik_http_multi_t *multi, size_t index)
{
    for (size_t j = index; j < multi->active_count - 1; j++) {
        multi->active_requests[j] = multi->active_requests[j + 1];
    }
    multi->active_count--;
}

static void invoke_completion_callback(active_request_t *completed, ik_http_completion_t *completion)
{
    if (completed->completion_cb != NULL) {
        completed->completion_cb(completion, completed->completion_ctx);
    }
}

static void cleanup_completion_resources(ik_http_completion_t *completion)
{
    if (completion->error_message != NULL) {
        talloc_free(completion->error_message);
    }
    if (completion->response_body != NULL) {
        talloc_free(completion->response_body);
    }
}

static void process_completed_request(ik_http_multi_t *multi, CURL *easy_handle,
                                       CURLcode curl_result, size_t index)
{
    active_request_t *completed = multi->active_requests[index];

    ik_http_completion_t completion = {0};
    completion.curl_code = curl_result;
    completion.response_body = NULL;
    completion.response_len = 0;

    if (curl_result == CURLE_OK) {
        build_completion_for_success(multi, easy_handle, completed, &completion);
    } else {
        build_completion_for_error(multi, curl_result, &completion);
    }

    invoke_completion_callback(completed, &completion);
    cleanup_completion_resources(&completion);
    cleanup_completed_request(multi, easy_handle, completed);
    remove_from_active_array(multi, index);
}

static bool find_and_process_completed_request(ik_http_multi_t *multi, CURL *easy_handle,
                                                CURLcode curl_result)
{
    for (size_t i = 0; i < multi->active_count; i++) {
        if (multi->active_requests[i]->easy_handle == easy_handle) {
            process_completed_request(multi, easy_handle, curl_result, i);
            return true;
        }
    }
    return false;
}

void ik_http_multi_info_read(ik_http_multi_t *multi, ik_logger_t *logger)
{
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    (void)logger;

    int msgs_left;
    CURLMsg *msg;

    while ((msg = curl_multi_info_read_(multi->multi_handle, &msgs_left)) != NULL) {
        if (msg->msg != CURLMSG_DONE) {
            continue;
        }

        CURL *easy_handle = msg->easy_handle;
        CURLcode curl_result = msg->data.result;
        find_and_process_completed_request(multi, easy_handle, curl_result);
    }
}

void ik_http_multi_cancel_all(ik_http_multi_t *multi)
{
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE

    // Remove and cleanup all active requests without invoking callbacks
    while (multi->active_count > 0) {
        active_request_t *req = multi->active_requests[0];
        curl_multi_remove_handle_(multi->multi_handle, req->easy_handle);
        curl_easy_cleanup_(req->easy_handle);
        curl_slist_free_all_(req->headers);
        talloc_free(req);
        remove_from_active_array(multi, 0);
    }
}
