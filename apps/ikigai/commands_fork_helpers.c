/**
 * @file commands_fork_helpers.c
 * @brief Fork command utility helpers
 */

#include "apps/ikigai/commands_fork_helpers.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/message.h"
#include "shared/panic.h"
#include "apps/ikigai/shared.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>


#include "shared/poison.h"
/**
 * Helper to convert thinking level enum to string
 */
const char *thinking_level_to_string(ik_thinking_level_t level)
{
    switch (level) {
        case IK_THINKING_NONE: return "none";
        case IK_THINKING_LOW:  return "low";
        case IK_THINKING_MED:  return "medium";
        case IK_THINKING_HIGH: return "high";
        default: return "unknown";
    }
}

/**
 * Helper to build fork feedback message
 */
char *build_fork_feedback(TALLOC_CTX *ctx, const ik_agent_ctx_t *child,
                          bool is_override)
{
    (void)is_override;  // No longer needed - format is the same regardless
    const char *thinking_level_str = thinking_level_to_string(child->thinking_level);

    return talloc_asprintf(ctx, "Forked child %s (%s/%s/%s)",
                           child->uuid, child->provider, child->model, thinking_level_str);
}

/**
 * Helper to insert fork events into database
 */
res_t insert_fork_events(TALLOC_CTX *ctx, ik_repl_ctx_t *repl,
                         ik_agent_ctx_t *parent, ik_agent_ctx_t *child,
                         int64_t fork_message_id)
{
    if (repl->shared->session_id <= 0) {
        return OK(NULL);
    }

    // Insert parent-side fork event with full model information
    const char *thinking_level_str = thinking_level_to_string(child->thinking_level);
    char *parent_content = talloc_asprintf(ctx, "Forked child %s (%s/%s/%s)",
                                           child->uuid, child->provider, child->model,
                                           thinking_level_str);
    if (parent_content == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    char *parent_data = talloc_asprintf(ctx,
                                        "{\"child_uuid\":\"%s\",\"fork_message_id\":%" PRId64
                                        ",\"role\":\"parent\"}",
                                        child->uuid, fork_message_id);
    if (parent_data == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    res_t res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                                     parent->uuid, "fork", parent_content, parent_data);
    talloc_free(parent_content);
    talloc_free(parent_data);
    if (is_err(&res)) {
        return res;
    }

    // Insert child-side fork event with pinned_paths snapshot
    char *child_content = talloc_asprintf(ctx, "Forked from %.22s", parent->uuid);
    if (child_content == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }

    // Build pinned_paths JSON array from parent's current pins
    char *pins_json = talloc_strdup(ctx, "[");
    if (pins_json == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    for (size_t i = 0; i < parent->pinned_count; i++) {
        const char *path = parent->pinned_paths[i];
        char *escaped_path = talloc_strdup(ctx, path);
        if (escaped_path == NULL) {     // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");     // LCOV_EXCL_LINE
        }
        char *new_pins = talloc_asprintf(ctx, "%s%s\"%s\"",
                                          pins_json,
                                          (i > 0) ? "," : "",
                                          escaped_path);
        if (new_pins == NULL) {     // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");     // LCOV_EXCL_LINE
        }
        talloc_free(pins_json);
        talloc_free(escaped_path);
        pins_json = new_pins;
    }
    char *final_pins_json = talloc_asprintf(ctx, "%s]", pins_json);
    if (final_pins_json == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    talloc_free(pins_json);

    // Build toolset_filter JSON array from parent's current filter
    char *toolset_json = talloc_strdup(ctx, "[");
    if (toolset_json == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    for (size_t i = 0; i < parent->toolset_count; i++) {
        const char *tool = parent->toolset_filter[i];
        char *new_toolset = talloc_asprintf(ctx, "%s%s\"%s\"",
                                            toolset_json,
                                            (i > 0) ? "," : "",
                                            tool);
        if (new_toolset == NULL) {     // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");     // LCOV_EXCL_LINE
        }
        talloc_free(toolset_json);
        toolset_json = new_toolset;
    }
    char *final_toolset_json = talloc_asprintf(ctx, "%s]", toolset_json);
    if (final_toolset_json == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    talloc_free(toolset_json);

    char *child_data = talloc_asprintf(ctx,
                                       "{\"parent_uuid\":\"%s\",\"fork_message_id\":%" PRId64 ",\"role\":\"child\",\"pinned_paths\":%s,\"toolset_filter\":%s}",
                                       parent->uuid, fork_message_id, final_pins_json, final_toolset_json);
    if (child_data == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    talloc_free(final_pins_json);
    talloc_free(final_toolset_json);

    res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                               child->uuid, "fork", child_content, child_data);
    talloc_free(child_content);
    talloc_free(child_data);

    return res;
}
