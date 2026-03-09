/**
 * @file commands_skill.c
 * @brief Skill management slash command handlers (/load, /unload, /skills)
 */

#include "apps/ikigai/commands_skill.h"

#include "apps/ikigai/db/message.h"
#include "apps/ikigai/doc_cache.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/scrollback_utils.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/template.h"
#include "apps/ikigai/token_cache.h"
#include "shared/error.h"
#include "shared/panic.h"
#include "shared/wrapper_internal.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

/* Match ${N} numeric positional var; returns NULL if non-numeric or out of range. */
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

/* Replace ${N} positional placeholders; unreferenced args ignored, unresolved remain literal. */
static char *apply_positional_args_(TALLOC_CTX *ctx, const char *content,
                                    char **pos_args, size_t pos_arg_count)
{
    if (pos_arg_count == 0) {
        char *result = talloc_strdup(ctx, content);
        if (!result) PANIC("OOM");  /* LCOV_EXCL_LINE */
        return result;
    }

    char *result = talloc_strdup(ctx, "");
    if (!result) PANIC("OOM");  /* LCOV_EXCL_LINE */

    const char *start = content;
    const char *p = content;

    while (*p != '\0') {
        if (p[0] == '$' && p[1] == '{') {
            const char *end = strchr(p + 2, '}');
            if (end != NULL) {
                const char *arg_val = match_positional_var_(p, end, pos_args, pos_arg_count);
                if (arg_val != NULL) {
                    char *tmp = talloc_asprintf(ctx, "%s%.*s%s",
                                                result, (int)(p - start), start, arg_val);
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
    /* Append remaining text */
    if (p > start) {
        char *tmp = talloc_asprintf(ctx, "%s%s", result, start);
        if (!tmp) PANIC("OOM");  /* LCOV_EXCL_LINE */
        talloc_free(result);
        result = tmp;
    }
    return result;
}

/* Parse whitespace-separated args into array, null-terminating each token. */
static char **parse_pos_args_(TALLOC_CTX *ctx, const char *rest, size_t *out_count)
{
    *out_count = 0;
    if (rest == NULL || *rest == '\0') return NULL;

    size_t count = 0;
    const char *scan = rest;
    while (*scan != '\0') {
        while (*scan && !isspace((unsigned char)*scan)) scan++;
        count++;
        while (*scan && isspace((unsigned char)*scan)) scan++;
    }
    if (count == 0) return NULL;  /* LCOV_EXCL_LINE */

    char *args_copy = talloc_strdup(ctx, rest);
    if (!args_copy) PANIC("OOM");  /* LCOV_EXCL_LINE */
    char **pos_args = talloc_zero_array(ctx, char *, (unsigned int)count);
    if (!pos_args) PANIC("OOM");  /* LCOV_EXCL_LINE */

    size_t idx = 0;
    char *tok = args_copy;
    while (*tok != '\0' && idx < count) {  /* LCOV_EXCL_BR_LINE */
        while (*tok && isspace((unsigned char)*tok)) tok++;  /* LCOV_EXCL_BR_LINE */
        if (*tok == '\0') break;  /* LCOV_EXCL_BR_LINE */
        pos_args[idx] = tok;
        while (*tok && !isspace((unsigned char)*tok)) tok++;
        if (*tok != '\0') { *tok = '\0'; tok++; }
        idx++;
    }
    *out_count = idx;
    return pos_args;
}

/* Add or replace a skill entry in agent->loaded_skills[]. */
void ik_skill_store_loaded(ik_agent_ctx_t *agent, const char *skill_name,
                           const char *content)
{
    for (size_t i = 0; i < agent->loaded_skill_count; i++) {
        if (strcmp(agent->loaded_skills[i]->name, skill_name) == 0) {
            talloc_free(agent->loaded_skills[i]);
            ik_loaded_skill_t *skill = talloc_zero(agent, ik_loaded_skill_t);
            if (!skill) PANIC("OOM");  /* LCOV_EXCL_LINE */
            skill->name = talloc_strdup(skill, skill_name);
            skill->content = talloc_strdup(skill, content);
            skill->load_position = agent->message_count;
            agent->loaded_skills[i] = skill;
            return;
        }
    }

    ik_loaded_skill_t **new_skills = talloc_realloc(agent, agent->loaded_skills,
                                                    ik_loaded_skill_t *,
                                                    (unsigned int)(agent->loaded_skill_count + 1));
    if (!new_skills) PANIC("OOM");  /* LCOV_EXCL_LINE */
    agent->loaded_skills = new_skills;

    ik_loaded_skill_t *skill = talloc_zero(agent, ik_loaded_skill_t);
    if (!skill) PANIC("OOM");  /* LCOV_EXCL_LINE */
    skill->name = talloc_strdup(skill, skill_name);
    skill->content = talloc_strdup(skill, content);
    skill->load_position = agent->message_count;
    agent->loaded_skills[agent->loaded_skill_count] = skill;
    agent->loaded_skill_count++;
}

/* Build and persist a skill_load DB event (uses yyjson for correct escaping). */
static void persist_skill_load_event_(TALLOC_CTX *ctx, ik_repl_ctx_t *repl,
                                      ik_agent_ctx_t *agent,
                                      const char *skill_name,
                                      char **pos_args, size_t pos_arg_count,
                                      const char *resolved_content)
{
    if (repl->shared->db_ctx == NULL || repl->shared->session_id <= 0) return;

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) PANIC("OOM");  /* LCOV_EXCL_LINE */
    yyjson_mut_val *root = yyjson_mut_obj(doc);  /* LCOV_EXCL_BR_LINE */
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "skill", skill_name);

    yyjson_mut_val *args_arr = yyjson_mut_arr(doc);
    for (size_t i = 0; i < pos_arg_count; i++) {
        yyjson_mut_val *s = yyjson_mut_str(doc, pos_args[i]);
        yyjson_mut_arr_append(args_arr, s);
    }
    yyjson_mut_obj_add_val(doc, root, "args", args_arr);
    yyjson_mut_obj_add_str(doc, root, "content", resolved_content);

    size_t json_len = 0;
    char *json_raw = yyjson_mut_write(doc, 0, &json_len);
    yyjson_mut_doc_free(doc);

    if (!json_raw) return;  /* LCOV_EXCL_LINE */
    char *data_json = talloc_strndup(ctx, json_raw, json_len);
    free(json_raw);
    if (!data_json) PANIC("OOM");  /* LCOV_EXCL_LINE */

    res_t db_res = ik_db_message_insert_(repl->shared->db_ctx,
                                         repl->shared->session_id,
                                         agent->uuid,
                                         "skill_load",
                                         NULL,
                                         data_json);
    talloc_free(data_json);
    if (is_err(&db_res)) talloc_free(db_res.err);
}

res_t ik_cmd_load(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */

    if (args == NULL) {
        const char *usage = "Usage: /load <skill-name> [args...]";
        char *warn = ik_scrollback_format_warning(ctx, usage);
        ik_scrollback_append_line(repl->current->scrollback, warn, strlen(warn));
        talloc_free(warn);
        return OK(NULL);
    }

    ik_agent_ctx_t *agent = repl->current;

    const char *p = args;
    while (*p && !isspace((unsigned char)*p)) p++;
    char *skill_name = talloc_strndup(ctx, args, (size_t)(p - args));
    if (!skill_name) PANIC("OOM");  /* LCOV_EXCL_LINE */
    while (isspace((unsigned char)*p)) p++;

    size_t pos_arg_count = 0;
    char **pos_args = parse_pos_args_(ctx, p, &pos_arg_count);

    /* Read skill file via doc cache */
    char *uri = talloc_asprintf(ctx, "ik://skills/%s/SKILL.md", skill_name);
    if (!uri) PANIC("OOM");  /* LCOV_EXCL_LINE */

    if (agent->doc_cache == NULL) {
        char *text = talloc_asprintf(ctx, "Skill not found: %s", skill_name);
        if (!text) PANIC("OOM");  /* LCOV_EXCL_LINE */
        char *warn = ik_scrollback_format_warning(ctx, text);
        talloc_free(text);
        ik_scrollback_append_line(agent->scrollback, warn, strlen(warn));
        talloc_free(warn);
        return OK(NULL);
    }

    char *file_content = NULL;
    res_t read_res = ik_doc_cache_get_(agent->doc_cache, uri, &file_content);
    if (is_err(&read_res) || file_content == NULL) {
        if (is_err(&read_res)) talloc_free(read_res.err);  /* LCOV_EXCL_BR_LINE */
        char *text = talloc_asprintf(ctx, "Skill not found: %s", skill_name);
        if (!text) PANIC("OOM");  /* LCOV_EXCL_LINE */
        char *warn = ik_scrollback_format_warning(ctx, text);
        talloc_free(text);
        ik_scrollback_append_line(agent->scrollback, warn, strlen(warn));
        talloc_free(warn);
        return OK(NULL);
    }

    /* Apply positional substitution then standard template processing */
    char *substituted = apply_positional_args_(ctx, file_content, pos_args, pos_arg_count);
    ik_config_t *config = (agent->shared != NULL) ? agent->shared->cfg : NULL;  /* LCOV_EXCL_BR_LINE */
    ik_template_result_t *template_result = NULL;
    res_t template_res = ik_template_process_(ctx, substituted, agent, config, (void **)&template_result);
    const char *resolved_content = substituted;
    if (is_ok(&template_res) && template_result != NULL) {  /* LCOV_EXCL_BR_LINE */
        resolved_content = template_result->processed;
    }

    ik_skill_store_loaded(agent, skill_name, resolved_content);
    persist_skill_load_event_(ctx, repl, agent, skill_name, pos_args, pos_arg_count,
                              resolved_content);

    if (template_result != NULL) talloc_free(template_result);
    talloc_free(substituted);

    if (agent->token_cache != NULL) {
        ik_token_cache_invalidate_system(agent->token_cache);
    }

    char *confirm = talloc_asprintf(ctx, "Skill loaded: %s", skill_name);
    if (!confirm) PANIC("OOM");  /* LCOV_EXCL_LINE */
    ik_scrollback_append_line(agent->scrollback, confirm, strlen(confirm));
    talloc_free(confirm);

    return OK(NULL);
}

res_t ik_cmd_skills(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */
    (void)ctx;
    (void)args;

    ik_agent_ctx_t *agent = repl->current;
    if (agent->loaded_skill_count == 0) {
        const char *msg = "No skills loaded.";
        ik_scrollback_append_line(agent->scrollback, msg, strlen(msg));
        return OK(NULL);
    }
    for (size_t i = 0; i < agent->loaded_skill_count; i++) {
        ik_loaded_skill_t *s = agent->loaded_skills[i];
        size_t n = strlen(s->content);
        char *line = (n >= 1024)
            ? talloc_asprintf(ctx, "%s (%zu KB)", s->name, n / 1024)
            : talloc_asprintf(ctx, "%s (%zu B)", s->name, n);
        if (!line) PANIC("OOM");  /* LCOV_EXCL_LINE */
        ik_scrollback_append_line(agent->scrollback, line, strlen(line));
        talloc_free(line);
    }
    return OK(NULL);
}

res_t ik_cmd_unload(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */

    if (args == NULL) {
        const char *usage = "Usage: /unload <skill-name>";
        char *warn = ik_scrollback_format_warning(ctx, usage);
        ik_scrollback_append_line(repl->current->scrollback, warn, strlen(warn));
        talloc_free(warn);
        return OK(NULL);
    }

    ik_agent_ctx_t *agent = repl->current;

    size_t found_idx = agent->loaded_skill_count; /* sentinel: not found */
    for (size_t i = 0; i < agent->loaded_skill_count; i++) {
        if (strcmp(agent->loaded_skills[i]->name, args) == 0) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == agent->loaded_skill_count) {
        char *text = talloc_asprintf(ctx, "Skill not loaded: %s", args);
        if (!text) PANIC("OOM");  /* LCOV_EXCL_LINE */
        char *warn = ik_scrollback_format_warning(ctx, text);
        talloc_free(text);
        ik_scrollback_append_line(repl->current->scrollback, warn, strlen(warn));
        talloc_free(warn);
        return OK(NULL);
    }

    talloc_free(agent->loaded_skills[found_idx]);
    for (size_t i = found_idx; i + 1 < agent->loaded_skill_count; i++) {
        agent->loaded_skills[i] = agent->loaded_skills[i + 1];
    }
    agent->loaded_skill_count--;

    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {
        char *data_json = talloc_asprintf(ctx, "{\"skill\":\"%s\"}", args);
        if (!data_json) PANIC("OOM");  /* LCOV_EXCL_LINE */
        res_t db_res = ik_db_message_insert_(repl->shared->db_ctx,
                                             repl->shared->session_id,
                                             agent->uuid,
                                             "skill_unload",
                                             NULL,
                                             data_json);
        talloc_free(data_json);
        if (is_err(&db_res)) {
            talloc_free(db_res.err);
        }
    }

    if (agent->token_cache != NULL) {
        ik_token_cache_invalidate_system(agent->token_cache);
    }

    char *confirm = talloc_asprintf(ctx, "Skill unloaded: %s", args);
    if (!confirm) PANIC("OOM");  /* LCOV_EXCL_LINE */
    ik_scrollback_append_line(repl->current->scrollback, confirm, strlen(confirm));
    talloc_free(confirm);

    return OK(NULL);
}

/* Load a single named skill without positional args (used by /skillset). */
bool ik_skill_load_by_name(void *ctx, ik_repl_ctx_t *repl, ik_agent_ctx_t *agent,
                           const char *skill_name)
{
    char *uri = talloc_asprintf(ctx, "ik://skills/%s/SKILL.md", skill_name);
    if (!uri) PANIC("OOM");  /* LCOV_EXCL_LINE */

    char *file_content = NULL;
    res_t read_res = ik_doc_cache_get_(agent->doc_cache, uri, &file_content);
    if (is_err(&read_res) || file_content == NULL) {
        if (is_err(&read_res)) talloc_free(read_res.err);  /* LCOV_EXCL_BR_LINE */
        talloc_free(uri);
        return false;
    }
    talloc_free(uri);

    ik_config_t *config = (agent->shared != NULL) ? agent->shared->cfg : NULL;  /* LCOV_EXCL_BR_LINE */
    ik_template_result_t *template_result = NULL;
    res_t template_res = ik_template_process_(ctx, file_content, agent, config, (void **)&template_result);
    const char *resolved_content = file_content;
    if (is_ok(&template_res) && template_result != NULL) {  /* LCOV_EXCL_BR_LINE */
        resolved_content = template_result->processed;
    }

    ik_skill_store_loaded(agent, skill_name, resolved_content);
    persist_skill_load_event_(ctx, repl, agent, skill_name, NULL, 0, resolved_content);

    if (template_result != NULL) talloc_free(template_result);

    return true;
}
