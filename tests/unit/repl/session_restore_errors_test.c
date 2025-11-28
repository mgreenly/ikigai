#include <check.h>
#include <stdlib.h>
#include <talloc.h>
#include <string.h>

#include "../../../src/repl.h"
#include "../../../src/db/session.h"
#include "../../../src/db/message.h"
#include "../../../src/db/replay.h"
#include "../../../src/scrollback.h"
#include "../../../src/error.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

// Mock state for ik_db_session_get_active
static bool mock_session_get_active_should_fail = false;
static int64_t mock_active_session_id = 0;

// Mock state for ik_db_session_create
static bool mock_session_create_should_fail = false;
static int64_t mock_created_session_id = 1;

// Mock state for ik_db_messages_load
static bool mock_messages_load_should_fail = false;
static ik_replay_context_t *mock_replay_context = NULL;

// Mock state for ik_db_message_insert
static bool mock_message_insert_should_fail = false;
static int mock_message_insert_fail_on_call = -1; // -1 means don't fail
static int mock_message_insert_call_count = 0;

// Mock state for ik_scrollback_append_line_
static bool mock_scrollback_append_should_fail = false;
static int mock_scrollback_append_fail_on_call = -1; // -1 means don't fail
static int mock_scrollback_append_call_count = 0;

// Forward declarations
res_t ik_repl_restore_session(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx, ik_cfg_t *cfg);

static TALLOC_CTX *mock_err_ctx = NULL;

// Cleanup function registered with atexit to prevent leaks in forked test processes
static void cleanup_mock_err_ctx(void)
{
    if (mock_err_ctx) {
        talloc_free(mock_err_ctx);
        mock_err_ctx = NULL;
    }
}

// Mock ik_db_session_get_active
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx;

    if (mock_session_get_active_should_fail) {
        if (mock_err_ctx == NULL) {
            mock_err_ctx = talloc_new(NULL);
            atexit(cleanup_mock_err_ctx);
        }
        return ERR(mock_err_ctx, DB_CONNECT, "Mock session get active failure");
    }

    *session_id_out = mock_active_session_id;
    return OK(NULL);
}

// Mock ik_db_session_create
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx;

    if (mock_session_create_should_fail) {
        if (mock_err_ctx == NULL) {
            mock_err_ctx = talloc_new(NULL);
            atexit(cleanup_mock_err_ctx);
        }
        return ERR(mock_err_ctx, DB_CONNECT, "Mock session create failure");
    }

    *session_id_out = mock_created_session_id;
    return OK(NULL);
}

// Mock ik_db_messages_load
res_t ik_db_messages_load(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id)
{
    (void)db_ctx;
    (void)session_id;

    if (mock_messages_load_should_fail) {
        return ERR(ctx, DB_CONNECT, "Mock messages load failure");
    }

    // Return the pre-configured mock replay context
    if (mock_replay_context == NULL) {
        // Empty context if not set
        ik_replay_context_t *empty = talloc_zero_(ctx, sizeof(ik_replay_context_t));
        empty->messages = NULL;
        empty->count = 0;
        empty->capacity = 0;
        empty->mark_stack.marks = NULL;
        empty->mark_stack.count = 0;
        empty->mark_stack.capacity = 0;
        return OK(empty);
    }

    return OK(mock_replay_context);
}

// Mock ik_db_message_insert
res_t ik_db_message_insert(ik_db_ctx_t *db_ctx,
                           int64_t session_id,
                           const char *kind,
                           const char *content,
                           const char *data_json)
{
    (void)db_ctx;
    (void)session_id;
    (void)kind;
    (void)content;
    (void)data_json;

    if (mock_message_insert_should_fail) {
        if (mock_err_ctx == NULL) {
            mock_err_ctx = talloc_new(NULL);
            atexit(cleanup_mock_err_ctx);
        }
        return ERR(mock_err_ctx, DB_CONNECT, "Mock message insert failure");
    }

    // Check if we should fail on this specific call
    if (mock_message_insert_fail_on_call >= 0 &&
        mock_message_insert_call_count == mock_message_insert_fail_on_call) {
        mock_message_insert_call_count++;
        if (mock_err_ctx == NULL) {
            mock_err_ctx = talloc_new(NULL);
            atexit(cleanup_mock_err_ctx);
        }
        return ERR(mock_err_ctx, DB_CONNECT, "Mock message insert failure on specific call");
    }

    mock_message_insert_call_count++;
    return OK(NULL);
}

// Mock ik_scrollback_append_line_ - needs weak attribute for override
MOCKABLE res_t ik_scrollback_append_line_(void *scrollback, const char *text, size_t length)
{
    (void)scrollback;
    (void)text;
    (void)length;

    if (mock_scrollback_append_should_fail) {
        if (mock_err_ctx == NULL) {
            mock_err_ctx = talloc_new(NULL);
            atexit(cleanup_mock_err_ctx);
        }
        return ERR(mock_err_ctx, IO, "Mock scrollback append failure");
    }

    // Check if we should fail on this specific call
    if (mock_scrollback_append_fail_on_call >= 0 &&
        mock_scrollback_append_call_count == mock_scrollback_append_fail_on_call) {
        mock_scrollback_append_call_count++;
        if (mock_err_ctx == NULL) {
            mock_err_ctx = talloc_new(NULL);
            atexit(cleanup_mock_err_ctx);
        }
        return ERR(mock_err_ctx, IO, "Mock scrollback append failure on specific call");
    }

    mock_scrollback_append_call_count++;
    return OK(NULL);
}

static void reset_mocks(void)
{
    // Reset session mocks
    mock_session_get_active_should_fail = false;
    mock_active_session_id = 0;
    mock_session_create_should_fail = false;
    mock_created_session_id = 1;

    // Reset messages load mock
    mock_messages_load_should_fail = false;
    mock_replay_context = NULL;

    // Reset message insert mock
    mock_message_insert_should_fail = false;
    mock_message_insert_fail_on_call = -1;
    mock_message_insert_call_count = 0;

    // Reset scrollback append mock
    mock_scrollback_append_should_fail = false;
    mock_scrollback_append_fail_on_call = -1;
    mock_scrollback_append_call_count = 0;

    // Clean up error context
    if (mock_err_ctx) {
        talloc_free(mock_err_ctx);
        mock_err_ctx = NULL;
    }
}

static ik_repl_ctx_t *create_test_repl(TALLOC_CTX *ctx)
{
    ik_repl_ctx_t *repl = talloc_zero_(ctx, sizeof(ik_repl_ctx_t));
    repl->scrollback = ik_scrollback_create(repl, 80);
    repl->current_session_id = 0;
    repl->marks = NULL;
    repl->mark_count = 0;
    return repl;
}

static ik_db_ctx_t *create_test_db_ctx(TALLOC_CTX *ctx)
{
    // Create a dummy db context (not used by mocks)
    return talloc_zero_(ctx, sizeof(ik_db_ctx_t));
}

static ik_message_t *create_mock_message(TALLOC_CTX *ctx, const char *kind, const char *content)
{
    ik_message_t *msg = talloc_zero_(ctx, sizeof(ik_message_t));
    msg->id = 1;
    msg->kind = talloc_strdup_(ctx, kind);
    msg->content = content ? talloc_strdup_(ctx, content) : NULL;
    msg->data_json = talloc_strdup_(ctx, "{}");
    return msg;
}

/* Test: Session get active fails - returns error */
START_TEST(test_restore_session_get_active_fails) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Make session get active fail
    mock_session_get_active_should_fail = true;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_err(&res));

    talloc_free(ctx);
}
END_TEST
/* Test: Session create fails - returns error */
START_TEST(test_restore_session_create_fails)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // No active session, but creation fails
    mock_active_session_id = 0;
    mock_session_create_should_fail = true;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_err(&res));

    talloc_free(ctx);
}

END_TEST
/* Test: Messages load fails - returns error */
START_TEST(test_restore_messages_load_fails)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Active session but messages load fails
    mock_active_session_id = 42;
    mock_messages_load_should_fail = true;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_err(&res));

    talloc_free(ctx);
}

END_TEST
/* Test: Message insert fails - returns error */
START_TEST(test_restore_message_insert_fails)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // No active session, but message insert fails
    mock_active_session_id = 0;
    mock_message_insert_should_fail = true;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_err(&res));

    talloc_free(ctx);
}

END_TEST
/* Test: Scrollback append fails during replay - lines 94-95 */
START_TEST(test_restore_scrollback_append_fails_during_replay)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Create replay context with a message that has content
    ik_replay_context_t *replay_ctx = talloc_zero_(ctx, sizeof(ik_replay_context_t));
    replay_ctx->capacity = 1;
    replay_ctx->count = 1;
    replay_ctx->messages = talloc_array_(ctx, sizeof(ik_message_t *), 1);
    replay_ctx->messages[0] = create_mock_message(ctx, "user", "Hello");
    replay_ctx->mark_stack.capacity = 0;
    replay_ctx->mark_stack.count = 0;
    replay_ctx->mark_stack.marks = NULL;
    mock_replay_context = replay_ctx;

    // Active session exists
    mock_active_session_id = 42;

    // Make scrollback append fail on first call (line 89-95)
    mock_scrollback_append_fail_on_call = 0;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    // Should fail due to scrollback append error
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_IO);

    talloc_free(ctx);
}

END_TEST
/* Test: Message insert fails for system message - lines 129-130 */
START_TEST(test_restore_message_insert_fails_for_system)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->openai_system_message = talloc_strdup_(ctx, "You are helpful");

    // No active session - will create new one
    mock_active_session_id = 0;

    // Make message insert fail on second call (system message - line 121-130)
    // First call is for "clear" event, second is for "system" event
    mock_message_insert_fail_on_call = 1;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    // Should fail due to message insert error on system message
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_DB_CONNECT);

    talloc_free(ctx);
}

END_TEST
/* Test: Scrollback append fails for system message - lines 140-141 */
START_TEST(test_restore_scrollback_append_fails_for_system)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->openai_system_message = talloc_strdup_(ctx, "You are helpful");

    // No active session - will create new one
    mock_active_session_id = 0;

    // Make scrollback append fail on first call (system message - line 134-141)
    mock_scrollback_append_fail_on_call = 0;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    // Should fail due to scrollback append error for system message
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_IO);

    talloc_free(ctx);
}

END_TEST

static Suite *session_restore_errors_suite(void)
{
    Suite *s = suite_create("Session Restoration - Error Paths");
    TCase *tc_errors = tcase_create("Error Handling");
    tcase_add_unchecked_fixture(tc_errors, NULL, reset_mocks);
    tcase_add_test(tc_errors, test_restore_session_get_active_fails);
    tcase_add_test(tc_errors, test_restore_session_create_fails);
    tcase_add_test(tc_errors, test_restore_messages_load_fails);
    tcase_add_test(tc_errors, test_restore_message_insert_fails);
    tcase_add_test(tc_errors, test_restore_scrollback_append_fails_during_replay);
    tcase_add_test(tc_errors, test_restore_message_insert_fails_for_system);
    tcase_add_test(tc_errors, test_restore_scrollback_append_fails_for_system);
    suite_add_tcase(s, tc_errors);
    return s;
}

int main(void)
{
    Suite *s = session_restore_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
