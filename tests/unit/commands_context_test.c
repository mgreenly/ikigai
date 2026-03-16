/**
 * @file commands_context_test.c
 * @brief Unit tests for /context command handler
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/commands_context.h"
#include "apps/ikigai/commands_context_box.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/scrollback_utils.h"
#include "shared/terminal.h"
#include "tests/helpers/test_utils_helper.h"

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

/* Helper: get scrollback line at index */
static const char *get_line(ik_scrollback_t *sb, size_t idx)
{
    const char *text = NULL;
    size_t len = 0;
    res_t r = ik_scrollback_get_line_text(sb, idx, &text, &len);
    if (is_err(&r)) { talloc_free(r.err); return NULL; }
    return text;
}

/* Helper: build minimal repl context pointing at agent */
static ik_repl_ctx_t *make_repl(TALLOC_CTX *ctx, ik_agent_ctx_t *agent)
{
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = agent;
    repl->shared  = agent->shared;
    return repl;
}

/* Helper: set terminal width on shared context */
static void set_term_width(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, int width)
{
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_cols = width;
    agent->shared->term = term;
}

START_TEST(test_context_basic_render) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    ck_assert_uint_gt(count, 10);
    ck_assert_ptr_nonnull(strstr(get_line(agent->scrollback, 0), "Context"));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_empty_groups) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->pinned_count, 0);
    ck_assert_uint_eq(agent->loaded_skill_count, 0);
    ck_assert_uint_eq(agent->skillset_catalog_count, 0);
    ck_assert_uint_eq(agent->session_summary_count, 0);
    ck_assert_ptr_null(agent->recent_summary);
    ck_assert_uint_eq(agent->message_count, 0);
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    int empty_count = 0;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "(empty)")) empty_count++;
    }
    ck_assert_int_ge(empty_count, 7);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_with_skill) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->loaded_skills = talloc_array(agent, ik_loaded_skill_t *, 1);
    ck_assert_ptr_nonnull(agent->loaded_skills);
    agent->loaded_skills[0] = talloc(agent, ik_loaded_skill_t);
    agent->loaded_skills[0]->name    = talloc_strdup(agent, "style");
    agent->loaded_skills[0]->content = talloc_strdup(agent, "style guide content here");
    agent->loaded_skills[0]->load_position = 0;
    agent->loaded_skill_count = 1;
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    int found_skill = 0, skills_empty = 0;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "style")) found_skill = 1;
        if (line && strstr(line, "Skills") && strstr(line, "(empty)")) skills_empty = 1;
    }
    ck_assert_int_eq(found_skill, 1);
    ck_assert_int_eq(skills_empty, 0);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_min_width) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    set_term_width(ctx, agent, 30);
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_gt(ik_scrollback_get_line_count(agent->scrollback), 5);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_wider_terminal) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    set_term_width(ctx, agent, 100);
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *line0 = get_line(agent->scrollback, 0);
    ck_assert_ptr_nonnull(line0);
    ck_assert_ptr_nonnull(strstr(line0, "Context"));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_line_trimming) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *trimmed = ctx_trim_middle(ctx, "/home/user/very/long/path/to/some/file.md", 10);
    ck_assert_ptr_nonnull(trimmed);
    ck_assert_int_le(ctx_disp_width(trimmed), 10);
    ck_assert_ptr_nonnull(strstr(trimmed, CTX_ELLIPSIS));
    talloc_free(trimmed);
    char *not_trimmed = ctx_trim_middle(ctx, "foo.md", 20);
    ck_assert_str_eq(not_trimmed, "foo.md");
    talloc_free(not_trimmed);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_format_tok) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *s;
    s = ctx_format_tok(ctx, 0);       ck_assert_str_eq(s, "0");         talloc_free(s);
    s = ctx_format_tok(ctx, 999);     ck_assert_str_eq(s, "999");       talloc_free(s);
    s = ctx_format_tok(ctx, 1000);    ck_assert_str_eq(s, "1,000");     talloc_free(s);
    s = ctx_format_tok(ctx, 12345);   ck_assert_str_eq(s, "12,345");    talloc_free(s);
    s = ctx_format_tok(ctx, 1234567); ck_assert_str_eq(s, "1,234,567"); talloc_free(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_with_session_summaries) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->session_summaries = talloc_array(agent, ik_session_summary_t *, 2);
    ck_assert_ptr_nonnull(agent->session_summaries);
    agent->session_summaries[0] = talloc(agent, ik_session_summary_t);
    agent->session_summaries[0]->summary     = talloc_strdup(agent, "first summary");
    agent->session_summaries[0]->token_count = 200;
    agent->session_summaries[1] = talloc(agent, ik_session_summary_t);
    agent->session_summaries[1]->summary     = talloc_strdup(agent, "second summary");
    agent->session_summaries[1]->token_count = 300;
    agent->session_summary_count = 2;
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    int found_s1 = 0, found_s2 = 0;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "Session 1")) found_s1 = 1;
        if (line && strstr(line, "Session 2")) found_s2 = 1;
    }
    ck_assert_int_eq(found_s1, 1);
    ck_assert_int_eq(found_s2, 1);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_with_recent_summary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->recent_summary        = talloc_strdup(agent, "summary text");
    agent->recent_summary_tokens = 150;
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    int found = 0;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "Current session")) found = 1;
    }
    ck_assert_int_eq(found, 1);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_footer_lines) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    int found_total = 0, found_budget = 0;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "Total"))  found_total = 1;
        if (line && strstr(line, "Budget")) found_budget = 1;
    }
    ck_assert_int_eq(found_total,  1);
    ck_assert_int_eq(found_budget, 1);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_skill_catalog) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->skillset_catalog = talloc_array(agent, ik_skillset_catalog_entry_t *, 2);
    ck_assert_ptr_nonnull(agent->skillset_catalog);
    agent->skillset_catalog[0] = talloc(agent, ik_skillset_catalog_entry_t);
    agent->skillset_catalog[0]->skill_name   = talloc_strdup(agent, "memory");
    agent->skillset_catalog[0]->description  = talloc_strdup(agent, "Memory docs");
    agent->skillset_catalog[0]->load_position = 0;
    agent->skillset_catalog[1] = talloc(agent, ik_skillset_catalog_entry_t);
    agent->skillset_catalog[1]->skill_name   = talloc_strdup(agent, "errors");
    agent->skillset_catalog[1]->description  = talloc_strdup(agent, "Error docs");
    agent->skillset_catalog[1]->load_position = 0;
    agent->skillset_catalog_count = 2;
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    int found_entries = 0;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "2 entries")) found_entries = 1;
    }
    ck_assert_int_eq(found_entries, 1);
    talloc_free(ctx);
}
END_TEST

static int line_disp_width(ik_scrollback_t *sb, size_t idx)
{
    const char *t = NULL; size_t n = 0;
    res_t r = ik_scrollback_get_line_text(sb, idx, &t, &n);
    if (is_err(&r)) { talloc_free(r.err); return -1; }
    return (int)ik_scrollback_calculate_display_width(t, n);
}

START_TEST(test_line_widths) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ctx_rend_t r;
    ctx_rend_init(&r, ctx, agent->scrollback, 80);
#define CLR ik_scrollback_clear(agent->scrollback)
#define WID line_disp_width(agent->scrollback, 0)
    CLR; ctx_render_outer_title(&r);                          ck_assert_int_eq(WID, 80);
    CLR; ctx_render_outer_blank(&r);                          ck_assert_int_eq(WID, 80);
    CLR; ctx_render_outer_close(&r);                          ck_assert_int_eq(WID, 80);
    CLR; ctx_render_group_header(&r, "Tools", "0 tok");       ck_assert_int_eq(WID, 80);
    CLR; ctx_render_group_row(&r, "label", "42 tok");         ck_assert_int_eq(WID, 80);
    CLR; ctx_render_group_row(&r, "3 turns \xc2\xb7 2 tool calls", NULL); ck_assert_int_eq(WID, 80);
    CLR; ctx_render_group_footer(&r);                         ck_assert_int_eq(WID, 80);
    CLR; ctx_render_total_line(&r, 1234567);                  ck_assert_int_eq(WID, 80);
    ctx_rend_t r2;
    ctx_rend_init(&r2, ctx, agent->scrollback, 60);
    CLR; ctx_render_budget_line(&r2, 2000000000, 1000000000); ck_assert_int_eq(WID, 60);
#undef CLR
#undef WID
    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_message_history_turns) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->messages = talloc_array(agent, ik_message_t *, 4);
    for (int i = 0; i < 4; i++) agent->messages[i] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_USER;
    agent->messages[1]->role = IK_ROLE_ASSISTANT;
    agent->messages[1]->content_blocks =
        talloc_array(agent->messages[1], ik_content_block_t, 1);
    agent->messages[1]->content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    agent->messages[1]->content_blocks[0].data.tool_call.arguments =
        talloc_strdup(agent->messages[1], "{}");
    agent->messages[1]->content_count = 1;
    agent->messages[2]->role = IK_ROLE_USER;
    agent->messages[3]->role = IK_ROLE_ASSISTANT;
    agent->message_count = 4;
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    int found = 0;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "2 turns") && strstr(line, "1 tool calls")) found = 1;
    }
    ck_assert_int_eq(found, 1);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_uniform_line_widths) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t r = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&r));
    agent->messages = talloc_array(agent, ik_message_t *, 2);
    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_USER;
    agent->messages[1] = talloc_zero(agent, ik_message_t);
    agent->messages[1]->role = IK_ROLE_ASSISTANT;
    agent->message_count = 2;
    set_term_width(ctx, agent, 100);
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    r = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&r));
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    ck_assert_uint_gt(count, 10);
    int expected = line_disp_width(agent->scrollback, 0);
    ck_assert_int_gt(expected, 0);
    for (size_t i = 1; i < count; i++)
        ck_assert_int_eq(line_disp_width(agent->scrollback, i), expected);
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Test suite registration
 * ================================================================ */

static Suite *commands_context_suite(void)
{
    Suite *s = suite_create("commands_context");
    TCase *tc = tcase_create("Core");
    tcase_add_unchecked_fixture(tc, suite_setup, NULL);
    tcase_add_test(tc, test_context_basic_render);
    tcase_add_test(tc, test_context_empty_groups);
    tcase_add_test(tc, test_context_with_skill);
    tcase_add_test(tc, test_context_min_width);
    tcase_add_test(tc, test_context_wider_terminal);
    tcase_add_test(tc, test_context_line_trimming);
    tcase_add_test(tc, test_context_format_tok);
    tcase_add_test(tc, test_context_with_session_summaries);
    tcase_add_test(tc, test_context_with_recent_summary);
    tcase_add_test(tc, test_context_footer_lines);
    tcase_add_test(tc, test_context_skill_catalog);
    tcase_add_test(tc, test_line_widths);
    tcase_add_test(tc, test_context_message_history_turns);
    tcase_add_test(tc, test_context_uniform_line_widths);
    suite_add_tcase(s, tc);
    return s;
}

int32_t main(void)
{
    Suite   *s  = commands_context_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands_context_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
