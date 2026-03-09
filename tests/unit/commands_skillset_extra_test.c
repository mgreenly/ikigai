/**
 * @file commands_skillset_extra_test.c
 * @brief Additional unit tests for skillset command: happy paths and DB
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
#include "apps/ikigai/token_cache.h"
#include "shared/error.h"
#include "shared/wrapper_internal.h"
#include "tests/helpers/test_utils_helper.h"

/* ---- mock controls ---- */

static const char *g_doc_content = "{\"preload\":[],\"advertise\":[]}";

res_t ik_doc_cache_get_(void *cache, const char *path, char **out_content)
{
    (void)cache; (void)path;
    *out_content = (char *)(uintptr_t)g_doc_content;
    return OK(NULL);
}

static int g_insert_call_count = 0;
static bool g_insert_fail = false;

res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
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

static int g_skill_load_call_count = 0;
static bool g_skill_load_fail = false;

bool ik_skill_load_by_name_(void *ctx, void *repl, void *agent, const char *skill_name)
{
    (void)ctx; (void)repl; (void)agent; (void)skill_name;
    g_skill_load_call_count++;
    return !g_skill_load_fail;
}

static void reset_mocks(void)
{
    g_doc_content = "{\"preload\":[],\"advertise\":[]}";
    g_insert_call_count = 0;
    g_insert_fail = false;
    g_skill_load_call_count = 0;
    g_skill_load_fail = false;
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
 * ik_cmd_skillset: happy path — empty preload/advertise, no DB
 * ================================================================ */

START_TEST(test_cmd_skillset_happy_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_skillset(ctx, repl, "developer");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->skillset_catalog_count, 0);
    ck_assert_int_eq(g_insert_call_count, 0);
    ck_assert_uint_gt(ik_scrollback_get_line_count(agent->scrollback), 0);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: preload missing (non-array key) → preload_count=0
 * ================================================================ */

START_TEST(test_cmd_skillset_no_preload_key) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_content = "{\"advertise\":[]}";

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;

    res = ik_cmd_skillset(ctx, repl, "minimal");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_skill_load_call_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: preload array with skills — found path
 * ================================================================ */

START_TEST(test_cmd_skillset_preload_found) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_content = "{\"preload\":[\"style\",\"errors\"],\"advertise\":[]}";

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;

    res = ik_cmd_skillset(ctx, repl, "developer");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_skill_load_call_count, 2);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: preload skill not found — warn and continue
 * ================================================================ */

START_TEST(test_cmd_skillset_preload_skill_not_found) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_content = "{\"preload\":[\"missing-skill\"],\"advertise\":[]}";
    g_skill_load_fail = true;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;
    ik_scrollback_clear(agent->scrollback);

    res = ik_cmd_skillset(ctx, repl, "skillset-with-missing");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_skill_load_call_count, 1);
    ck_assert_uint_gt(ik_scrollback_get_line_count(agent->scrollback), 0);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: advertise entries added to catalog
 * ================================================================ */

START_TEST(test_cmd_skillset_advertise_entries) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_content =
        "{\"preload\":[],"
        "\"advertise\":["
        "{\"skill\":\"style\",\"description\":\"Style guide\"},"
        "{\"skill\":\"errors\",\"description\":\"Error patterns\"}"
        "]}";

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;

    res = ik_cmd_skillset(ctx, repl, "developer");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->skillset_catalog_count, 2);
    ck_assert_str_eq(agent->skillset_catalog[0]->skill_name, "style");
    ck_assert_str_eq(agent->skillset_catalog[1]->skill_name, "errors");

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: advertise entry with NULL description
 * ================================================================ */

START_TEST(test_cmd_skillset_advertise_null_desc) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_content =
        "{\"preload\":[],"
        "\"advertise\":["
        "{\"skill\":\"nodesc\"}"
        "]}";

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;

    res = ik_cmd_skillset(ctx, repl, "developer");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->skillset_catalog_count, 1);
    ck_assert_str_eq(agent->skillset_catalog[0]->description, "");

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: advertise is non-array → add_catalog_entries_ returns 0
 * ================================================================ */

START_TEST(test_cmd_skillset_advertise_non_array) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_doc_content = "{\"preload\":[],\"advertise\":\"not-an-array\"}";

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;

    res = ik_cmd_skillset(ctx, repl, "developer");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->skillset_catalog_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: DB persist with valid session_id
 * ================================================================ */

START_TEST(test_cmd_skillset_db_persist) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 42;
    repl->shared->db_ctx = (void *)(uintptr_t)1;

    res = ik_cmd_skillset(ctx, repl, "developer");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_insert_call_count, 1);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: DB persist with insert failure (no crash)
 * ================================================================ */

START_TEST(test_cmd_skillset_db_insert_fail) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_insert_fail = true;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 42;
    repl->shared->db_ctx = (void *)(uintptr_t)1;

    res = ik_cmd_skillset(ctx, repl, "developer");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_insert_call_count, 1);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: token_cache invalidated when present
 * ================================================================ */

START_TEST(test_cmd_skillset_token_cache_invalidated) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);
    agent->token_cache = ik_token_cache_create(ctx, agent);
    ck_assert_ptr_nonnull(agent->token_cache);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 0;

    res = ik_cmd_skillset(ctx, repl, "developer");
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_cmd_skillset: db_ctx NULL → skip persist
 * ================================================================ */

START_TEST(test_cmd_skillset_db_ctx_null) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->doc_cache = make_fake_cache(ctx);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    repl->shared->session_id = 42;
    repl->shared->db_ctx = NULL;

    res = ik_cmd_skillset(ctx, repl, "developer");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_insert_call_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite registration
 * ================================================================ */

static Suite *commands_skillset_extra_suite(void)
{
    Suite *s = suite_create("commands_skillset_extra");

    TCase *tc = tcase_create("cmd_skillset_happy");
    tcase_add_checked_fixture(tc, suite_setup, NULL);
    tcase_add_test(tc, test_cmd_skillset_happy_empty);
    tcase_add_test(tc, test_cmd_skillset_no_preload_key);
    tcase_add_test(tc, test_cmd_skillset_preload_found);
    tcase_add_test(tc, test_cmd_skillset_preload_skill_not_found);
    tcase_add_test(tc, test_cmd_skillset_advertise_entries);
    tcase_add_test(tc, test_cmd_skillset_advertise_null_desc);
    tcase_add_test(tc, test_cmd_skillset_advertise_non_array);
    tcase_add_test(tc, test_cmd_skillset_db_persist);
    tcase_add_test(tc, test_cmd_skillset_db_insert_fail);
    tcase_add_test(tc, test_cmd_skillset_token_cache_invalidated);
    tcase_add_test(tc, test_cmd_skillset_db_ctx_null);
    suite_add_tcase(s, tc);

    return s;
}

int32_t main(void)
{
    Suite *s = commands_skillset_extra_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands_skillset_extra_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
