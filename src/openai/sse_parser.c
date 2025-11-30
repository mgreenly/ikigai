#include "openai/sse_parser.h"
#include "error.h"
#include "tool.h"
#include "vendor/yyjson/yyjson.h"
#include <talloc.h>
#include <string.h>
#include <assert.h>

/**
 * SSE parser implementation
 *
 * This module provides Server-Sent Events parsing functionality for streaming
 * HTTP responses. It accumulates incoming data and extracts complete events
 * delimited by double newlines (\n\n).
 */

/*
 * yyjson wrapper functions
 *
 * These wrappers consolidate yyjson inline functions into single testable
 * locations. The inline functions contain defensive ternaries (e.g.,
 * doc ? doc->root : NULL) that create branches at every call site.
 * By wrapping them, we can test both branches once in unit tests.
 */

yyjson_val *yyjson_doc_get_root_wrapper(yyjson_doc *doc)
{
    return yyjson_doc_get_root(doc);
}

yyjson_val *yyjson_arr_get_wrapper(yyjson_val *arr, size_t idx)
{
    return yyjson_arr_get(arr, idx);
}

bool yyjson_is_obj_wrapper(yyjson_val *val)
{
    return yyjson_is_obj(val);
}

const char *yyjson_get_str_wrapper(yyjson_val *val)
{
    return yyjson_get_str(val);
}

/*
 * SSE parser
 */

#define SSE_INITIAL_BUFFER_SIZE 4096

ik_openai_sse_parser_t *ik_openai_sse_parser_create(void *parent) {
    ik_openai_sse_parser_t *parser = talloc_zero(parent, ik_openai_sse_parser_t);
    if (!parser) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate SSE parser"); // LCOV_EXCL_LINE
    }

    /* Allocate initial buffer */
    parser->buffer = talloc_array(parser, char, SSE_INITIAL_BUFFER_SIZE);
    if (!parser->buffer) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate SSE parser buffer"); // LCOV_EXCL_LINE
    }

    parser->buffer[0] = '\0';
    parser->buffer_len = 0;
    parser->buffer_cap = SSE_INITIAL_BUFFER_SIZE - 1; /* Reserve space for null terminator */

    return parser;
}

void ik_openai_sse_parser_feed(ik_openai_sse_parser_t *parser,
                                const char *data, size_t len) {
    assert(parser != NULL); // LCOV_EXCL_BR_LINE
    assert(data != NULL || len == 0); // LCOV_EXCL_BR_LINE

    if (len == 0) {
        return;
    }

    /* Check if we need to grow the buffer */
    if (parser->buffer_len + len > parser->buffer_cap) {
        /* Grow buffer to accommodate new data (double capacity or fit data, whichever is larger) */
        size_t new_cap = parser->buffer_cap * 2;
        while (new_cap < parser->buffer_len + len) {
            new_cap *= 2;
        }

        size_t alloc_size = new_cap + 1;
        char *new_buffer = talloc_realloc(parser, parser->buffer, char, (unsigned int)alloc_size);
        if (!new_buffer) { // LCOV_EXCL_BR_LINE
            PANIC("Failed to grow SSE parser buffer"); // LCOV_EXCL_LINE
        }

        parser->buffer = new_buffer;
        parser->buffer_cap = new_cap;
    }

    /* Append data to buffer */
    memcpy(parser->buffer + parser->buffer_len, data, len);
    parser->buffer_len += len;
    parser->buffer[parser->buffer_len] = '\0';
}

char *ik_openai_sse_parser_get_event(ik_openai_sse_parser_t *parser) {
    assert(parser != NULL); // LCOV_EXCL_BR_LINE

    /* Look for \n\n delimiter */
    const char *delimiter = strstr(parser->buffer, "\n\n");
    if (!delimiter) {
        /* No complete event yet */
        return NULL;
    }

    /* Calculate event length (excluding the \n\n) */
    size_t event_len = (size_t)(delimiter - parser->buffer);

    /* Extract event string */
    char *event = talloc_strndup(parser, parser->buffer, event_len);
    if (!event) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate event string"); // LCOV_EXCL_LINE
    }

    /* Remove event and delimiter from buffer */
    size_t consumed = event_len + 2; /* +2 for \n\n */
    size_t remaining = parser->buffer_len - consumed;

    if (remaining > 0) {
        /* Move remaining data to start of buffer */
        memmove(parser->buffer, parser->buffer + consumed, remaining);
    }

    parser->buffer_len = remaining;
    parser->buffer[parser->buffer_len] = '\0';

    return event;
}

res_t ik_openai_parse_sse_event(void *parent, const char *event) {
    assert(event != NULL); // LCOV_EXCL_BR_LINE

    /* Check for "data: " prefix */
    const char *data_prefix = "data: ";
    if (strncmp(event, data_prefix, strlen(data_prefix)) != 0) {
        return ERR(parent, PARSE, "SSE event missing 'data: ' prefix");
    }

    /* Get JSON payload (after "data: ") */
    const char *json_str = event + strlen(data_prefix);

    /* Check for [DONE] marker */
    if (strcmp(json_str, "[DONE]") == 0) {
        /* End of stream */
        return OK(NULL);
    }

    /* Parse JSON */
    yyjson_doc *doc = yyjson_read(json_str, strlen(json_str), 0);
    if (!doc) {
        return ERR(parent, PARSE, "Failed to parse SSE event JSON");
    }

    yyjson_val *root = yyjson_doc_get_root_wrapper(doc);
    if (!yyjson_is_obj_wrapper(root)) {
        yyjson_doc_free(doc);
        return ERR(parent, PARSE, "SSE event JSON root is not an object");
    }

    /* Extract choices[0].delta.content */
    yyjson_val *choices = yyjson_obj_get(root, "choices");
    if (!choices || !yyjson_is_arr(choices) || yyjson_arr_size(choices) == 0) {
        /* No choices array or empty - no content */
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    yyjson_val *choice0 = yyjson_arr_get_wrapper(choices, 0);
    if (!choice0 || !yyjson_is_obj_wrapper(choice0)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    yyjson_val *delta = yyjson_obj_get(choice0, "delta");
    if (!delta || !yyjson_is_obj_wrapper(delta)) {
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    yyjson_val *content = yyjson_obj_get(delta, "content");
    if (!content || !yyjson_is_str(content)) {
        /* No content field or not a string - may be role or other delta */
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    /* Extract content string */
    const char *content_str = yyjson_get_str(content);
    char *result = talloc_strdup(parent, content_str);
    if (!result) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate content string"); // LCOV_EXCL_LINE
    }

    yyjson_doc_free(doc);
    return OK(result);
}

res_t ik_openai_parse_tool_calls(void *parent, const char *event) {
    assert(event != NULL); // LCOV_EXCL_BR_LINE

    /* Check for "data: " prefix */
    const char *data_prefix = "data: ";
    if (strncmp(event, data_prefix, strlen(data_prefix)) != 0) {
        return ERR(parent, PARSE, "SSE event missing 'data: ' prefix");
    }

    /* Get JSON payload (after "data: ") */
    const char *json_str = event + strlen(data_prefix);

    /* Check for [DONE] marker */
    if (strcmp(json_str, "[DONE]") == 0) {
        /* End of stream */
        return OK(NULL);
    }

    /* Parse JSON */
    yyjson_doc *doc = yyjson_read(json_str, strlen(json_str), 0);
    if (!doc) {
        return ERR(parent, PARSE, "Failed to parse SSE event JSON");
    }

    yyjson_val *root = yyjson_doc_get_root_wrapper(doc);
    if (!yyjson_is_obj_wrapper(root)) {
        yyjson_doc_free(doc);
        return ERR(parent, PARSE, "SSE event JSON root is not an object");
    }

    /* Extract choices[0].delta.tool_calls */
    yyjson_val *choices = yyjson_obj_get(root, "choices");
    if (!choices || !yyjson_is_arr(choices) || yyjson_arr_size(choices) == 0) {
        /* No choices array or empty - no tool_calls */
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    yyjson_val *choice0 = yyjson_arr_get_wrapper(choices, 0);
    if (!choice0 || !yyjson_is_obj_wrapper(choice0)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    yyjson_val *delta = yyjson_obj_get(choice0, "delta");
    if (!delta || !yyjson_is_obj_wrapper(delta)) {
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    yyjson_val *tool_calls = yyjson_obj_get(delta, "tool_calls");
    if (!tool_calls || !yyjson_is_arr(tool_calls) || yyjson_arr_size(tool_calls) == 0) {
        /* No tool_calls field or empty array */
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    /* Extract first tool call (Story 02 scope: single complete tool call) */
    yyjson_val *tool_call = yyjson_arr_get_wrapper(tool_calls, 0);
    if (!tool_call || !yyjson_is_obj_wrapper(tool_call)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    /* Extract id (optional for streaming chunks - may be absent in subsequent chunks) */
    yyjson_val *id_val = yyjson_obj_get(tool_call, "id");
    bool has_id = (id_val != NULL);
    if (has_id) {
        has_id = yyjson_is_str(id_val);
    }

    /* Extract function object */
    yyjson_val *function = yyjson_obj_get(tool_call, "function");
    if (!function || !yyjson_is_obj_wrapper(function)) {
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    /* Extract function.name (optional for streaming chunks - may be absent in subsequent chunks) */
    yyjson_val *name_val = yyjson_obj_get(function, "name");
    bool has_name = (name_val != NULL);
    if (has_name) {
        has_name = yyjson_is_str(name_val);
    }

    /* For streaming: id and name must both be present OR both be absent */
    /* First chunk: has both id and name. Subsequent chunks: has neither. */
    if (has_id != has_name) {
        /* Malformed: has one but not the other */
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    const char *id_str;
    const char *name_str;
    if (has_id) {
        id_str = yyjson_get_str_wrapper(id_val);
        name_str = yyjson_get_str_wrapper(name_val);
    } else {
        id_str = "";
        name_str = "";
    }

    /* Extract function.arguments */
    yyjson_val *arguments_val = yyjson_obj_get(function, "arguments");
    if (!arguments_val || !yyjson_is_str(arguments_val)) {
        yyjson_doc_free(doc);
        return OK(NULL);
    }
    const char *arguments_str = yyjson_get_str_wrapper(arguments_val);

    /* Create ik_tool_call_t */
    ik_tool_call_t *result = ik_tool_call_create(parent, id_str, name_str, arguments_str);
    if (!result) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        PANIC("Failed to allocate tool_call struct"); // LCOV_EXCL_LINE
    }

    yyjson_doc_free(doc);
    return OK(result);
}
