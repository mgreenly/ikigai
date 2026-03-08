/**
 * @file internal_tool_load_skillset_test.c
 * @brief Unit tests for load_skillset handler and on_complete hook
 */

#include "tests/test_constants.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_skill.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/doc_cache.h"
#include "apps/ikigai/internal_tool_skill.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/template.h"
#include "apps/ikigai/token_cache.h"
#include "shared/error.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/* ================================================================
 * Mock control flags
 * ================================================================ */

static bool mock_doc_cache_fail = false;
static bool mock_yyjson_read_fail = false;
static int mock_skill_store_loaded_calls = 0;
static int mock_catalog_store_calls = 0;
static int mock_token_cache_invalidate_calls = 0;

static const char *mock_skillset_json = NULL;
static const char *mock_skill_content = "# Skill\nContent.";

/* ================================================================
 * Mocks
 * ================================================================ */

yyjson_doc *yyjson_read_(const char *dat, size_t len, yyjson_read_flag flg)
{
    if (mock_yyjson_read_fail) return NULL;
    return yyjson_read(dat, len, flg);
}

res_t ik_doc_cache_get(ik_doc_cache_t *cache, const char *path,
                       char **out_content)
{
    (void)path;
    if (mock_doc_cache_fail) {
        return ERR(cache, IO, "Mock doc cache failure");
    }
    if (strstr(path, "skillsets/") != NULL && mock_skillset_json != NULL) {
        *out_content = talloc_strdup((TALLOC_CTX *)cache, mock_skillset_json);
    } else {
        *out_content = talloc_strdup((TALLOC_CTX *)cache, mock_skill_content);
    }
    return OK(NULL);
}

res_t ik_template_process(TALLOC_CTX *ctx, const char *text,
                          ik_agent_ctx_t *agent, ik_config_t *config,
                          ik_template_result_t **out)
{
    (void)ctx; (void)text; (void)agent; (void)config;
    *out = NULL;
    return OK(NULL);
}

res_t ik_db_message_insert(ik_db_ctx_t *db, int64_t session_id,
                           const char *agent_uuid, const char *kind,
                           const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    return OK(NULL);
}

void ik_skill_store_loaded(ik_agent_ctx_t *agent, const char *skill_name,
                           const char *content)
{
    (void)agent; (void)skill_name; (void)content;
    mock_skill_store_loaded_calls++;
}

void ik_skillset_store_catalog_entry(ik_agent_ctx_t *agent,
                                     const char *skill_name,
                                     const char *description)
{
    (void)agent; (void)skill_name; (void)description;
    mock_catalog_store_calls++;
}

void ik_token_cache_invalidate_system(ik_token_cache_t *cache)
{
    (void)cache;
    mock_token_cache_invalidate_calls++;
}

/* ================================================================
 * Test fixture
 * ================================================================ */

static TALLOC_CTX *test_ctx;
static ik_agent_ctx_t *agent;
static ik_shared_ctx_t *shared;
static ik_db_ctx_t *db_ctx;
static ik_doc_cache_t *doc_cache;
static ik_token_cache_t *token_cache;

static void setup(void)
{
    mock_doc_cache_fail = false;
    mock_yyjson_read_fail = false;
    mock_skill_store_loaded_calls = 0;
    mock_catalog_store_calls = 0;
    mock_token_cache_invalidate_calls = 0;
    mock_skillset_json = NULL;

    test_ctx = talloc_new(NULL);
    shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared->session_id = 1;
    db_ctx = talloc_zero(test_ctx, ik_db_ctx_t);
    shared->db_ctx = db_ctx;
    doc_cache = (ik_doc_cache_t *)talloc_zero(test_ctx, char);
    token_cache = (ik_token_cache_t *)talloc_zero(test_ctx, char);

    agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared;
    agent->worker_db_ctx = db_ctx;
    agent->uuid = talloc_strdup(agent, "test-agent-uuid");
    agent->doc_cache = doc_cache;
    agent->token_cache = token_cache;
    agent->tool_thread_ctx = talloc_new(agent);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * load_skillset handler tests
 * ================================================================ */

START_TEST(test_load_skillset_success) {
    mock_skillset_json =
        "{\"preload\":[\"style\"],"
        "\"advertise\":[{\"skill\":\"database\",\"description\":\"DB\"}]}";
    char *result = ik_internal_tool_load_skillset_handler(
        test_ctx, agent, "{\"skillset\":\"developer\"}");
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_get_bool(yyjson_obj_get(root, "tool_success")));
    ck_assert_ptr_nonnull(agent->tool_deferred_data);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_load_skillset_missing_param) {
    char *result = ik_internal_tool_load_skillset_handler(test_ctx, agent,
                                                          "{}");
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "INVALID_ARG") != NULL);
}
END_TEST

START_TEST(test_load_skillset_invalid_json) {
    mock_yyjson_read_fail = true;
    char *result = ik_internal_tool_load_skillset_handler(test_ctx, agent,
                                                          "{bad}");
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "PARSE_ERROR") != NULL);
}
END_TEST

START_TEST(test_load_skillset_not_found) {
    mock_doc_cache_fail = true;
    char *result = ik_internal_tool_load_skillset_handler(
        test_ctx, agent, "{\"skillset\":\"nope\"}");
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "SKILLSET_NOT_FOUND") != NULL);
}
END_TEST

START_TEST(test_load_skillset_no_doc_cache) {
    agent->doc_cache = NULL;
    char *result = ik_internal_tool_load_skillset_handler(
        test_ctx, agent, "{\"skillset\":\"developer\"}");
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "SKILLSET_NOT_FOUND") != NULL);
}
END_TEST

START_TEST(test_load_skillset_malformed_json) {
    mock_skillset_json = "not-json";
    char *result = ik_internal_tool_load_skillset_handler(
        test_ctx, agent, "{\"skillset\":\"bad\"}");
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "SKILLSET_MALFORMED") != NULL);
}
END_TEST

/* ================================================================
 * load_skillset on_complete tests
 * ================================================================ */

START_TEST(test_load_skillset_on_complete_stores_all) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->shared = shared;

    mock_skillset_json =
        "{\"preload\":[\"style\",\"errors\"],"
        "\"advertise\":[{\"skill\":\"database\",\"description\":\"DB\"}]}";
    ik_internal_tool_load_skillset_handler(test_ctx, agent,
                                           "{\"skillset\":\"developer\"}");
    ck_assert_ptr_nonnull(agent->tool_deferred_data);

    ik_internal_tool_load_skillset_on_complete(repl, agent);

    ck_assert_int_eq(mock_skill_store_loaded_calls, 2);
    ck_assert_int_eq(mock_catalog_store_calls, 1);
    ck_assert_int_eq(mock_token_cache_invalidate_calls, 1);
    ck_assert_ptr_null(agent->tool_deferred_data);
}
END_TEST

START_TEST(test_load_skillset_on_complete_null_data) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->shared = shared;
    agent->tool_deferred_data = NULL;
    ik_internal_tool_load_skillset_on_complete(repl, agent);
    ck_assert_int_eq(mock_skill_store_loaded_calls, 0);
    ck_assert_int_eq(mock_catalog_store_calls, 0);
}
END_TEST

START_TEST(test_load_skillset_empty_preload) {
    mock_skillset_json = "{\"preload\":[],\"advertise\":[]}";
    char *result = ik_internal_tool_load_skillset_handler(
        test_ctx, agent, "{\"skillset\":\"empty\"}");
    ck_assert_ptr_nonnull(result);
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_get_bool(yyjson_obj_get(root, "tool_success")));
    yyjson_doc_free(doc);
}
END_TEST

static Suite *internal_tool_load_skillset_suite(void)
{
    Suite *s = suite_create("InternalToolLoadSkillset");

    TCase *tc = tcase_create("LoadSkillset");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_load_skillset_success);
    tcase_add_test(tc, test_load_skillset_missing_param);
    tcase_add_test(tc, test_load_skillset_invalid_json);
    tcase_add_test(tc, test_load_skillset_not_found);
    tcase_add_test(tc, test_load_skillset_no_doc_cache);
    tcase_add_test(tc, test_load_skillset_malformed_json);
    tcase_add_test(tc, test_load_skillset_on_complete_stores_all);
    tcase_add_test(tc, test_load_skillset_on_complete_null_data);
    tcase_add_test(tc, test_load_skillset_empty_preload);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = internal_tool_load_skillset_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/tool/internal_tool_load_skillset_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
