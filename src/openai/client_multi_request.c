#include "openai/client_multi_internal.h"

#include "credentials.h"
#include "openai/client.h"
#include "openai/sse_parser.h"
#include "openai/tool_choice.h"
#include "error.h"
#include "logger.h"
#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <curl/curl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/**
 * Request management for client_multi
 *
 * Handles adding new requests to the multi-handle manager.
 */

res_t ik_openai_multi_add_request(ik_openai_multi_t *multi,
                                   const ik_cfg_t *cfg,
                                   ik_openai_conversation_t *conv,
                                   ik_openai_stream_cb_t stream_cb,
                                   void *stream_ctx,
                                   ik_http_completion_cb_t completion_cb,
                                   void *completion_ctx,
                                   bool limit_reached,
                                   ik_logger_t *logger) {
    assert(multi != NULL);  // LCOV_EXCL_BR_LINE
    assert(cfg != NULL);  // LCOV_EXCL_BR_LINE
    assert(conv != NULL);  // LCOV_EXCL_BR_LINE

    // Validate inputs
    if (conv->message_count == 0) {
        return ERR(multi, INVALID_ARG, "Conversation must contain at least one message");
    }

    // Load credentials
    ik_credentials_t *creds = NULL;
    res_t creds_res = ik_credentials_load(multi, NULL, &creds);
    if (creds_res.is_err) {
        return creds_res;
    }

    // Get OpenAI API key
    const char *api_key = ik_credentials_get(creds, "openai");
    if (api_key == NULL || strlen(api_key) == 0) {
        return ERR(multi, INVALID_ARG, "No OpenAI credentials. Set OPENAI_API_KEY or add to ~/.config/ikigai/credentials.json");
    }

    // Create request
    ik_openai_request_t *request = ik_openai_request_create(multi, cfg, conv);

    // Serialize request to JSON with tool_choice based on limit_reached
    ik_tool_choice_t tool_choice = limit_reached ? ik_tool_choice_none() : ik_tool_choice_auto();
    char *json_body = ik_openai_serialize_request(multi, request, tool_choice);

    // Create active request context
    active_request_t *active_req = talloc_zero(multi, active_request_t);
    if (active_req == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate active request");  // LCOV_EXCL_LINE
    }

    // Keep request body alive for the duration of the request
    active_req->request_body = talloc_steal(active_req, json_body);

    // Initialize libcurl easy handle
    active_req->easy_handle = curl_easy_init_();
    if (active_req->easy_handle == NULL) {
        talloc_free(active_req);
        return ERR(multi, IO, "Failed to initialize curl easy handle");
    }

    // Create write callback context
    active_req->write_ctx = talloc_zero(active_req, http_write_ctx_t);
    if (active_req->write_ctx == NULL) {  // LCOV_EXCL_BR_LINE
        curl_easy_cleanup_(active_req->easy_handle);  // LCOV_EXCL_LINE
        talloc_free(active_req);  // LCOV_EXCL_LINE
        PANIC("Failed to allocate write context");  // LCOV_EXCL_LINE
    }

    // Create SSE parser
    active_req->write_ctx->parser = ik_openai_sse_parser_create(active_req->write_ctx);
    active_req->write_ctx->user_callback = stream_cb;
    active_req->write_ctx->user_ctx = stream_ctx;
    active_req->write_ctx->complete_response = NULL;
    active_req->write_ctx->response_len = 0;
    active_req->write_ctx->has_error = false;

    // Store completion callback
    active_req->completion_cb = completion_cb;
    active_req->completion_ctx = completion_ctx;

    // Set up curl options
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

    // Set headers
    active_req->headers = NULL;
    active_req->headers = curl_slist_append_(active_req->headers, "Content-Type: application/json");

    char auth_header[512];  // Increased from 256 to handle longer API keys
    int32_t written = snprintf_(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
    if (written < 0 || (size_t)written >= sizeof(auth_header)) {
        curl_easy_cleanup_(active_req->easy_handle);
        curl_slist_free_all_(active_req->headers);
        talloc_free(active_req);
        return ERR(multi, INVALID_ARG, "API key too long");
    }
    active_req->headers = curl_slist_append_(active_req->headers, auth_header);

    curl_easy_setopt_(active_req->easy_handle, CURLOPT_HTTPHEADER, active_req->headers);

    // Log HTTP request
    yyjson_mut_doc *log_doc = ik_log_create();
    if (log_doc != NULL) {  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root_wrapper(log_doc);

        // Add event field
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "http_request");

        // Add method field
        yyjson_mut_obj_add_str(log_doc, log_root, "method", "POST");

        // Add url field
        yyjson_mut_obj_add_str(log_doc, log_root, "url", url);

        // Add headers object (excluding Authorization)
        yyjson_mut_val *headers_obj = yyjson_mut_obj_add_obj_wrapper(log_doc, log_root, "headers");
        yyjson_mut_obj_add_str(log_doc, headers_obj, "Content-Type", "application/json");

        // Parse request body JSON and add as body object
        yyjson_doc *body_doc = yyjson_read(active_req->request_body, strlen(active_req->request_body), 0);
        if (body_doc != NULL) {  // LCOV_EXCL_BR_LINE
            yyjson_val *body_root = yyjson_doc_get_root(body_doc);
            yyjson_mut_val *body_copy = yyjson_val_mut_copy(log_doc, body_root);
            yyjson_mut_obj_add_val(log_doc, log_root, "body", body_copy);
            yyjson_doc_free(body_doc);
        }

        ik_logger_debug_json(logger, log_doc);
    }

    // Add to multi handle
    CURLMcode mres = curl_multi_add_handle_(multi->multi_handle, active_req->easy_handle);
    if (mres != CURLM_OK) {
        curl_easy_cleanup_(active_req->easy_handle);
        curl_slist_free_all_(active_req->headers);
        talloc_free(active_req);
        return ERR(multi, IO, "Failed to add handle to multi: %s", curl_multi_strerror_(mres));
    }

    // Add to active requests array
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
