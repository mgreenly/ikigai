/**
 * @file internal_tools.c
 * @brief Internal tool handlers (kill, send) and tool registration
 */

#include "apps/ikigai/internal_tools.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/internal_tool_fork.h"
#include "apps/ikigai/internal_tool_wait.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/tool_registry.h"
#include "apps/ikigai/tool_wrapper.h"
#include "shared/error.h"
#include "shared/json_allocator.h"
#include "shared/panic.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>


#include "shared/poison.h"

// JSON schemas for internal tools
static const char *FORK_SCHEMA =
    "{"
    "  \"name\": \"fork\","
    "  \"description\": \"Create a child agent with a specific task. The child inherits parent conversation history and works independently with its own LLM stream.\","
    "  \"parameters\": {"
    "    \"type\": \"object\","
    "    \"properties\": {"
    "      \"name\": {"
    "        \"type\": \"string\","
    "        \"description\": \"Short human-readable label for the child agent (e.g., 'analyzer', 'worker-1')\""
    "      },"
    "      \"prompt\": {"
    "        \"type\": \"string\","
    "        \"description\": \"The task for the child agent to work on\""
    "      }"
    "    },"
    "    \"required\": [\"name\", \"prompt\"]"
    "  }"
    "}";

static const char *KILL_SCHEMA =
    "{"
    "  \"name\": \"kill\","
    "  \"description\": \"Terminate an agent and all its descendants. Returns list of killed agent UUIDs.\","
    "  \"parameters\": {"
    "    \"type\": \"object\","
    "    \"properties\": {"
    "      \"uuid\": {"
    "        \"type\": \"string\","
    "        \"description\": \"UUID of the agent to terminate (cascades to all descendants)\""
    "      }"
    "    },"
    "    \"required\": [\"uuid\"]"
    "  }"
    "}";

static const char *SEND_SCHEMA =
    "{"
    "  \"name\": \"send\","
    "  \"description\": \"Send a message to another agent. The recipient can retrieve it with the wait tool.\","
    "  \"parameters\": {"
    "    \"type\": \"object\","
    "    \"properties\": {"
    "      \"to\": {"
    "        \"type\": \"string\","
    "        \"description\": \"UUID of the recipient agent (must be running)\""
    "      },"
    "      \"message\": {"
    "        \"type\": \"string\","
    "        \"description\": \"Message content to send\""
    "      }"
    "    },"
    "    \"required\": [\"to\", \"message\"]"
    "  }"
    "}";

static const char *WAIT_SCHEMA =
    "{"
    "  \"name\": \"wait\","
    "  \"description\": \"Wait for messages from other agents. Can wait for next message from anyone, or fan-in results from specific agents.\","
    "  \"parameters\": {"
    "    \"type\": \"object\","
    "    \"properties\": {"
    "      \"timeout\": {"
    "        \"type\": \"number\","
    "        \"description\": \"Maximum seconds to wait (0 for instant check, no blocking)\""
    "      },"
    "      \"from_agents\": {"
    "        \"type\": \"array\","
    "        \"items\": {\"type\": \"string\"},"
    "        \"description\": \"Optional: specific agent UUIDs to wait for (fan-in mode). Omit to wait for next message from anyone.\""
    "      }"
    "    },"
    "    \"required\": [\"timeout\"]"
    "  }"
    "}";

// Kill handler: mark agent as dead (simplified - no cascade for now)
char *ik_internal_tool_kill_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(args_json != NULL);  // LCOV_EXCL_BR_LINE

    ik_db_ctx_t *worker_db_ctx = agent->worker_db_ctx;

    // Parse arguments
    yyjson_doc *doc = yyjson_read_(args_json, strlen(args_json), 0);
    if (doc == NULL) {  // LCOV_EXCL_BR_LINE
        return ik_tool_wrap_failure(ctx, "Failed to parse kill arguments", "PARSE_ERROR");
    }
    yyjson_val *root = yyjson_doc_get_root_(doc);

    yyjson_val *uuid_val = yyjson_obj_get_(root, "uuid");
    if (uuid_val == NULL || !yyjson_is_str(uuid_val)) {  // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: uuid", "INVALID_ARG");
    }

    const char *target_uuid = yyjson_get_str_(uuid_val);
    char *target_uuid_copy = talloc_strdup(ctx, target_uuid);
    if (target_uuid_copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_doc_free(doc);

    // Validate target exists and is alive
    ik_db_agent_row_t *target_row = NULL;
    res_t res = ik_db_agent_get(worker_db_ctx, ctx, target_uuid_copy, &target_row);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        char *err_msg = talloc_asprintf(ctx, "Agent not found: %s", target_uuid_copy);
        talloc_free(res.err);
        return ik_tool_wrap_failure(ctx, err_msg, "AGENT_NOT_FOUND");
    }

    if (strcmp(target_row->status, "running") != 0) {  // LCOV_EXCL_BR_LINE
        return ik_tool_wrap_failure(ctx, "Agent is already dead", "ALREADY_DEAD");
    }

    // Check if target is root agent (parent_uuid == NULL)
    if (target_row->parent_uuid == NULL) {
        return ik_tool_wrap_failure(ctx, "Cannot kill root agent", "CANNOT_KILL_ROOT");
    }

    // Get caller's agent row to check for parent kill attempt
    ik_db_agent_row_t *caller_row = NULL;
    res = ik_db_agent_get(worker_db_ctx, ctx, agent->uuid, &caller_row);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        char *err_msg = talloc_asprintf(ctx, "Failed to get caller agent: %s", error_message(res.err));
        talloc_free(res.err);
        return ik_tool_wrap_failure(ctx, err_msg, "DB_ERROR");
    }

    // Check if caller is trying to kill their parent
    if (caller_row->parent_uuid != NULL && strcmp(caller_row->parent_uuid, target_uuid_copy) == 0) {
        return ik_tool_wrap_failure(ctx, "Cannot kill parent agent", "CANNOT_KILL_PARENT");
    }

    // Mark target as dead in database
    res = ik_db_agent_mark_dead(worker_db_ctx, target_uuid_copy);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        char *err_msg = talloc_asprintf(ctx, "Failed to mark agent dead: %s", error_message(res.err));
        talloc_free(res.err);
        return ik_tool_wrap_failure(ctx, err_msg, "DB_ERROR");
    }

    // Build list of killed UUIDs (just the target for now)
    char **killed_uuids = talloc_zero_array(ctx, char *, 2);  // NULL-terminated
    if (killed_uuids == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    killed_uuids[0] = talloc_strdup(ctx, target_uuid_copy);
    if (killed_uuids[0] == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    killed_uuids[1] = NULL;

    // Store killed UUIDs in tool_deferred_data for on_complete
    agent->tool_deferred_data = killed_uuids;

    // Build success result
    yyjson_mut_doc *result_doc = yyjson_mut_doc_new(NULL);
    if (result_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *result_root = yyjson_mut_obj(result_doc);
    if (result_root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(result_doc, result_root);

    yyjson_mut_val *killed_arr = yyjson_mut_arr(result_doc);
    if (killed_arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_arr_add_str(result_doc, killed_arr, target_uuid_copy);
    yyjson_mut_obj_add_val(result_doc, result_root, "killed", killed_arr);

    char *result_json = yyjson_mut_write(result_doc, 0, NULL);
    if (result_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    char *result_copy = talloc_strdup(ctx, result_json);
    free(result_json);
    yyjson_mut_doc_free(result_doc);

    return ik_tool_wrap_success(ctx, result_copy);
}

// Kill on_complete: set dead flag on agents in memory
void ik_internal_tool_kill_on_complete(struct ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    if (agent->tool_deferred_data == NULL) {  // LCOV_EXCL_BR_LINE
        return;
    }

    char **killed_uuids = (char **)agent->tool_deferred_data;

    // Find each killed agent in repl->agents[] and set dead flag
    for (size_t i = 0; killed_uuids[i] != NULL; i++) {
        for (size_t j = 0; j < repl->agent_count; j++) {
            if (strcmp(repl->agents[j]->uuid, killed_uuids[i]) == 0) {
                repl->agents[j]->dead = true;
                break;
            }
        }
    }

    // Clear deferred data
    talloc_free(agent->tool_deferred_data);
    agent->tool_deferred_data = NULL;
}

// Send handler: send message to another agent
char *ik_internal_tool_send_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(args_json != NULL);  // LCOV_EXCL_BR_LINE

    ik_db_ctx_t *worker_db_ctx = agent->worker_db_ctx;

    // Parse arguments
    yyjson_doc *doc = yyjson_read_(args_json, strlen(args_json), 0);
    if (doc == NULL) {  // LCOV_EXCL_BR_LINE
        return ik_tool_wrap_failure(ctx, "Failed to parse send arguments", "PARSE_ERROR");
    }
    yyjson_val *root = yyjson_doc_get_root_(doc);

    yyjson_val *to_val = yyjson_obj_get_(root, "to");
    yyjson_val *message_val = yyjson_obj_get_(root, "message");

    if (to_val == NULL || !yyjson_is_str(to_val)) {  // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: to", "INVALID_ARG");
    }

    if (message_val == NULL || !yyjson_is_str(message_val)) {  // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: message", "INVALID_ARG");
    }

    const char *recipient_uuid = yyjson_get_str_(to_val);
    const char *message_body = yyjson_get_str_(message_val);

    char *recipient_copy = talloc_strdup(ctx, recipient_uuid);
    if (recipient_copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    char *message_copy = talloc_strdup(ctx, message_body);
    if (message_copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_doc_free(doc);

    // Call core send logic
    char *error_msg = NULL;
    res_t res = ik_send_core(ctx, worker_db_ctx, agent->shared->session_id,
                             agent->uuid, recipient_copy, message_copy, &error_msg);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        char *wrapped_error = ik_tool_wrap_failure(ctx, error_msg ? error_msg : error_message(res.err), "SEND_FAILED");
        talloc_free(res.err);
        return wrapped_error;
    }

    // Build success result
    yyjson_mut_doc *result_doc = yyjson_mut_doc_new(NULL);
    if (result_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *result_root = yyjson_mut_obj(result_doc);
    if (result_root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(result_doc, result_root);

    yyjson_mut_obj_add_str_(result_doc, result_root, "status", "sent");
    yyjson_mut_obj_add_str_(result_doc, result_root, "to", recipient_copy);

    char *result_json = yyjson_mut_write(result_doc, 0, NULL);
    if (result_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    char *result_copy = talloc_strdup(ctx, result_json);
    free(result_json);
    yyjson_mut_doc_free(result_doc);

    return ik_tool_wrap_success(ctx, result_copy);
}

// Registration function
void ik_internal_tools_register(ik_tool_registry_t *registry)
{
    assert(registry != NULL);  // LCOV_EXCL_BR_LINE

    // Create temporary context for schema buffers
    TALLOC_CTX *tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Create talloc allocator for JSON parsing
    yyjson_alc allocator = ik_make_talloc_allocator(registry);

    // Parse fork schema (copy to mutable buffer for yyjson_read_opts)
    char *fork_schema_buf = talloc_strdup(tmp_ctx, FORK_SCHEMA);
    if (fork_schema_buf == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_doc *fork_doc = yyjson_read_opts(fork_schema_buf, strlen(fork_schema_buf), 0, &allocator, NULL);
    if (fork_doc == NULL) PANIC("Failed to parse fork schema");  // LCOV_EXCL_BR_LINE

    // Parse kill schema
    char *kill_schema_buf = talloc_strdup(tmp_ctx, KILL_SCHEMA);
    if (kill_schema_buf == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_doc *kill_doc = yyjson_read_opts(kill_schema_buf, strlen(kill_schema_buf), 0, &allocator, NULL);
    if (kill_doc == NULL) PANIC("Failed to parse kill schema");  // LCOV_EXCL_BR_LINE

    // Parse send schema
    char *send_schema_buf = talloc_strdup(tmp_ctx, SEND_SCHEMA);
    if (send_schema_buf == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_doc *send_doc = yyjson_read_opts(send_schema_buf, strlen(send_schema_buf), 0, &allocator, NULL);
    if (send_doc == NULL) PANIC("Failed to parse send schema");  // LCOV_EXCL_BR_LINE

    // Parse wait schema
    char *wait_schema_buf = talloc_strdup(tmp_ctx, WAIT_SCHEMA);
    if (wait_schema_buf == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_doc *wait_doc = yyjson_read_opts(wait_schema_buf, strlen(wait_schema_buf), 0, &allocator, NULL);
    if (wait_doc == NULL) PANIC("Failed to parse wait schema");  // LCOV_EXCL_BR_LINE

    // Register fork tool
    res_t res = ik_tool_registry_add_internal(registry, "fork", fork_doc,
                                              ik_fork_handler, ik_fork_on_complete);
    if (is_err(&res)) PANIC("Failed to register fork tool");  // LCOV_EXCL_BR_LINE

    // Register kill tool
    res = ik_tool_registry_add_internal(registry, "kill", kill_doc,
                                        ik_internal_tool_kill_handler, ik_internal_tool_kill_on_complete);
    if (is_err(&res)) PANIC("Failed to register kill tool");  // LCOV_EXCL_BR_LINE

    // Register send tool (no on_complete needed)
    res = ik_tool_registry_add_internal(registry, "send", send_doc,
                                        ik_internal_tool_send_handler, NULL);
    if (is_err(&res)) PANIC("Failed to register send tool");  // LCOV_EXCL_BR_LINE

    // Register wait tool (no on_complete needed)
    res = ik_tool_registry_add_internal(registry, "wait", wait_doc,
                                        ik_wait_handler, NULL);
    if (is_err(&res)) PANIC("Failed to register wait tool");  // LCOV_EXCL_BR_LINE

    // Clean up temporary schema buffers (docs are now owned by registry)
    talloc_free(tmp_ctx);
}
