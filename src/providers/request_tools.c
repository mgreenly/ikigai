/**
 * @file request_tools.c
 * @brief Standard tool definitions and request building from agent conversation
 */

#include "providers/request.h"

#include "agent.h"
#include "doc_cache.h"
#include "error.h"
#include "panic.h"
#include "shared.h"
#include "tool_registry.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

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
                if (dst->data.text.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                break;

            case IK_CONTENT_TOOL_CALL:
                dst->data.tool_call.id = talloc_strdup(req, src->data.tool_call.id);
                dst->data.tool_call.name = talloc_strdup(req, src->data.tool_call.name);
                dst->data.tool_call.arguments = talloc_strdup(req, src->data.tool_call.arguments);
                if (dst->data.tool_call.id == NULL || dst->data.tool_call.name == NULL || // LCOV_EXCL_BR_LINE
                    dst->data.tool_call.arguments == NULL) { // LCOV_EXCL_BR_LINE
                    PANIC("Out of memory"); // LCOV_EXCL_LINE
                }
                break;

            case IK_CONTENT_TOOL_RESULT:
                dst->data.tool_result.tool_call_id = talloc_strdup(req, src->data.tool_result.tool_call_id);
                dst->data.tool_result.content = talloc_strdup(req, src->data.tool_result.content);
                dst->data.tool_result.is_error = src->data.tool_result.is_error;
                if (dst->data.tool_result.tool_call_id == NULL || dst->data.tool_result.content == NULL) { // LCOV_EXCL_BR_LINE
                    PANIC("Out of memory"); // LCOV_EXCL_LINE
                }
                break;

            case IK_CONTENT_THINKING:
                dst->data.thinking.text = talloc_strdup(req, src->data.thinking.text);
                if (dst->data.thinking.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                if (src->data.thinking.signature != NULL) {
                    dst->data.thinking.signature = talloc_strdup(req, src->data.thinking.signature);
                    if (dst->data.thinking.signature == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                } else {
                    dst->data.thinking.signature = NULL;
                }
                break;

            case IK_CONTENT_REDACTED_THINKING:
                dst->data.redacted_thinking.data = talloc_strdup(req, src->data.redacted_thinking.data);
                if (dst->data.redacted_thinking.data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
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

static res_t build_system_prompt_from_agent(ik_request_t *req, ik_agent_ctx_t *agent)
{
    assert(req != NULL);   // LCOV_EXCL_BR_LINE
    assert(agent != NULL); // LCOV_EXCL_BR_LINE

    // Use effective system prompt with fallback chain:
    // 1. Pinned files (if any)
    // 2. $IKIGAI_DATA_DIR/system/prompt.md (if exists)
    // 3. cfg->openai_system_message (config fallback)
    char *system_prompt = NULL;
    res_t prompt_res = ik_agent_get_effective_system_prompt(agent, &system_prompt);
    if (is_err(&prompt_res)) {
        return prompt_res;  // LCOV_EXCL_LINE
    }

    if (system_prompt != NULL && strlen(system_prompt) > 0) {
        // Copy to request context since ik_agent_get_effective_system_prompt
        // allocates on agent context
        char *req_prompt = talloc_strdup(req, system_prompt);
        talloc_free(system_prompt);
        if (req_prompt == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        res_t res = ik_request_set_system(req, req_prompt);
        if (is_err(&res)) return res;  // LCOV_EXCL_BR_LINE
    } else if (system_prompt != NULL) {
        talloc_free(system_prompt);
    }

    return OK(NULL);
}

static res_t add_tools_from_registry(ik_request_t *req, ik_tool_registry_t *registry, ik_agent_ctx_t *agent)
{
    assert(req != NULL); // LCOV_EXCL_BR_LINE

    if (registry == NULL || registry->count == 0) {
        return OK(NULL);
    }

    for (size_t i = 0; i < registry->count; i++) {
        ik_tool_registry_entry_t *entry = &registry->entries[i];

        if (agent != NULL && agent->toolset_filter != NULL && agent->toolset_count > 0) {
            bool allowed = false;
            for (size_t j = 0; j < agent->toolset_count; j++) {
                if (strcmp(entry->name, agent->toolset_filter[j]) == 0) {
                    allowed = true;
                    break;
                }
            }
            if (!allowed) {
                continue;
            }
        }

        const char *description = yyjson_get_str(yyjson_obj_get(entry->schema_root, "description"));  // LCOV_EXCL_BR_LINE
        if (description == NULL) description = "";

        yyjson_val *parameters = yyjson_obj_get(entry->schema_root, "parameters");  // LCOV_EXCL_BR_LINE

        char *params_json = NULL;
        if (parameters != NULL) {
            params_json = yyjson_val_write(parameters, YYJSON_WRITE_NOFLAG, NULL);
            if (params_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }

        res_t res = ik_request_add_tool(req, entry->name, description,
                                        params_json ? params_json : "{}", false);

        if (params_json != NULL) {
            free(params_json);
        }

        if (is_err(&res)) return res;  // LCOV_EXCL_BR_LINE
    }

    return OK(NULL);
}

res_t ik_request_build_from_conversation(TALLOC_CTX *ctx,
                                         void *agent_ptr,
                                         ik_tool_registry_t *registry,
                                         ik_request_t **out)
{
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

    res = build_system_prompt_from_agent(req, agent);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        talloc_free(req);  // LCOV_EXCL_LINE
        return res;        // LCOV_EXCL_LINE
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

    res = add_tools_from_registry(req, registry, agent);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        talloc_free(req);  // LCOV_EXCL_LINE
        return res;        // LCOV_EXCL_LINE
    }

    *out = req;
    return OK(req);
}
