#include "providers/request.h"
#include "agent.h"
#include "error.h"
#include "msg.h"
#include "openai/client.h"
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
    req->tool_choice_mode = 0;  // IK_TOOL_AUTO (temporarily int during coexistence)
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

/**
 * Map ik_msg_t kind to ik_role_t
 * Note: System messages are handled separately via ik_request_set_system(),
 * not via message roles.
 */
static ik_role_t kind_to_role(const char *kind) {
    if (strcmp(kind, "user") == 0) {
        return IK_ROLE_USER;
    } else if (strcmp(kind, "assistant") == 0) {
        return IK_ROLE_ASSISTANT;
    } else if (strcmp(kind, "tool_result") == 0 || strcmp(kind, "tool") == 0) {
        return IK_ROLE_TOOL;
    }
    return IK_ROLE_USER; // Default for unknown kinds
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
        if (def->params[i].required) {
            if (!yyjson_mut_arr_add_str(doc, required, def->params[i].name)) {  // LCOV_EXCL_BR_LINE
                PANIC("Failed to add required parameter");  // LCOV_EXCL_LINE
            }
        }
    }

    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) {  // LCOV_EXCL_BR_LINE
        PANIC("Failed to add required array");  // LCOV_EXCL_LINE
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

    /* Iterate conversation messages and add to request */
    if (agent->conversation != NULL) {
        for (size_t i = 0; i < agent->conversation->message_count; i++) {
            ik_msg_t *msg = agent->conversation->messages[i];
            if (msg == NULL || msg->kind == NULL) continue;

            /* Skip system messages - they are handled via system prompt */
            if (strcmp(msg->kind, "system") == 0) {
                /* Set as system prompt if we have content */
                if (msg->content != NULL) {
                    res = ik_request_set_system(req, msg->content);
                    if (is_err(&res)) {
                        talloc_free(req);
                        return res;
                    }
                }
                continue;
            }

            /* Skip non-conversation kinds (clear, mark, rewind, etc.) */
            if (!ik_msg_is_conversation_kind(msg->kind)) {
                continue;
            }

            /* Handle tool_call messages - parse data_json for structured data */
            if (strcmp(msg->kind, "tool_call") == 0 && msg->data_json != NULL) {
                /* Parse tool call data from data_json */
                yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
                if (!doc) {
                    talloc_free(req);
                    return ERR(ctx, PARSE, "Invalid tool_call data_json");
                }

                yyjson_val *root = yyjson_doc_get_root(doc);
                yyjson_val *id_val = yyjson_obj_get(root, "tool_call_id");
                yyjson_val *name_val = yyjson_obj_get(root, "name");
                yyjson_val *args_val = yyjson_obj_get(root, "arguments");

                if (!id_val || !name_val || !args_val) {
                    yyjson_doc_free(doc);
                    talloc_free(req);
                    return ERR(ctx, PARSE, "Missing tool_call fields in data_json");
                }

                const char *id = yyjson_get_str(id_val);
                const char *name = yyjson_get_str(name_val);
                const char *arguments = yyjson_get_str(args_val);

                /* Create tool call content block */
                ik_content_block_t *block = ik_content_block_tool_call(req, id, name, arguments);
                yyjson_doc_free(doc);

                if (!block) { // LCOV_EXCL_BR_LINE
                    PANIC("Out of memory"); // LCOV_EXCL_LINE
                }

                /* Add as assistant message with tool_call block */
                res = ik_request_add_message_blocks(req, IK_ROLE_ASSISTANT, block, 1);
                if (is_err(&res)) {
                    talloc_free(req);
                    return res;
                }
            } else if (strcmp(msg->kind, "tool_result") == 0 || strcmp(msg->kind, "tool") == 0) {
                /* Handle tool results - parse data_json for structured data */
                if (msg->data_json != NULL) {
                    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
                    if (!doc) {
                        talloc_free(req);
                        return ERR(ctx, PARSE, "Invalid tool_result data_json");
                    }

                    yyjson_val *root = yyjson_doc_get_root(doc);
                    yyjson_val *id_val = yyjson_obj_get(root, "tool_call_id");
                    yyjson_val *output_val = yyjson_obj_get(root, "output");
                    yyjson_val *success_val = yyjson_obj_get(root, "success");

                    if (!id_val || !output_val) {
                        yyjson_doc_free(doc);
                        talloc_free(req);
                        return ERR(ctx, PARSE, "Missing tool_result fields in data_json");
                    }

                    const char *tool_call_id = yyjson_get_str(id_val);
                    const char *output = yyjson_get_str(output_val);
                    bool is_error = success_val ? !yyjson_get_bool(success_val) : false;

                    /* Create tool result content block */
                    ik_content_block_t *block = ik_content_block_tool_result(req, tool_call_id, output, is_error);
                    yyjson_doc_free(doc);

                    if (!block) { // LCOV_EXCL_BR_LINE
                        PANIC("Out of memory"); // LCOV_EXCL_LINE
                    }

                    /* Add as tool message with tool_result block */
                    res = ik_request_add_message_blocks(req, IK_ROLE_TOOL, block, 1);
                    if (is_err(&res)) {
                        talloc_free(req);
                        return res;
                    }
                } else if (msg->content != NULL) {
                    /* Legacy tool result without data_json - fallback to text */
                    res = ik_request_add_message(req, IK_ROLE_TOOL, msg->content);
                    if (is_err(&res)) {
                        talloc_free(req);
                        return res;
                    }
                }
            } else {
                /* Regular text message (user or assistant) */
                ik_role_t role = kind_to_role(msg->kind);
                if (msg->content != NULL) {
                    res = ik_request_add_message(req, role, msg->content);
                    if (is_err(&res)) {
                        talloc_free(req);
                        return res;
                    }
                }
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
