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
    if (skill_name == NULL) return;

    // Replace if skill with same name already exists
    for (size_t i = 0; i < agent->loaded_skill_count; i++) {
        if (strcmp(agent->loaded_skills[i]->name, skill_name) == 0) {
            talloc_free(agent->loaded_skills[i]);
            ik_loaded_skill_t *skill = talloc_zero(agent, ik_loaded_skill_t);
            if (skill == NULL) return;  // LCOV_EXCL_LINE
            skill->name = talloc_strdup(skill, skill_name);
            skill->content = talloc_strdup(skill, content ? content : "");
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
    skill->content = talloc_strdup(skill, content ? content : "");
    skill->load_position = conv_count;
    agent->loaded_skills[agent->loaded_skill_count] = skill;
    agent->loaded_skill_count++;
}

// Helper: remove a skill from loaded_skills[] by name
static void skill_unload_entry(ik_agent_ctx_t *agent, const char *skill_name)
{
    if (skill_name == NULL) return;

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

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (root != NULL) {
        yyjson_val *skill_val = yyjson_obj_get_(root, "skill");
        yyjson_val *content_val = yyjson_obj_get_(root, "content");
        const char *skill_name = yyjson_get_str(skill_val);
        const char *content = yyjson_get_str(content_val);
        skill_load_entry(agent, skill_name, content, conv_count);
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

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (root != NULL) {
        yyjson_val *skill_val = yyjson_obj_get_(root, "skill");
        const char *skill_name = yyjson_get_str(skill_val);
        skill_unload_entry(agent, skill_name);
    }

    yyjson_doc_free(doc);
}
