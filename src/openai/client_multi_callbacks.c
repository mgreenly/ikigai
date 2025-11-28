// Internal HTTP callback handlers for OpenAI multi client
#include "openai/client_multi_callbacks.h"
#include "openai/sse_parser.h"
#include "panic.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>
#include "vendor/yyjson/yyjson.h"

/*
 * Wrapper for yyjson_get_int inline function
 *
 * This wrapper consolidates the inline expansion to a single location.
 * The false branch (non-int) cannot occur because we check yyjson_is_int()
 * before calling this wrapper.
 */
static int64_t yyjson_get_int_wrapper(yyjson_val *val) {  // LCOV_EXCL_BR_LINE
    return (int64_t)yyjson_get_int(val);
}

/*
 * Extract model from SSE event
 *
 * Parses the raw SSE event JSON to extract the model field if present.
 *
 * @param parent  Talloc context parent
 * @param event   Raw SSE event string (with "data: " prefix)
 * @return        Model string or NULL if not present
 */
static char *extract_model(void *parent, const char *event) {
    assert(event != NULL); // LCOV_EXCL_BR_LINE

    /* Check for "data: " prefix */
    const char *data_prefix = "data: ";
    if (strncmp(event, data_prefix, strlen(data_prefix)) != 0) { // LCOV_EXCL_BR_LINE
        return NULL; // LCOV_EXCL_LINE
    }

    /* Get JSON payload */
    const char *json_str = event + strlen(data_prefix);

    /* Check for [DONE] marker */
    if (strcmp(json_str, "[DONE]") == 0) {
        return NULL;
    }

    /* Parse JSON */
    yyjson_doc *doc = yyjson_read(json_str, strlen(json_str), 0);
    if (!doc) { // LCOV_EXCL_BR_LINE
        return NULL; // LCOV_EXCL_LINE
    }

    /* Validate root is an object */
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL; // LCOV_EXCL_LINE
    }

    /* Extract model field */
    yyjson_val *model_val = yyjson_obj_get(root, "model");
    if (!model_val || !yyjson_is_str(model_val)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL;
    }

    /* Extract string */
    const char *model_str = yyjson_get_str(model_val);
    char *result = talloc_strdup(parent, model_str);
    if (!result) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate model string"); // LCOV_EXCL_LINE
    }

    yyjson_doc_free(doc);
    return result;
}

/*
 * Extract completion_tokens from SSE event
 *
 * Parses the raw SSE event JSON to extract usage.completion_tokens if present.
 *
 * @param event   Raw SSE event string (with "data: " prefix)
 * @return        Completion token count or -1 if not present
 */
static int32_t extract_completion_tokens(const char *event) {
    assert(event != NULL); // LCOV_EXCL_BR_LINE

    /* Check for "data: " prefix */
    const char *data_prefix = "data: ";
    if (strncmp(event, data_prefix, strlen(data_prefix)) != 0) { // LCOV_EXCL_BR_LINE
        return -1; // LCOV_EXCL_LINE
    }

    /* Get JSON payload */
    const char *json_str = event + strlen(data_prefix);

    /* Check for [DONE] marker */
    if (strcmp(json_str, "[DONE]") == 0) {
        return -1;
    }

    /* Parse JSON */
    yyjson_doc *doc = yyjson_read(json_str, strlen(json_str), 0);
    if (!doc) { // LCOV_EXCL_BR_LINE
        return -1; // LCOV_EXCL_LINE
    }

    /* Validate root is an object */
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return -1; // LCOV_EXCL_LINE
    }

    /* Extract usage.completion_tokens */
    yyjson_val *usage = yyjson_obj_get(root, "usage");
    if (!usage || !yyjson_is_obj(usage)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return -1;
    }

    yyjson_val *completion_tokens_val = yyjson_obj_get(usage, "completion_tokens");
    if (!completion_tokens_val || !yyjson_is_int(completion_tokens_val)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return -1;
    }

    /* Extract integer value */
    int64_t tokens = yyjson_get_int_wrapper(completion_tokens_val);
    yyjson_doc_free(doc);

    return (int32_t)tokens;
}

/*
 * Extract finish_reason from SSE event
 *
 * Parses the raw SSE event JSON to extract choices[0].finish_reason if present.
 *
 * @param parent  Talloc context parent
 * @param event   Raw SSE event string (with "data: " prefix)
 * @return        Finish reason string or NULL if not present
 */
static char *extract_finish_reason(void *parent, const char *event) {
    assert(event != NULL); // LCOV_EXCL_BR_LINE

    /* Check for "data: " prefix */
    const char *data_prefix = "data: ";
    if (strncmp(event, data_prefix, strlen(data_prefix)) != 0) { // LCOV_EXCL_BR_LINE
        return NULL; // LCOV_EXCL_LINE
    }

    /* Get JSON payload */
    const char *json_str = event + strlen(data_prefix);

    /* Check for [DONE] marker */
    if (strcmp(json_str, "[DONE]") == 0) {
        return NULL;
    }

    /* Parse JSON */
    yyjson_doc *doc = yyjson_read(json_str, strlen(json_str), 0);
    if (!doc) { // LCOV_EXCL_BR_LINE
        return NULL; // LCOV_EXCL_LINE
    }

    /* Validate root is an object */
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL; // LCOV_EXCL_LINE
    }

    /* Extract choices[0].finish_reason */
    yyjson_val *choices = yyjson_obj_get(root, "choices");
    if (!choices || !yyjson_is_arr(choices) || yyjson_arr_size(choices) == 0) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL;
    }

    yyjson_val *choice0 = yyjson_arr_get(choices, 0);
    if (!choice0 || !yyjson_is_obj(choice0)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL;
    }

    yyjson_val *finish_reason_val = yyjson_obj_get(choice0, "finish_reason");
    if (!finish_reason_val || !yyjson_is_str(finish_reason_val)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL;
    }

    /* Extract string */
    const char *finish_reason_str = yyjson_get_str(finish_reason_val);
    char *result = talloc_strdup(parent, finish_reason_str);
    if (!result) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate finish_reason string"); // LCOV_EXCL_LINE
    }

    yyjson_doc_free(doc);
    return result;
}

/*
 * libcurl write callback
 *
 * Called by libcurl as data arrives from the server.
 * Feeds data to SSE parser and invokes user callback for each content chunk.
 */
size_t http_write_callback(char *data, size_t size, size_t nmemb, void *userdata) {
    http_write_ctx_t *ctx = (http_write_ctx_t *)userdata;
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    size_t total_size = size * nmemb;

    /* Feed data to SSE parser */
    ik_openai_sse_parser_feed(ctx->parser, data, total_size);

    /* Extract and process all complete SSE events */
    while (true) {
        char *event = ik_openai_sse_parser_get_event(ctx->parser);
        if (event == NULL) {
            break; /* No more complete events */
        }

        /* Parse SSE event to extract content */
        res_t content_res = ik_openai_parse_sse_event(ctx->parser, event);
        if (content_res.is_err) {
            /* Parse error - log but continue */
            talloc_free(event);
            continue;
        }

        char *content = content_res.ok;
        if (content != NULL) {
            /* Invoke user's streaming callback if provided */
            if (ctx->user_callback != NULL) {
                res_t cb_res = ctx->user_callback(content, ctx->user_ctx);
                if (cb_res.is_err) {
                    ctx->has_error = true;
                    talloc_free(content);
                    talloc_free(event);
                    return 0;
                }
            }

            /* Accumulate to complete response */
            if (ctx->complete_response == NULL) {
                ctx->complete_response = talloc_strdup(ctx->parser, content);
            } else {
                ctx->complete_response = talloc_strdup_append(ctx->complete_response, content);
            }

            if (ctx->complete_response == NULL) {  // LCOV_EXCL_BR_LINE
                PANIC("Failed to accumulate response");  // LCOV_EXCL_LINE
            }

            talloc_free(content);
        }

        /* Extract model if not already captured */
        if (ctx->model == NULL) {
            char *model = extract_model(ctx->parser, event);
            if (model != NULL) {
                ctx->model = model;
            }
        }

        /* Extract finish_reason if present */
        if (ctx->finish_reason == NULL) {
            char *finish_reason = extract_finish_reason(ctx->parser, event);
            if (finish_reason != NULL) {
                ctx->finish_reason = finish_reason;
            }
        }

        /* Extract completion_tokens if present */
        if (ctx->completion_tokens == 0) {
            int32_t tokens = extract_completion_tokens(event);
            if (tokens > 0) {
                ctx->completion_tokens = tokens;
            }
        }

        talloc_free(event);
    }

    return total_size;
}
