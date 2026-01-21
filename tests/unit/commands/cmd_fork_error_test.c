/**
 * @file cmd_fork_error_test.c
 * @brief Unit tests for /fork command - error paths and edge cases
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/commands_fork_helpers.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/providers/provider.h"
#include "../../../src/providers/request.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <inttypes.h>
#include <talloc.h>

// Mock posix_rename_ to prevent PANIC during logger rotation
int posix_rename_(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return 0;
}

// Mock ik_agent_get_provider_ - default passthrough
res_t ik_agent_get_provider_(void *agent, void **provider_out)
{
    *provider_out = ((ik_agent_ctx_t *)agent)->provider_instance;
    return OK(NULL);
}

// Mock ik_request_build_from_conversation_ - default passthrough
res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void *registry, void **req_out)
{
    (void)agent;
    (void)registry;
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    if (req == NULL) {
        TALLOC_CTX *err_ctx = talloc_new(NULL);
        return ERR(err_ctx, OUT_OF_MEMORY, "Out of memory");
    }
    *req_out = req;
    return OK(NULL);
}

// Test fixtures
static const char *DB_NAME;
static ik_db_ctx_t *db;
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;

// Helper: Create minimal REPL for testing
static void setup_repl(void)
{
    ik_scrollback_t *sb = ik_scrollback_create(test_ctx, 80);
    ck_assert_ptr_nonnull(sb);

    ik_config_t *cfg = talloc_zero(test_ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4o-mini");

    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = sb;

    agent->uuid = talloc_strdup(agent, "parent-uuid-123");
    agent->name = NULL;
    agent->parent_uuid = NULL;
    agent->created_at = 1234567890;
    agent->fork_message_id = 0;
    agent->model = talloc_strdup(agent, "gpt-4o");
    agent->thinking_level = IK_THINKING_HIGH;
    repl->current = agent;

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db;
    atomic_init(&shared->fork_pending, false);
    repl->shared = shared;
    agent->shared = shared;

    // Initialize agent array
    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->agent_capacity = 16;

    // Insert parent agent into registry
    res_t res = ik_db_agent_insert(db, agent);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to insert parent agent: %s\n", error_message(res.err));
        ck_abort_msg("Failed to setup parent agent in registry");
    }
}

static bool suite_setup(void)
{
    DB_NAME = ik_test_db_name(NULL, __FILE__);
    res_t res = ik_test_db_create(DB_NAME);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to create database: %s\n", error_message(res.err));
        talloc_free(res.err);
        return false;
    }
    res = ik_test_db_migrate(NULL, DB_NAME);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to migrate database: %s\n", error_message(res.err));
        talloc_free(res.err);
        ik_test_db_destroy(DB_NAME);
        return false;
    }
    return true;
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    res_t db_res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&db_res)) {
        fprintf(stderr, "Failed to connect to database: %s\n", error_message(db_res.err));
        ck_abort_msg("Database connection failed");
    }
    ck_assert_ptr_nonnull(db);
    ck_assert_ptr_nonnull(db->conn);

    setup_repl();
}

static void teardown(void)
{
    // Clean up database state for next test BEFORE freeing context
    if (db != NULL && test_ctx != NULL) {
        ik_test_db_truncate_all(db);
    }

    // Now free everything (this also closes db connection via destructor)
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }

    db = NULL;
}

static void suite_teardown(void)
{
    ik_test_db_destroy(DB_NAME);
}

// Test: Warning displayed when model doesn't support thinking
START_TEST(test_fork_warning_no_thinking_support) {
    // Fork with a model that doesn't support thinking (gpt-4o-mini) but has thinking level set
    res_t res = ik_cmd_fork(test_ctx, repl, "--model gpt-4o-mini/high");
    ck_assert(is_ok(&res));

    // Check scrollback for warning message
    ik_agent_ctx_t *child = repl->current;
    size_t line_count = ik_scrollback_get_line_count(child->scrollback);
    bool found_warning = false;
    for (size_t i = 0; i < line_count; i++) {
        const char *text = NULL;
        size_t length = 0;
        res_t line_res = ik_scrollback_get_line_text(child->scrollback, i, &text, &length);
        if (is_ok(&line_res) && text && strstr(text, "does not support thinking")) {
            found_warning = true;
            break;
        }
    }
    ck_assert(found_warning);
}
END_TEST
// Test: thinking_level_to_string handles all enum values
START_TEST(test_thinking_level_to_string_all_values) {
    ck_assert_str_eq(thinking_level_to_string(IK_THINKING_NONE), "none");
    ck_assert_str_eq(thinking_level_to_string(IK_THINKING_LOW), "low");
    ck_assert_str_eq(thinking_level_to_string(IK_THINKING_MED), "medium");
    ck_assert_str_eq(thinking_level_to_string(IK_THINKING_HIGH), "high");
    ck_assert_str_eq(thinking_level_to_string((ik_thinking_level_t)999), "unknown");
}

END_TEST
// Test: build_fork_feedback with override=true
START_TEST(test_build_fork_feedback_override) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->provider = talloc_strdup(agent, "openai");
    agent->model = talloc_strdup(agent, "gpt-4o");
    agent->thinking_level = IK_THINKING_MED;

    char *feedback = build_fork_feedback(test_ctx, agent, true);
    ck_assert_ptr_nonnull(feedback);
    ck_assert(strstr(feedback, "Forked child with openai/gpt-4o/medium") != NULL);
}

END_TEST
// Test: build_fork_feedback with override=false
START_TEST(test_build_fork_feedback_inherit) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->provider = talloc_strdup(agent, "anthropic");
    agent->model = talloc_strdup(agent, "claude-3-5-sonnet-20241022");
    agent->thinking_level = IK_THINKING_LOW;

    char *feedback = build_fork_feedback(test_ctx, agent, false);
    ck_assert_ptr_nonnull(feedback);
    ck_assert(strstr(feedback, "parent's model") != NULL);
    ck_assert(strstr(feedback, "anthropic/claude-3-5-sonnet-20241022/low") != NULL);
}

END_TEST
// Test: insert_fork_events with no session_id
START_TEST(test_insert_fork_events_no_session) {
    ik_agent_ctx_t *parent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(parent);
    parent->uuid = talloc_strdup(parent, "parent-uuid");

    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);
    child->uuid = talloc_strdup(child, "child-uuid");

    // Create minimal repl with session_id=0
    ik_repl_ctx_t *test_repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared->session_id = 0;
    shared->db_ctx = db;
    test_repl->shared = shared;

    res_t res = insert_fork_events(test_ctx, test_repl, parent, child, 123);
    ck_assert(is_ok(&res));  // Should return OK without inserting
}

END_TEST
// Test: insert_fork_events with database error on parent insert
START_TEST(test_insert_fork_events_db_error_parent) {
    ik_agent_ctx_t *parent = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(parent);
    parent->uuid = talloc_strdup(parent, "parent-uuid-nonexistent");

    ik_agent_ctx_t *child = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(child);
    child->uuid = talloc_strdup(child, "child-uuid-nonexistent");

    // Create minimal repl with valid session_id
    ik_repl_ctx_t *test_repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared->session_id = 1;
    shared->db_ctx = db;
    test_repl->shared = shared;

    // This will fail because parent agent doesn't exist in registry
    res_t res = insert_fork_events(test_ctx, test_repl, parent, child, 123);
    ck_assert(is_err(&res));
}

END_TEST

static Suite *cmd_fork_error_suite(void)
{
    Suite *s = suite_create("Fork Command Errors");
    TCase *tc_errors = tcase_create("Error Paths");
    tcase_set_timeout(tc_errors, IK_TEST_TIMEOUT);
    TCase *tc_helpers = tcase_create("Helper Functions");
    tcase_set_timeout(tc_helpers, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_checked_fixture(tc_helpers, setup, teardown);

    tcase_add_test(tc_errors, test_fork_warning_no_thinking_support);
    tcase_add_test(tc_helpers, test_thinking_level_to_string_all_values);
    tcase_add_test(tc_helpers, test_build_fork_feedback_override);
    tcase_add_test(tc_helpers, test_build_fork_feedback_inherit);
    tcase_add_test(tc_helpers, test_insert_fork_events_no_session);
    tcase_add_test(tc_helpers, test_insert_fork_events_db_error_parent);

    suite_add_tcase(s, tc_errors);
    suite_add_tcase(s, tc_helpers);

    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = cmd_fork_error_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands/cmd_fork_error_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
