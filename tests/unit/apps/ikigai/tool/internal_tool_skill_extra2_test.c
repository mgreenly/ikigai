/**
 * @file internal_tool_skill_extra2_test.c
 * @brief Additional coverage tests for apply_positional_args_ edge cases
 *        and unload handler/on_complete branch paths
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

static int mock_skill_store_loaded_calls = 0;
static int mock_token_cache_invalidate_calls = 0;
static const char *mock_skill_content = "plain content";

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
    mock_skill_store_loaded_calls = 0;
    mock_token_cache_invalidate_calls = 0;
    mock_skill_content = "plain content";

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

/* ================================================================
 * apply_positional_args_: edge cases for match_positional_var_
 * ================================================================ */

/* content = "$notvar text" - '$' not followed by '{' (line 62 false via second cond) */
START_TEST(test_load_skill_dollar_no_brace) {
    mock_skill_content = "$notvar text";
    const char *args = "{\"skill\":\"tmpl\",\"args\":[\"x\"]}";
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
}
END_TEST

/* content = "${}" - empty var name (line 37: var_len == 0) */
START_TEST(test_load_skill_empty_var_name) {
    mock_skill_content = "${} text";
    const char *args = "{\"skill\":\"tmpl\",\"args\":[\"x\"]}";
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
}
END_TEST

/* content = "${abc}" - non-digit var name (line 39: !isdigit, line 67: v==NULL) */
START_TEST(test_load_skill_nondigit_var) {
    mock_skill_content = "${abc} text";
    const char *args = "{\"skill\":\"tmpl\",\"args\":[\"x\"]}";
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
}
END_TEST

/* content = "${" (unclosed) - end == NULL (line 64 false) */
START_TEST(test_load_skill_unclosed_brace) {
    mock_skill_content = "${unclosed text";
    const char *args = "{\"skill\":\"tmpl\",\"args\":[\"x\"]}";
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
}
END_TEST

/* content = "${99}" with 1 arg - idx > pos_arg_count (line 45 false) */
START_TEST(test_load_skill_idx_too_large) {
    mock_skill_content = "${99} text";
    const char *args = "{\"skill\":\"tmpl\",\"args\":[\"x\"]}";
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
}
END_TEST

/* content = "${0}" - idx < 1 (line 45 idx<1 branch) */
START_TEST(test_load_skill_idx_zero) {
    mock_skill_content = "${0} text";
    const char *args = "{\"skill\":\"tmpl\",\"args\":[\"x\"]}";
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
}
END_TEST

/* content = "${1234567890}" - var_len >= 10 (line 37 var_len>=10 branch) */
START_TEST(test_load_skill_long_var_name) {
    mock_skill_content = "${1234567890} text";
    const char *args = "{\"skill\":\"tmpl\",\"args\":[\"x\"]}";
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
}
END_TEST

/* ================================================================
 * load_skill handler: args edge cases (lines 133, 135, 142)
 * ================================================================ */

/* args is a string, not an array - line 133 false */
START_TEST(test_load_skill_args_not_array) {
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent,
                                                       "{\"skill\":\"db\","
                                                       "\"args\":\"notarray\"}");
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
}
END_TEST

/* args is empty array - pos_arg_count == 0, line 135 false */
START_TEST(test_load_skill_empty_args_array) {
    char *result = ik_internal_tool_load_skill_handler(test_ctx, agent,
                                                       "{\"skill\":\"db\","
                                                       "\"args\":[]}");
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"loaded\"") != NULL);
}
END_TEST

/* ================================================================
 * unload_skill handler: skill found at non-first index (line 284 false)
 * ================================================================ */

START_TEST(test_unload_skill_second_skill) {
    add_loaded_skill(agent, "database");
    add_loaded_skill(agent, "style");
    char *result = ik_internal_tool_unload_skill_handler(test_ctx, agent,
                                                         "{\"skill\":\"style\"}");
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "\"status\":\"unloaded\"") != NULL);
}
END_TEST

/* ================================================================
 * unload on_complete: loop finds second skill (lines 322-323 coverage)
 * ================================================================ */

START_TEST(test_unload_on_complete_second_skill) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->shared = shared;
    add_loaded_skill(agent, "database");
    add_loaded_skill(agent, "style");
    ik_internal_tool_unload_skill_handler(test_ctx, agent,
                                          "{\"skill\":\"style\"}");
    ck_assert_ptr_nonnull(agent->tool_deferred_data);
    ik_internal_tool_unload_skill_on_complete(repl, agent);
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "database");
    ck_assert_ptr_null(agent->tool_deferred_data);
}
END_TEST

/* ================================================================
 * unload on_complete: skill not in loaded list (lines 322, 328 false)
 * ================================================================ */

START_TEST(test_unload_on_complete_skill_not_found) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->shared = shared;
    /* manually set deferred_data to a skill name not in loaded_skills */
    agent->tool_deferred_data = talloc_strdup(agent, "nonexistent");
    ik_internal_tool_unload_skill_on_complete(repl, agent);
    /* loaded_skill_count unchanged (0) */
    ck_assert_uint_eq(agent->loaded_skill_count, 0);
    ck_assert_ptr_null(agent->tool_deferred_data);
}
END_TEST

/* ================================================================
 * unload on_complete: session_id == 0 (line 336 false via second cond)
 * ================================================================ */

START_TEST(test_unload_on_complete_session_zero) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->shared = shared;
    shared->session_id = 0;
    add_loaded_skill(agent, "database");
    ik_internal_tool_unload_skill_handler(test_ctx, agent,
                                          "{\"skill\":\"database\"}");
    ik_internal_tool_unload_skill_on_complete(repl, agent);
    ck_assert_uint_eq(agent->loaded_skill_count, 0);
    ck_assert_ptr_null(agent->tool_deferred_data);
}
END_TEST

static Suite *internal_tool_skill_extra2_suite(void)
{
    Suite *s = suite_create("InternalToolSkillExtra2");

    TCase *tc = tcase_create("SkillExtra2");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_load_skill_dollar_no_brace);
    tcase_add_test(tc, test_load_skill_empty_var_name);
    tcase_add_test(tc, test_load_skill_nondigit_var);
    tcase_add_test(tc, test_load_skill_unclosed_brace);
    tcase_add_test(tc, test_load_skill_idx_too_large);
    tcase_add_test(tc, test_load_skill_idx_zero);
    tcase_add_test(tc, test_load_skill_long_var_name);
    tcase_add_test(tc, test_load_skill_args_not_array);
    tcase_add_test(tc, test_load_skill_empty_args_array);
    tcase_add_test(tc, test_unload_skill_second_skill);
    tcase_add_test(tc, test_unload_on_complete_second_skill);
    tcase_add_test(tc, test_unload_on_complete_skill_not_found);
    tcase_add_test(tc, test_unload_on_complete_session_zero);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = internal_tool_skill_extra2_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/tool/"
        "internal_tool_skill_extra2_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
