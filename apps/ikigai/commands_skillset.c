/**
 * @file commands_skillset.c
 * @brief Skillset slash command handler (/skillset)
 */

#include "apps/ikigai/commands_skill.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/doc_cache.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/scrollback_utils.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/token_cache.h"
#include "shared/error.h"
#include "shared/panic.h"
#include "shared/wrapper_internal.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

/* Emit a warning line to agent scrollback. */
static void warn_(void *ctx, ik_agent_ctx_t *agent, const char *text)
{
    char *warn = ik_scrollback_format_warning(ctx, text);
    ik_scrollback_append_line(agent->scrollback, warn, strlen(warn));
    talloc_free(warn);
}

/* Add a catalog entry to agent->skillset_catalog[]. */
void ik_skillset_store_catalog_entry(ik_agent_ctx_t *agent, const char *skill_name,
                                     const char *description)
{
    ik_skillset_catalog_entry_t **new_catalog = talloc_realloc(
        agent, agent->skillset_catalog, ik_skillset_catalog_entry_t *,
        (unsigned int)(agent->skillset_catalog_count + 1));
    if (!new_catalog) PANIC("OOM");  /* LCOV_EXCL_LINE */
    agent->skillset_catalog = new_catalog;

    ik_skillset_catalog_entry_t *e = talloc_zero(agent, ik_skillset_catalog_entry_t);
    if (!e) PANIC("OOM");  /* LCOV_EXCL_LINE */
    e->skill_name = talloc_strdup(e, skill_name);
    e->description = talloc_strdup(e, description ? description : "");
    e->load_position = agent->message_count;
    agent->skillset_catalog[agent->skillset_catalog_count] = e;
    agent->skillset_catalog_count++;
}

/* Read and parse the skillset JSON; returns NULL and warns on any error. */
static yyjson_doc *load_skillset_json_(void *ctx, ik_agent_ctx_t *agent,
                                       const char *args)
{
    if (agent->doc_cache == NULL) {
        char *text = talloc_asprintf(ctx, "Skillset not found: %s", args);
        if (!text) PANIC("OOM");  /* LCOV_EXCL_LINE */
        warn_(ctx, agent, text);
        talloc_free(text);
        return NULL;
    }

    char *uri = talloc_asprintf(ctx, "ik://skillsets/%s.json", args);
    if (!uri) PANIC("OOM");  /* LCOV_EXCL_LINE */

    char *file_content = NULL;
    res_t read_res = ik_doc_cache_get_(agent->doc_cache, uri, &file_content);
    talloc_free(uri);

    if (is_err(&read_res) || file_content == NULL) {
        if (is_err(&read_res)) talloc_free(read_res.err);
        char *text = talloc_asprintf(ctx, "Skillset not found: %s", args);
        if (!text) PANIC("OOM");  /* LCOV_EXCL_LINE */
        warn_(ctx, agent, text);
        talloc_free(text);
        return NULL;
    }

    yyjson_doc *doc = yyjson_read(file_content, strlen(file_content), 0);
    if (doc == NULL || !yyjson_is_obj(yyjson_doc_get_root(doc))) {  /* LCOV_EXCL_BR_LINE */
        if (doc) yyjson_doc_free(doc);  /* LCOV_EXCL_BR_LINE */
        char *text = talloc_asprintf(ctx, "Skillset JSON malformed: %s", args);
        if (!text) PANIC("OOM");  /* LCOV_EXCL_LINE */
        warn_(ctx, agent, text);
        talloc_free(text);
        return NULL;
    }

    return doc;
}

/* Load each skill named in preload_arr; returns count of successfully loaded. */
static size_t preload_skills_(void *ctx, ik_repl_ctx_t *repl,
                              ik_agent_ctx_t *agent, yyjson_val *preload_arr)
{
    size_t count = 0;
    if (!yyjson_is_arr(preload_arr)) return 0;

    size_t idx = 0, max_idx = 0;
    yyjson_val *item = NULL;
    yyjson_arr_foreach(preload_arr, idx, max_idx, item) {  /* LCOV_EXCL_BR_LINE */
        const char *skill_name = yyjson_get_str(item);
        if (skill_name == NULL) continue;  /* LCOV_EXCL_BR_LINE */
        if (!ik_skill_load_by_name_(ctx, repl, agent, skill_name)) {
            char *text = talloc_asprintf(ctx, "Skill not found (skipping): %s", skill_name);
            if (!text) PANIC("OOM");  /* LCOV_EXCL_LINE */
            warn_(ctx, agent, text);
            talloc_free(text);
            continue;
        }
        count++;
    }
    return count;
}

/* Add advertise entries to catalog and JSON output; returns entry count. */
static size_t add_catalog_entries_(ik_agent_ctx_t *agent,
                                   yyjson_val *advertise_arr,
                                   yyjson_mut_doc *out_doc,
                                   yyjson_mut_val *out_entries)
{
    size_t count = 0;
    if (!yyjson_is_arr(advertise_arr)) return 0;

    size_t idx = 0, max_idx = 0;
    yyjson_val *entry = NULL;
    yyjson_arr_foreach(advertise_arr, idx, max_idx, entry) {  /* LCOV_EXCL_BR_LINE */
        if (!yyjson_is_obj(entry)) continue;  /* LCOV_EXCL_BR_LINE */
        const char *sn = yyjson_get_str(yyjson_obj_get(entry, "skill"));  /* LCOV_EXCL_BR_LINE */
        const char *desc = yyjson_get_str(yyjson_obj_get(entry, "description"));  /* LCOV_EXCL_BR_LINE */
        if (sn == NULL) continue;  /* LCOV_EXCL_BR_LINE */
        ik_skillset_store_catalog_entry(agent, sn, desc);

        yyjson_mut_val *e_obj = yyjson_mut_obj(out_doc);  /* LCOV_EXCL_BR_LINE */
        yyjson_mut_obj_add_str(out_doc, e_obj, "skill", sn);
        yyjson_mut_obj_add_str(out_doc, e_obj, "description", desc ? desc : "");  /* LCOV_EXCL_BR_LINE */
        yyjson_mut_arr_append(out_entries, e_obj);
        count++;
    }
    return count;
}

/* Persist a skillset DB event. */
static void persist_skillset_event_(void *ctx, ik_repl_ctx_t *repl,
                                    ik_agent_ctx_t *agent,
                                    yyjson_mut_doc *out_doc)
{
    if (repl->shared->db_ctx == NULL || repl->shared->session_id <= 0) return;  /* LCOV_EXCL_BR_LINE */

    size_t json_len = 0;
    char *json_raw = yyjson_mut_write(out_doc, 0, &json_len);
    if (json_raw == NULL) return;  /* LCOV_EXCL_LINE */

    char *data_json = talloc_strndup(ctx, json_raw, json_len);
    free(json_raw);
    if (!data_json) PANIC("OOM");  /* LCOV_EXCL_LINE */

    res_t db_res = ik_db_message_insert_(repl->shared->db_ctx,
                                        repl->shared->session_id,
                                        agent->uuid,
                                        "skillset",
                                        NULL,
                                        data_json);
    talloc_free(data_json);
    if (is_err(&db_res)) talloc_free(db_res.err);
}

res_t ik_cmd_skillset(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */

    if (args == NULL) {
        warn_(ctx, repl->current, "Usage: /skillset <name>");
        return OK(NULL);
    }

    ik_agent_ctx_t *agent = repl->current;

    yyjson_doc *doc = load_skillset_json_(ctx, agent, args);
    if (doc == NULL) return OK(NULL);

    yyjson_val *root = yyjson_doc_get_root(doc);  /* LCOV_EXCL_BR_LINE */
    yyjson_val *preload_arr = yyjson_obj_get(root, "preload");  /* LCOV_EXCL_BR_LINE */
    yyjson_val *advertise_arr = yyjson_obj_get(root, "advertise");  /* LCOV_EXCL_BR_LINE */

    size_t preload_count = preload_skills_(ctx, repl, agent, preload_arr);

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    if (!out_doc) PANIC("OOM");  /* LCOV_EXCL_LINE */
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);  /* LCOV_EXCL_BR_LINE */
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_str(out_doc, out_root, "skillset", args);
    yyjson_mut_val *out_entries = yyjson_mut_arr(out_doc);  /* LCOV_EXCL_BR_LINE */
    yyjson_mut_obj_add_val(out_doc, out_root, "catalog_entries", out_entries);  /* LCOV_EXCL_BR_LINE */

    size_t advertise_count = add_catalog_entries_(agent, advertise_arr, out_doc, out_entries);

    yyjson_doc_free(doc);

    persist_skillset_event_(ctx, repl, agent, out_doc);
    yyjson_mut_doc_free(out_doc);

    if (agent->token_cache != NULL) {
        ik_token_cache_invalidate_system(agent->token_cache);
    }

    char *confirm = talloc_asprintf(ctx,
                                    "Skillset loaded: %s (%zu skills preloaded, %zu catalog entries)",
                                    args, preload_count, advertise_count);
    if (!confirm) PANIC("OOM");  /* LCOV_EXCL_LINE */
    ik_scrollback_append_line(agent->scrollback, confirm, strlen(confirm));
    talloc_free(confirm);

    return OK(NULL);
}
