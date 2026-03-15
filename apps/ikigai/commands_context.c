/**
 * @file commands_context.c
 * @brief /context command handler — renders full LLM context layout
 */

#include "apps/ikigai/commands_context.h"
#include "apps/ikigai/commands_context_box.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/provider_types.h"
#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/token_count.h"
#include "apps/ikigai/tool_registry.h"
#include "shared/error.h"
#include "shared/panic.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

/* ================================================================
 * Token collection helpers
 * ================================================================ */

typedef struct {
    int32_t sys, pinned, skill, catalog, sess, recent;
} ctx_block_toks_t;

static ctx_block_toks_t collect_block_toks(ik_request_t *req)
{
    ctx_block_toks_t t = {0, 0, 0, 0, 0, 0};
    if (req == NULL) return t;
    for (size_t i = 0; i < req->system_block_count; i++) {
        ik_system_block_t *blk = &req->system_blocks[i];
        int32_t tok = ik_token_count_from_bytes(blk->text ? strlen(blk->text) : 0);
        switch (blk->type) {
            case IK_SYSTEM_BLOCK_BASE_PROMPT:     t.sys     += tok; break;
            case IK_SYSTEM_BLOCK_PINNED_DOC:      t.pinned  += tok; break;
            case IK_SYSTEM_BLOCK_SKILL:           t.skill   += tok; break;
            case IK_SYSTEM_BLOCK_SKILL_CATALOG:   t.catalog += tok; break;
            case IK_SYSTEM_BLOCK_SESSION_SUMMARY: t.sess    += tok; break;
            case IK_SYSTEM_BLOCK_RECENT_SUMMARY:  t.recent  += tok; break;
            case IK_SYSTEM_BLOCK_AGENTS_MD:                         break;
        }
    }
    return t;
}

static int32_t collect_tool_toks(ik_tool_registry_t *reg)
{
    if (reg == NULL) return 0;
    int32_t total = 0;
    for (size_t i = 0; i < reg->count; i++) {
        char *s = yyjson_val_write(reg->entries[i].schema_root, 0, NULL);
        if (s) { total += ik_token_count_from_bytes(strlen(s)); free(s); }
        total += ik_token_count_from_bytes(strlen(reg->entries[i].name));
    }
    return total;
}

typedef struct {
    int32_t tokens;
    size_t  user_count, asst_count, tool_pairs;
} ctx_msg_stats_t;

static ctx_msg_stats_t collect_msg_stats(ik_agent_ctx_t *agent)
{
    ctx_msg_stats_t s = {0, 0, 0, 0};
    for (size_t i = 0; i < agent->message_count; i++) {
        ik_message_t *msg = agent->messages[i];
        if (msg->role == IK_ROLE_USER) s.user_count++;
        else if (msg->role == IK_ROLE_ASSISTANT) s.asst_count++;
        for (size_t j = 0; j < msg->content_count; j++) {
            ik_content_block_t *cb = &msg->content_blocks[j];
            const char *text = NULL;
            if (cb->type == IK_CONTENT_TEXT)
                text = cb->data.text.text;
            else if (cb->type == IK_CONTENT_TOOL_CALL)
                text = cb->data.tool_call.arguments;
            else if (cb->type == IK_CONTENT_TOOL_RESULT)
                text = cb->data.tool_result.content;
            if (text) s.tokens += ik_token_count_from_bytes(strlen(text));
            if (cb->type == IK_CONTENT_TOOL_CALL) s.tool_pairs++;
        }
    }
    return s;
}

/* ================================================================
 * Group renderers
 * ================================================================ */

static void render_sys_prompt(ctx_rend_t *r, int32_t sys_tok)
{
    ctx_render_outer_blank(r);
    if (sys_tok == 0) {
        ctx_render_group_header(r, "System Prompt", "(empty)");
    } else {
        char *rl = ctx_make_tok_label(r->ctx, sys_tok);
        ctx_render_group_header(r, "System Prompt", rl); talloc_free(rl);
        char *ir = ctx_make_tok_label(r->ctx, sys_tok);
        ctx_render_group_row(r, "System prompt", ir); talloc_free(ir);
    }
    ctx_render_group_footer(r);
}

static void render_pinned_docs(ctx_rend_t *r, ik_agent_ctx_t *agent,
                               ik_request_t *req, int32_t pinned_tok)
{
    ctx_render_outer_blank(r);
    if (agent->pinned_count == 0) {
        ctx_render_group_header(r, "Pinned Documents", "(empty)");
    } else {
        char *rl = ctx_make_tok_label(r->ctx, pinned_tok);
        ctx_render_group_header(r, "Pinned Documents", rl); talloc_free(rl);
        size_t pidx = 0;
        for (size_t i = 0; req && i < req->system_block_count &&
                           pidx < agent->pinned_count; i++) {
            if (req->system_blocks[i].type != IK_SYSTEM_BLOCK_PINNED_DOC) continue;
            size_t blen = req->system_blocks[i].text
                          ? strlen(req->system_blocks[i].text) : 0;
            int32_t t = ik_token_count_from_bytes(blen);
            char *ir = ctx_make_tok_label(r->ctx, t);
            ctx_render_group_row(r, agent->pinned_paths[pidx++], ir);
            talloc_free(ir);
        }
    }
    ctx_render_group_footer(r);
}

static void render_skills(ctx_rend_t *r, ik_agent_ctx_t *agent, int32_t skill_tok)
{
    ctx_render_outer_blank(r);
    if (agent->loaded_skill_count == 0) {
        ctx_render_group_header(r, "Skills", "(empty)");
    } else {
        char *rl = ctx_make_tok_label(r->ctx, skill_tok);
        ctx_render_group_header(r, "Skills", rl); talloc_free(rl);
        for (size_t i = 0; i < agent->loaded_skill_count; i++) {
            ik_loaded_skill_t *sk = agent->loaded_skills[i];
            int32_t t = ik_token_count_from_bytes(sk->content ? strlen(sk->content) : 0);
            char *ir = ctx_make_tok_label(r->ctx, t);
            ctx_render_group_row(r, sk->name ? sk->name : "(unnamed)", ir);
            talloc_free(ir);
        }
    }
    ctx_render_group_footer(r);
}

static void render_skill_catalog(ctx_rend_t *r, ik_agent_ctx_t *agent, int32_t catalog_tok)
{
    ctx_render_outer_blank(r);
    if (agent->skillset_catalog_count == 0) {
        ctx_render_group_header(r, "Skill Catalog", "(empty)");
    } else {
        char *rl = ctx_make_tok_label(r->ctx, catalog_tok);
        ctx_render_group_header(r, "Skill Catalog", rl); talloc_free(rl);
        char *names = talloc_strdup(r->ctx, "");
        if (!names) PANIC("OOM");  /* LCOV_EXCL_LINE */
        for (size_t i = 0; i < agent->skillset_catalog_count; i++) {
            const char *sn = agent->skillset_catalog[i]->skill_name;
            char *tmp = (i == 0) ? talloc_asprintf(r->ctx, "%s", sn)
                                 : talloc_asprintf(r->ctx, "%s, %s", names, sn);
            if (!tmp) PANIC("OOM");  /* LCOV_EXCL_LINE */
            talloc_free(names);
            names = tmp;
        }
        char ebuf[32];
        snprintf(ebuf, sizeof(ebuf), "%zu entries", agent->skillset_catalog_count);
        ctx_render_group_row(r, names, ebuf);
        talloc_free(names);
    }
    ctx_render_group_footer(r);
}

static void render_session_summaries(ctx_rend_t *r, ik_agent_ctx_t *agent, int32_t sess_tok)
{
    ctx_render_outer_blank(r);
    if (agent->session_summary_count == 0) {
        ctx_render_group_header(r, "Session Summaries", "(empty)");
    } else {
        char *rl = ctx_make_tok_label(r->ctx, sess_tok);
        ctx_render_group_header(r, "Session Summaries", rl); talloc_free(rl);
        for (size_t i = 0; i < agent->session_summary_count; i++) {
            char label[64];
            snprintf(label, sizeof(label), "Session %zu", i + 1);
            char *ir = ctx_make_tok_label(r->ctx, agent->session_summaries[i]->token_count);
            ctx_render_group_row(r, label, ir); talloc_free(ir);
        }
    }
    ctx_render_group_footer(r);
}

static void render_recent_summary(ctx_rend_t *r, ik_agent_ctx_t *agent, int32_t recent_tok)
{
    ctx_render_outer_blank(r);
    if (agent->recent_summary == NULL) {
        ctx_render_group_header(r, "Recent Summary", "(empty)");
    } else {
        char *rl = ctx_make_tok_label(r->ctx, recent_tok);
        ctx_render_group_header(r, "Recent Summary", rl); talloc_free(rl);
        char *ir = ctx_make_tok_label(r->ctx, agent->recent_summary_tokens);
        ctx_render_group_row(r, "Current session", ir); talloc_free(ir);
    }
    ctx_render_group_footer(r);
}

static void render_tools(ctx_rend_t *r, ik_tool_registry_t *reg, int32_t tool_tok)
{
    ctx_render_outer_blank(r);
    size_t cnt = (reg != NULL) ? reg->count : 0;
    if (cnt == 0) {
        ctx_render_group_header(r, "Tools", "(empty)");
    } else {
        char *rl = ctx_make_tok_label(r->ctx, tool_tok);
        ctx_render_group_header(r, "Tools", rl); talloc_free(rl);
        char cline[64];
        snprintf(cline, sizeof(cline), "%zu tools registered", cnt);
        ctx_render_group_row(r, cline, NULL);
        char *names = talloc_strdup(r->ctx, "");
        if (!names) PANIC("OOM");  /* LCOV_EXCL_LINE */
        for (size_t i = 0; i < reg->count; i++) {
            char *tmp = (i == 0) ? talloc_asprintf(r->ctx, "%s", reg->entries[i].name)
                                 : talloc_asprintf(r->ctx, "%s  %s", names, reg->entries[i].name);
            if (!tmp) PANIC("OOM");  /* LCOV_EXCL_LINE */
            talloc_free(names);
            names = tmp;
        }
        ctx_render_group_row(r, names, NULL);
        talloc_free(names);
    }
    ctx_render_group_footer(r);
}

static void render_message_history(ctx_rend_t *r, ik_agent_ctx_t *agent,
                                   const ctx_msg_stats_t *ms)
{
    ctx_render_outer_blank(r);
    if (agent->message_count == 0) {
        ctx_render_group_header(r, "Message History", "(empty)");
    } else {
        char *rl = ctx_make_tok_label(r->ctx, ms->tokens);
        ctx_render_group_header(r, "Message History", rl); talloc_free(rl);
        char tline[128];
        snprintf(tline, sizeof(tline), "%zu turns · %zu user · %zu assistant",
                 agent->message_count, ms->user_count, ms->asst_count);
        ctx_render_group_row(r, tline, NULL);
        if (ms->tool_pairs > 0) {
            char tpline[64];
            snprintf(tpline, sizeof(tpline), "%zu tool_use / tool_result pairs", ms->tool_pairs);
            ctx_render_group_row(r, tpline, NULL);
        }
    }
    ctx_render_group_footer(r);
}

/* ================================================================
 * Main handler
 * ================================================================ */

res_t ik_cmd_context(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */
    (void)args;

    ik_agent_ctx_t *agent = repl->current;

    int W = CTX_MIN_WIDTH;
    if (repl->shared && repl->shared->term &&
        repl->shared->term->screen_cols > CTX_MIN_WIDTH) {
        W = repl->shared->term->screen_cols;
    }

    /* Build system blocks for content + token estimates */
    TALLOC_CTX *blk_ctx = talloc_new(ctx);
    if (!blk_ctx) PANIC("OOM");  /* LCOV_EXCL_LINE */
    ik_request_t *req = NULL;
    res_t br = ik_request_create(blk_ctx, "", &req);
    if (is_ok(&br)) {
        br = ik_agent_build_system_blocks(req, agent);
        if (is_err(&br)) { talloc_free(br.err); req = NULL; }
    } else {
        talloc_free(br.err);
        req = NULL;
    }

    ctx_block_toks_t bt  = collect_block_toks(req);
    int32_t tool_tok     = collect_tool_toks(
        repl->shared ? repl->shared->tool_registry : NULL);
    ctx_msg_stats_t ms   = collect_msg_stats(agent);

    int32_t grand_total  = bt.sys + bt.pinned + bt.skill + bt.catalog
                         + bt.sess + bt.recent + tool_tok + ms.tokens;

    ctx_rend_t r;
    ctx_rend_init(&r, ctx, agent->scrollback, W);

    ctx_render_outer_title(&r);
    render_tools(&r, repl->shared ? repl->shared->tool_registry : NULL, tool_tok);
    render_sys_prompt(&r, bt.sys);
    render_pinned_docs(&r, agent, req, bt.pinned);
    render_skills(&r, agent, bt.skill);
    render_skill_catalog(&r, agent, bt.catalog);
    render_session_summaries(&r, agent, bt.sess);
    render_recent_summary(&r, agent, bt.recent);
    render_message_history(&r, agent, &ms);
    ctx_render_outer_blank(&r);
    ctx_render_total_line(&r, grand_total);
    ctx_render_budget_line(&r, 100000, grand_total);
    ctx_render_outer_close(&r);

    talloc_free(blk_ctx);
    return r.err;
}
