/**
 * @file openai_serializer.c
 * @brief OpenAI Chat Completions and Responses API SSE serializers
 */

#include "apps/mock-provider/openai_serializer.h"

#include "shared/json_allocator.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/**
 * Write a single SSE data line: "data: <json>\n\n"
 */
static void write_sse_line(int fd, const char *json_str)
{
    const char *prefix = "data: ";
    const char *suffix = "\n\n";
    (void)write(fd, prefix, strlen(prefix));
    (void)write(fd, json_str, strlen(json_str));
    (void)write(fd, suffix, strlen(suffix));
}

/**
 * Write the [DONE] sentinel.
 */
static void write_sse_done(int fd)
{
    const char *done = "data: [DONE]\n\n";
    (void)write(fd, done, strlen(done));
}

/**
 * Build the role chunk: delta with "role":"assistant"
 */
static char *build_role_chunk(TALLOC_CTX *ctx)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "id", "mock-1");
    yyjson_mut_obj_add_str(doc, root, "object", "chat.completion.chunk");
    yyjson_mut_obj_add_str(doc, root, "model", "mock");

    yyjson_mut_val *choices = yyjson_mut_arr(doc);
    yyjson_mut_val *choice = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, choice, "index", 0);

    yyjson_mut_val *delta = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, delta, "role", "assistant");
    yyjson_mut_obj_add_val(doc, choice, "delta", delta);
    yyjson_mut_obj_add_null(doc, choice, "finish_reason");
    yyjson_mut_arr_append(choices, choice);

    yyjson_mut_obj_add_val(doc, root, "choices", choices);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);

    return result;
}

/**
 * Build a content chunk: delta with "content":"..."
 */
static char *build_content_chunk(TALLOC_CTX *ctx, const char *content)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "id", "mock-1");
    yyjson_mut_obj_add_str(doc, root, "object", "chat.completion.chunk");
    yyjson_mut_obj_add_str(doc, root, "model", "mock");

    yyjson_mut_val *choices = yyjson_mut_arr(doc);
    yyjson_mut_val *choice = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, choice, "index", 0);

    yyjson_mut_val *delta = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, delta, "content", content);
    yyjson_mut_obj_add_val(doc, choice, "delta", delta);
    yyjson_mut_obj_add_null(doc, choice, "finish_reason");
    yyjson_mut_arr_append(choices, choice);

    yyjson_mut_obj_add_val(doc, root, "choices", choices);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);

    return result;
}

/**
 * Build the stop chunk with usage.
 */
static char *build_stop_chunk(TALLOC_CTX *ctx)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "id", "mock-1");
    yyjson_mut_obj_add_str(doc, root, "object", "chat.completion.chunk");
    yyjson_mut_obj_add_str(doc, root, "model", "mock");

    yyjson_mut_val *choices = yyjson_mut_arr(doc);
    yyjson_mut_val *choice = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, choice, "index", 0);

    yyjson_mut_val *delta = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, choice, "delta", delta);
    yyjson_mut_obj_add_str(doc, choice, "finish_reason", "stop");
    yyjson_mut_arr_append(choices, choice);

    yyjson_mut_obj_add_val(doc, root, "choices", choices);

    yyjson_mut_val *usage = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, usage, "prompt_tokens", 0);
    yyjson_mut_obj_add_int(doc, usage, "completion_tokens", 0);
    yyjson_mut_obj_add_int(doc, usage, "total_tokens", 0);
    yyjson_mut_obj_add_val(doc, root, "usage", usage);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);

    return result;
}

/**
 * Build a tool call chunk: delta with tool_calls array.
 */
static char *build_tool_call_chunk(TALLOC_CTX *ctx,
                                   const mock_tool_call_t *tool_calls,
                                   int32_t count)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "id", "mock-1");
    yyjson_mut_obj_add_str(doc, root, "object", "chat.completion.chunk");
    yyjson_mut_obj_add_str(doc, root, "model", "mock");

    yyjson_mut_val *choices = yyjson_mut_arr(doc);
    yyjson_mut_val *choice = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, choice, "index", 0);

    yyjson_mut_val *delta = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, delta, "role", "assistant");

    yyjson_mut_val *tc_arr = yyjson_mut_arr(doc);

    for (int32_t i = 0; i < count; i++) {
        yyjson_mut_val *tc = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_int(doc, tc, "index", i);

        /* Generate a tool call ID like "call_mock_0" */
        char id_buf[32];
        (void)snprintf(id_buf, sizeof(id_buf), "call_mock_%d", i);
        yyjson_mut_obj_add_str(doc, tc, "id", id_buf);
        yyjson_mut_obj_add_str(doc, tc, "type", "function");

        yyjson_mut_val *func = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_str(doc, func, "name",
                               tool_calls[i].name);
        yyjson_mut_obj_add_str(doc, func, "arguments",
                               tool_calls[i].arguments_json);
        yyjson_mut_obj_add_val(doc, tc, "function", func);

        yyjson_mut_arr_append(tc_arr, tc);
    }

    yyjson_mut_obj_add_val(doc, delta, "tool_calls", tc_arr);
    yyjson_mut_obj_add_val(doc, choice, "delta", delta);
    yyjson_mut_obj_add_null(doc, choice, "finish_reason");
    yyjson_mut_arr_append(choices, choice);

    yyjson_mut_obj_add_val(doc, root, "choices", choices);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);

    return result;
}

/**
 * Build a stop chunk with finish_reason "tool_calls".
 */
static char *build_tool_call_stop_chunk(TALLOC_CTX *ctx)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "id", "mock-1");
    yyjson_mut_obj_add_str(doc, root, "object", "chat.completion.chunk");
    yyjson_mut_obj_add_str(doc, root, "model", "mock");

    yyjson_mut_val *choices = yyjson_mut_arr(doc);
    yyjson_mut_val *choice = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, choice, "index", 0);

    yyjson_mut_val *delta = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, choice, "delta", delta);
    yyjson_mut_obj_add_str(doc, choice, "finish_reason", "tool_calls");
    yyjson_mut_arr_append(choices, choice);

    yyjson_mut_obj_add_val(doc, root, "choices", choices);

    yyjson_mut_val *usage = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, usage, "prompt_tokens", 0);
    yyjson_mut_obj_add_int(doc, usage, "completion_tokens", 0);
    yyjson_mut_obj_add_int(doc, usage, "total_tokens", 0);
    yyjson_mut_obj_add_val(doc, root, "usage", usage);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);

    return result;
}

void openai_serialize_text(TALLOC_CTX *ctx, const char *content, int fd)
{
    assert(ctx != NULL);
    assert(content != NULL);
    assert(fd >= 0);

    TALLOC_CTX *tmp = talloc_new(ctx);

    char *role_json = build_role_chunk(tmp);
    write_sse_line(fd, role_json);

    char *content_json = build_content_chunk(tmp, content);
    write_sse_line(fd, content_json);

    char *stop_json = build_stop_chunk(tmp);
    write_sse_line(fd, stop_json);

    write_sse_done(fd);

    talloc_free(tmp);
}

void openai_serialize_tool_calls(TALLOC_CTX *ctx,
                                 const mock_tool_call_t *tool_calls,
                                 int32_t count, int fd)
{
    assert(ctx != NULL);
    assert(tool_calls != NULL);
    assert(count > 0);
    assert(fd >= 0);

    TALLOC_CTX *tmp = talloc_new(ctx);

    char *tc_json = build_tool_call_chunk(tmp, tool_calls, count);
    write_sse_line(fd, tc_json);

    char *stop_json = build_tool_call_stop_chunk(tmp);
    write_sse_line(fd, stop_json);

    write_sse_done(fd);

    talloc_free(tmp);
}

/* ================================================================
 * Responses API Serializers (/v1/responses)
 * ================================================================ */

/**
 * Write a named SSE event: "event: <name>\ndata: <json>\n\n"
 */
static void write_sse_event(int fd, const char *event_name,
                             const char *json_str)
{
    const char *event_prefix = "event: ";
    const char *data_prefix = "\ndata: ";
    const char *suffix = "\n\n";
    (void)write(fd, event_prefix, strlen(event_prefix));
    (void)write(fd, event_name, strlen(event_name));
    (void)write(fd, data_prefix, strlen(data_prefix));
    (void)write(fd, json_str, strlen(json_str));
    (void)write(fd, suffix, strlen(suffix));
}

/**
 * Build response.created event data:
 * {"response":{"model":"mock"}}
 */
static char *build_responses_created(TALLOC_CTX *ctx)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_val *response = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, response, "model", "mock");
    yyjson_mut_obj_add_val(doc, root, "response", response);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);
    return result;
}

/**
 * Build response.output_text.delta event data:
 * {"delta":"<text>","content_index":0}
 */
static char *build_responses_text_delta(TALLOC_CTX *ctx, const char *content)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "delta", content);
    yyjson_mut_obj_add_int(doc, root, "content_index", 0);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);
    return result;
}

void openai_responses_serialize_text(TALLOC_CTX *ctx, const char *content,
                                     int fd)
{
    assert(ctx != NULL);
    assert(content != NULL);
    assert(fd >= 0);

    TALLOC_CTX *tmp = talloc_new(ctx);

    char *created = build_responses_created(tmp);
    write_sse_event(fd, "response.created", created);

    char *delta = build_responses_text_delta(tmp, content);
    write_sse_event(fd, "response.output_text.delta", delta);

    write_sse_event(fd, "response.completed", "{}");

    talloc_free(tmp);
}

void openai_responses_serialize_tool_calls(TALLOC_CTX *ctx,
                                           const mock_tool_call_t *tool_calls,
                                           int32_t count, int fd)
{
    assert(ctx != NULL);
    assert(tool_calls != NULL);
    assert(count > 0);
    assert(fd >= 0);

    TALLOC_CTX *tmp = talloc_new(ctx);

    char *created = build_responses_created(tmp);
    write_sse_event(fd, "response.created", created);

    for (int32_t i = 0; i < count; i++) {
        /* response.output_item.added */
        yyjson_alc alc = ik_make_talloc_allocator(tmp);
        yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
        yyjson_mut_val *root = yyjson_mut_obj(doc);
        yyjson_mut_doc_set_root(doc, root);

        yyjson_mut_obj_add_int(doc, root, "output_index", i);

        yyjson_mut_val *item = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_str(doc, item, "type", "function_call");
        char id_buf[32];
        (void)snprintf(id_buf, sizeof(id_buf), "call_mock_%d", i);
        yyjson_mut_obj_add_str(doc, item, "call_id", id_buf);
        yyjson_mut_obj_add_str(doc, item, "name", tool_calls[i].name);
        yyjson_mut_obj_add_val(doc, root, "item", item);

        size_t len = 0;
        char *json = yyjson_mut_write(doc, 0, &len);
        char *result = talloc_strdup(tmp, json);
        free(json);
        write_sse_event(fd, "response.output_item.added", result);

        /* response.function_call_arguments.delta */
        yyjson_alc alc2 = ik_make_talloc_allocator(tmp);
        yyjson_mut_doc *doc2 = yyjson_mut_doc_new(&alc2);
        yyjson_mut_val *root2 = yyjson_mut_obj(doc2);
        yyjson_mut_doc_set_root(doc2, root2);

        yyjson_mut_obj_add_int(doc2, root2, "output_index", i);
        yyjson_mut_obj_add_str(doc2, root2, "delta",
                               tool_calls[i].arguments_json);

        size_t len2 = 0;
        char *json2 = yyjson_mut_write(doc2, 0, &len2);
        char *result2 = talloc_strdup(tmp, json2);
        free(json2);
        write_sse_event(fd, "response.function_call_arguments.delta", result2);

        /* response.output_item.done */
        yyjson_alc alc3 = ik_make_talloc_allocator(tmp);
        yyjson_mut_doc *doc3 = yyjson_mut_doc_new(&alc3);
        yyjson_mut_val *root3 = yyjson_mut_obj(doc3);
        yyjson_mut_doc_set_root(doc3, root3);

        yyjson_mut_obj_add_int(doc3, root3, "output_index", i);

        size_t len3 = 0;
        char *json3 = yyjson_mut_write(doc3, 0, &len3);
        char *result3 = talloc_strdup(tmp, json3);
        free(json3);
        write_sse_event(fd, "response.output_item.done", result3);
    }

    write_sse_event(fd, "response.completed", "{}");

    talloc_free(tmp);
}
