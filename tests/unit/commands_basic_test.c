/**
 * @file commands_basic_test.c
 * @brief Unit tests for basic command handlers (ik_cmd_summary)
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_basic.h"
#include "apps/ikigai/marks.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "tests/helpers/test_utils_helper.h"

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

/* Helper: get scrollback line text at index */
static const char *get_line(ik_scrollback_t *sb, size_t idx)
{
    const char *text = NULL;
    size_t len = 0;
    res_t r = ik_scrollback_get_line_text(sb, idx, &text, &len);
    if (is_err(&r)) {
        talloc_free(r.err);
        return NULL;
    }
    return text;
}

/* Helper: build a minimal repl context pointing at agent */
static ik_repl_ctx_t *make_repl(TALLOC_CTX *ctx, ik_agent_ctx_t *agent)
{
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = agent;
    repl->shared = agent->shared;
    return repl;
}

/* ---- /summary: no summaries ---- */

START_TEST(test_summary_no_summaries) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);

    /* Confirm default state */
    ck_assert_ptr_null(agent->recent_summary);
    ck_assert_uint_eq(agent->session_summary_count, 0);

    ik_scrollback_clear(agent->scrollback);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_summary(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    /*
     * Expected scrollback layout (4 lines):
     *   [0] "── Recent Summary ──"
     *   [1] "(none)"
     *   [2] ""
     *   [3] "── Previous Sessions (0/5) ──"
     */
    ck_assert_uint_eq(ik_scrollback_get_line_count(agent->scrollback), 4);

    const char *line1 = get_line(agent->scrollback, 1);
    ck_assert_ptr_nonnull(line1);
    ck_assert_str_eq(line1, "(none)");

    const char *line3 = get_line(agent->scrollback, 3);
    ck_assert_ptr_nonnull(line3);
    ck_assert_ptr_nonnull(strstr(line3, "(0/5)"));

    talloc_free(ctx);
}
END_TEST

/* ---- /summary: agent with summaries ---- */

START_TEST(test_summary_with_summaries) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);

    /* Set recent summary */
    agent->recent_summary = talloc_strdup(agent, "recent text here");
    ck_assert_ptr_nonnull(agent->recent_summary);

    /* Set two session summaries (oldest-first) */
    agent->session_summaries = talloc_array(agent, ik_session_summary_t *, 2);
    ck_assert_ptr_nonnull(agent->session_summaries);
    agent->session_summaries[0] = talloc(agent, ik_session_summary_t);
    agent->session_summaries[0]->summary = talloc_strdup(agent, "first session");
    agent->session_summaries[0]->token_count = 100;
    agent->session_summaries[1] = talloc(agent, ik_session_summary_t);
    agent->session_summaries[1]->summary = talloc_strdup(agent, "second session");
    agent->session_summaries[1]->token_count = 200;
    agent->session_summary_count = 2;

    ik_scrollback_clear(agent->scrollback);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_summary(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    /*
     * Expected scrollback layout (6 lines):
     *   [0] "── Recent Summary ──"
     *   [1] "recent text here"
     *   [2] ""
     *   [3] "── Previous Sessions (2/5) ──"
     *   [4] "[1] first session"
     *   [5] "[2] second session"
     */
    ck_assert_uint_eq(ik_scrollback_get_line_count(agent->scrollback), 6);

    const char *line1 = get_line(agent->scrollback, 1);
    ck_assert_ptr_nonnull(line1);
    ck_assert_str_eq(line1, "recent text here");

    const char *line3 = get_line(agent->scrollback, 3);
    ck_assert_ptr_nonnull(line3);
    ck_assert_ptr_nonnull(strstr(line3, "(2/5)"));

    const char *line4 = get_line(agent->scrollback, 4);
    ck_assert_ptr_nonnull(line4);
    ck_assert_ptr_nonnull(strstr(line4, "[1]"));
    ck_assert_ptr_nonnull(strstr(line4, "first session"));

    const char *line5 = get_line(agent->scrollback, 5);
    ck_assert_ptr_nonnull(line5);
    ck_assert_ptr_nonnull(strstr(line5, "[2]"));
    ck_assert_ptr_nonnull(strstr(line5, "second session"));

    talloc_free(ctx);
}
END_TEST

/* ---- Fork: child does not inherit summary fields ---- */

START_TEST(test_fork_child_no_summary_inheritance) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    /* Create parent with summary state */
    ik_agent_ctx_t *parent = NULL;
    res_t res = ik_test_create_agent(ctx, &parent);
    ck_assert(is_ok(&res));

    parent->recent_summary = talloc_strdup(parent, "a summary");
    parent->recent_summary_tokens = 42;
    parent->recent_summary_generation = 3;

    parent->session_summaries = talloc_array(parent, ik_session_summary_t *, 1);
    ck_assert_ptr_nonnull(parent->session_summaries);
    parent->session_summaries[0] = talloc(parent, ik_session_summary_t);
    parent->session_summaries[0]->summary = talloc_strdup(parent, "old session");
    parent->session_summaries[0]->token_count = 10;
    parent->session_summary_count = 1;

    /* Create child and copy conversation (simulates fork) */
    ik_agent_ctx_t *child = NULL;
    res = ik_test_create_agent(ctx, &child);
    ck_assert(is_ok(&res));

    res = ik_agent_copy_conversation(child, parent);
    ck_assert(is_ok(&res));

    /* Child must not inherit any summary fields */
    ck_assert_ptr_null(child->recent_summary);
    ck_assert_int_eq(child->recent_summary_tokens, 0);
    ck_assert_uint_eq(child->recent_summary_generation, 0);
    ck_assert_ptr_null(child->session_summaries);
    ck_assert_uint_eq(child->session_summary_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ---- Rewind: resets recent_summary, does not generate session summary ---- */

START_TEST(test_rewind_clears_recent_summary) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = make_repl(ctx, agent);

    /* Set a mark at the start (message_index = 0) */
    res = ik_mark_create(repl, "checkpoint");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->mark_count, 1);

    /* Add a message after the mark */
    ik_message_t *msg = ik_message_create_text(ctx, IK_ROLE_USER, "hello");
    ck_assert_ptr_nonnull(msg);
    res = ik_agent_add_message(agent, msg);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->message_count, 1);

    /* Set recent summary on the agent */
    agent->recent_summary = talloc_strdup(agent, "summary of pruned messages");
    agent->recent_summary_tokens = 55;
    uint32_t gen_before = agent->recent_summary_generation;

    /* Set no session summaries to confirm none are added */
    ck_assert_uint_eq(agent->session_summary_count, 0);

    /* Rewind to the checkpoint */
    ik_mark_t *target = agent->marks[0];
    res = ik_mark_rewind_to_mark(repl, target);
    ck_assert(is_ok(&res));

    /* Conversation truncated */
    ck_assert_uint_eq(agent->message_count, 0);

    /* recent_summary must be NULL after rewind */
    ck_assert_ptr_null(agent->recent_summary);
    ck_assert_int_eq(agent->recent_summary_tokens, 0);

    /* Generation incremented to invalidate any in-flight summaries */
    ck_assert_uint_gt(agent->recent_summary_generation, gen_before);

    /* No session summary was generated (epoch did not end) */
    ck_assert_uint_eq(agent->session_summary_count, 0);

    talloc_free(ctx);
}
END_TEST

/* Helper: add a loaded skill entry to an agent */
static void add_skill(ik_agent_ctx_t *agent, const char *name, size_t load_position)
{
    size_t cap = agent->loaded_skill_count + 1;
    agent->loaded_skills = talloc_realloc(agent, agent->loaded_skills,
                                          ik_loaded_skill_t *, (unsigned int)cap);
    ik_loaded_skill_t *skill = talloc_zero(agent, ik_loaded_skill_t);
    skill->name = talloc_strdup(skill, name);
    skill->content = talloc_strdup(skill, "skill content");
    skill->load_position = load_position;
    agent->loaded_skills[agent->loaded_skill_count] = skill;
    agent->loaded_skill_count++;
}

/* ---- /clear: drops all loaded skills ---- */

START_TEST(test_clear_drops_skills) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    add_skill(agent, "database", 0);
    add_skill(agent, "style", 2);
    ck_assert_uint_eq(agent->loaded_skill_count, 2);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);

    res = ik_cmd_clear(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(agent->loaded_skill_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ---- /rewind: trims skills loaded after target ---- */

START_TEST(test_rewind_trims_skills_after_target) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = make_repl(ctx, agent);

    /* Mark at message_index=0 */
    res = ik_mark_create(repl, "checkpoint");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->mark_count, 1);

    /* Simulate adding messages */
    ik_message_t *msg1 = ik_message_create_text(ctx, IK_ROLE_USER, "hello");
    ck_assert_ptr_nonnull(msg1);
    res = ik_agent_add_message(agent, msg1);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->message_count, 1);

    /* skill loaded before mark (load_position=0, same as mark) */
    add_skill(agent, "database", 0);
    /* skill loaded after mark (load_position=1) */
    add_skill(agent, "style", 1);
    ck_assert_uint_eq(agent->loaded_skill_count, 2);

    /* Rewind to the checkpoint (message_index=0) */
    ik_mark_t *target = agent->marks[0];
    res = ik_mark_rewind_to_mark(repl, target);
    ck_assert(is_ok(&res));

    /* "style" was loaded at position 1 >= 0, so it should be trimmed */
    /* "database" was loaded at position 0 >= 0 (mark's message_index), also trimmed */
    /* Both are >= target_mark->message_index (0), so both trimmed */
    ck_assert_uint_eq(agent->loaded_skill_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ---- /rewind: retains skills loaded before target ---- */

START_TEST(test_rewind_retains_skills_before_target) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = make_repl(ctx, agent);

    /* Add two assistant messages (non-user role avoids token cache turn counting) */
    ik_message_t *msg1 = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, "reply1");
    ck_assert_ptr_nonnull(msg1);
    res = ik_agent_add_message(agent, msg1);
    ck_assert(is_ok(&res));

    ik_message_t *msg2 = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, "reply2");
    ck_assert_ptr_nonnull(msg2);
    res = ik_agent_add_message(agent, msg2);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->message_count, 2);

    /* Mark at position 2 (after both messages) */
    res = ik_mark_create(repl, "checkpoint");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->mark_count, 1);
    ck_assert_uint_eq(agent->marks[0]->message_index, 2);

    /* Override mark to be at position 1 so skill at 0 is before it */
    agent->marks[0]->message_index = 1;

    /* Skill at position 0 (< 1): should be retained */
    add_skill(agent, "database", 0);
    /* Skill at position 1 (>= 1): should be trimmed */
    add_skill(agent, "style", 1);
    ck_assert_uint_eq(agent->loaded_skill_count, 2);

    ik_mark_t *target = agent->marks[0];
    res = ik_mark_rewind_to_mark(repl, target);
    ck_assert(is_ok(&res));

    /* "style" (position=1 >= 1) trimmed, "database" (position=0 < 1) retained */
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "database");

    talloc_free(ctx);
}
END_TEST

static Suite *commands_basic_suite(void)
{
    Suite *s = suite_create("commands_basic");

    TCase *tc_summary = tcase_create("Summary");
    tcase_add_unchecked_fixture(tc_summary, suite_setup, NULL);
    tcase_add_test(tc_summary, test_summary_no_summaries);
    tcase_add_test(tc_summary, test_summary_with_summaries);
    suite_add_tcase(s, tc_summary);

    TCase *tc_fork = tcase_create("Fork");
    tcase_add_unchecked_fixture(tc_fork, suite_setup, NULL);
    tcase_add_test(tc_fork, test_fork_child_no_summary_inheritance);
    suite_add_tcase(s, tc_fork);

    TCase *tc_rewind = tcase_create("Rewind");
    tcase_add_unchecked_fixture(tc_rewind, suite_setup, NULL);
    tcase_add_test(tc_rewind, test_rewind_clears_recent_summary);
    suite_add_tcase(s, tc_rewind);

    TCase *tc_skills = tcase_create("Skills");
    tcase_add_unchecked_fixture(tc_skills, suite_setup, NULL);
    tcase_add_test(tc_skills, test_clear_drops_skills);
    tcase_add_test(tc_skills, test_rewind_trims_skills_after_target);
    tcase_add_test(tc_skills, test_rewind_retains_skills_before_target);
    suite_add_tcase(s, tc_skills);

    return s;
}

int32_t main(void)
{
    Suite *s = commands_basic_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands_basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
