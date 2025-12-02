/**
 * @file handle_request_success_test.c
 * @brief Unit tests for handle_request_success function - basic and metadata tests
 *
 * Tests basic code paths and metadata handling in handle_request_success.
 * Uses per-file database isolation for parallel test execution.
 */

#include "../../../src/repl.h"
#include "../../../src/repl_event_handlers.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/debug_pipe.h"
#include "../../../src/openai/client.h"
#include "../../../src/scrollback.h"
#include "../../../src/tool.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <fcntl.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

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
    ck_assert_ptr_nonnull(repl);

    // Create conversation
    res_t res = ik_openai_conversation_create(test_ctx);
    ck_assert(is_ok(&res));
    repl->conversation = res.ok;
    ck_assert_ptr_nonnull(repl->conversation);

    if (!db_available) {
        db = NULL;
        repl->db_ctx = NULL;
        return;
    }

    res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&res)) {
        db = NULL;
        repl->db_ctx = NULL;
        return;
    }

    res = ik_test_db_begin(db);
    if (is_err(&res)) {
        db = NULL;
        repl->db_ctx = NULL;
        return;
    }

    // Create a session for tests
    session_id = 0;
    res = ik_db_session_create(db, &session_id);
    if (is_err(&res)) {
        ik_test_db_rollback(db);
        db = NULL;
        repl->db_ctx = NULL;
        return;
    }

    repl->db_ctx = db;
    repl->current_session_id = session_id;
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

// Test: No assistant response (early exit)
START_TEST(test_no_assistant_response) {
    repl->assistant_response = NULL;

    handle_request_success(repl);

    // Nothing should happen, conversation should be empty
    ck_assert_uint_eq(repl->conversation->message_count, 0);
}
END_TEST

#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// Test: Empty assistant response (early exit)
START_TEST(test_empty_assistant_response)
{
    repl->assistant_response = talloc_strdup(test_ctx, "");

    handle_request_success(repl);

    // Nothing should happen, conversation should be empty
    ck_assert_uint_eq(repl->conversation->message_count, 0);
}

END_TEST
// Test: Assistant response without DB
START_TEST(test_assistant_response_no_db)
{
    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->db_ctx = NULL;
    repl->current_session_id = 0;

    handle_request_success(repl);

    // Message should be added to conversation
    ck_assert_uint_eq(repl->conversation->message_count, 1);

    // Assistant response should be cleared
    ck_assert_ptr_null(repl->assistant_response);
}

END_TEST
// Test: Assistant response with DB but no session ID
START_TEST(test_assistant_response_db_no_session)
{
    SKIP_IF_NO_DB();

    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->current_session_id = 0;  // No session

    handle_request_success(repl);

    // Message should be added to conversation but not persisted
    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_ptr_null(repl->assistant_response);
}

END_TEST
// Test: All metadata fields present
START_TEST(test_all_metadata_fields)
{
    SKIP_IF_NO_DB();

    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->response_model = talloc_strdup(test_ctx, "gpt-4");
    repl->response_completion_tokens = 10;
    repl->response_finish_reason = talloc_strdup(test_ctx, "stop");

    handle_request_success(repl);

    // Message should be added and persisted
    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_ptr_null(repl->assistant_response);
}

END_TEST
// Test: Only model metadata
START_TEST(test_only_model_metadata)
{
    SKIP_IF_NO_DB();

    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->response_model = talloc_strdup(test_ctx, "gpt-4");
    repl->response_completion_tokens = 0;
    repl->response_finish_reason = NULL;

    handle_request_success(repl);

    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_ptr_null(repl->assistant_response);
}

END_TEST
// Test: Only tokens metadata
START_TEST(test_only_tokens_metadata)
{
    SKIP_IF_NO_DB();

    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->response_model = NULL;
    repl->response_completion_tokens = 10;
    repl->response_finish_reason = NULL;

    handle_request_success(repl);

    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_ptr_null(repl->assistant_response);
}

END_TEST
// Test: Only finish_reason metadata
START_TEST(test_only_finish_reason_metadata)
{
    SKIP_IF_NO_DB();

    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->response_model = NULL;
    repl->response_completion_tokens = 0;
    repl->response_finish_reason = talloc_strdup(test_ctx, "stop");

    handle_request_success(repl);

    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_ptr_null(repl->assistant_response);
}

END_TEST
// Test: Model + tokens metadata
START_TEST(test_model_tokens_metadata)
{
    SKIP_IF_NO_DB();

    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->response_model = talloc_strdup(test_ctx, "gpt-4");
    repl->response_completion_tokens = 10;
    repl->response_finish_reason = NULL;

    handle_request_success(repl);

    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_ptr_null(repl->assistant_response);
}

END_TEST
// Test: Model + finish_reason metadata
START_TEST(test_model_finish_reason_metadata)
{
    SKIP_IF_NO_DB();

    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->response_model = talloc_strdup(test_ctx, "gpt-4");
    repl->response_completion_tokens = 0;
    repl->response_finish_reason = talloc_strdup(test_ctx, "stop");

    handle_request_success(repl);

    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_ptr_null(repl->assistant_response);
}

END_TEST
// Test: Tokens + finish_reason metadata
START_TEST(test_tokens_finish_reason_metadata)
{
    SKIP_IF_NO_DB();

    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->response_model = NULL;
    repl->response_completion_tokens = 10;
    repl->response_finish_reason = talloc_strdup(test_ctx, "stop");

    handle_request_success(repl);

    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_ptr_null(repl->assistant_response);
}

END_TEST
// Test: No metadata
START_TEST(test_no_metadata)
{
    SKIP_IF_NO_DB();

    repl->assistant_response = talloc_strdup(test_ctx, "Test response");
    repl->response_model = NULL;
    repl->response_completion_tokens = 0;
    repl->response_finish_reason = NULL;

    handle_request_success(repl);

    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_ptr_null(repl->assistant_response);
}

END_TEST

static Suite *handle_request_success_suite(void)
{
    Suite *s = suite_create("handle_request_success");

    TCase *tc_core = tcase_create("Core");
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_no_assistant_response);
    tcase_add_test(tc_core, test_empty_assistant_response);
    tcase_add_test(tc_core, test_assistant_response_no_db);
    tcase_add_test(tc_core, test_assistant_response_db_no_session);
    tcase_add_test(tc_core, test_all_metadata_fields);
    tcase_add_test(tc_core, test_only_model_metadata);
    tcase_add_test(tc_core, test_only_tokens_metadata);
    tcase_add_test(tc_core, test_only_finish_reason_metadata);
    tcase_add_test(tc_core, test_model_tokens_metadata);
    tcase_add_test(tc_core, test_model_finish_reason_metadata);
    tcase_add_test(tc_core, test_tokens_finish_reason_metadata);
    tcase_add_test(tc_core, test_no_metadata);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = handle_request_success_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
