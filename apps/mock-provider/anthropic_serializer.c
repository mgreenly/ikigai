/**
 * @file anthropic_serializer.c
 * @brief Anthropic Messages API SSE serializer
 */

#include "apps/mock-provider/anthropic_serializer.h"

#include "shared/json_allocator.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
 * Build message_start event data.
 */
static char *build_message_start(TALLOC_CTX *ctx)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_val *message = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, message, "model", "mock");

    yyjson_mut_val *usage = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, usage, "input_tokens", 0);
    yyjson_mut_obj_add_val(doc, message, "usage", usage);

    yyjson_mut_obj_add_val(doc, root, "message", message);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);
    return result;
}

/**
 * Build content_block_start for a text block.
 */
static char *build_text_block_start(TALLOC_CTX *ctx, int index)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_int(doc, root, "index", index);

    yyjson_mut_val *block = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, block, "type", "text");
    yyjson_mut_obj_add_str(doc, block, "text", "");
    yyjson_mut_obj_add_val(doc, root, "content_block", block);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);
    return result;
}

/**
 * Build content_block_delta for text.
 */
static char *build_text_delta(TALLOC_CTX *ctx, int index, const char *text)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_int(doc, root, "index", index);

    yyjson_mut_val *delta = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, delta, "type", "text_delta");
    yyjson_mut_obj_add_str(doc, delta, "text", text);
    yyjson_mut_obj_add_val(doc, root, "delta", delta);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);
    return result;
}

/**
 * Build content_block_stop.
 */
static char *build_block_stop(TALLOC_CTX *ctx, int index)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_int(doc, root, "index", index);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);
    return result;
}

/**
 * Build message_delta with stop_reason.
 */
static char *build_message_delta(TALLOC_CTX *ctx, const char *stop_reason)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_val *delta = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, delta, "stop_reason", stop_reason);
    yyjson_mut_obj_add_val(doc, root, "delta", delta);

    yyjson_mut_val *usage = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, usage, "output_tokens", 0);
    yyjson_mut_obj_add_val(doc, root, "usage", usage);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);
    return result;
}

/**
 * Build content_block_start for a tool_use block.
 */
static char *build_tool_block_start(TALLOC_CTX *ctx, int index,
                                    const char *name)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_int(doc, root, "index", index);

    yyjson_mut_val *block = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, block, "type", "tool_use");

    char id_buf[32];
    (void)snprintf(id_buf, sizeof(id_buf), "toolu_mock_%d", index);
    yyjson_mut_obj_add_str(doc, block, "id", id_buf);
    yyjson_mut_obj_add_str(doc, block, "name", name);
    yyjson_mut_obj_add_val(doc, root, "content_block", block);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);
    return result;
}

/**
 * Build content_block_delta for tool input JSON.
 */
static char *build_tool_delta(TALLOC_CTX *ctx, int index,
                              const char *partial_json)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_int(doc, root, "index", index);

    yyjson_mut_val *delta = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, delta, "type", "input_json_delta");
    yyjson_mut_obj_add_str(doc, delta, "partial_json", partial_json);
    yyjson_mut_obj_add_val(doc, root, "delta", delta);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);
    return result;
}

void anthropic_serialize_text(TALLOC_CTX *ctx, const char *content, int fd)
{
    assert(ctx != NULL);
    assert(content != NULL);
    assert(fd >= 0);

    TALLOC_CTX *tmp = talloc_new(ctx);

    write_sse_event(fd, "message_start", build_message_start(tmp));
    write_sse_event(fd, "content_block_start", build_text_block_start(tmp, 0));
    write_sse_event(fd, "content_block_delta", build_text_delta(tmp, 0, content));
    write_sse_event(fd, "content_block_stop", build_block_stop(tmp, 0));
    write_sse_event(fd, "message_delta", build_message_delta(tmp, "end_turn"));
    write_sse_event(fd, "message_stop", "{}");

    talloc_free(tmp);
}

void anthropic_serialize_tool_calls(TALLOC_CTX *ctx,
                                    const mock_tool_call_t *tool_calls,
                                    int32_t count, int fd)
{
    assert(ctx != NULL);
    assert(tool_calls != NULL);
    assert(count > 0);
    assert(fd >= 0);

    TALLOC_CTX *tmp = talloc_new(ctx);

    write_sse_event(fd, "message_start", build_message_start(tmp));

    for (int32_t i = 0; i < count; i++) {
        char *start = build_tool_block_start(tmp, i, tool_calls[i].name);
        write_sse_event(fd, "content_block_start", start);

        char *delta = build_tool_delta(tmp, i,
                                       tool_calls[i].arguments_json);
        write_sse_event(fd, "content_block_delta", delta);

        char *stop = build_block_stop(tmp, i);
        write_sse_event(fd, "content_block_stop", stop);
    }

    write_sse_event(fd, "message_delta", build_message_delta(tmp, "tool_use"));
    write_sse_event(fd, "message_stop", "{}");

    talloc_free(tmp);
}
