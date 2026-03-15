/**
 * @file agent_system_prompt.c
 * @brief Agent system prompt construction: effective prompt and system blocks
 *
 * Contains logic for building the agent's system prompt content, including
 * template processing for pinned documents and multi-block system prompt
 * construction for provider requests.
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/config_defaults.h"
#include "apps/ikigai/debug_log.h"
#include "apps/ikigai/doc_cache.h"
#include "apps/ikigai/file_utils.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/scrollback_utils.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/template.h"
#include "shared/panic.h"
#include "shared/wrapper.h"
#include "shared/wrapper_posix.h"

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

/* ================================================================
 * Internal Helpers
 * ================================================================ */

static void display_template_warnings(ik_agent_ctx_t *agent, ik_template_result_t *template_result)
{
    if (template_result->unresolved_count == 0 || agent->scrollback == NULL) {
        return;
    }

    for (size_t j = 0; j < template_result->unresolved_count; j++) {
        char *warning_text = talloc_asprintf(agent, "Unknown template variable: %s",
                                             template_result->unresolved[j]);
        if (warning_text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        char *formatted_warning = ik_scrollback_format_warning(agent, warning_text);
        ik_scrollback_append_line(agent->scrollback, formatted_warning, strlen(formatted_warning));

        talloc_free(warning_text);
        talloc_free(formatted_warning);
    }
}

static char *process_pinned_content(ik_agent_ctx_t *agent, const char *content)
{
    ik_config_t *config = (agent->shared != NULL) ? agent->shared->cfg : NULL;
    ik_template_result_t *template_result = NULL;
    res_t template_res = ik_template_process_(agent, content, agent, config, (void **)&template_result);

    const char *processed_content = content;
    if (is_ok(&template_res) && template_result != NULL) {
        processed_content = template_result->processed;
        display_template_warnings(agent, template_result);
    }

    char *result = talloc_strdup(agent, processed_content);
    if (template_result != NULL) {
        talloc_free(template_result);
    }

    return result;
}

/* ================================================================
 * Public API
 * ================================================================ */

/* Append loaded skill content to a base prompt string for token counting. */
static char *append_loaded_skills_(ik_agent_ctx_t *agent, char *base)
{
    char *result = base;
    for (size_t i = 0; i < agent->loaded_skill_count; i++) {
        if (agent->loaded_skills[i]->content != NULL) {
            char *extended = talloc_asprintf(agent, "%s%s", result,
                                             agent->loaded_skills[i]->content);
            if (extended == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            talloc_free(result);
            result = extended;
        }
    }
    return result;
}

res_t ik_agent_get_effective_system_prompt(ik_agent_ctx_t *agent, char **out)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);    // LCOV_EXCL_BR_LINE

    *out = NULL;

    // Priority 1: Pinned files (if any)
    if (agent->pinned_count > 0 && agent->doc_cache != NULL) {
        char *assembled = talloc_strdup(agent, "");
        if (assembled == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        for (size_t i = 0; i < agent->pinned_count; i++) {
            const char *path = agent->pinned_paths[i];
            char *content = NULL;
            res_t doc_res = ik_doc_cache_get(agent->doc_cache, path, &content);

            if (is_ok(&doc_res) && content != NULL) {
                char *processed_content = process_pinned_content(agent, content);
                char *new_assembled = talloc_asprintf(agent, "%s%s", assembled, processed_content);
                if (new_assembled == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                talloc_free(assembled);
                assembled = new_assembled;
                talloc_free(processed_content);
            }
        }

        if (strlen(assembled) > 0) {
            *out = append_loaded_skills_(agent, assembled);
            return OK(*out);
        }
        talloc_free(assembled);
    }

    // Priority 2: $IKIGAI_DATA_DIR/system/prompt.md
    if (agent->shared != NULL && agent->shared->paths != NULL) {
        const char *data_dir = ik_paths_get_data_dir(agent->shared->paths);
        char *prompt_path = talloc_asprintf(agent, "%s/system/prompt.md", data_dir);
        if (prompt_path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        char *content = NULL;
        res_t read_res = ik_file_read_all(agent, prompt_path, &content, NULL);
        talloc_free(prompt_path);

        if (is_ok(&read_res) && content != NULL && strlen(content) > 0) {
            char *base = process_pinned_content(agent, content);
            talloc_free(content);
            *out = append_loaded_skills_(agent, base);
            return OK(*out);
        }
        if (content != NULL) {
            talloc_free(content);
        }
    }

    // Priority 3: Config fallback
    if (agent->shared != NULL && agent->shared->cfg != NULL &&
        agent->shared->cfg->openai_system_message != NULL) {
        char *base = process_pinned_content(agent, agent->shared->cfg->openai_system_message);
        *out = append_loaded_skills_(agent, base);
        return OK(*out);
    }

    // Priority 4: Hardcoded default
    char *base = talloc_strdup(agent, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);
    if (base == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    *out = append_loaded_skills_(agent, base);
    return OK(*out);
}

static char *resolve_base_prompt_(ik_agent_ctx_t *agent)
{
    // Priority 1: $IKIGAI_DATA_DIR/system/prompt.md
    if (agent->shared != NULL && agent->shared->paths != NULL) {
        const char *data_dir = ik_paths_get_data_dir(agent->shared->paths);
        char *prompt_path = talloc_asprintf(agent, "%s/system/prompt.md", data_dir);
        if (prompt_path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        char *content = NULL;
        res_t read_res = ik_file_read_all(agent, prompt_path, &content, NULL);
        talloc_free(prompt_path);

        if (is_ok(&read_res) && content != NULL && strlen(content) > 0) {
            char *result = process_pinned_content(agent, content);
            talloc_free(content);
            return result;
        }
        if (content != NULL) talloc_free(content);
    }

    // Priority 2: Config fallback
    if (agent->shared != NULL && agent->shared->cfg != NULL &&
        agent->shared->cfg->openai_system_message != NULL &&
        strlen(agent->shared->cfg->openai_system_message) > 0) {
        return process_pinned_content(agent, agent->shared->cfg->openai_system_message);
    }

    // Priority 3: Hardcoded default
    char *result = talloc_strdup(agent, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return result;
}

static void load_agents_md_(ik_agent_ctx_t *agent)
{
    if (agent->agents_md_loaded) return;
    agent->agents_md_loaded = true;

    /* Skip for minimal test agents created without shared context */
    if (agent->shared == NULL) return;

    char cwd[PATH_MAX];
    if (posix_getcwd_(cwd, sizeof(cwd)) == NULL) return;

    char *path = talloc_asprintf(agent, "%s/AGENTS.md", cwd);
    if (path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *content = NULL;
    res_t read_res = ik_file_read_all(agent, path, &content, NULL);
    talloc_free(path);

    if (is_ok(&read_res) && content != NULL && strlen(content) > 0) {
        agent->agents_md_content = process_pinned_content(agent, content);
        talloc_free(content);
    } else {
        if (content != NULL) talloc_free(content);
        if (is_err(&read_res)) talloc_free(read_res.err);
    }
}

static res_t add_pinned_doc_blocks_(ik_request_t *req, ik_agent_ctx_t *agent)
{
    if (agent->pinned_count == 0 || agent->doc_cache == NULL) return OK(NULL);

    for (size_t i = 0; i < agent->pinned_count; i++) {
        const char *path = agent->pinned_paths[i];
        char *content = NULL;
        res_t doc_res = ik_doc_cache_get(agent->doc_cache, path, &content);

        if (is_ok(&doc_res) && content != NULL) {
            char *processed = process_pinned_content(agent, content);
            res_t res = ik_request_add_system_block(req, processed, true, IK_SYSTEM_BLOCK_PINNED_DOC);
            talloc_free(processed);
            if (is_err(&res)) return res;  // LCOV_EXCL_BR_LINE
        }
    }
    return OK(NULL);
}

static res_t add_skill_catalog_block_(ik_request_t *req, ik_agent_ctx_t *agent)
{
    if (agent->skillset_catalog_count == 0) return OK(NULL);

    char *catalog = talloc_strdup(agent,
                                  "## Available Skills\n"
                                  "Load these with /load <name> when relevant to the current task.\n");
    if (catalog == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    for (size_t i = 0; i < agent->skillset_catalog_count; i++) {
        ik_skillset_catalog_entry_t *entry = agent->skillset_catalog[i];
        char *line = talloc_asprintf(agent, "- %s: %s\n", entry->skill_name, entry->description);
        if (line == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        char *extended = talloc_asprintf(agent, "%s%s", catalog, line);
        if (extended == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        talloc_free(catalog);
        talloc_free(line);
        catalog = extended;
    }
    res_t res = ik_request_add_system_block(req, catalog, true, IK_SYSTEM_BLOCK_SKILL_CATALOG);
    talloc_free(catalog);
    return res;
}

res_t ik_agent_build_system_blocks(ik_request_t *req, ik_agent_ctx_t *agent)
{
    assert(req != NULL);   // LCOV_EXCL_BR_LINE
    assert(agent != NULL); // LCOV_EXCL_BR_LINE

    // Block 0: Base system prompt (prompt.md / config / default), not cacheable.
    char *base_prompt = resolve_base_prompt_(agent);
    res_t res = ik_request_add_system_block(req, base_prompt, false, IK_SYSTEM_BLOCK_BASE_PROMPT);
    talloc_free(base_prompt);
    if (is_err(&res)) return res;  // LCOV_EXCL_BR_LINE

    // Blocks 1..N: Each pinned document as a separate cacheable block.
    res = add_pinned_doc_blocks_(req, agent);
    if (is_err(&res)) return res;  // LCOV_EXCL_BR_LINE

    // Loaded skills: each as a separate cacheable block, after pinned docs
    for (size_t i = 0; i < agent->loaded_skill_count; i++) {
        if (agent->loaded_skills[i]->content != NULL) {
            res = ik_request_add_system_block(req, agent->loaded_skills[i]->content, true,
                                              IK_SYSTEM_BLOCK_SKILL);
            if (is_err(&res)) return res;  // LCOV_EXCL_BR_LINE
        }
    }

    // Skill catalog block: list of available-but-not-loaded skills (cacheable)
    res = add_skill_catalog_block_(req, agent);
    if (is_err(&res)) return res;  // LCOV_EXCL_BR_LINE

    // AGENTS.md block: loaded from CWD, cached until /clear (cacheable)
    load_agents_md_(agent);
    if (agent->agents_md_content != NULL) {
        res = ik_request_add_system_block(req, agent->agents_md_content, true,
                                          IK_SYSTEM_BLOCK_AGENTS_MD);
        if (is_err(&res)) return res;  // LCOV_EXCL_BR_LINE
    }

    // Blocks N+1..M: Previous-session summaries (oldest first, cacheable)
    for (size_t i = 0; i < agent->session_summary_count; i++) {
        if (agent->session_summaries[i]->summary != NULL) {
            res = ik_request_add_system_block(req, agent->session_summaries[i]->summary, true,
                                              IK_SYSTEM_BLOCK_SESSION_SUMMARY);
            if (is_err(&res)) return res;  // LCOV_EXCL_BR_LINE
        }
    }

    // Final block: Recent summary (if present, not cacheable)
    if (agent->recent_summary != NULL) {
        res = ik_request_add_system_block(req, agent->recent_summary, false,
                                          IK_SYSTEM_BLOCK_RECENT_SUMMARY);
        if (is_err(&res)) return res;  // LCOV_EXCL_BR_LINE
    }

    size_t cnt_base = 0, cnt_pinned = 0, cnt_skills = 0;
    size_t cnt_catalog = 0, cnt_agents_md = 0, cnt_summaries = 0, cnt_recent = 0;
    for (size_t i = 0; i < req->system_block_count; i++) {
        switch (req->system_blocks[i].type) {
            case IK_SYSTEM_BLOCK_BASE_PROMPT:    cnt_base++;      break;
            case IK_SYSTEM_BLOCK_PINNED_DOC:     cnt_pinned++;    break;
            case IK_SYSTEM_BLOCK_SKILL:          cnt_skills++;    break;
            case IK_SYSTEM_BLOCK_SKILL_CATALOG:  cnt_catalog++;   break;
            case IK_SYSTEM_BLOCK_AGENTS_MD:      cnt_agents_md++; break;
            case IK_SYSTEM_BLOCK_SESSION_SUMMARY: cnt_summaries++; break;
            case IK_SYSTEM_BLOCK_RECENT_SUMMARY: cnt_recent++;    break;
        }
    }
    DEBUG_LOG("[system_blocks] built %zu blocks: base=%zu pinned=%zu skills=%zu catalog=%zu agents_md=%zu summaries=%zu recent=%zu",
              req->system_block_count,
              cnt_base, cnt_pinned, cnt_skills, cnt_catalog, cnt_agents_md, cnt_summaries, cnt_recent);

    return OK(NULL);
}
