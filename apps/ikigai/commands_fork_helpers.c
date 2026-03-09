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
#include "shared/wrapper_internal.h"
#include "shared/wrapper_json.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "shared/poison.h"
/**
 * Copy parent's loaded_skills to child (no-prompt fork only).
 * load_position is reset to 0 so no /rewind in the child can drop inherited skills.
 */
void ik_commands_fork_copy_loaded_skills(ik_agent_ctx_t *child, const ik_agent_ctx_t *parent)
{
    if (parent->loaded_skill_count == 0) {
        return;
    }

    child->loaded_skills = talloc_zero_array(child, ik_loaded_skill_t *,
                                             (unsigned int)parent->loaded_skill_count);
    if (child->loaded_skills == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < parent->loaded_skill_count; i++) {
        ik_loaded_skill_t *src = parent->loaded_skills[i];
        ik_loaded_skill_t *dst = talloc_zero(child, ik_loaded_skill_t);
        if (dst == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dst->name = talloc_strdup(dst, src->name);
        if (dst->name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dst->content = talloc_strdup(dst, src->content);
        if (dst->content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dst->load_position = 0;
        child->loaded_skills[child->loaded_skill_count] = dst;
        child->loaded_skill_count++;
    }
}

/**
 * Copy parent's skillset_catalog to child (no-prompt fork only).
 * load_position is reset to 0 so no /rewind in the child can drop inherited entries.
 */
void ik_commands_fork_copy_skillset_catalog(ik_agent_ctx_t *child, const ik_agent_ctx_t *parent)
{
    if (parent->skillset_catalog_count == 0) {
        return;
    }

    child->skillset_catalog = talloc_zero_array(child, ik_skillset_catalog_entry_t *,
                                                (unsigned int)parent->skillset_catalog_count);
    if (child->skillset_catalog == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < parent->skillset_catalog_count; i++) {
        ik_skillset_catalog_entry_t *src = parent->skillset_catalog[i];
        ik_skillset_catalog_entry_t *dst = talloc_zero(child, ik_skillset_catalog_entry_t);
        if (dst == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dst->skill_name = talloc_strdup(dst, src->skill_name);
        if (dst->skill_name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dst->description = talloc_strdup(dst, src->description);
        if (dst->description == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dst->load_position = 0;
        child->skillset_catalog[child->skillset_catalog_count] = dst;
        child->skillset_catalog_count++;
    }
}

/**
 * Helper to convert thinking level enum to string
 */
const char *ik_commands_thinking_level_to_string(ik_thinking_level_t level)
{
    switch (level) {
        case IK_THINKING_MIN: return "min";
        case IK_THINKING_LOW:  return "low";
        case IK_THINKING_MED:  return "medium";
        case IK_THINKING_HIGH: return "high";
        default: return "unknown";
    }
}

/**
 * Helper to build fork feedback message
 */
char *ik_commands_build_fork_feedback(TALLOC_CTX *ctx, const ik_agent_ctx_t *child,
                                      bool is_override)
{
    (void)is_override;  // No longer needed - format is the same regardless
    const char *thinking_level_str = ik_commands_thinking_level_to_string(child->thinking_level);

    return talloc_asprintf(ctx, "Forked child %s (%s/%s/%s)",
                           child->uuid, child->provider, child->model, thinking_level_str);
}

/**
 * Helper to insert fork events into database
 */
res_t ik_commands_insert_fork_events(TALLOC_CTX *ctx, ik_repl_ctx_t *repl,
                                     ik_agent_ctx_t *parent, ik_agent_ctx_t *child,
                                     int64_t fork_message_id)
{
    if (repl->shared->session_id <= 0) {
        return OK(NULL);
    }

    // Insert parent-side fork event with full model information
    const char *thinking_level_str = ik_commands_thinking_level_to_string(child->thinking_level);
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
    res_t res = ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
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

    // Build loaded_skills JSON array from child's skills (inherited or empty)
    // Use yyjson mutation API to correctly escape skill content (may contain newlines, quotes, etc.)
    yyjson_mut_doc *skills_doc = yyjson_mut_doc_new(NULL);
    if (skills_doc == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    yyjson_mut_val *skills_arr = yyjson_mut_arr(skills_doc);  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(skills_doc, skills_arr);
    for (size_t i = 0; i < child->loaded_skill_count; i++) {
        ik_loaded_skill_t *sk = child->loaded_skills[i];  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *obj = yyjson_mut_obj(skills_doc);
        yyjson_mut_obj_add_str(skills_doc, obj, "skill", sk->name ? sk->name : "");  // LCOV_EXCL_BR_LINE
        yyjson_mut_obj_add_str(skills_doc, obj, "content", sk->content ? sk->content : "");  // LCOV_EXCL_BR_LINE
        yyjson_mut_arr_append(skills_arr, obj);
    }
    size_t skills_json_len = 0;
    char *skills_json_raw = yyjson_mut_write(skills_doc, 0, &skills_json_len);
    yyjson_mut_doc_free(skills_doc);
    char *final_skills_json = skills_json_raw  // LCOV_EXCL_BR_LINE
        ? talloc_strndup(ctx, skills_json_raw, skills_json_len)
        : talloc_strdup(ctx, "[]");  // LCOV_EXCL_BR_LINE
    free(skills_json_raw);
    if (final_skills_json == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }

    // Build skillset_catalog JSON array from child's catalog (inherited or empty)
    yyjson_mut_doc *catalog_doc = yyjson_mut_doc_new(NULL);
    if (catalog_doc == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    yyjson_mut_val *catalog_arr = yyjson_mut_arr(catalog_doc);  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(catalog_doc, catalog_arr);
    for (size_t i = 0; i < child->skillset_catalog_count; i++) {
        ik_skillset_catalog_entry_t *ce = child->skillset_catalog[i];  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *obj = yyjson_mut_obj(catalog_doc);
        yyjson_mut_obj_add_str(catalog_doc, obj, "skill", ce->skill_name ? ce->skill_name : "");  // LCOV_EXCL_BR_LINE
        yyjson_mut_obj_add_str(catalog_doc, obj, "description", ce->description ? ce->description : "");  // LCOV_EXCL_BR_LINE
        yyjson_mut_arr_append(catalog_arr, obj);
    }
    size_t catalog_json_len = 0;
    char *catalog_json_raw = yyjson_mut_write(catalog_doc, 0, &catalog_json_len);
    yyjson_mut_doc_free(catalog_doc);
    char *final_catalog_json = catalog_json_raw  // LCOV_EXCL_BR_LINE
        ? talloc_strndup(ctx, catalog_json_raw, catalog_json_len)
        : talloc_strdup(ctx, "[]");  // LCOV_EXCL_BR_LINE
    free(catalog_json_raw);
    if (final_catalog_json == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }

    char *child_data = talloc_asprintf(ctx,
                                       "{\"parent_uuid\":\"%s\",\"fork_message_id\":%" PRId64
                                       ",\"role\":\"child\",\"pinned_paths\":%s,\"toolset_filter\":%s"
                                       ",\"loaded_skills\":%s,\"skillset_catalog\":%s}",
                                       parent->uuid,
                                       fork_message_id,
                                       final_pins_json,
                                       final_toolset_json,
                                       final_skills_json,
                                       final_catalog_json);
    if (child_data == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    talloc_free(final_pins_json);
    talloc_free(final_toolset_json);
    talloc_free(final_skills_json);
    talloc_free(final_catalog_json);

    res = ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                                child->uuid, "fork", child_content, child_data);
    talloc_free(child_content);
    talloc_free(child_data);

    return res;
}
