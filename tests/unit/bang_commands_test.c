/**
 * @file bang_commands_test.c
 * @brief Unit tests for bang command handlers (!unload)
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/bang_commands.h"
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

/* ---- !skills: no skills loaded ---- */

START_TEST(test_skills_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_bang_dispatch(ctx, repl, "!skills");
    ck_assert(is_ok(&res));

    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    ck_assert_uint_ge(count, 2);

    bool found = false;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "No skills loaded.")) {
            found = true;
            break;
        }
    }
    ck_assert_msg(found, "Expected 'No skills loaded.' in scrollback");

    talloc_free(ctx);
}
END_TEST

/* ---- !skills: one skill loaded ---- */

START_TEST(test_skills_one) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    add_skill(agent, "database", 0);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_bang_dispatch(ctx, repl, "!skills");
    ck_assert(is_ok(&res));

    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    ck_assert_uint_ge(count, 2);

    bool found = false;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "database") && strstr(line, "B")) {
            found = true;
            break;
        }
    }
    ck_assert_msg(found, "Expected skill entry for 'database' in scrollback");

    talloc_free(ctx);
}
END_TEST

/* ---- !skills: multiple skills loaded ---- */

START_TEST(test_skills_multiple) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    add_skill(agent, "memory", 0);
    add_skill(agent, "errors", 1);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_bang_dispatch(ctx, repl, "!skills");
    ck_assert(is_ok(&res));

    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    ck_assert_uint_ge(count, 3);

    bool found_memory = false;
    bool found_errors = false;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "memory")) found_memory = true;
        if (line && strstr(line, "errors")) found_errors = true;
    }
    ck_assert_msg(found_memory, "Expected skill entry for 'memory' in scrollback");
    ck_assert_msg(found_errors, "Expected skill entry for 'errors' in scrollback");

    talloc_free(ctx);
}
END_TEST

/* ---- !unload: no skill name given ---- */

START_TEST(test_unload_no_name) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_bang_dispatch(ctx, repl, "!unload");
    ck_assert(is_ok(&res));

    /* Should show usage warning in scrollback (after the echoed command line) */
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    ck_assert_uint_ge(count, 2);

    /* Check that a usage message appears */
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "Usage:") && strstr(line, "!unload")) {
            found = true;
            break;
        }
    }
    ck_assert_msg(found, "Expected usage warning in scrollback");
    ck_assert_uint_eq(agent->loaded_skill_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ---- !unload: skill not loaded ---- */

START_TEST(test_unload_not_loaded) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_bang_dispatch(ctx, repl, "!unload nonexistent");
    ck_assert(is_ok(&res));

    /* Should show "Skill not loaded" warning */
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    ck_assert_uint_ge(count, 2);

    bool found = false;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "Skill not loaded") && strstr(line, "nonexistent")) {
            found = true;
            break;
        }
    }
    ck_assert_msg(found, "Expected 'Skill not loaded' warning in scrollback");
    ck_assert_uint_eq(agent->loaded_skill_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ---- !unload: successful unload ---- */

START_TEST(test_unload_success) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Add two skills */
    add_skill(agent, "database", 0);
    add_skill(agent, "style", 2);
    ck_assert_uint_eq(agent->loaded_skill_count, 2);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_bang_dispatch(ctx, repl, "!unload database");
    ck_assert(is_ok(&res));

    /* Skill count should drop by one */
    ck_assert_uint_eq(agent->loaded_skill_count, 1);

    /* Remaining skill should be "style" */
    ck_assert_str_eq(agent->loaded_skills[0]->name, "style");

    /* Confirmation in scrollback */
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        const char *line = get_line(agent->scrollback, i);
        if (line && strstr(line, "Skill unloaded") && strstr(line, "database")) {
            found = true;
            break;
        }
    }
    ck_assert_msg(found, "Expected confirmation in scrollback");

    talloc_free(ctx);
}
END_TEST

/* ---- !unload: middle skill removed, array shifts correctly ---- */

START_TEST(test_unload_middle_skill) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    add_skill(agent, "memory", 0);
    add_skill(agent, "errors", 1);
    add_skill(agent, "style", 2);
    ck_assert_uint_eq(agent->loaded_skill_count, 3);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_bang_dispatch(ctx, repl, "!unload errors");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(agent->loaded_skill_count, 2);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "memory");
    ck_assert_str_eq(agent->loaded_skills[1]->name, "style");

    talloc_free(ctx);
}
END_TEST

static Suite *bang_commands_suite(void)
{
    Suite *s = suite_create("bang_commands");

    TCase *tc_skills = tcase_create("Skills");
    tcase_add_unchecked_fixture(tc_skills, suite_setup, NULL);
    tcase_add_test(tc_skills, test_skills_empty);
    tcase_add_test(tc_skills, test_skills_one);
    tcase_add_test(tc_skills, test_skills_multiple);
    suite_add_tcase(s, tc_skills);

    TCase *tc_unload = tcase_create("Unload");
    tcase_add_unchecked_fixture(tc_unload, suite_setup, NULL);
    tcase_add_test(tc_unload, test_unload_no_name);
    tcase_add_test(tc_unload, test_unload_not_loaded);
    tcase_add_test(tc_unload, test_unload_success);
    tcase_add_test(tc_unload, test_unload_middle_skill);
    suite_add_tcase(s, tc_unload);

    return s;
}

int32_t main(void)
{
    Suite *s = bang_commands_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/bang_commands_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
