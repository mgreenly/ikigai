/**
 * @file google_serializer.c
 * @brief Google Gemini API SSE serializer
 */

#include "apps/mock-provider/google_serializer.h"

#include "shared/json_allocator.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/**
 * Write an SSE data line: "data: <json>\n\n"
 * Google does not use event: fields.
 */
static void write_sse_data(int fd, const char *json_str)
{
    const char *prefix = "data: ";
    const char *suffix = "\n\n";
    (void)write(fd, prefix, strlen(prefix));
    (void)write(fd, json_str, strlen(json_str));
    (void)write(fd, suffix, strlen(suffix));
}

/**
 * Add usageMetadata object to root.
 */
static void add_usage_metadata(yyjson_mut_doc *doc, yyjson_mut_val *root)
{
    yyjson_mut_val *usage = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, usage, "promptTokenCount", 0);
    yyjson_mut_obj_add_int(doc, usage, "candidatesTokenCount", 0);
    yyjson_mut_obj_add_int(doc, usage, "totalTokenCount", 0);
    yyjson_mut_obj_add_val(doc, root, "usageMetadata", usage);
}

/**
 * Build a text response chunk.
 */
static char *build_text_chunk(TALLOC_CTX *ctx, const char *content)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    /* candidates array */
    yyjson_mut_val *candidates = yyjson_mut_arr(doc);
    yyjson_mut_val *candidate = yyjson_mut_obj(doc);

    /* content.parts[0].text */
    yyjson_mut_val *cand_content = yyjson_mut_obj(doc);
    yyjson_mut_val *parts = yyjson_mut_arr(doc);
    yyjson_mut_val *part = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, part, "text", content);
    yyjson_mut_arr_append(parts, part);
    yyjson_mut_obj_add_val(doc, cand_content, "parts", parts);
    yyjson_mut_obj_add_str(doc, cand_content, "role", "model");
    yyjson_mut_obj_add_val(doc, candidate, "content", cand_content);

    yyjson_mut_obj_add_str(doc, candidate, "finishReason", "STOP");
    yyjson_mut_arr_append(candidates, candidate);
    yyjson_mut_obj_add_val(doc, root, "candidates", candidates);

    yyjson_mut_obj_add_str(doc, root, "modelVersion", "mock");
    add_usage_metadata(doc, root);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);
    return result;
}

/**
 * Build a tool calls response chunk.
 */
static char *build_tool_calls_chunk(TALLOC_CTX *ctx,
                                    const mock_tool_call_t *tool_calls,
                                    int32_t count)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    /* candidates array */
    yyjson_mut_val *candidates = yyjson_mut_arr(doc);
    yyjson_mut_val *candidate = yyjson_mut_obj(doc);

    /* content with functionCall parts */
    yyjson_mut_val *cand_content = yyjson_mut_obj(doc);
    yyjson_mut_val *parts = yyjson_mut_arr(doc);

    for (int32_t i = 0; i < count; i++) {
        yyjson_mut_val *part = yyjson_mut_obj(doc);
        yyjson_mut_val *fc = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_str(doc, fc, "name", tool_calls[i].name);

        /* Parse arguments_json string back to object */
        yyjson_doc *args_doc = yyjson_read(tool_calls[i].arguments_json,
                                           strlen(tool_calls[i].arguments_json),
                                           0);
        if (args_doc != NULL) {
            yyjson_val *args_root = yyjson_doc_get_root(args_doc);
            yyjson_mut_val *args_mut = yyjson_val_mut_copy(doc, args_root);
            yyjson_mut_obj_add_val(doc, fc, "args", args_mut);
            yyjson_doc_free(args_doc);
        } else {
            yyjson_mut_val *empty = yyjson_mut_obj(doc);
            yyjson_mut_obj_add_val(doc, fc, "args", empty);
        }

        yyjson_mut_obj_add_val(doc, part, "functionCall", fc);
        yyjson_mut_arr_append(parts, part);
    }

    yyjson_mut_obj_add_val(doc, cand_content, "parts", parts);
    yyjson_mut_obj_add_str(doc, cand_content, "role", "model");
    yyjson_mut_obj_add_val(doc, candidate, "content", cand_content);

    yyjson_mut_obj_add_str(doc, candidate, "finishReason", "STOP");
    yyjson_mut_arr_append(candidates, candidate);
    yyjson_mut_obj_add_val(doc, root, "candidates", candidates);

    yyjson_mut_obj_add_str(doc, root, "modelVersion", "mock");
    add_usage_metadata(doc, root);

    size_t len = 0;
    char *json = yyjson_mut_write(doc, 0, &len);
    char *result = talloc_strdup(ctx, json);
    free(json);
    return result;
}

void google_serialize_text(TALLOC_CTX *ctx, const char *content, int fd)
{
    assert(ctx != NULL);
    assert(content != NULL);
    assert(fd >= 0);

    TALLOC_CTX *tmp = talloc_new(ctx);

    write_sse_data(fd, build_text_chunk(tmp, content));

    talloc_free(tmp);
}

void google_serialize_tool_calls(TALLOC_CTX *ctx,
                                 const mock_tool_call_t *tool_calls,
                                 int32_t count, int fd)
{
    assert(ctx != NULL);
    assert(tool_calls != NULL);
    assert(count > 0);
    assert(fd >= 0);

    TALLOC_CTX *tmp = talloc_new(ctx);

    write_sse_data(fd, build_tool_calls_chunk(tmp, tool_calls, count));

    talloc_free(tmp);
}
