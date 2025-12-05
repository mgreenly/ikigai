// OpenAI client message serialization helpers
#include "openai/client.h"

#include "panic.h"

#include <string.h>
#include <talloc.h>
#include "vendor/yyjson/yyjson.h"

// Serialize a tool_call message to OpenAI wire format
// Transforms canonical role="tool_call" to role="assistant" + tool_calls array
void ik_openai_serialize_tool_call_msg(yyjson_mut_doc *doc, yyjson_mut_val *msg_obj, // LCOV_EXCL_BR_LINE
                                        const ik_msg_t *msg, void *parent)
{
    if (!yyjson_mut_obj_add_str(doc, msg_obj, "role", "assistant")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add role field to message"); // LCOV_EXCL_LINE
    }

    if (!msg->data_json) PANIC("tool_call message missing data_json"); // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp_ctx = talloc_new(parent);
    if (!tmp_ctx) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_doc *data_doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    if (!data_doc) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Failed to parse tool call data_json"); // LCOV_EXCL_LINE
    }

    yyjson_val *data_root = yyjson_doc_get_root(data_doc);
    if (!data_root || !yyjson_is_obj(data_root)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(data_doc); // LCOV_EXCL_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Invalid tool call data_json structure"); // LCOV_EXCL_LINE
    }

    const char *call_id_src = yyjson_get_str(yyjson_obj_get(data_root, "id")); // LCOV_EXCL_BR_LINE
    const char *call_type_src = yyjson_get_str(yyjson_obj_get(data_root, "type")); // LCOV_EXCL_BR_LINE
    yyjson_val *function_val = yyjson_obj_get(data_root, "function");

    if (!call_id_src || !call_type_src || !function_val || !yyjson_is_obj(function_val)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(data_doc); // LCOV_EXCL_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Missing required fields in tool call data_json"); // LCOV_EXCL_LINE
    }

    const char *func_name_src = yyjson_get_str(yyjson_obj_get(function_val, "name")); // LCOV_EXCL_BR_LINE
    const char *func_args_src = yyjson_get_str(yyjson_obj_get(function_val, "arguments")); // LCOV_EXCL_BR_LINE

    if (!func_name_src || !func_args_src) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(data_doc); // LCOV_EXCL_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Missing function fields in tool call data_json"); // LCOV_EXCL_LINE
    }

    char *call_id = talloc_strdup(tmp_ctx, call_id_src);
    char *call_type = talloc_strdup(tmp_ctx, call_type_src);
    char *func_name = talloc_strdup(tmp_ctx, func_name_src);
    char *func_args = talloc_strdup(tmp_ctx, func_args_src);
    yyjson_doc_free(data_doc);

    if (!call_id || !call_type || !func_name || !func_args) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Out of memory copying tool call strings"); // LCOV_EXCL_LINE
    }

    yyjson_mut_val *tool_calls_arr = yyjson_mut_arr(doc);
    if (tool_calls_arr == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    yyjson_mut_val *tool_call_obj = yyjson_mut_arr_add_obj(doc, tool_calls_arr);
    if (tool_call_obj == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_strcpy(doc, tool_call_obj, "id", call_id)) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Failed to add id to tool call"); // LCOV_EXCL_LINE
    }
    if (!yyjson_mut_obj_add_strcpy(doc, tool_call_obj, "type", call_type)) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Failed to add type to tool call"); // LCOV_EXCL_LINE
    }

    yyjson_mut_val *func_obj = yyjson_mut_obj(doc);
    if (func_obj == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_strcpy(doc, func_obj, "name", func_name)) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Failed to add function name"); // LCOV_EXCL_LINE
    }
    if (!yyjson_mut_obj_add_strcpy(doc, func_obj, "arguments", func_args)) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Failed to add function arguments"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, tool_call_obj, "function", func_obj)) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Failed to add function to tool call"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, msg_obj, "tool_calls", tool_calls_arr)) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Failed to add tool_calls array to message"); // LCOV_EXCL_LINE
    }

    talloc_free(tmp_ctx);
}

// Serialize a tool_result message to OpenAI wire format
// Transforms canonical role="tool_result" to role="tool" + tool_call_id + content
void ik_openai_serialize_tool_result_msg(yyjson_mut_doc *doc, yyjson_mut_val *msg_obj, // LCOV_EXCL_BR_LINE
                                          const ik_msg_t *msg, void *parent)
{
    if (!yyjson_mut_obj_add_str(doc, msg_obj, "role", "tool")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add role field to message"); // LCOV_EXCL_LINE
    }

    if (!msg->data_json) PANIC("tool_result message missing data_json"); // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp_ctx = talloc_new(parent);
    if (!tmp_ctx) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_doc *data_doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    if (!data_doc) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Failed to parse tool result data_json"); // LCOV_EXCL_LINE
    }

    yyjson_val *data_root = yyjson_doc_get_root(data_doc);
    if (!data_root || !yyjson_is_obj(data_root)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(data_doc); // LCOV_EXCL_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Invalid tool result data_json structure"); // LCOV_EXCL_LINE
    }

    const char *tool_call_id_src = yyjson_get_str(yyjson_obj_get(data_root, "tool_call_id")); // LCOV_EXCL_BR_LINE
    if (!tool_call_id_src) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(data_doc); // LCOV_EXCL_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Missing tool_call_id in tool result data_json"); // LCOV_EXCL_LINE
    }

    char *tool_call_id = talloc_strdup(tmp_ctx, tool_call_id_src);
    yyjson_doc_free(data_doc);

    if (!tool_call_id) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Out of memory copying tool_call_id"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_strcpy(doc, msg_obj, "tool_call_id", tool_call_id)) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Failed to add tool_call_id to message"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_str(doc, msg_obj, "content", msg->content)) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        PANIC("Failed to add content field to message"); // LCOV_EXCL_LINE
    }

    talloc_free(tmp_ctx);
}
