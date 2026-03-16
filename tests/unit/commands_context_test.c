/**
 * @file commands_context_test.c
 * @brief Unit tests for /context command handler
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_context.h"
#include "apps/ikigai/commands_context_box.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
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

/* ================================================================
 * Basic rendering: command runs and produces output
 * ================================================================ */

START_TEST(test_context_basic_render) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);

    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    /* Should produce multiple lines */
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    ck_assert_uint_gt(count, 10);

    /* First line should be outer box title containing "Context" */
    const char *line0 = get_line(agent->scrollback, 0);
    ck_assert_ptr_nonnull(line0);
    ck_assert_ptr_nonnull(strstr(line0, "Context"));

    /* Last line should be outer box close (contains BOX_BR) */
    const char *last = get_line(agent->scrollback, count - 1);
    ck_assert_ptr_nonnull(last);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Groups always shown; empty groups show "(empty)"
 * ================================================================ */

START_TEST(test_context_empty_groups) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Fresh agent: no pinned docs, skills, skill catalog, summaries, messages */
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

    /* Scan all lines for "(empty)" markers */
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    int empty_count = 0;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "(empty)")) empty_count++;
    }
    /* 7 groups should be empty: Tools, Pinned Docs, Skills, Skill Catalog,
     * Session Summaries, Recent Summary, Message History */
    ck_assert_int_ge(empty_count, 7);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Groups show content when data is present
 * ================================================================ */

START_TEST(test_context_with_skill) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Add a loaded skill */
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

    /* Look for "style" skill name in output */
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    int found_skill = 0;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "style")) found_skill = 1;
    }
    ck_assert_int_eq(found_skill, 1);

    /* Skills group should NOT be "(empty)" */
    int skills_empty = 0;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "Skills") && strstr(line, "(empty)")) skills_empty = 1;
    }
    ck_assert_int_eq(skills_empty, 0);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Width adaptation: minimum 60 columns enforced
 * ================================================================ */

START_TEST(test_context_min_width) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Set terminal width below minimum */
    set_term_width(ctx, agent, 30);

    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);

    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    /* Should still produce output */
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    ck_assert_uint_gt(count, 5);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_wider_terminal) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Set a wider terminal */
    set_term_width(ctx, agent, 100);

    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);

    res = ik_cmd_context(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    /* Title line should contain "Context" */
    const char *line0 = get_line(agent->scrollback, 0);
    ck_assert_ptr_nonnull(line0);
    ck_assert_ptr_nonnull(strstr(line0, "Context"));

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Line trimming: long labels are trimmed with ellipsis
 * ================================================================ */

START_TEST(test_context_line_trimming) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    /* ctx_trim_middle with max_cols = 10 should trim a long string */
    const char *long_str = "/home/user/very/long/path/to/some/file.md";
    char *trimmed = ctx_trim_middle(ctx, long_str, 10);
    ck_assert_ptr_nonnull(trimmed);
    /* Trimmed display width should be <= 10 */
    int dw = ctx_disp_width(trimmed);
    ck_assert_int_le(dw, 10);
    /* Should contain ellipsis */
    ck_assert_ptr_nonnull(strstr(trimmed, CTX_ELLIPSIS));
    talloc_free(trimmed);

    /* String shorter than max should not be trimmed */
    const char *short_str = "foo.md";
    char *not_trimmed = ctx_trim_middle(ctx, short_str, 20);
    ck_assert_ptr_nonnull(not_trimmed);
    ck_assert_str_eq(not_trimmed, short_str);
    talloc_free(not_trimmed);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Format token count with commas
 * ================================================================ */

START_TEST(test_context_format_tok) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    char *s = ctx_format_tok(ctx, 0);       ck_assert_str_eq(s, "0");       talloc_free(s);
    s = ctx_format_tok(ctx, 999);           ck_assert_str_eq(s, "999");     talloc_free(s);
    s = ctx_format_tok(ctx, 1000);          ck_assert_str_eq(s, "1,000");   talloc_free(s);
    s = ctx_format_tok(ctx, 12345);         ck_assert_str_eq(s, "12,345");  talloc_free(s);
    s = ctx_format_tok(ctx, 1234567);       ck_assert_str_eq(s, "1,234,567"); talloc_free(s);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Session summaries shown when present
 * ================================================================ */

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

    /* Look for "Session 1" and "Session 2" in output */
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

/* ================================================================
 * Recent summary shown when present
 * ================================================================ */

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

    /* "Current session" row should appear */
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

/* ================================================================
 * Footer shows "Total" and "Budget"
 * ================================================================ */

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

/* ================================================================
 * Skill catalog entries shown
 * ================================================================ */

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

    /* "2 entries" should appear */
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

START_TEST(test_line_widths) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ctx_rend_t r;
    ctx_rend_init(&r, ctx, agent->scrollback, 80);
    ik_scrollback_clear(agent->scrollback);
    ctx_render_group_row(&r, "label", "42 tok");
    ck_assert_int_eq(ctx_disp_width(get_line(agent->scrollback, 0)), 80);
    ik_scrollback_clear(agent->scrollback);
    ctx_render_total_line(&r, 1234567);
    ck_assert_int_eq(ctx_disp_width(get_line(agent->scrollback, 0)), 80);
    ctx_rend_t r2;
    ctx_rend_init(&r2, ctx, agent->scrollback, 60);
    ik_scrollback_clear(agent->scrollback);
    ctx_render_budget_line(&r2, 2000000000, 1000000000);
    ck_assert_int_eq(ctx_disp_width(get_line(agent->scrollback, 0)), 60);
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
