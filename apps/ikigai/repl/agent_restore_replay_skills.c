// Agent restoration skill replay helpers
#include "apps/ikigai/repl/agent_restore_replay.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/msg.h"
#include "shared/error.h"
#include "shared/wrapper_json.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

// Helper: add or replace a skill in loaded_skills[]
static void skill_load_entry(ik_agent_ctx_t *agent, const char *skill_name,
                             const char *content, size_t conv_count)
{
    if (skill_name == NULL) return; // LCOV_EXCL_BR_LINE

    // Replace if skill with same name already exists
    for (size_t i = 0; i < agent->loaded_skill_count; i++) {
        if (strcmp(agent->loaded_skills[i]->name, skill_name) == 0) {
            talloc_free(agent->loaded_skills[i]);
            ik_loaded_skill_t *skill = talloc_zero(agent, ik_loaded_skill_t);
            if (skill == NULL) return;  // LCOV_EXCL_LINE
            skill->name = talloc_strdup(skill, skill_name);
            skill->content = talloc_strdup(skill, content ? content : ""); // LCOV_EXCL_BR_LINE
            skill->load_position = conv_count;
            agent->loaded_skills[i] = skill;
            return;
        }
    }

    // Add new skill
    ik_loaded_skill_t **new_skills = talloc_realloc(agent, agent->loaded_skills,
                                                    ik_loaded_skill_t *,
                                                    (unsigned int)(agent->loaded_skill_count + 1));
    if (new_skills == NULL) return;  // LCOV_EXCL_LINE
    agent->loaded_skills = new_skills;

    ik_loaded_skill_t *skill = talloc_zero(agent, ik_loaded_skill_t);
    if (skill == NULL) return;  // LCOV_EXCL_LINE
    skill->name = talloc_strdup(skill, skill_name);
    skill->content = talloc_strdup(skill, content ? content : ""); // LCOV_EXCL_BR_LINE
    skill->load_position = conv_count;
    agent->loaded_skills[agent->loaded_skill_count] = skill;
    agent->loaded_skill_count++;
}

// Helper: remove a skill from loaded_skills[] by name
static void skill_unload_entry(ik_agent_ctx_t *agent, const char *skill_name)
{
    if (skill_name == NULL) return; // LCOV_EXCL_BR_LINE

    int64_t found_index = -1;
    for (size_t i = 0; i < agent->loaded_skill_count; i++) {
        if (strcmp(agent->loaded_skills[i]->name, skill_name) == 0) {
            found_index = (int64_t)i;
            break;
        }
    }

    if (found_index < 0) return;

    talloc_free(agent->loaded_skills[found_index]);
    for (size_t i = (size_t)found_index; i + 1 < agent->loaded_skill_count; i++) {
        agent->loaded_skills[i] = agent->loaded_skills[i + 1];
    }
    agent->loaded_skill_count--;
}

// Public: add a catalog entry directly (used by fork event replay and skillset replay)
void ik_agent_restore_replay_catalog_entry_add(ik_agent_ctx_t *agent, const char *skill_name,
                                               const char *description, size_t conv_count)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    if (skill_name == NULL) return; // LCOV_EXCL_BR_LINE

    ik_skillset_catalog_entry_t **new_catalog = talloc_realloc(
        agent, agent->skillset_catalog, ik_skillset_catalog_entry_t *,
        (unsigned int)(agent->skillset_catalog_count + 1));
    if (new_catalog == NULL) return;  // LCOV_EXCL_LINE
    agent->skillset_catalog = new_catalog;

    ik_skillset_catalog_entry_t *e = talloc_zero(agent, ik_skillset_catalog_entry_t);
    if (e == NULL) return;  // LCOV_EXCL_LINE
    e->skill_name = talloc_strdup(e, skill_name);
    e->description = talloc_strdup(e, description ? description : ""); // LCOV_EXCL_BR_LINE
    e->load_position = conv_count;
    agent->skillset_catalog[agent->skillset_catalog_count] = e;
    agent->skillset_catalog_count++;
}

// Public: add skill by name+content directly (used by fork event replay)
void ik_agent_restore_replay_skill_load_named(ik_agent_ctx_t *agent, const char *skill_name,
                                              const char *content, size_t conv_count)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    skill_load_entry(agent, skill_name, content, conv_count);
}

// Public: replay skill_load event - add skill to loaded_skills[]
void ik_agent_restore_replay_skill_load(ik_agent_ctx_t *agent, ik_msg_t *msg, size_t conv_count)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(msg != NULL);    // LCOV_EXCL_BR_LINE

    if (msg->data_json == NULL) return;

    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    if (doc == NULL) return;

    yyjson_val *root = yyjson_doc_get_root_(doc); // LCOV_EXCL_BR_LINE
    if (root != NULL) { // LCOV_EXCL_BR_LINE
        yyjson_val *skill_val = yyjson_obj_get_(root, "skill"); // LCOV_EXCL_BR_LINE
        yyjson_val *content_val = yyjson_obj_get_(root, "content"); // LCOV_EXCL_BR_LINE
        const char *skill_name = yyjson_get_str(skill_val); // LCOV_EXCL_BR_LINE
        const char *content = yyjson_get_str(content_val); // LCOV_EXCL_BR_LINE
        skill_load_entry(agent, skill_name, content, conv_count);
    }

    yyjson_doc_free(doc);
}

// Public: replay skillset event - populate skillset_catalog[] from catalog_entries
void ik_agent_restore_replay_skillset(ik_agent_ctx_t *agent, ik_msg_t *msg, size_t conv_count)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(msg != NULL);    // LCOV_EXCL_BR_LINE

    if (msg->data_json == NULL) return;

    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    if (doc == NULL) return;

    yyjson_val *root = yyjson_doc_get_root_(doc); // LCOV_EXCL_BR_LINE
    if (root == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return;
    }

    yyjson_val *entries_val = yyjson_obj_get_(root, "catalog_entries"); // LCOV_EXCL_BR_LINE
    if (!yyjson_is_arr(entries_val)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return;
    }

    size_t idx = 0;
    size_t max_idx = 0;
    yyjson_val *entry_val = NULL;
    yyjson_arr_foreach(entries_val, idx, max_idx, entry_val) {  // LCOV_EXCL_BR_LINE
        if (!yyjson_is_obj(entry_val)) continue;  // LCOV_EXCL_BR_LINE
        const char *sn = yyjson_get_str(yyjson_obj_get_(entry_val, "skill"));
        const char *desc = yyjson_get_str(yyjson_obj_get_(entry_val, "description"));
        ik_agent_restore_replay_catalog_entry_add(agent, sn, desc, conv_count);
    }

    yyjson_doc_free(doc);
}

// Public: replay skill_unload event - remove skill from loaded_skills[]
void ik_agent_restore_replay_skill_unload(ik_agent_ctx_t *agent, ik_msg_t *msg)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(msg != NULL);    // LCOV_EXCL_BR_LINE

    if (msg->data_json == NULL) return;

    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    if (doc == NULL) return;

    yyjson_val *root = yyjson_doc_get_root_(doc); // LCOV_EXCL_BR_LINE
    if (root != NULL) { // LCOV_EXCL_BR_LINE
        yyjson_val *skill_val = yyjson_obj_get_(root, "skill"); // LCOV_EXCL_BR_LINE
        const char *skill_name = yyjson_get_str(skill_val); // LCOV_EXCL_BR_LINE
        skill_unload_entry(agent, skill_name);
    }

    yyjson_doc_free(doc);
}
