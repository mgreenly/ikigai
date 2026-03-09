/**
 * @file internal_tool_skill.c
 * @brief load_skill, unload_skill, and list_skills internal tool handlers
 */

#include "apps/ikigai/internal_tool_skill.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_skill.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/doc_cache.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/template.h"
#include "apps/ikigai/token_cache.h"
#include "apps/ikigai/tool_wrapper.h"
#include "shared/error.h"
#include "shared/panic.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

/* ================================================================
 * Private helpers: positional arg substitution (mirrors commands_skill.c)
 * ================================================================ */

static const char *match_positional_var_(const char *p, const char *end,
                                         char **pos_args, size_t pos_arg_count)
{
    size_t var_len = (size_t)(end - p - 2);
    if (var_len == 0 || var_len >= 10) return NULL;
    for (size_t k = 0; k < var_len; k++) {
        if (!isdigit((unsigned char)p[2 + k])) return NULL;
    }
    char num_buf[12];
    memcpy(num_buf, p + 2, var_len);
    num_buf[var_len] = '\0';
    size_t idx = (size_t)strtoul(num_buf, NULL, 10);
    if (idx < 1 || idx > pos_arg_count) return NULL;
    return pos_args[idx - 1];
}

static char *apply_positional_args_(TALLOC_CTX *ctx, const char *content,
                                    char **pos_args, size_t pos_arg_count)
{
    if (pos_arg_count == 0) {
        char *r = talloc_strdup(ctx, content);
        if (!r) PANIC("OOM");  /* LCOV_EXCL_LINE */
        return r;
    }
    char *result = talloc_strdup(ctx, "");
    if (!result) PANIC("OOM");  /* LCOV_EXCL_LINE */
    const char *start = content;
    const char *p = content;
    while (*p != '\0') {
        if (p[0] == '$' && p[1] == '{') {
            const char *end = strchr(p + 2, '}');
            if (end != NULL) {
                const char *v = match_positional_var_(p, end, pos_args,
                                                      pos_arg_count);
                if (v != NULL) {
                    char *tmp = talloc_asprintf(ctx, "%s%.*s%s",
                                               result, (int)(p - start),
                                               start, v);
                    if (!tmp) PANIC("OOM");  /* LCOV_EXCL_LINE */
                    talloc_free(result);
                    result = tmp;
                    p = end + 1;
                    start = p;
                    continue;
                }
            }
        }
        p++;
    }
    if (p > start) {
        char *tmp = talloc_asprintf(ctx, "%s%s", result, start);
        if (!tmp) PANIC("OOM");  /* LCOV_EXCL_LINE */
        talloc_free(result);
        result = tmp;
    }
    return result;
}

/* Deferred data: load_skill */
typedef struct {
    char *skill_name;
    char *content;
    char **pos_args;
    size_t pos_arg_count;
} ik_skill_load_data_t;

/* load_skill */
char *ik_internal_tool_load_skill_handler(TALLOC_CTX *ctx,
                                          ik_agent_ctx_t *agent,
                                          const char *args_json)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(args_json != NULL);  // LCOV_EXCL_BR_LINE

    yyjson_doc *doc = yyjson_read_(args_json, strlen(args_json), 0);
    if (doc == NULL) {  // LCOV_EXCL_BR_LINE
        return ik_tool_wrap_failure(ctx, "Failed to parse load_skill arguments",
                                    "PARSE_ERROR");
    }
    yyjson_val *root = yyjson_doc_get_root_(doc);

    yyjson_val *skill_val = yyjson_obj_get_(root, "skill");
    if (skill_val == NULL || !yyjson_is_str(skill_val)) {
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: skill",
                                    "INVALID_ARG");
    }
    char *skill_name = talloc_strdup(ctx, yyjson_get_str_(skill_val));
    if (!skill_name) PANIC("OOM");  // LCOV_EXCL_LINE

    size_t pos_arg_count = 0;
    char **pos_args = NULL;
    yyjson_val *args_val = yyjson_obj_get_(root, "args");
    if (args_val != NULL && yyjson_is_arr(args_val)) {
        pos_arg_count = yyjson_arr_size(args_val);
        if (pos_arg_count > 0) {
            pos_args = talloc_zero_array(ctx, char *, (unsigned int)pos_arg_count);
            if (!pos_args) PANIC("OOM");  // LCOV_EXCL_LINE
            size_t i = 0, max = 0;
            yyjson_val *item = NULL;
            yyjson_arr_foreach(args_val, i, max, item) {  /* LCOV_EXCL_BR_LINE */
                const char *s = yyjson_get_str_(item);
                pos_args[i] = talloc_strdup(ctx, s ? s : "");
                if (!pos_args[i]) PANIC("OOM");  // LCOV_EXCL_LINE
            }
        }
    }
    yyjson_doc_free(doc);

    if (agent->doc_cache == NULL) {
        char *msg = talloc_asprintf(ctx, "Skill not found: %s", skill_name);
        if (!msg) PANIC("OOM");  // LCOV_EXCL_LINE
        return ik_tool_wrap_failure(ctx, msg, "SKILL_NOT_FOUND");
    }

    char *uri = talloc_asprintf(ctx, "ik://skills/%s/SKILL.md", skill_name);
    if (!uri) PANIC("OOM");  // LCOV_EXCL_LINE
    char *file_content = NULL;
    res_t read_res = ik_doc_cache_get(agent->doc_cache, uri, &file_content);
    if (is_err(&read_res) || file_content == NULL) {
        if (is_err(&read_res)) talloc_free(read_res.err);
        char *msg = talloc_asprintf(ctx, "Skill not found: %s", skill_name);
        if (!msg) PANIC("OOM");  // LCOV_EXCL_LINE
        return ik_tool_wrap_failure(ctx, msg, "SKILL_NOT_FOUND");
    }

    char *substituted = apply_positional_args_(ctx, file_content, pos_args,
                                               pos_arg_count);
    ik_config_t *config = (agent->shared != NULL) ? agent->shared->cfg : NULL;
    ik_template_result_t *tmpl = NULL;
    res_t tr = ik_template_process(ctx, substituted, agent, config, &tmpl);
    const char *resolved = substituted;
    if (is_ok(&tr) && tmpl != NULL) resolved = tmpl->processed;  /* LCOV_EXCL_BR_LINE */

    ik_skill_load_data_t *data = talloc_zero(ctx, ik_skill_load_data_t);
    if (!data) PANIC("OOM");  // LCOV_EXCL_LINE
    data->skill_name = talloc_strdup(data, skill_name);
    data->content = talloc_strdup(data, resolved);
    if (!data->skill_name || !data->content) PANIC("OOM");  // LCOV_EXCL_LINE
    if (pos_arg_count > 0 && pos_args != NULL) {  // LCOV_EXCL_BR_LINE
        data->pos_args = talloc_zero_array(data, char *,
                                           (unsigned int)pos_arg_count);
        if (!data->pos_args) PANIC("OOM");  // LCOV_EXCL_LINE
        for (size_t i = 0; i < pos_arg_count; i++) {
            data->pos_args[i] = talloc_strdup(data, pos_args[i]);
            if (!data->pos_args[i]) PANIC("OOM");  // LCOV_EXCL_LINE
        }
    }
    data->pos_arg_count = pos_arg_count;
    if (tmpl != NULL) talloc_free(tmpl);
    agent->tool_deferred_data = data;

    yyjson_mut_doc *rdoc = yyjson_mut_doc_new(NULL);
    if (!rdoc) PANIC("OOM");  // LCOV_EXCL_LINE
    yyjson_mut_val *rroot = yyjson_mut_obj(rdoc);  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(rdoc, rroot);
    yyjson_mut_obj_add_str_(rdoc, rroot, "skill", skill_name);
    yyjson_mut_obj_add_str_(rdoc, rroot, "status", "loaded");
    char *json = yyjson_mut_write(rdoc, 0, NULL);
    if (!json) PANIC("OOM");  // LCOV_EXCL_LINE
    char *result = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(rdoc);
    return ik_tool_wrap_success(ctx, result);
}

void ik_internal_tool_load_skill_on_complete(ik_repl_ctx_t *repl,
                                             ik_agent_ctx_t *agent)
{
    assert(repl != NULL);   // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    if (agent->tool_deferred_data == NULL) {  // LCOV_EXCL_BR_LINE
        return;
    }
    ik_skill_load_data_t *data = (ik_skill_load_data_t *)agent->tool_deferred_data;
    ik_skill_store_loaded(agent, data->skill_name, data->content);

    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {
        yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
        if (!doc) PANIC("OOM");  /* LCOV_EXCL_LINE */
        /* LCOV_EXCL_BR_START */
        yyjson_mut_val *root = yyjson_mut_obj(doc);
        yyjson_mut_doc_set_root(doc, root);
        yyjson_mut_obj_add_str(doc, root, "skill", data->skill_name);
        yyjson_mut_val *arr = yyjson_mut_arr(doc);
        for (size_t i = 0; i < data->pos_arg_count; i++) {
            yyjson_mut_arr_add_str(doc, arr, data->pos_args[i]);
        }
        yyjson_mut_obj_add_val(doc, root, "args", arr);
        yyjson_mut_obj_add_str(doc, root, "content", data->content);
        size_t jlen = 0;
        char *jraw = yyjson_mut_write(doc, 0, &jlen);
        yyjson_mut_doc_free(doc);
        if (jraw != NULL) {
            TALLOC_CTX *tmp = talloc_new(NULL);
            char *dj = talloc_strndup(tmp, jraw, jlen);
            free(jraw);
            if (dj) {
                res_t r = ik_db_message_insert(repl->shared->db_ctx,
                                               repl->shared->session_id,
                                               agent->uuid, "skill_load",
                                               NULL, dj);
                if (is_err(&r)) talloc_free(r.err);
            }
            talloc_free(tmp);
        }
        /* LCOV_EXCL_BR_STOP */
    }
    if (agent->token_cache != NULL) {
        ik_token_cache_invalidate_system(agent->token_cache);
    }
    talloc_free(agent->tool_deferred_data);
    agent->tool_deferred_data = NULL;
}

/* ================================================================
 * unload_skill
 * ================================================================ */

char *ik_internal_tool_unload_skill_handler(TALLOC_CTX *ctx,
                                            ik_agent_ctx_t *agent,
                                            const char *args_json)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(args_json != NULL);  // LCOV_EXCL_BR_LINE

    yyjson_doc *doc = yyjson_read_(args_json, strlen(args_json), 0);
    if (doc == NULL) {  // LCOV_EXCL_BR_LINE
        return ik_tool_wrap_failure(ctx, "Failed to parse unload_skill arguments",
                                    "PARSE_ERROR");
    }
    yyjson_val *root = yyjson_doc_get_root_(doc);
    yyjson_val *skill_val = yyjson_obj_get_(root, "skill");
    if (skill_val == NULL || !yyjson_is_str(skill_val)) {
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: skill",
                                    "INVALID_ARG");
    }
    char *skill_name = talloc_strdup(ctx, yyjson_get_str_(skill_val));
    if (!skill_name) PANIC("OOM");  // LCOV_EXCL_LINE
    yyjson_doc_free(doc);

    bool found = false;
    for (size_t i = 0; i < agent->loaded_skill_count; i++) {
        if (strcmp(agent->loaded_skills[i]->name, skill_name) == 0) {
            found = true;
            break;
        }
    }
    if (!found) {
        char *msg = talloc_asprintf(ctx, "Skill not loaded: %s", skill_name);
        if (!msg) PANIC("OOM");  // LCOV_EXCL_LINE
        return ik_tool_wrap_failure(ctx, msg, "SKILL_NOT_LOADED");
    }
    agent->tool_deferred_data = skill_name;

    yyjson_mut_doc *rdoc = yyjson_mut_doc_new(NULL);
    if (!rdoc) PANIC("OOM");  // LCOV_EXCL_LINE
    yyjson_mut_val *rroot = yyjson_mut_obj(rdoc);  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(rdoc, rroot);
    yyjson_mut_obj_add_str_(rdoc, rroot, "skill", skill_name);
    yyjson_mut_obj_add_str_(rdoc, rroot, "status", "unloaded");
    char *json = yyjson_mut_write(rdoc, 0, NULL);
    if (!json) PANIC("OOM");  // LCOV_EXCL_LINE
    char *result = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(rdoc);
    return ik_tool_wrap_success(ctx, result);
}

void ik_internal_tool_unload_skill_on_complete(ik_repl_ctx_t *repl,
                                               ik_agent_ctx_t *agent)
{
    assert(repl != NULL);   // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    if (agent->tool_deferred_data == NULL) {  // LCOV_EXCL_BR_LINE
        return;
    }
    const char *skill_name = (const char *)agent->tool_deferred_data;

    size_t found_idx = agent->loaded_skill_count;
    for (size_t i = 0; i < agent->loaded_skill_count; i++) {
        if (strcmp(agent->loaded_skills[i]->name, skill_name) == 0) {
            found_idx = i;
            break;
        }
    }
    if (found_idx < agent->loaded_skill_count) {
        talloc_free(agent->loaded_skills[found_idx]);
        for (size_t i = found_idx; i + 1 < agent->loaded_skill_count; i++) {
            agent->loaded_skills[i] = agent->loaded_skills[i + 1];
        }
        agent->loaded_skill_count--;
    }

    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {
        yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
        if (!doc) PANIC("OOM");  /* LCOV_EXCL_LINE */
        /* LCOV_EXCL_BR_START */
        yyjson_mut_val *root = yyjson_mut_obj(doc);
        yyjson_mut_doc_set_root(doc, root);
        yyjson_mut_obj_add_str(doc, root, "skill", skill_name);
        size_t jlen = 0;
        char *jraw = yyjson_mut_write(doc, 0, &jlen);
        yyjson_mut_doc_free(doc);
        if (jraw != NULL) {
            TALLOC_CTX *tmp = talloc_new(NULL);
            char *dj = talloc_strndup(tmp, jraw, jlen);
            free(jraw);
            if (dj) {
                res_t r = ik_db_message_insert(repl->shared->db_ctx,
                                               repl->shared->session_id,
                                               agent->uuid, "skill_unload",
                                               NULL, dj);
                if (is_err(&r)) talloc_free(r.err);
            }
            talloc_free(tmp);
        }
        /* LCOV_EXCL_BR_STOP */
    }
    if (agent->token_cache != NULL) {
        ik_token_cache_invalidate_system(agent->token_cache);
    }
    agent->tool_deferred_data = NULL;
}

/* ================================================================
 * list_skills (read-only, no on_complete)
 * ================================================================ */

char *ik_internal_tool_list_skills_handler(TALLOC_CTX *ctx,
                                           ik_agent_ctx_t *agent,
                                           const char *args_json)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    (void)args_json;

    yyjson_mut_doc *rdoc = yyjson_mut_doc_new(NULL);
    if (!rdoc) PANIC("OOM");  // LCOV_EXCL_LINE
    /* LCOV_EXCL_BR_START */
    yyjson_mut_val *rroot = yyjson_mut_obj(rdoc);
    yyjson_mut_doc_set_root(rdoc, rroot);

    yyjson_mut_val *loaded_arr = yyjson_mut_arr(rdoc);
    for (size_t i = 0; i < agent->loaded_skill_count; i++) {
        ik_loaded_skill_t *s = agent->loaded_skills[i];
        yyjson_mut_val *e = yyjson_mut_obj(rdoc);
        yyjson_mut_obj_add_str(rdoc, e, "name", s->name);
        yyjson_mut_obj_add_uint(rdoc, e, "size", (uint64_t)strlen(s->content));
        yyjson_mut_arr_append(loaded_arr, e);
    }
    yyjson_mut_obj_add_val(rdoc, rroot, "loaded", loaded_arr);

    yyjson_mut_val *catalog_arr = yyjson_mut_arr(rdoc);
    for (size_t i = 0; i < agent->skillset_catalog_count; i++) {
        ik_skillset_catalog_entry_t *e = agent->skillset_catalog[i];
        yyjson_mut_val *entry = yyjson_mut_obj(rdoc);
        yyjson_mut_obj_add_str(rdoc, entry, "name", e->skill_name);
        yyjson_mut_obj_add_str(rdoc, entry, "description", e->description);
        yyjson_mut_arr_append(catalog_arr, entry);
    }
    /* LCOV_EXCL_BR_STOP */
    yyjson_mut_obj_add_val(rdoc, rroot, "catalog", catalog_arr);

    char *json = yyjson_mut_write(rdoc, 0, NULL);
    if (!json) PANIC("OOM");  // LCOV_EXCL_LINE
    char *result = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(rdoc);
    return ik_tool_wrap_success(ctx, result);
}
