/**
 * @file internal_tool_skill_extra_test.c
 * @brief Additional coverage tests for load_skill, unload_skill, list_skills
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
static bool mock_template_return_result = false;
static int mock_skill_store_loaded_calls = 0;
static int mock_token_cache_invalidate_calls = 0;

static const char *mock_skill_content = "${1} world";

/* ================================================================
 * Mocks
 * ================================================================ */

yyjson_doc *yyjson_read_(const char *dat, size_t len, yyjson_read_flag flg)
{
    return yyjson_read(dat, len, flg);
}

res_t ik_doc_cache_get(ik_doc_cache_t *cache, const char *path,
                       char **out_content)
{
    (void)path;
    if (mock_doc_cache_fail) {
        return ERR(cache, IO, "Mock doc cache failure");
    }
    if (mock_skill_content == NULL) {
        *out_content = NULL;
        return OK(NULL);
    }
    *out_content = talloc_strdup((TALLOC_CTX *)cache, mock_skill_content);
    return OK(NULL);
}

res_t ik_template_process(TALLOC_CTX *ctx, const char *text,
                          ik_agent_ctx_t *agent, ik_config_t *config,
                          ik_template_result_t **out)
{
    (void)text; (void)agent; (void)config;
    if (mock_template_return_result) {
        ik_template_result_t *r = talloc_zero(ctx, ik_template_result_t);
        r->processed = talloc_strdup(r, "processed content");
        *out = r;
    } else {
        *out = NULL;
    }
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
    mock_template_return_result = false;
    mock_skill_store_loaded_calls = 0;
    mock_token_cache_invalidate_calls = 0;
    mock_skill_content = "${1} world";

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

static void add_loaded_skill(ik_agent_ctx_t *a, const char *name)
{
    ik_loaded_skill_t **ns = talloc_realloc(a, a->loaded_skills,
                                             ik_loaded_skill_t *,
                                             (unsigned int)(a->loaded_skill_count + 1));
    a->loaded_skills = ns;
    ik_loaded_skill_t *s = talloc_zero(a, ik_loaded_skill_t);
    s->name = talloc_strdup(s, name);
    s->content = talloc_strdup(s, "content");
    a->loaded_skills[a->loaded_skill_count] = s;
    a->loaded_skill_count++;
}

static void add_catalog_entry(ik_agent_ctx_t *a, const char *skill,
                              const char *desc)
{
    ik_skillset_catalog_entry_t **ns = talloc_realloc(
        a, a->skillset_catalog, ik_skillset_catalog_entry_t *,
        (unsigned int)(a->skillset_catalog_count + 1));
    a->skillset_catalog = ns;
    ik_skillset_catalog_entry_t *e = talloc_zero(a, ik_skillset_catalog_entry_t);
    e->skill_name = talloc_strdup(e, skill);
    e->description = talloc_strdup(e, desc);
    a->skillset_catalog[a->skillset_catalog_count] = e;
    a->skillset_catalog_count++;
}

/* ================================================================
 * apply_positional_args_ coverage: ${1} substitution
 * ================================================================ */

/* skill content = "${1} world", args = ["Hello"] -> "Hello world" */
START_TEST(test_load_skill_substitution_with_trailing) {
    mock_skill_content = "${1} world";
    const char *args = "{\"skill\":\"tmpl\",\"args\":[\"Hello\"]}";
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
    ck_assert_ptr_nonnull(agent->tool_deferred_data);
}
END_TEST

/* skill content = "${1}", args = ["only"] -> "only" (no trailing text) */
START_TEST(test_load_skill_exact_substitution) {
    mock_skill_content = "${1}";
    const char *args = "{\"skill\":\"tmpl\",\"args\":[\"only\"]}";
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
}
END_TEST

/* ================================================================
 * on_complete with pos_args (covers line 226)
 * ================================================================ */

START_TEST(test_load_skill_on_complete_with_pos_args) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->shared = shared;
    mock_skill_content = "${1} world";
    const char *args = "{\"skill\":\"tmpl\",\"args\":[\"Hello\"]}";
    ik_internal_tool_load_skill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(agent->tool_deferred_data);
    ik_internal_tool_load_skill_on_complete(repl, agent);
    ck_assert_int_eq(mock_skill_store_loaded_calls, 1);
    ck_assert_ptr_null(agent->tool_deferred_data);
}
END_TEST

/* ================================================================
 * load_skill: shared=NULL covers line 168 null cfg path
 * ================================================================ */

START_TEST(test_load_skill_no_shared) {
    agent->shared = NULL;
    const char *args = "{\"skill\":\"db\"}";
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
}
END_TEST

/* ================================================================
 * load_skill: template returns non-null result (covers lines 172, 189)
 * ================================================================ */

START_TEST(test_load_skill_template_result) {
    mock_template_return_result = true;
    const char *args = "{\"skill\":\"db\"}";
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
}
END_TEST

/* ================================================================
 * load_skill: handler with skill_val not string (covers line 122)
 * ================================================================ */

START_TEST(test_load_skill_skill_not_string) {
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent,
                                                       "{\"skill\":42}");
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "INVALID_ARG") != NULL);
}
END_TEST

/* ================================================================
 * on_complete: db_ctx=NULL (covers line 218 false branch)
 * ================================================================ */

START_TEST(test_load_skill_on_complete_no_db) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->shared = shared;
    shared->db_ctx = NULL;
    ik_internal_tool_load_skill_handler(test_ctx, agent, "{\"skill\":\"db\"}");
    ik_internal_tool_load_skill_on_complete(repl, agent);
    ck_assert_int_eq(mock_skill_store_loaded_calls, 1);
    ck_assert_ptr_null(agent->tool_deferred_data);
}
END_TEST

/* ================================================================
 * on_complete: session_id=0 (covers line 218 false branch)
 * ================================================================ */

START_TEST(test_load_skill_on_complete_session_zero) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->shared = shared;
    shared->session_id = 0;
    ik_internal_tool_load_skill_handler(test_ctx, agent, "{\"skill\":\"db\"}");
    ik_internal_tool_load_skill_on_complete(repl, agent);
    ck_assert_int_eq(mock_skill_store_loaded_calls, 1);
    ck_assert_ptr_null(agent->tool_deferred_data);
}
END_TEST

/* ================================================================
 * unload_skill: skill_val not string (covers line 273)
 * ================================================================ */

START_TEST(test_unload_skill_not_string) {
    add_loaded_skill(agent, "database");
    char *result = ik_internal_tool_unload_skill_handler(test_ctx, agent,
                                                         "{\"skill\":42}");
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "INVALID_ARG") != NULL);
}
END_TEST

/* ================================================================
 * unload on_complete: no token_cache (covers line 359 false branch)
 * ================================================================ */

START_TEST(test_unload_skill_on_complete_no_token_cache) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->shared = shared;
    agent->token_cache = NULL;
    add_loaded_skill(agent, "database");
    ik_internal_tool_unload_skill_handler(test_ctx, agent,
                                          "{\"skill\":\"database\"}");
    ik_internal_tool_unload_skill_on_complete(repl, agent);
    ck_assert_int_eq(mock_token_cache_invalidate_calls, 0);
    ck_assert_ptr_null(agent->tool_deferred_data);
    ck_assert_uint_eq(agent->loaded_skill_count, 0);
}
END_TEST

/* ================================================================
 * unload on_complete: no db_ctx (covers line 336 false branch)
 * ================================================================ */

START_TEST(test_unload_skill_on_complete_no_db) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->shared = shared;
    shared->db_ctx = NULL;
    add_loaded_skill(agent, "database");
    ik_internal_tool_unload_skill_handler(test_ctx, agent,
                                          "{\"skill\":\"database\"}");
    ik_internal_tool_unload_skill_on_complete(repl, agent);
    ck_assert_ptr_null(agent->tool_deferred_data);
    ck_assert_uint_eq(agent->loaded_skill_count, 0);
}
END_TEST

/* ================================================================
 * list_skills: with catalog entries (covers lines 394-397)
 * ================================================================ */

START_TEST(test_list_skills_with_catalog) {
    add_loaded_skill(agent, "database");
    add_catalog_entry(agent, "style", "Code style guide");
    add_catalog_entry(agent, "naming", "Naming conventions");
    char *result = ik_internal_tool_list_skills_handler(test_ctx, agent, "{}");
    ck_assert_ptr_nonnull(result);
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_get_bool(yyjson_obj_get(root, "tool_success")));
    yyjson_val *rv = yyjson_obj_get(root, "result");
    ck_assert_uint_eq(yyjson_arr_size(yyjson_obj_get(rv, "loaded")), 1);
    ck_assert_uint_eq(yyjson_arr_size(yyjson_obj_get(rv, "catalog")), 2);
    yyjson_doc_free(doc);
}
END_TEST

static Suite *internal_tool_skill_extra_suite(void)
{
    Suite *s = suite_create("InternalToolSkillExtra");

    TCase *tc = tcase_create("SkillExtra");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_load_skill_substitution_with_trailing);
    tcase_add_test(tc, test_load_skill_exact_substitution);
    tcase_add_test(tc, test_load_skill_on_complete_with_pos_args);
    tcase_add_test(tc, test_load_skill_no_shared);
    tcase_add_test(tc, test_load_skill_template_result);
    tcase_add_test(tc, test_load_skill_skill_not_string);
    tcase_add_test(tc, test_load_skill_on_complete_no_db);
    tcase_add_test(tc, test_load_skill_on_complete_session_zero);
    tcase_add_test(tc, test_unload_skill_not_string);
    tcase_add_test(tc, test_unload_skill_on_complete_no_token_cache);
    tcase_add_test(tc, test_unload_skill_on_complete_no_db);
    tcase_add_test(tc, test_list_skills_with_catalog);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = internal_tool_skill_extra_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/tool/"
        "internal_tool_skill_extra_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
