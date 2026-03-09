/**
 * @file commands_skillset_test.c
 * @brief Unit tests for skillset command handler: catalog entry and error paths
 */

#include <check.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_skill.h"
#include "apps/ikigai/doc_cache.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "shared/error.h"
#include "shared/wrapper_internal.h"
#include "tests/helpers/test_utils_helper.h"

/* ---- mock controls ---- */

static int g_doc_cache_call_count = 0;
static bool g_doc_cache_fail = false;
static bool g_doc_cache_null_content = false;
static const char *g_doc_cache_content = NULL;

res_t ik_doc_cache_get_(void *cache, const char *path, char **out_content)
{
    (void)cache; (void)path;
    g_doc_cache_call_count++;
    if (g_doc_cache_fail) {
        TALLOC_CTX *tmp = talloc_new(NULL);
        return ERR(tmp, IO, "mock doc cache failure");
    }
    if (g_doc_cache_null_content) {
        *out_content = NULL;
        return OK(NULL);
    }
    *out_content = (char *)(uintptr_t)g_doc_cache_content;
    return OK(NULL);
}

res_t ik_db_message_insert_(ik_db_ctx_t *db, int64_t session_id, const char *agent_uuid,
                             const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    return OK(NULL);
}

bool ik_skill_load_by_name_(void *ctx, void *repl, void *agent, const char *skill_name)
{
    (void)ctx; (void)repl; (void)agent; (void)skill_name;
    return true;
}

static void reset_mocks(void)
{
    g_doc_cache_call_count = 0;
    g_doc_cache_fail = false;
    g_doc_cache_null_content = false;
    g_doc_cache_content = "{\"preload\":[],\"advertise\":[]}";
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
    return (ik_doc_cache_t *)talloc_zero(ctx, char);
}

/* ================================================================
 * ik_skillset_store_catalog_entry: direct store
 * ================================================================ */

START_TEST(test_store_catalog_entry_basic) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->skillset_catalog_count, 0);

    ik_skillset_store_catalog_entry(agent, "style", "Code style guide");
    ck_assert_uint_eq(agent->skillset_catalog_count, 1);
    ck_assert_str_eq(agent->skillset_catalog[0]->skill_name, "style");
    ck_assert_str_eq(agent->skillset_catalog[0]->description, "Code style guide");

    talloc_free(ctx);
}
END_TEST

START_TEST(test_store_catalog_entry_null_desc) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_skillset_store_catalog_entry(agent, "memory", NULL);
    ck_assert_uint_eq(agent->skillset_catalog_count, 1);
    ck_assert_str_eq(agent->skillset_catalog[0]->description, "");

    talloc_free(ctx);
}
END_TEST

START_TEST(test_store_catalog_entry_multiple) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_skillset_store_catalog_entry(agent, "style", "Style guide");
    ik_skillset_store_catalog_entry(agent, "errors", "Error patterns");
    ck_assert_uint_eq(agent->skillset_catalog_count, 2);
    ck_assert_str_eq(agent->skillset_catalog[1]->skill_name, "errors");

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: NULL args → usage warning
 * ================================================================ */

START_TEST(test_cmd_skillset_null_args) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_skillset(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_line_count(agent->scrollback), 1);
    ck_assert_int_eq(g_doc_cache_call_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: doc_cache == NULL → warn and return OK
 * ================================================================ */

START_TEST(test_cmd_skillset_no_doc_cache) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_null(agent->doc_cache);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_skillset(ctx, repl, "developer");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_doc_cache_call_count, 0);
    ck_assert_uint_eq(ik_scrollback_get_line_count(agent->scrollback), 1);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: doc cache returns error
 * ================================================================ */

START_TEST(test_cmd_skillset_doc_cache_error) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_cache_fail = true;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_skillset(ctx, repl, "badskillset");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_doc_cache_call_count, 1);
    ck_assert_uint_eq(ik_scrollback_get_line_count(agent->scrollback), 1);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: doc cache returns NULL content
 * ================================================================ */

START_TEST(test_cmd_skillset_null_content) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_cache_null_content = true;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_skillset(ctx, repl, "missing");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_line_count(agent->scrollback), 1);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: malformed JSON
 * ================================================================ */

START_TEST(test_cmd_skillset_malformed_json) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_cache_content = "not-valid-json!!!";

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_skillset(ctx, repl, "badformat");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_line_count(agent->scrollback), 1);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: JSON is array, not object
 * ================================================================ */

START_TEST(test_cmd_skillset_json_not_object) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_cache_content = "[1,2,3]";

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_skillset(ctx, repl, "notobj");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_line_count(agent->scrollback), 1);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite registration
 * ================================================================ */

static Suite *commands_skillset_suite(void)
{
    Suite *s = suite_create("commands_skillset");

    TCase *tc_store = tcase_create("store_catalog_entry");
    tcase_add_checked_fixture(tc_store, suite_setup, NULL);
    tcase_add_test(tc_store, test_store_catalog_entry_basic);
    tcase_add_test(tc_store, test_store_catalog_entry_null_desc);
    tcase_add_test(tc_store, test_store_catalog_entry_multiple);
    suite_add_tcase(s, tc_store);

    TCase *tc_err = tcase_create("cmd_skillset_errors");
    tcase_add_checked_fixture(tc_err, suite_setup, NULL);
    tcase_add_test(tc_err, test_cmd_skillset_null_args);
    tcase_add_test(tc_err, test_cmd_skillset_no_doc_cache);
    tcase_add_test(tc_err, test_cmd_skillset_doc_cache_error);
    tcase_add_test(tc_err, test_cmd_skillset_null_content);
    tcase_add_test(tc_err, test_cmd_skillset_malformed_json);
    tcase_add_test(tc_err, test_cmd_skillset_json_not_object);
    suite_add_tcase(s, tc_err);

    return s;
}

int32_t main(void)
{
    Suite *s = commands_skillset_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands_skillset_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
