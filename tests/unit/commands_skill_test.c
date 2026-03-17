/**
 * @file commands_skill_test.c
 * @brief Unit tests for skill command handlers (/load, /unload, /skills)
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

static int g_doc_cache_call_count = 0;
static bool g_doc_cache_fail = false;
static const char *g_doc_cache_content = "# Skill Content";

res_t ik_doc_cache_get_(void *cache, const char *path, char **out_content)
{
    (void)cache; (void)path;
    g_doc_cache_call_count++;
    if (g_doc_cache_fail) {
        TALLOC_CTX *tmp = talloc_new(NULL);
        return ERR(tmp, IO, "mock doc cache failure");
    }
    *out_content = (char *)(uintptr_t)g_doc_cache_content;
    return OK(NULL);
}

static int g_insert_call_count = 0;
static bool g_insert_fail = false;

res_t ik_db_message_insert_(ik_db_ctx_t *db, int64_t session_id, const char *agent_uuid,
                             const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    g_insert_call_count++;
    if (g_insert_fail) {
        TALLOC_CTX *tmp = talloc_new(NULL);
        return ERR(tmp, IO, "mock insert failure");
    }
    return OK(NULL);
}

static void reset_mocks(void)
{
    g_doc_cache_call_count = 0;
    g_doc_cache_fail = false;
    g_doc_cache_content = "# Skill Content";
    g_insert_call_count = 0;
    g_insert_fail = false;
}

static void suite_setup(void) { ik_test_set_log_dir(__FILE__); }

/* ---- helpers ---- */

static ik_repl_ctx_t *make_repl(TALLOC_CTX *ctx, ik_agent_ctx_t *agent)
{
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = agent;
    repl->shared = agent->shared;
    return repl;
}

static ik_doc_cache_t *make_fake_cache(TALLOC_CTX *ctx)
{
    /* We just need a non-NULL pointer; the wrapper is mocked */
    return (ik_doc_cache_t *)talloc_zero(ctx, char);
}

/* ================================================================
 * ik_skill_store_loaded: replace existing skill
 * ================================================================ */

START_TEST(test_skill_store_loaded_replace) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Store initial */
    ik_skill_store_loaded(agent, "errors", "content-v1");
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->content, "content-v1");

    /* Replace */
    ik_skill_store_loaded(agent, "errors", "content-v2");
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->content, "content-v2");

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_load: NULL args → usage warning
 * ================================================================ */

START_TEST(test_cmd_load_null_args) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_load(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_line_count(agent->scrollback), 1);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_load: doc_cache == NULL → "Skill not found"
 * ================================================================ */

START_TEST(test_cmd_load_null_doc_cache) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_null(agent->doc_cache);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_load(ctx, repl, "myskill");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_doc_cache_call_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_load: doc_cache miss → "Skill not found"
 * ================================================================ */

START_TEST(test_cmd_load_doc_cache_miss) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_cache_fail = true;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_load(ctx, repl, "badskill");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_doc_cache_call_count, 1);
    ck_assert_uint_eq(agent->loaded_skill_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_load: happy path (no template, no DB, no token cache)
 * ================================================================ */

START_TEST(test_cmd_load_happy_no_db) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0; /* skip DB insert */
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_load(ctx, repl, "myskill");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "myskill");
    ck_assert_int_eq(g_insert_call_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_load: happy path with DB insert and token cache invalidation
 * ================================================================ */

START_TEST(test_cmd_load_happy_with_db) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);
    agent->token_cache = ik_token_cache_create(ctx, agent);
    ck_assert_ptr_nonnull(agent->token_cache);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 42;
    repl->shared->db_ctx = (void *)(uintptr_t)1; /* non-NULL sentinel; wrapper is mocked */
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_load(ctx, repl, "myskill");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_int_eq(g_insert_call_count, 1);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_load: with positional args covers apply_positional_args_
 * ================================================================ */

START_TEST(test_cmd_load_with_positional_args) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_cache_content = "Hello ${1}, welcome to ${2}!";

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_load(ctx, repl, "tmpl World Ikigai");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    /* content should have substitutions applied */
    ck_assert_str_eq(agent->loaded_skills[0]->content, "Hello World, welcome to Ikigai!");

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_load: template result used when template_process returns one
 * ================================================================ */

START_TEST(test_cmd_load_with_template_result) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_cache_content = "cost: $$10";  /* $$ → $ after real template processing */

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_load(ctx, repl, "myskill");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->content, "cost: $10");

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite
 * ================================================================ */

static Suite *commands_skill_suite(void)
{
    Suite *s = suite_create("commands_skill");

    TCase *tc_store = tcase_create("StoreLoaded");
    tcase_add_checked_fixture(tc_store, suite_setup, NULL);
    tcase_add_test(tc_store, test_skill_store_loaded_replace);
    suite_add_tcase(s, tc_store);

    TCase *tc_load = tcase_create("CmdLoad");
    tcase_add_checked_fixture(tc_load, suite_setup, NULL);
    tcase_add_test(tc_load, test_cmd_load_null_args);
    tcase_add_test(tc_load, test_cmd_load_null_doc_cache);
    tcase_add_test(tc_load, test_cmd_load_doc_cache_miss);
    tcase_add_test(tc_load, test_cmd_load_happy_no_db);
    tcase_add_test(tc_load, test_cmd_load_happy_with_db);
    tcase_add_test(tc_load, test_cmd_load_with_positional_args);
    tcase_add_test(tc_load, test_cmd_load_with_template_result);
    suite_add_tcase(s, tc_load);

    return s;
}

int32_t main(void)
{
    Suite *s = commands_skill_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands_skill_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
