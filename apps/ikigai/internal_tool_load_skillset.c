/**
 * @file internal_tool_load_skillset.c
 * @brief load_skillset internal tool handler and on_complete hook
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
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

/* ================================================================
 * Deferred data: load_skillset
 * ================================================================ */

typedef struct {
    char *skillset_name;
    char **skill_names;
    char **skill_contents;
    size_t skill_count;
    char **catalog_names;
    char **catalog_descs;
    size_t catalog_count;
} ik_skillset_load_data_t;

/* ================================================================
 * Private helpers
 * ================================================================ */

/* Load one skill from doc_cache into data->skill_names/contents[]. */
static void load_one_preload_skill_(TALLOC_CTX *ctx, ik_agent_ctx_t *agent,
                                    const char *sname,
                                    ik_skillset_load_data_t *data)
{
    char *uri = talloc_asprintf(ctx, "ik://skills/%s/SKILL.md", sname);
    if (!uri) PANIC("OOM");  /* LCOV_EXCL_LINE */
    char *content = NULL;
    res_t sr = ik_doc_cache_get(agent->doc_cache, uri, &content);
    talloc_free(uri);
    if (is_err(&sr) || content == NULL) {
        if (is_err(&sr)) talloc_free(sr.err);
        return;  /* skip missing skills */
    }

    ik_config_t *cfg = (agent->shared != NULL) ? agent->shared->cfg : NULL;  // LCOV_EXCL_BR_LINE
    ik_template_result_t *tmpl = NULL;
    res_t tr = ik_template_process(ctx, content, agent, cfg, &tmpl);
    const char *resolved = content;
    if (is_ok(&tr) && tmpl != NULL) resolved = tmpl->processed;

    size_t i = data->skill_count;
    data->skill_names[i] = talloc_strdup(data, sname);
    data->skill_contents[i] = talloc_strdup(data, resolved);
    if (!data->skill_names[i] || !data->skill_contents[i]) PANIC("OOM");  /* LCOV_EXCL_LINE */
    data->skill_count++;

    if (tmpl != NULL) talloc_free(tmpl);  // LCOV_EXCL_BR_LINE
}

/* Populate skill_names/contents from preload array. */
static void load_preload_skills_(TALLOC_CTX *ctx, ik_agent_ctx_t *agent,  // LCOV_EXCL_BR_LINE
                                 yyjson_val *preload_arr,
                                 ik_skillset_load_data_t *data)
{
    if (!yyjson_is_arr(preload_arr)) return;
    size_t n = yyjson_arr_size(preload_arr);
    if (n == 0) return;

    data->skill_names = talloc_zero_array(data, char *, (unsigned int)n);
    data->skill_contents = talloc_zero_array(data, char *, (unsigned int)n);
    if (!data->skill_names || !data->skill_contents) PANIC("OOM");  /* LCOV_EXCL_LINE */

    size_t idx = 0, max = 0;
    yyjson_val *item = NULL;
    yyjson_arr_foreach(preload_arr, idx, max, item) {  /* LCOV_EXCL_BR_LINE */
        const char *sname = yyjson_get_str(item);
        if (sname == NULL) continue;  /* LCOV_EXCL_BR_LINE */
        load_one_preload_skill_(ctx, agent, sname, data);
    }
}

/* Populate catalog_names/descs from advertise array. */
static void parse_advertise_entries_(yyjson_val *advertise_arr,  // LCOV_EXCL_BR_LINE
                                     ik_skillset_load_data_t *data)
{
    if (!yyjson_is_arr(advertise_arr)) return;
    size_t n = yyjson_arr_size(advertise_arr);
    if (n == 0) return;

    data->catalog_names = talloc_zero_array(data, char *, (unsigned int)n);
    data->catalog_descs = talloc_zero_array(data, char *, (unsigned int)n);
    if (!data->catalog_names || !data->catalog_descs) PANIC("OOM");  /* LCOV_EXCL_LINE */

    size_t idx = 0, max = 0;
    yyjson_val *entry = NULL;
    yyjson_arr_foreach(advertise_arr, idx, max, entry) {  /* LCOV_EXCL_BR_LINE */
        if (!yyjson_is_obj(entry)) continue;  /* LCOV_EXCL_BR_LINE */
        const char *sn = yyjson_get_str(yyjson_obj_get(entry, "skill"));    // LCOV_EXCL_BR_LINE
        const char *desc = yyjson_get_str(yyjson_obj_get(entry, "description")); // LCOV_EXCL_BR_LINE
        if (sn == NULL) continue;  /* LCOV_EXCL_BR_LINE */
        size_t i = data->catalog_count;
        data->catalog_names[i] = talloc_strdup(data, sn);
        data->catalog_descs[i] = talloc_strdup(data, desc ? desc : "");
        if (!data->catalog_names[i] || !data->catalog_descs[i]) PANIC("OOM");  /* LCOV_EXCL_LINE */
        data->catalog_count++;
    }
}

/* Persist skillset DB event from on_complete (main thread). */
static void persist_skillset_event_(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent,
                                    const ik_skillset_load_data_t *data)
{
    if (repl->shared->db_ctx == NULL || repl->shared->session_id <= 0) return;

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) PANIC("OOM");  /* LCOV_EXCL_LINE */
    yyjson_mut_val *root = yyjson_mut_obj(doc);          // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_str(doc, root, "skillset", data->skillset_name);  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *entries = yyjson_mut_arr(doc);
    for (size_t i = 0; i < data->catalog_count; i++) {
        yyjson_mut_val *e = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_str(doc, e, "skill", data->catalog_names[i]);  // LCOV_EXCL_BR_LINE
        yyjson_mut_obj_add_str(doc, e, "description", data->catalog_descs[i]);  // LCOV_EXCL_BR_LINE
        yyjson_mut_arr_append(entries, e);
    }
    yyjson_mut_obj_add_val(doc, root, "catalog_entries", entries);

    size_t jlen = 0;
    char *jraw = yyjson_mut_write(doc, 0, &jlen);
    yyjson_mut_doc_free(doc);
    if (jraw != NULL) {                                    // LCOV_EXCL_BR_LINE
        TALLOC_CTX *tmp = talloc_new(NULL);
        char *dj = talloc_strndup(tmp, jraw, jlen);
        free(jraw);
        if (dj) {                                          // LCOV_EXCL_BR_LINE
            res_t r = ik_db_message_insert(repl->shared->db_ctx,
                                           repl->shared->session_id,
                                           agent->uuid, "skillset",
                                           NULL, dj);
            if (is_err(&r)) talloc_free(r.err);           // LCOV_EXCL_BR_LINE
        }
        talloc_free(tmp);
    }
}

/* ================================================================
 * load_skillset handler (worker thread)
 * ================================================================ */

char *ik_internal_tool_load_skillset_handler(TALLOC_CTX *ctx,
                                             ik_agent_ctx_t *agent,
                                             const char *args_json)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(args_json != NULL);  // LCOV_EXCL_BR_LINE

    yyjson_doc *doc = yyjson_read_(args_json, strlen(args_json), 0);
    if (doc == NULL) {  // LCOV_EXCL_BR_LINE
        return ik_tool_wrap_failure(ctx, "Failed to parse load_skillset arguments",
                                    "PARSE_ERROR");
    }
    yyjson_val *root = yyjson_doc_get_root_(doc);
    yyjson_val *name_val = yyjson_obj_get_(root, "skillset");
    if (name_val == NULL || !yyjson_is_str(name_val)) {
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: skillset",
                                    "INVALID_ARG");
    }
    char *skillset_name = talloc_strdup(ctx, yyjson_get_str_(name_val));
    if (!skillset_name) PANIC("OOM");  // LCOV_EXCL_LINE
    yyjson_doc_free(doc);

    if (agent->doc_cache == NULL) {
        char *msg = talloc_asprintf(ctx, "Skillset not found: %s", skillset_name);
        if (!msg) PANIC("OOM");  // LCOV_EXCL_LINE
        return ik_tool_wrap_failure(ctx, msg, "SKILLSET_NOT_FOUND");
    }

    char *uri = talloc_asprintf(ctx, "ik://skillsets/%s.json", skillset_name);
    if (!uri) PANIC("OOM");  // LCOV_EXCL_LINE
    char *file_content = NULL;
    res_t read_res = ik_doc_cache_get(agent->doc_cache, uri, &file_content);
    if (is_err(&read_res) || file_content == NULL) {
        if (is_err(&read_res)) talloc_free(read_res.err);
        char *msg = talloc_asprintf(ctx, "Skillset not found: %s", skillset_name);
        if (!msg) PANIC("OOM");  // LCOV_EXCL_LINE
        return ik_tool_wrap_failure(ctx, msg, "SKILLSET_NOT_FOUND");
    }

    yyjson_doc *sdoc = yyjson_read(file_content, strlen(file_content), 0);
    if (sdoc == NULL || !yyjson_is_obj(yyjson_doc_get_root(sdoc))) {  // LCOV_EXCL_BR_LINE
        if (sdoc) yyjson_doc_free(sdoc);
        char *msg = talloc_asprintf(ctx, "Skillset JSON malformed: %s",
                                    skillset_name);
        if (!msg) PANIC("OOM");  // LCOV_EXCL_LINE
        return ik_tool_wrap_failure(ctx, msg, "SKILLSET_MALFORMED");
    }

    yyjson_val *sroot = yyjson_doc_get_root(sdoc);                  // LCOV_EXCL_BR_LINE
    yyjson_val *preload_arr = yyjson_obj_get(sroot, "preload");      // LCOV_EXCL_BR_LINE
    yyjson_val *advertise_arr = yyjson_obj_get(sroot, "advertise");

    ik_skillset_load_data_t *data = talloc_zero(ctx, ik_skillset_load_data_t);
    if (!data) PANIC("OOM");  // LCOV_EXCL_LINE
    data->skillset_name = talloc_strdup(data, skillset_name);
    if (!data->skillset_name) PANIC("OOM");  // LCOV_EXCL_LINE

    load_preload_skills_(ctx, agent, preload_arr, data);
    parse_advertise_entries_(advertise_arr, data);
    yyjson_doc_free(sdoc);

    agent->tool_deferred_data = data;

    yyjson_mut_doc *rdoc = yyjson_mut_doc_new(NULL);
    if (!rdoc) PANIC("OOM");  // LCOV_EXCL_LINE
    yyjson_mut_val *rroot = yyjson_mut_obj(rdoc);              // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(rdoc, rroot);
    yyjson_mut_obj_add_str_(rdoc, rroot, "skillset", skillset_name);
    yyjson_mut_obj_add_uint(rdoc, rroot, "skills_loaded", (uint64_t)data->skill_count);   // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_uint(rdoc, rroot, "catalog_entries", (uint64_t)data->catalog_count); // LCOV_EXCL_BR_LINE
    char *json = yyjson_mut_write(rdoc, 0, NULL);
    if (!json) PANIC("OOM");  // LCOV_EXCL_LINE
    char *result = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(rdoc);
    return ik_tool_wrap_success(ctx, result);
}

/* ================================================================
 * load_skillset on_complete (main thread)
 * ================================================================ */

void ik_internal_tool_load_skillset_on_complete(ik_repl_ctx_t *repl,
                                                ik_agent_ctx_t *agent)
{
    assert(repl != NULL);   // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    if (agent->tool_deferred_data == NULL) {  // LCOV_EXCL_BR_LINE
        return;
    }
    ik_skillset_load_data_t *data =
        (ik_skillset_load_data_t *)agent->tool_deferred_data;

    for (size_t i = 0; i < data->skill_count; i++) {
        ik_skill_store_loaded(agent, data->skill_names[i],
                              data->skill_contents[i]);
    }
    for (size_t i = 0; i < data->catalog_count; i++) {
        ik_skillset_store_catalog_entry(agent, data->catalog_names[i],
                                        data->catalog_descs[i]);
    }

    persist_skillset_event_(repl, agent, data);

    if (agent->token_cache != NULL) {
        ik_token_cache_invalidate_system(agent->token_cache);
    }
    talloc_free(agent->tool_deferred_data);
    agent->tool_deferred_data = NULL;
}
