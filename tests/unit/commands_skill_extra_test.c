/**
 * @file commands_skill_extra_test.c
 * @brief Additional unit tests for skill commands (/skills, /unload, load_by_name)
 */

#include <check.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_skill.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/doc_cache.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/template.h"
#include "apps/ikigai/token_cache.h"
#include "shared/error.h"
#include "shared/wrapper_internal.h"
#include "tests/helpers/test_utils_helper.h"

/* ---- mock controls ---- */

static bool g_doc_fail = false;
static const char *g_doc_content = "# Skill Content";

res_t ik_doc_cache_get_(void *cache, const char *path, char **out_content)
{
    (void)cache; (void)path;
    if (g_doc_fail) {
        TALLOC_CTX *tmp = talloc_new(NULL);
        return ERR(tmp, IO, "mock doc cache failure");
    }
    *out_content = (char *)(uintptr_t)g_doc_content;
    return OK(NULL);
}

static const char *g_tpl_output = NULL;

res_t ik_template_process_file(TALLOC_CTX *ctx, const char *text, ik_agent_ctx_t *agent,
                                ik_config_t *config, const char *file_path, ik_template_result_t **out)
{
    (void)agent; (void)config; (void)text; (void)file_path;
    if (g_tpl_output != NULL) {
        ik_template_result_t *r = talloc_zero(ctx, ik_template_result_t);
        r->processed = talloc_strdup(ctx, g_tpl_output);
        *out = r;
    } else {
        *out = NULL;
    }
    return OK(NULL);
}

static int g_ins_count = 0;
static bool g_ins_fail = false;

res_t ik_db_message_insert_(ik_db_ctx_t *db, int64_t session_id, const char *agent_uuid,
                             const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    g_ins_count++;
    if (g_ins_fail) {
        TALLOC_CTX *tmp = talloc_new(NULL);
        return ERR(tmp, IO, "mock insert failure");
    }
    return OK(NULL);
}

static void reset_mocks(void)
{
    g_doc_fail = false;
    g_doc_content = "# Skill Content";
    g_tpl_output = NULL;
    g_ins_count = 0;
    g_ins_fail = false;
}

static void suite_setup(void) { ik_test_set_log_dir(__FILE__); }

static ik_repl_ctx_t *make_repl(TALLOC_CTX *ctx, ik_agent_ctx_t *agent)
{
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = agent;
    repl->shared = agent->shared;
    return repl;
}

static ik_doc_cache_t *fake_cache(TALLOC_CTX *ctx)
{
    return (ik_doc_cache_t *)talloc_zero(ctx, char);
}

/* ================================================================
 * ik_cmd_skills: no skills, small, large
 * ================================================================ */

START_TEST(test_cmd_skills_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_skills(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_line_count(agent->scrollback), 1);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_cmd_skills_small) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ik_skill_store_loaded(agent, "errors", "short");
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_skills(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *text = NULL; size_t len = 0;
    ik_scrollback_get_line_text(agent->scrollback, 0, &text, &len);
    ck_assert_ptr_nonnull(strstr(text, " B)"));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_cmd_skills_large) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    char *big = talloc_zero_array(ctx, char, 2048);
    memset(big, 'x', 2047);
    ik_skill_store_loaded(agent, "big", big);
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_skills(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *text = NULL; size_t len = 0;
    ik_scrollback_get_line_text(agent->scrollback, 0, &text, &len);
    ck_assert_ptr_nonnull(strstr(text, " KB)"));
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_unload: variants
 * ================================================================ */

START_TEST(test_cmd_unload_null_args) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_unload(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_line_count(agent->scrollback), 1);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_cmd_unload_not_found) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_unload(ctx, repl, "nosuchskill");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_ins_count, 0);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_cmd_unload_happy_with_db) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ik_skill_store_loaded(agent, "errors", "content");
    agent->token_cache = ik_token_cache_create(ctx, agent);
    ik_scrollback_clear(agent->scrollback);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 1;
    repl->shared->db_ctx = (void *)(uintptr_t)1;
    res = ik_cmd_unload(ctx, repl, "errors");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->loaded_skill_count, 0);
    ck_assert_int_eq(g_ins_count, 1);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_cmd_unload_insert_fail) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_ins_fail = true;
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ik_skill_store_loaded(agent, "errors", "content");
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 1;
    repl->shared->db_ctx = (void *)(uintptr_t)1;
    res = ik_cmd_unload(ctx, repl, "errors");
    /* unload silently ignores insert errors */
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->loaded_skill_count, 0);
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_skill_load_by_name: not found, happy, with template
 * ================================================================ */

START_TEST(test_skill_load_by_name_not_found) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_fail = true;
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = fake_cache(ctx);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    bool ok = ik_skill_load_by_name(ctx, repl, agent, "nosuch");
    ck_assert(!ok);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_skill_load_by_name_happy) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = fake_cache(ctx);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;
    bool ok = ik_skill_load_by_name(ctx, repl, agent, "myskill");
    ck_assert(ok);
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "myskill");
    talloc_free(ctx);
}
END_TEST

/* Covers line 393: resolved_content = template_result->processed in ik_skill_load_by_name */
START_TEST(test_skill_load_by_name_with_template) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_tpl_output = "processed content";
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = fake_cache(ctx);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;
    bool ok = ik_skill_load_by_name(ctx, repl, agent, "myskill");
    ck_assert(ok);
    ck_assert_str_eq(agent->loaded_skills[0]->content, "processed content");
    talloc_free(ctx);
}
END_TEST

/* Covers line 175: pos_args loop in persist_skill_load_event_ */
START_TEST(test_cmd_load_pos_args_with_db) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_content = "${1} content";
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = fake_cache(ctx);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 5;
    repl->shared->db_ctx = (void *)(uintptr_t)1;
    ik_scrollback_clear(agent->scrollback);
    res = ik_cmd_load(ctx, repl, "myskill world");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->content, "world content");
    ck_assert_int_eq(g_ins_count, 1);
    talloc_free(ctx);
}
END_TEST

/* Covers line 356: talloc_free(db_res.err) in ik_cmd_unload DB failure */
START_TEST(test_cmd_load_insert_fail) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_ins_fail = true;
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = fake_cache(ctx);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 5;
    repl->shared->db_ctx = (void *)(uintptr_t)1;
    ik_scrollback_clear(agent->scrollback);
    res = ik_cmd_load(ctx, repl, "myskill");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_apply_positional_dollar_no_brace) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_content = "price $10 each";
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = fake_cache(ctx);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;
    ik_scrollback_clear(agent->scrollback);
    res = ik_cmd_load(ctx, repl, "myskill val");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(agent->loaded_skills[0]->content, "price $10 each");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_apply_positional_unclosed_brace) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_content = "Hello ${1 world";
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = fake_cache(ctx);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;
    ik_scrollback_clear(agent->scrollback);
    res = ik_cmd_load(ctx, repl, "myskill val");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(agent->loaded_skills[0]->content, "Hello ${1 world");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_apply_positional_no_trailing_text) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_content = "${1}";
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = fake_cache(ctx);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;
    ik_scrollback_clear(agent->scrollback);
    res = ik_cmd_load(ctx, repl, "myskill hello");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(agent->loaded_skills[0]->content, "hello");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_apply_positional_invalid_vars) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_content = "${} and ${0} stay literal";
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = fake_cache(ctx);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;
    ik_scrollback_clear(agent->scrollback);
    res = ik_cmd_load(ctx, repl, "myskill val");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(agent->loaded_skills[0]->content, "${} and ${0} stay literal");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_apply_positional_non_digit_var) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_content = "Hello ${name}!";
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = fake_cache(ctx);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;
    ik_scrollback_clear(agent->scrollback);
    res = ik_cmd_load(ctx, repl, "myskill val");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(agent->loaded_skills[0]->content, "Hello ${name}!");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_persist_skill_event_no_session) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = fake_cache(ctx);
    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;  /* <= 0 */
    repl->shared->db_ctx = (void *)(uintptr_t)1;
    ik_scrollback_clear(agent->scrollback);
    res = ik_cmd_load(ctx, repl, "myskill");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_ins_count, 0);
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite
 * ================================================================ */

static Suite *commands_skill_extra_suite(void)
{
    Suite *s = suite_create("commands_skill_extra");

    TCase *tc_skills = tcase_create("CmdSkills");
    tcase_add_checked_fixture(tc_skills, suite_setup, NULL);
    tcase_add_test(tc_skills, test_cmd_skills_empty);
    tcase_add_test(tc_skills, test_cmd_skills_small);
    tcase_add_test(tc_skills, test_cmd_skills_large);
    suite_add_tcase(s, tc_skills);

    TCase *tc_unload = tcase_create("CmdUnload");
    tcase_add_checked_fixture(tc_unload, suite_setup, NULL);
    tcase_add_test(tc_unload, test_cmd_unload_null_args);
    tcase_add_test(tc_unload, test_cmd_unload_not_found);
    tcase_add_test(tc_unload, test_cmd_unload_happy_with_db);
    tcase_add_test(tc_unload, test_cmd_unload_insert_fail);
    suite_add_tcase(s, tc_unload);

    TCase *tc_byname = tcase_create("SkillLoadByName");
    tcase_add_checked_fixture(tc_byname, suite_setup, NULL);
    tcase_add_test(tc_byname, test_skill_load_by_name_not_found);
    tcase_add_test(tc_byname, test_skill_load_by_name_happy);
    tcase_add_test(tc_byname, test_skill_load_by_name_with_template);
    suite_add_tcase(s, tc_byname);

    TCase *tc_extra = tcase_create("CmdLoadExtra");
    tcase_add_checked_fixture(tc_extra, suite_setup, NULL);
    tcase_add_test(tc_extra, test_cmd_load_pos_args_with_db);
    tcase_add_test(tc_extra, test_cmd_load_insert_fail);
    tcase_add_test(tc_extra, test_apply_positional_dollar_no_brace);
    tcase_add_test(tc_extra, test_apply_positional_unclosed_brace);
    tcase_add_test(tc_extra, test_apply_positional_no_trailing_text);
    tcase_add_test(tc_extra, test_apply_positional_invalid_vars);
    tcase_add_test(tc_extra, test_apply_positional_non_digit_var);
    tcase_add_test(tc_extra, test_persist_skill_event_no_session);
    suite_add_tcase(s, tc_extra);

    return s;
}

int32_t main(void)
{
    Suite *s = commands_skill_extra_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands_skill_extra_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
