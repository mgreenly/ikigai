/**
 * @file handle_request_success_metadata_test.c
 * @brief Unit tests for handle_request_success - provider and thinking level metadata
 *
 * Tests provider and thinking_level metadata fields that were previously uncovered.
 * Uses per-file database isolation for parallel test execution.
 */

#include "../../../src/agent.h"
#include "../../../src/repl.h"
#include "../../../src/repl_event_handlers.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <string.h>
#include <talloc.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;
static ik_repl_ctx_t *repl;

// Suite-level setup
static void suite_setup(void)
{
    const char *skip_live = getenv("SKIP_LIVE_DB_TESTS");
    if (skip_live && strcmp(skip_live, "1") == 0) {
        db_available = false;
        return;
    }

    DB_NAME = ik_test_db_name(NULL, __FILE__);

    res_t res = ik_test_db_create(DB_NAME);
    if (is_err(&res)) {
        db_available = false;
        return;
    }

    res = ik_test_db_migrate(NULL, DB_NAME);
    if (is_err(&res)) {
        ik_test_db_destroy(DB_NAME);
        db_available = false;
        return;
    }

    db_available = true;
}

// Suite-level teardown
static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

// Per-test setup
static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    // Create REPL context
    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create shared context
    repl->shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->uuid = talloc_strdup(agent, "test-agent-uuid");

    repl->current = agent;
    ck_assert_uint_eq(repl->current->message_count, 0);

    if (!db_available) {
        db = NULL;
        repl->shared->db_ctx = NULL;
        return;
    }

    res_t res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&res)) {
        db = NULL;
        repl->shared->db_ctx = NULL;
        return;
    }

    res = ik_test_db_begin(db);
    if (is_err(&res)) {
        db = NULL;
        repl->shared->db_ctx = NULL;
        return;
    }

    // Create a session for tests
    session_id = 0;
    res = ik_db_session_create(db, &session_id);
    if (is_err(&res)) {
        ik_test_db_rollback(db);
        db = NULL;
        repl->shared->db_ctx = NULL;
        return;
    }

    repl->shared->db_ctx = db;
    repl->shared->session_id = session_id;
}

// Per-test teardown
static void teardown(void)
{
    if (test_ctx != NULL) {
        if (db != NULL) {
            ik_test_db_rollback(db);
        }
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// Test: Provider metadata field
START_TEST(test_provider_metadata) {
    SKIP_IF_NO_DB();

    repl->current->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->current->provider = talloc_strdup(test_ctx, "anthropic");
    repl->current->response_model = NULL;
    repl->current->thinking_level = 0;

    ik_repl_handle_agent_request_success(repl, repl->current);

    ck_assert_uint_eq(repl->current->message_count, 1);
    ck_assert_ptr_null(repl->current->assistant_response);
}
END_TEST
// Test: Provider + model metadata
START_TEST(test_provider_and_model_metadata)
{
    SKIP_IF_NO_DB();

    repl->current->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->current->provider = talloc_strdup(test_ctx, "openai");
    repl->current->response_model = talloc_strdup(test_ctx, "gpt-4");
    repl->current->thinking_level = 0;

    ik_repl_handle_agent_request_success(repl, repl->current);

    ck_assert_uint_eq(repl->current->message_count, 1);
    ck_assert_ptr_null(repl->current->assistant_response);
}

END_TEST
// Test: Thinking level = 1 (low)
START_TEST(test_thinking_level_low)
{
    SKIP_IF_NO_DB();

    repl->current->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->current->provider = NULL;
    repl->current->response_model = NULL;
    repl->current->thinking_level = 1;

    ik_repl_handle_agent_request_success(repl, repl->current);

    ck_assert_uint_eq(repl->current->message_count, 1);
    ck_assert_ptr_null(repl->current->assistant_response);
}

END_TEST
// Test: Thinking level = 2 (med)
START_TEST(test_thinking_level_med)
{
    SKIP_IF_NO_DB();

    repl->current->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->current->provider = NULL;
    repl->current->response_model = NULL;
    repl->current->thinking_level = 2;

    ik_repl_handle_agent_request_success(repl, repl->current);

    ck_assert_uint_eq(repl->current->message_count, 1);
    ck_assert_ptr_null(repl->current->assistant_response);
}

END_TEST
// Test: Thinking level = 3 (high)
START_TEST(test_thinking_level_high)
{
    SKIP_IF_NO_DB();

    repl->current->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->current->provider = NULL;
    repl->current->response_model = NULL;
    repl->current->thinking_level = 3;

    ik_repl_handle_agent_request_success(repl, repl->current);

    ck_assert_uint_eq(repl->current->message_count, 1);
    ck_assert_ptr_null(repl->current->assistant_response);
}

END_TEST
// Test: Provider + thinking level
START_TEST(test_provider_and_thinking_level)
{
    SKIP_IF_NO_DB();

    repl->current->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->current->provider = talloc_strdup(test_ctx, "anthropic");
    repl->current->response_model = NULL;
    repl->current->thinking_level = 2;

    ik_repl_handle_agent_request_success(repl, repl->current);

    ck_assert_uint_eq(repl->current->message_count, 1);
    ck_assert_ptr_null(repl->current->assistant_response);
}

END_TEST
// Test: All metadata including provider and thinking level
START_TEST(test_all_metadata_with_provider_thinking)
{
    SKIP_IF_NO_DB();

    repl->current->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->current->provider = talloc_strdup(test_ctx, "google");
    repl->current->response_model = talloc_strdup(test_ctx, "gemini-2.5-flash-thinking");
    repl->current->thinking_level = 3;
    repl->current->response_input_tokens = 100;
    repl->current->response_output_tokens = 50;
    repl->current->response_thinking_tokens = 200;
    repl->current->response_finish_reason = talloc_strdup(test_ctx, "stop");

    ik_repl_handle_agent_request_success(repl, repl->current);

    ck_assert_uint_eq(repl->current->message_count, 1);
    ck_assert_ptr_null(repl->current->assistant_response);
}

END_TEST

static Suite *handle_request_success_metadata_suite(void)
{
    Suite *s = suite_create("handle_request_success_metadata");

    TCase *tc_core = tcase_create("Core");
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_provider_metadata);
    tcase_add_test(tc_core, test_provider_and_model_metadata);
    tcase_add_test(tc_core, test_thinking_level_low);
    tcase_add_test(tc_core, test_thinking_level_med);
    tcase_add_test(tc_core, test_thinking_level_high);
    tcase_add_test(tc_core, test_provider_and_thinking_level);
    tcase_add_test(tc_core, test_all_metadata_with_provider_thinking);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = handle_request_success_metadata_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
