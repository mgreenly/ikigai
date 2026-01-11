/**
 * @file request_tools.c
 * @brief Standard tool definitions and request building from agent conversation
 */

#include "providers/request.h"

#include "agent.h"
#include "error.h"
#include "shared.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/* ================================================================
 * Standard Tool Definitions
 * ================================================================ */

static const ik_tool_param_def_t glob_params[] = {
    {"pattern", "Glob pattern (e.g., 'src/**/*.c')", true},
    {"path", "Base directory (default: cwd)", false}
};

static const ik_tool_schema_def_t glob_schema_def = {
    .name = "glob",
    .description = "Find files matching a glob pattern",
    .params = glob_params,
    .param_count = 2
};

static const ik_tool_param_def_t file_read_params[] = {
    {"path", "Path to file", true}
};

static const ik_tool_schema_def_t file_read_schema_def = {
    .name = "file_read",
    .description = "Read contents of a file",
    .params = file_read_params,
    .param_count = 1
};

static const ik_tool_param_def_t grep_params[] = {
    {"pattern", "Search pattern (regex)", true},
    {"path", "File or directory to search", false},
    {"glob", "File pattern filter (e.g., '*.c')", false}
};

static const ik_tool_schema_def_t grep_schema_def = {
    .name = "grep",
    .description = "Search file contents for a pattern",
    .params = grep_params,
    .param_count = 3
};

static const ik_tool_param_def_t file_write_params[] = {
    {"path", "Path to file", true},
    {"content", "Content to write", true}
};

static const ik_tool_schema_def_t file_write_schema_def = {
    .name = "file_write",
    .description = "Write content to a file",
    .params = file_write_params,
    .param_count = 2
};

static const ik_tool_param_def_t bash_params[] = {
    {"command", "Command to execute", true}
};

static const ik_tool_schema_def_t bash_schema_def = {
    .name = "bash",
    .description = "Execute a shell command",
    .params = bash_params,
    .param_count = 1
};

/* ================================================================
 * Tool JSON Schema Building
 * ================================================================ */

/**
 * Build JSON parameter schema string from tool schema definition
 */
static char *build_tool_parameters_json(TALLOC_CTX *ctx, const ik_tool_schema_def_t *def) {
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(def != NULL);  // LCOV_EXCL_BR_LINE

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (!parameters) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field");  // LCOV_EXCL_LINE
    }

    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (!properties) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < def->param_count; i++) {
        yyjson_mut_val *prop = yyjson_mut_obj(doc);
        if (!prop) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        if (!yyjson_mut_obj_add_str(doc, prop, "type", "string")) {  // LCOV_EXCL_BR_LINE
            PANIC("Failed to add type field");  // LCOV_EXCL_LINE
        }

        if (!yyjson_mut_obj_add_str(doc, prop, "description", def->params[i].description)) {  // LCOV_EXCL_BR_LINE
            PANIC("Failed to add description field");  // LCOV_EXCL_LINE
        }

        if (!yyjson_mut_obj_add_val(doc, properties, def->params[i].name, prop)) {  // LCOV_EXCL_BR_LINE
            PANIC("Failed to add parameter to properties");  // LCOV_EXCL_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add properties to parameters");  // LCOV_EXCL_LINE
    }

    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (!required) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < def->param_count; i++) {
        if (def->params[i].required) {
            if (!yyjson_mut_arr_add_str(doc, required, def->params[i].name)) {  // LCOV_EXCL_BR_LINE
                PANIC("Failed to add required parameter");  // LCOV_EXCL_LINE
            }
        }
    }

    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add required array");  // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_bool(doc, parameters, "additionalProperties", false)) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add additionalProperties field");  // LCOV_EXCL_LINE
    }

    yyjson_mut_doc_set_root(doc, parameters);

    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (!json_str) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *result = talloc_strdup(ctx, json_str);
    free(json_str);
    yyjson_mut_doc_free(doc);

    return result;
}

/* ================================================================
 * Message Deep Copy
 * ================================================================ */

/**
 * Deep copy existing message into request message array
 */
static res_t ik_request_add_message_direct(ik_request_t *req, const ik_message_t *msg)
{
    assert(req != NULL);  // LCOV_EXCL_BR_LINE
    assert(msg != NULL);  // LCOV_EXCL_BR_LINE

    ik_message_t *copy = talloc(req, ik_message_t);
    if (copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    copy->role = msg->role;
    copy->content_count = msg->content_count;
    copy->provider_metadata = NULL;

    copy->content_blocks = talloc_array(req, ik_content_block_t, (unsigned int)msg->content_count);
    if (copy->content_blocks == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < msg->content_count; i++) {
        ik_content_block_t *src = &msg->content_blocks[i];
        ik_content_block_t *dst = &copy->content_blocks[i];
        dst->type = src->type;

        switch (src->type) {  // LCOV_EXCL_BR_LINE
        case IK_CONTENT_TEXT:
            dst->data.text.text = talloc_strdup(req, src->data.text.text);
            if (dst->data.text.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            break;

        case IK_CONTENT_TOOL_CALL:
            dst->data.tool_call.id = talloc_strdup(req, src->data.tool_call.id);
            dst->data.tool_call.name = talloc_strdup(req, src->data.tool_call.name);
            dst->data.tool_call.arguments = talloc_strdup(req, src->data.tool_call.arguments);
            if (dst->data.tool_call.id == NULL || dst->data.tool_call.name == NULL ||  // LCOV_EXCL_BR_LINE
                dst->data.tool_call.arguments == NULL) {  // LCOV_EXCL_BR_LINE
                PANIC("Out of memory");  // LCOV_EXCL_LINE
            }
            break;

        case IK_CONTENT_TOOL_RESULT:
            dst->data.tool_result.tool_call_id = talloc_strdup(req, src->data.tool_result.tool_call_id);
            dst->data.tool_result.content = talloc_strdup(req, src->data.tool_result.content);
            dst->data.tool_result.is_error = src->data.tool_result.is_error;
            if (dst->data.tool_result.tool_call_id == NULL || dst->data.tool_result.content == NULL) {  // LCOV_EXCL_BR_LINE
                PANIC("Out of memory");  // LCOV_EXCL_LINE
            }
            break;

        case IK_CONTENT_THINKING:
            dst->data.thinking.text = talloc_strdup(req, src->data.thinking.text);
            if (dst->data.thinking.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            if (src->data.thinking.signature != NULL) {
                dst->data.thinking.signature = talloc_strdup(req, src->data.thinking.signature);
                if (dst->data.thinking.signature == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            } else {
                dst->data.thinking.signature = NULL;
            }
            break;

        case IK_CONTENT_REDACTED_THINKING:
            dst->data.redacted_thinking.data = talloc_strdup(req, src->data.redacted_thinking.data);
            if (dst->data.redacted_thinking.data == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            break;

        default: // LCOV_EXCL_LINE
            PANIC("Unknown content type"); // LCOV_EXCL_LINE
        }
    }

    size_t new_count = req->message_count + 1;
    req->messages = talloc_realloc(req, req->messages, ik_message_t, (unsigned int)new_count);
    if (req->messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req->messages[req->message_count] = *copy;
    req->message_count = new_count;

    talloc_free(copy);

    return OK(&req->messages[req->message_count - 1]);
}

/* ================================================================
 * Request Building from Agent Conversation
 * ================================================================ */

res_t ik_request_build_from_conversation(TALLOC_CTX *ctx, void *agent_ptr, ik_request_t **out) {
    assert(agent_ptr != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);       // LCOV_EXCL_BR_LINE

    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)agent_ptr;

    if (agent->model == NULL || strlen(agent->model) == 0) {
        return ERR(ctx, INVALID_ARG, "No model configured");
    }

    ik_request_t *req = NULL;
    res_t res = ik_request_create(ctx, agent->model, &req);
    if (is_err(&res)) return res;  // LCOV_EXCL_BR_LINE

    ik_request_set_thinking(req, (ik_thinking_level_t)agent->thinking_level, false);

    if (agent->shared && agent->shared->cfg && agent->shared->cfg->openai_system_message) {
        res = ik_request_set_system(req, agent->shared->cfg->openai_system_message);
        if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
            talloc_free(req);  // LCOV_EXCL_LINE
            return res;        // LCOV_EXCL_LINE
        }
    }

    if (agent->messages != NULL) {
        for (size_t i = 0; i < agent->message_count; i++) {
            ik_message_t *msg = agent->messages[i];
            if (msg == NULL) continue;

            res = ik_request_add_message_direct(req, msg);
            if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
                talloc_free(req);  // LCOV_EXCL_LINE
                return res;        // LCOV_EXCL_LINE
            }
        }
    }

    const ik_tool_schema_def_t *tool_defs[] = {
        &glob_schema_def,
        &file_read_schema_def,
        &grep_schema_def,
        &file_write_schema_def,
        &bash_schema_def
    };

    for (size_t i = 0; i < 5; i++) {
        char *params_json = build_tool_parameters_json(req, tool_defs[i]);
        res = ik_request_add_tool(req, tool_defs[i]->name, tool_defs[i]->description, params_json, false);
        if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
            talloc_free(req);  // LCOV_EXCL_LINE
            return res;        // LCOV_EXCL_LINE
        }
    }

    *out = req;
    return OK(req);
}
