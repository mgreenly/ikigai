#include "providers/request.h"
#include "agent.h"
#include "error.h"
#include "msg.h"
#include "shared.h"
#include "tool.h"
#include "wrapper.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>
#include <talloc.h>

/**
 * Request Builder Implementation
 *
 * This module provides builder functions for constructing ik_request_t
 * structures with system prompts, messages, content blocks, tools, and
 * thinking configuration.
 */

/* ================================================================
 * Content Block Builders
 * ================================================================ */

ik_content_block_t *ik_content_block_text(TALLOC_CTX *ctx, const char *text) {
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    ik_content_block_t *block = talloc_zero(ctx, ik_content_block_t);
    if (!block) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->type = IK_CONTENT_TEXT;
    block->data.text.text = talloc_strdup(block, text);
    if (!block->data.text.text) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return block;
}

ik_content_block_t *ik_content_block_tool_call(TALLOC_CTX *ctx, const char *id,
                                                const char *name, const char *arguments) {
    assert(id != NULL);        // LCOV_EXCL_BR_LINE
    assert(name != NULL);      // LCOV_EXCL_BR_LINE
    assert(arguments != NULL); // LCOV_EXCL_BR_LINE

    ik_content_block_t *block = talloc_zero(ctx, ik_content_block_t);
    if (!block) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->type = IK_CONTENT_TOOL_CALL;
    block->data.tool_call.id = talloc_strdup(block, id);
    if (!block->data.tool_call.id) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->data.tool_call.name = talloc_strdup(block, name);
    if (!block->data.tool_call.name) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->data.tool_call.arguments = talloc_strdup(block, arguments);
    if (!block->data.tool_call.arguments) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return block;
}

ik_content_block_t *ik_content_block_tool_result(TALLOC_CTX *ctx, const char *tool_call_id,
                                                  const char *content, bool is_error) {
    assert(tool_call_id != NULL); // LCOV_EXCL_BR_LINE
    assert(content != NULL);      // LCOV_EXCL_BR_LINE

    ik_content_block_t *block = talloc_zero(ctx, ik_content_block_t);
    if (!block) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->type = IK_CONTENT_TOOL_RESULT;
    block->data.tool_result.tool_call_id = talloc_strdup(block, tool_call_id);
    if (!block->data.tool_result.tool_call_id) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->data.tool_result.content = talloc_strdup(block, content);
    if (!block->data.tool_result.content) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->data.tool_result.is_error = is_error;

    return block;
}

ik_content_block_t *ik_content_block_thinking(TALLOC_CTX *ctx, const char *text) {
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    ik_content_block_t *block = talloc_zero(ctx, ik_content_block_t);
    if (!block) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->type = IK_CONTENT_THINKING;
    block->data.thinking.text = talloc_strdup(block, text);
    if (!block->data.thinking.text) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return block;
}

/* ================================================================
 * Request Builder Functions
 * ================================================================ */

res_t ik_request_create(TALLOC_CTX *ctx, const char *model, ik_request_t **out) {
    assert(model != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);   // LCOV_EXCL_BR_LINE

    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    if (!req) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req->model = talloc_strdup(req, model);
    if (!req->model) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req->system_prompt = NULL;
    req->messages = NULL;
    req->message_count = 0;
    req->tools = NULL;
    req->tool_count = 0;
    req->max_output_tokens = -1;
    req->thinking.level = IK_THINKING_NONE;
    req->thinking.include_summary = false;
    req->tool_choice_mode = 0;  // IK_TOOL_AUTO
    req->tool_choice_name = NULL;

    *out = req;
    return OK(*out);
}

res_t ik_request_set_system(ik_request_t *req, const char *text) {
    assert(req != NULL);  // LCOV_EXCL_BR_LINE
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    /* Free old system prompt if it exists */
    if (req->system_prompt) {
        talloc_free(req->system_prompt);
    }

    /* Create new system prompt as child of request */
    req->system_prompt = talloc_strdup(req, text);
    if (!req->system_prompt) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return OK(NULL);
}

res_t ik_request_add_message(ik_request_t *req, ik_role_t role, const char *text) {
    assert(req != NULL);  // LCOV_EXCL_BR_LINE
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    /* Create text content block */
    ik_content_block_t *block = ik_content_block_text(req, text);

    /* Create message */
    ik_message_t *msg = talloc_zero(req, ik_message_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->role = role;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Copy block contents */
    msg->content_blocks[0] = *block;
    msg->content_count = 1;
    msg->provider_metadata = NULL;

    /* Steal block to message (becomes child of message) */
    talloc_steal(msg, block);

    /* Append to messages array */
    size_t new_count = req->message_count + 1;
    req->messages = talloc_realloc(req, req->messages, ik_message_t, (unsigned int)new_count);
    if (!req->messages) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req->messages[req->message_count] = *msg;
    req->message_count = new_count;

    /* Steal message to request */
    talloc_steal(req, msg);

    return OK(NULL);
}

res_t ik_request_add_message_blocks(ik_request_t *req, ik_role_t role,
                                     ik_content_block_t *blocks, size_t count) {
    assert(req != NULL);    // LCOV_EXCL_BR_LINE
    assert(blocks != NULL); // LCOV_EXCL_BR_LINE
    assert(count > 0);      // LCOV_EXCL_BR_LINE

    /* Create message */
    ik_message_t *msg = talloc_zero(req, ik_message_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->role = role;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, (unsigned int)count);
    if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Copy block contents */
    for (size_t i = 0; i < count; i++) {
        msg->content_blocks[i] = blocks[i];
    }
    msg->content_count = count;
    msg->provider_metadata = NULL;

    /* Append to messages array */
    size_t new_msg_count = req->message_count + 1;
    req->messages = talloc_realloc(req, req->messages, ik_message_t, (unsigned int)new_msg_count);
    if (!req->messages) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req->messages[req->message_count] = *msg;
    req->message_count = new_msg_count;

    /* Steal message to request */
    talloc_steal(req, msg);

    return OK(NULL);
}

void ik_request_set_thinking(ik_request_t *req, ik_thinking_level_t level,
                              bool include_summary) {
    assert(req != NULL); // LCOV_EXCL_BR_LINE

    req->thinking.level = level;
    req->thinking.include_summary = include_summary;
}

res_t ik_request_add_tool(ik_request_t *req, const char *name, const char *description,
                           const char *parameters, bool strict) {
    assert(req != NULL);         // LCOV_EXCL_BR_LINE
    assert(name != NULL);        // LCOV_EXCL_BR_LINE
    assert(description != NULL); // LCOV_EXCL_BR_LINE
    assert(parameters != NULL);  // LCOV_EXCL_BR_LINE

    /* Create tool definition */
    ik_tool_def_t *tool = talloc_zero(req, ik_tool_def_t);
    if (!tool) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    tool->name = talloc_strdup(tool, name);
    if (!tool->name) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    tool->description = talloc_strdup(tool, description);
    if (!tool->description) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    tool->parameters = talloc_strdup(tool, parameters);
    if (!tool->parameters) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    tool->strict = strict;

    /* Append to tools array */
    size_t new_tool_count = req->tool_count + 1;
    req->tools = talloc_realloc(req, req->tools, ik_tool_def_t, (unsigned int)new_tool_count);
    if (!req->tools) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req->tools[req->tool_count] = *tool;
    req->tool_count = new_tool_count;

    /* Steal tool to request */
    talloc_steal(req, tool);

    return OK(NULL);
}

/* ================================================================
 * Standard Tool Definitions
 * ================================================================ */

/* Tool schema definitions for standard tools (glob, file_read, grep, file_write, bash) */
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

/**
 * Build JSON parameter schema string from tool schema definition
 *
 * Creates a JSON string suitable for ik_request_add_tool's parameters argument.
 * Format matches OpenAI function calling schema.
 *
 * @param ctx Talloc parent context for result string
 * @param def Tool schema definition
 * @return JSON string (owned by ctx), or NULL on error
 */
static char *build_tool_parameters_json(TALLOC_CTX *ctx, const ik_tool_schema_def_t *def) {
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(def != NULL);  // LCOV_EXCL_BR_LINE

    /* Create yyjson document for building schema */
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Build parameters object */
    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (!parameters) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Set type to "object" */
    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field");  // LCOV_EXCL_LINE
    }

    /* Build properties object */
    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (!properties) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Add each parameter as a string property */
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

    /* Build required array */
    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (!required) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < def->param_count; i++) {
        if (!yyjson_mut_arr_add_str(doc, required, def->params[i].name)) {  // LCOV_EXCL_BR_LINE
            PANIC("Failed to add required parameter");  // LCOV_EXCL_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add required array");  // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_bool(doc, parameters, "additionalProperties", false)) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add additionalProperties field");  // LCOV_EXCL_LINE
    }

    /* Set parameters as document root */
    yyjson_mut_doc_set_root(doc, parameters);

    /* Serialize to JSON string */
    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (!json_str) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Copy to talloc context and free yyjson memory */
    char *result = talloc_strdup(ctx, json_str);
    free(json_str);
    yyjson_mut_doc_free(doc);

    return result;
}

/**
 * Deep copy existing message into request message array
 */
static res_t ik_request_add_message_direct(ik_request_t *req, const ik_message_t *msg)
{
    assert(req != NULL);  // LCOV_EXCL_BR_LINE
    assert(msg != NULL);  // LCOV_EXCL_BR_LINE

    /* Allocate new message in request context */
    ik_message_t *copy = talloc(req, ik_message_t);
    if (copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    copy->role = msg->role;
    copy->content_count = msg->content_count;
    copy->provider_metadata = NULL;  /* Don't copy response metadata into requests */

    /* Allocate content blocks array */
    copy->content_blocks = talloc_array(copy, ik_content_block_t, (unsigned int)msg->content_count);
    if (copy->content_blocks == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Deep copy each content block */
    for (size_t i = 0; i < msg->content_count; i++) {
        ik_content_block_t *src = &msg->content_blocks[i];
        ik_content_block_t *dst = &copy->content_blocks[i];
        dst->type = src->type;

        switch (src->type) {
        case IK_CONTENT_TEXT:
            dst->data.text.text = talloc_strdup(copy, src->data.text.text);
            if (dst->data.text.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            break;

        case IK_CONTENT_TOOL_CALL:
            dst->data.tool_call.id = talloc_strdup(copy, src->data.tool_call.id);
            dst->data.tool_call.name = talloc_strdup(copy, src->data.tool_call.name);
            dst->data.tool_call.arguments = talloc_strdup(copy, src->data.tool_call.arguments);
            if (dst->data.tool_call.id == NULL || dst->data.tool_call.name == NULL ||
                dst->data.tool_call.arguments == NULL) {
                PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            }
            break;

        case IK_CONTENT_TOOL_RESULT:
            dst->data.tool_result.tool_call_id = talloc_strdup(copy, src->data.tool_result.tool_call_id);
            dst->data.tool_result.content = talloc_strdup(copy, src->data.tool_result.content);
            dst->data.tool_result.is_error = src->data.tool_result.is_error;
            if (dst->data.tool_result.tool_call_id == NULL || dst->data.tool_result.content == NULL) {
                PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            }
            break;

        case IK_CONTENT_THINKING:
            dst->data.thinking.text = talloc_strdup(copy, src->data.thinking.text);
            if (dst->data.thinking.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            break;

        default:
            PANIC("Unknown content type");  // LCOV_EXCL_LINE
        }
    }

    /* Grow request messages array if needed */
    size_t new_count = req->message_count + 1;
    req->messages = talloc_realloc(req, req->messages, ik_message_t, (unsigned int)new_count);
    if (req->messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Copy message into array */
    req->messages[req->message_count] = *copy;
    req->message_count = new_count;

    /* Steal message to request */
    talloc_steal(req, copy);

    return OK(copy);
}

res_t ik_request_build_from_conversation(TALLOC_CTX *ctx, void *agent_ptr, ik_request_t **out) {
    assert(agent_ptr != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);       // LCOV_EXCL_BR_LINE

    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)agent_ptr;

    /* Validate agent has required fields */
    if (agent->model == NULL || strlen(agent->model) == 0) {
        return ERR(ctx, INVALID_ARG, "No model configured");
    }

    /* Create request with agent's model */
    ik_request_t *req = NULL;
    res_t res = ik_request_create(ctx, agent->model, &req);
    if (is_err(&res)) return res;

    /* Set thinking level from agent */
    ik_request_set_thinking(req, (ik_thinking_level_t)agent->thinking_level, false);

    /* Set system prompt from config if available */
    if (agent->shared && agent->shared->cfg && agent->shared->cfg->openai_system_message) {
        res = ik_request_set_system(req, agent->shared->cfg->openai_system_message);
        if (is_err(&res)) {
            talloc_free(req);
            return res;
        }
    }

    /* Iterate message storage and add to request */
    if (agent->messages != NULL) {
        for (size_t i = 0; i < agent->message_count; i++) {
            ik_message_t *msg = agent->messages[i];
            if (msg == NULL) continue;

            /* Deep copy message into request */
            res = ik_request_add_message_direct(req, msg);
            if (is_err(&res)) {
                talloc_free(req);
                return res;
            }
        }
    }

    /* Add standard tool definitions (glob, file_read, grep, file_write, bash) */
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
        if (is_err(&res)) {
            talloc_free(req);
            return res;
        }
    }

    *out = req;
    return OK(req);
}
