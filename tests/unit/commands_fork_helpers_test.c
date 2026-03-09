/**
 * @file commands_fork_helpers_test.c
 * @brief Unit tests for fork command helper functions
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_fork_helpers.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "shared/wrapper_internal.h"
#include "tests/helpers/test_utils_helper.h"

/* ---- mock for ik_db_message_insert_ ---- */

static int g_insert_call_count = 0;
static bool g_insert_fail = false;

res_t ik_db_message_insert_(void *db,
                             int64_t session_id,
                             const char *agent_uuid,
                             const char *kind,
                             const char *content,
                             const char *data_json)
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
    g_insert_call_count = 0;
    g_insert_fail = false;
}

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

/* ================================================================
 * ik_commands_fork_copy_loaded_skills: empty parent — no-op
 * ================================================================ */

START_TEST(test_copy_loaded_skills_empty_parent) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *parent = NULL;
    res_t res = ik_test_create_agent(ctx, &parent);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = NULL;
    res = ik_test_create_agent(ctx, &child);
    ck_assert(is_ok(&res));

    /* parent has no loaded skills */
    ck_assert_uint_eq(parent->loaded_skill_count, 0);

    ik_commands_fork_copy_loaded_skills(child, parent);

    ck_assert_uint_eq(child->loaded_skill_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_commands_fork_copy_loaded_skills: non-empty parent — copies skills
 * ================================================================ */

START_TEST(test_copy_loaded_skills_with_skills) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *parent = NULL;
    res_t res = ik_test_create_agent(ctx, &parent);
    ck_assert(is_ok(&res));

    /* Add two loaded skills to parent */
    parent->loaded_skills = talloc_array(parent, ik_loaded_skill_t *, 2);
    ck_assert_ptr_nonnull(parent->loaded_skills);
    parent->loaded_skills[0] = talloc_zero(parent, ik_loaded_skill_t);
    parent->loaded_skills[0]->name = talloc_strdup(parent, "skill-a");
    parent->loaded_skills[0]->content = talloc_strdup(parent, "content-a");
    parent->loaded_skills[0]->load_position = 5;
    parent->loaded_skills[1] = talloc_zero(parent, ik_loaded_skill_t);
    parent->loaded_skills[1]->name = talloc_strdup(parent, "skill-b");
    parent->loaded_skills[1]->content = talloc_strdup(parent, "content-b");
    parent->loaded_skills[1]->load_position = 10;
    parent->loaded_skill_count = 2;

    ik_agent_ctx_t *child = NULL;
    res = ik_test_create_agent(ctx, &child);
    ck_assert(is_ok(&res));

    ik_commands_fork_copy_loaded_skills(child, parent);

    ck_assert_uint_eq(child->loaded_skill_count, 2);
    ck_assert_str_eq(child->loaded_skills[0]->name, "skill-a");
    ck_assert_str_eq(child->loaded_skills[0]->content, "content-a");
    ck_assert_uint_eq(child->loaded_skills[0]->load_position, 0);  /* reset */
    ck_assert_str_eq(child->loaded_skills[1]->name, "skill-b");
    ck_assert_str_eq(child->loaded_skills[1]->content, "content-b");
    ck_assert_uint_eq(child->loaded_skills[1]->load_position, 0);  /* reset */

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_commands_fork_copy_skillset_catalog: empty parent — no-op
 * ================================================================ */

START_TEST(test_copy_skillset_catalog_empty_parent) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *parent = NULL;
    res_t res = ik_test_create_agent(ctx, &parent);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = NULL;
    res = ik_test_create_agent(ctx, &child);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(parent->skillset_catalog_count, 0);

    ik_commands_fork_copy_skillset_catalog(child, parent);

    ck_assert_uint_eq(child->skillset_catalog_count, 0);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_commands_fork_copy_skillset_catalog: non-empty parent — copies catalog
 * ================================================================ */

START_TEST(test_copy_skillset_catalog_with_entries) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *parent = NULL;
    res_t res = ik_test_create_agent(ctx, &parent);
    ck_assert(is_ok(&res));

    /* Add one catalog entry to parent */
    parent->skillset_catalog = talloc_array(parent, ik_skillset_catalog_entry_t *, 1);
    ck_assert_ptr_nonnull(parent->skillset_catalog);
    parent->skillset_catalog[0] = talloc_zero(parent, ik_skillset_catalog_entry_t);
    parent->skillset_catalog[0]->skill_name = talloc_strdup(parent, "database");
    parent->skillset_catalog[0]->description = talloc_strdup(parent, "PostgreSQL schema");
    parent->skillset_catalog[0]->load_position = 3;
    parent->skillset_catalog_count = 1;

    ik_agent_ctx_t *child = NULL;
    res = ik_test_create_agent(ctx, &child);
    ck_assert(is_ok(&res));

    ik_commands_fork_copy_skillset_catalog(child, parent);

    ck_assert_uint_eq(child->skillset_catalog_count, 1);
    ck_assert_str_eq(child->skillset_catalog[0]->skill_name, "database");
    ck_assert_str_eq(child->skillset_catalog[0]->description, "PostgreSQL schema");
    ck_assert_uint_eq(child->skillset_catalog[0]->load_position, 0);  /* reset */

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_commands_thinking_level_to_string: all enum values
 * ================================================================ */

START_TEST(test_thinking_level_to_string) {
    ck_assert_str_eq(ik_commands_thinking_level_to_string(IK_THINKING_MIN), "min");
    ck_assert_str_eq(ik_commands_thinking_level_to_string(IK_THINKING_LOW), "low");
    ck_assert_str_eq(ik_commands_thinking_level_to_string(IK_THINKING_MED), "medium");
    ck_assert_str_eq(ik_commands_thinking_level_to_string(IK_THINKING_HIGH), "high");
    /* Unknown value hits default */
    ck_assert_str_eq(ik_commands_thinking_level_to_string((ik_thinking_level_t)99), "unknown");
}
END_TEST

/* ================================================================
 * ik_commands_build_fork_feedback: basic output format
 * ================================================================ */

START_TEST(test_build_fork_feedback) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *child = NULL;
    res_t res = ik_test_create_agent(ctx, &child);
    ck_assert(is_ok(&res));

    /* Set required fields */
    child->uuid     = talloc_strdup(child, "test-uuid-1234");
    child->provider = talloc_strdup(child, "anthropic");
    child->model    = talloc_strdup(child, "claude-3");
    child->thinking_level = IK_THINKING_MIN;

    char *msg = ik_commands_build_fork_feedback(ctx, child, false);
    ck_assert_ptr_nonnull(msg);
    ck_assert_ptr_nonnull(strstr(msg, "test-uuid-1234"));
    ck_assert_ptr_nonnull(strstr(msg, "anthropic"));
    ck_assert_ptr_nonnull(strstr(msg, "claude-3"));
    ck_assert_ptr_nonnull(strstr(msg, "min"));

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_commands_insert_fork_events: session_id=0 → early return
 * ================================================================ */

START_TEST(test_insert_fork_events_no_session) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *parent = NULL;
    res_t res = ik_test_create_agent(ctx, &parent);
    ck_assert(is_ok(&res));
    parent->uuid = talloc_strdup(parent, "parent-uuid");

    ik_agent_ctx_t *child = NULL;
    res = ik_test_create_agent(ctx, &child);
    ck_assert(is_ok(&res));
    child->uuid     = talloc_strdup(child, "child-uuid");
    child->provider = talloc_strdup(child, "openai");
    child->model    = talloc_strdup(child, "gpt-4");
    child->thinking_level = IK_THINKING_MIN;

    /* Build minimal repl with session_id=0 */
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->shared = parent->shared;
    repl->shared->session_id = 0;

    res = ik_commands_insert_fork_events(ctx, repl, parent, child, 0);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_insert_call_count, 0);  /* early return, no inserts */

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_commands_insert_fork_events: with loaded skills covers loop body
 * ================================================================ */

START_TEST(test_insert_fork_events_with_loaded_skills) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *parent = NULL;
    res_t res = ik_test_create_agent(ctx, &parent);
    ck_assert(is_ok(&res));
    parent->uuid = talloc_strdup(parent, "parent-uuid");

    ik_agent_ctx_t *child = NULL;
    res = ik_test_create_agent(ctx, &child);
    ck_assert(is_ok(&res));
    child->uuid     = talloc_strdup(child, "child-uuid");
    child->provider = talloc_strdup(child, "anthropic");
    child->model    = talloc_strdup(child, "claude-3");
    child->thinking_level = IK_THINKING_LOW;

    /* Give child a loaded skill */
    child->loaded_skills = talloc_array(child, ik_loaded_skill_t *, 1);
    ck_assert_ptr_nonnull(child->loaded_skills);
    child->loaded_skills[0] = talloc_zero(child, ik_loaded_skill_t);
    child->loaded_skills[0]->name    = talloc_strdup(child, "errors");
    child->loaded_skills[0]->content = talloc_strdup(child, "error handling docs");
    child->loaded_skill_count = 1;

    /* Build minimal repl with session_id=1 */
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->shared = parent->shared;
    repl->shared->session_id = 1;
    repl->shared->db_ctx = NULL;  /* mocked — not used */

    res = ik_commands_insert_fork_events(ctx, repl, parent, child, 42);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_insert_call_count, 2);  /* parent + child inserts */

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_commands_insert_fork_events: with skillset catalog covers loop body
 * ================================================================ */

START_TEST(test_insert_fork_events_with_skillset_catalog) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *parent = NULL;
    res_t res = ik_test_create_agent(ctx, &parent);
    ck_assert(is_ok(&res));
    parent->uuid = talloc_strdup(parent, "parent-uuid");

    ik_agent_ctx_t *child = NULL;
    res = ik_test_create_agent(ctx, &child);
    ck_assert(is_ok(&res));
    child->uuid     = talloc_strdup(child, "child-uuid");
    child->provider = talloc_strdup(child, "google");
    child->model    = talloc_strdup(child, "gemini-pro");
    child->thinking_level = IK_THINKING_HIGH;

    /* Give child a skillset catalog entry */
    child->skillset_catalog = talloc_array(child, ik_skillset_catalog_entry_t *, 1);
    ck_assert_ptr_nonnull(child->skillset_catalog);
    child->skillset_catalog[0] = talloc_zero(child, ik_skillset_catalog_entry_t);
    child->skillset_catalog[0]->skill_name  = talloc_strdup(child, "database");
    child->skillset_catalog[0]->description = talloc_strdup(child, "PostgreSQL schema");
    child->skillset_catalog_count = 1;

    /* Build minimal repl with session_id=1 */
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->shared = parent->shared;
    repl->shared->session_id = 1;
    repl->shared->db_ctx = NULL;  /* mocked — not used */

    res = ik_commands_insert_fork_events(ctx, repl, parent, child, 7);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(g_insert_call_count, 2);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * ik_commands_insert_fork_events: insert failure propagates error
 * ================================================================ */

START_TEST(test_insert_fork_events_db_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_insert_fail = true;

    ik_agent_ctx_t *parent = NULL;
    res_t res = ik_test_create_agent(ctx, &parent);
    ck_assert(is_ok(&res));
    parent->uuid = talloc_strdup(parent, "parent-uuid");

    ik_agent_ctx_t *child = NULL;
    res = ik_test_create_agent(ctx, &child);
    ck_assert(is_ok(&res));
    child->uuid     = talloc_strdup(child, "child-uuid");
    child->provider = talloc_strdup(child, "anthropic");
    child->model    = talloc_strdup(child, "claude-3");
    child->thinking_level = IK_THINKING_MIN;

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->shared = parent->shared;
    repl->shared->session_id = 1;
    repl->shared->db_ctx = NULL;

    res = ik_commands_insert_fork_events(ctx, repl, parent, child, 0);
    ck_assert(!is_ok(&res));

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite
 * ================================================================ */

static Suite *commands_fork_helpers_suite(void)
{
    Suite *s = suite_create("commands_fork_helpers");

    TCase *tc_copy_skills = tcase_create("CopyLoadedSkills");
    tcase_add_checked_fixture(tc_copy_skills, suite_setup, NULL);
    tcase_add_test(tc_copy_skills, test_copy_loaded_skills_empty_parent);
    tcase_add_test(tc_copy_skills, test_copy_loaded_skills_with_skills);
    suite_add_tcase(s, tc_copy_skills);

    TCase *tc_copy_catalog = tcase_create("CopySkillsetCatalog");
    tcase_add_checked_fixture(tc_copy_catalog, suite_setup, NULL);
    tcase_add_test(tc_copy_catalog, test_copy_skillset_catalog_empty_parent);
    tcase_add_test(tc_copy_catalog, test_copy_skillset_catalog_with_entries);
    suite_add_tcase(s, tc_copy_catalog);

    TCase *tc_helpers = tcase_create("Helpers");
    tcase_add_checked_fixture(tc_helpers, suite_setup, NULL);
    tcase_add_test(tc_helpers, test_thinking_level_to_string);
    tcase_add_test(tc_helpers, test_build_fork_feedback);
    suite_add_tcase(s, tc_helpers);

    TCase *tc_insert = tcase_create("InsertForkEvents");
    tcase_add_checked_fixture(tc_insert, suite_setup, NULL);
    tcase_add_test(tc_insert, test_insert_fork_events_no_session);
    tcase_add_test(tc_insert, test_insert_fork_events_with_loaded_skills);
    tcase_add_test(tc_insert, test_insert_fork_events_with_skillset_catalog);
    tcase_add_test(tc_insert, test_insert_fork_events_db_failure);
    suite_add_tcase(s, tc_insert);

    return s;
}

int32_t main(void)
{
    Suite *s = commands_fork_helpers_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands_fork_helpers_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
