#include <check.h>
#include "../../../src/agent.h"
#include <talloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>

#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/db/message.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/session.h"
#include "../../../src/db/replay.h"
#include "../../../src/error.h"
#include "../../../src/openai/client.h"
#include "../../../src/openai/client_multi.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../test_utils.h"

// Mock state for ik_db_message_insert
static bool mock_message_insert_should_fail = false;
static TALLOC_CTX *mock_err_ctx = NULL;

// Mock ik_db_message_insert to simulate database error
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
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, DB_CONNECT, "Mock database error: Failed to insert message");
    }

    return OK(NULL);
}

// Mock ik_db_session_get_active (not used in this test, but needed for linking)
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx;
    (void)session_id_out;
    return OK(NULL);
}

// Mock ik_db_session_create (not used in this test, but needed for linking)
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx;
    (void)session_id_out;
    return OK(NULL);
}

// Mock ik_db_messages_load (not used in this test, but needed for linking)
res_t ik_db_messages_load(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id)
{
    (void)ctx;
    (void)db_ctx;
    (void)session_id;
    return OK(NULL);
}

static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;
static ik_db_ctx_t *mock_db_ctx;
static int db_debug_pipe_fds[2];

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    // Create mock database context (just a talloc pointer)
    mock_db_ctx = talloc_zero_(test_ctx, 1);
    ck_assert_ptr_nonnull(mock_db_ctx);

    // Create pipe for db_debug_pipe
    int ret = pipe(db_debug_pipe_fds);
    ck_assert_int_eq(ret, 0);

    // Create minimal REPL context for testing
    repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);
    repl->shared = shared;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero_(repl, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    repl->current = agent;

    // Create config
    shared->cfg = talloc_zero_(test_ctx, sizeof(ik_cfg_t));
    ck_assert_ptr_nonnull(shared->cfg);
    shared->cfg->openai_model = talloc_strdup_(shared->cfg, "gpt-4");
    shared->cfg->openai_temperature = 0.7;
    shared->cfg->openai_max_completion_tokens = 2048;

    // Create scrollback
    repl->current->scrollback = ik_scrollback_create(repl, 80);
    ck_assert_ptr_nonnull(repl->current->scrollback);

    // Create input buffer
    repl->input_buffer = ik_input_buffer_create(repl);
    ck_assert_ptr_nonnull(repl->input_buffer);

    // Create conversation
    res_t conv_res = ik_openai_conversation_create(repl);
    ck_assert(is_ok(&conv_res));
    repl->conversation = conv_res.ok;

    // Create multi client (opaque pointer)
    repl->multi = talloc_zero_(repl, 1);
    ck_assert_ptr_nonnull(repl->multi);

    // Create terminal context
    repl->shared->term = talloc_zero_(repl, sizeof(ik_term_ctx_t));
    ck_assert_ptr_nonnull(repl->shared->term);
    repl->shared->term->screen_rows = 24;
    repl->shared->term->screen_cols = 80;

    // Set up database connection
    repl->shared->db_ctx = mock_db_ctx;
    repl->shared->session_id = 1;

    // Create db_debug_pipe
    repl->shared->db_debug_pipe = talloc_zero_(repl, sizeof(ik_debug_pipe_t));
    ck_assert_ptr_nonnull(repl->shared->db_debug_pipe);
    repl->shared->db_debug_pipe->write_end = fdopen(db_debug_pipe_fds[1], "w");
    ck_assert_ptr_nonnull(repl->shared->db_debug_pipe->write_end);

    // Set viewport offset
    repl->current->viewport_offset = 0;

    // Initialize curl_still_running
    repl->curl_still_running = 0;

    // Reset mock state
    mock_message_insert_should_fail = false;
    if (mock_err_ctx != NULL) {
        talloc_free(mock_err_ctx);
        mock_err_ctx = NULL;
    }
}

static void teardown(void)
{
    if (repl->shared->db_debug_pipe && repl->shared->db_debug_pipe->write_end) {
        fclose(repl->shared->db_debug_pipe->write_end);
    }
    close(db_debug_pipe_fds[0]);

    if (mock_err_ctx != NULL) {
        talloc_free(mock_err_ctx);
        mock_err_ctx = NULL;
    }

    talloc_free(test_ctx);
}

// Test that DB error during message persistence doesn't crash
// Tests lines 145-162 in repl_actions.c
START_TEST(test_db_message_insert_error) {
    // Set up: Insert text into input buffer
    const char *test_text = "Hello, world!";
    for (const char *p = test_text; *p; p++) {
        res_t r = ik_byte_array_append(repl->input_buffer->text, (uint8_t)*p);
        ck_assert(is_ok(&r));
    }

    // Enable DB error simulation
    mock_message_insert_should_fail = true;

    // Create newline action
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};

    // Process newline action (should trigger DB persistence error path)
    res_t result = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&result));

    // Read from db_debug_pipe to verify error message was logged
    fflush(repl->shared->db_debug_pipe->write_end);

    char buffer[512];
    ssize_t n = read(db_debug_pipe_fds[0], buffer, sizeof(buffer) - 1);
    ck_assert(n > 0);
    buffer[n] = '\0';

    // Verify error message was logged
    ck_assert(strstr(buffer, "Warning: Failed to persist user message to database") != NULL);
    ck_assert(strstr(buffer, "Mock database error") != NULL);

    // Verify the user message was still added to conversation (memory state is authoritative)
    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_str_eq(repl->conversation->messages[0]->kind, "user");
    ck_assert_str_eq(repl->conversation->messages[0]->content, test_text);

    // Verify scrollback has the user input (scrollback may have 1 or 2 lines depending on rendering)
    ck_assert(repl->current->scrollback->count >= 1);
}
END_TEST
// Test normal path (no DB error) for comparison
START_TEST(test_db_message_insert_success)
{
    // Set up: Insert text into input buffer
    const char *test_text = "Test message";
    for (const char *p = test_text; *p; p++) {
        res_t r = ik_byte_array_append(repl->input_buffer->text, (uint8_t)*p);
        ck_assert(is_ok(&r));
    }

    // No DB error
    mock_message_insert_should_fail = false;

    // Create newline action
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};

    // Process newline action
    res_t result = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&result));

    // No error message should be logged
    fflush(repl->shared->db_debug_pipe->write_end);

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(db_debug_pipe_fds[0], &readfds);
    struct timeval timeout = {0, 0}; // Non-blocking
    int ready = select(db_debug_pipe_fds[0] + 1, &readfds, NULL, NULL, &timeout);

    // Should be no data to read (no error logged)
    ck_assert_int_eq(ready, 0);

    // Verify the user message was added to conversation
    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_str_eq(repl->conversation->messages[0]->kind, "user");
    ck_assert_str_eq(repl->conversation->messages[0]->content, test_text);

    // Verify scrollback has the user input (scrollback may have 1 or 2 lines depending on rendering)
    ck_assert(repl->current->scrollback->count >= 1);
}

END_TEST
// Test DB error when db_debug_pipe is NULL (shouldn't crash)
START_TEST(test_db_message_insert_error_no_debug_pipe)
{
    // Close and remove debug pipe
    fclose(repl->shared->db_debug_pipe->write_end);
    repl->shared->db_debug_pipe->write_end = NULL;
    repl->shared->db_debug_pipe = NULL;

    // Set up: Insert text into input buffer
    const char *test_text = "Test";
    for (const char *p = test_text; *p; p++) {
        res_t r = ik_byte_array_append(repl->input_buffer->text, (uint8_t)*p);
        ck_assert(is_ok(&r));
    }

    // Enable DB error simulation
    mock_message_insert_should_fail = true;

    // Create newline action
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};

    // Process newline action (should handle DB error gracefully without debug pipe)
    res_t result = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&result));

    // Verify the user message was still added to conversation
    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_str_eq(repl->conversation->messages[0]->kind, "user");
    ck_assert_str_eq(repl->conversation->messages[0]->content, test_text);
}

END_TEST
// Test message submission when db_ctx is NULL (no DB persistence)
START_TEST(test_message_submission_no_db_ctx)
{
    // Set db_ctx to NULL
    repl->shared->db_ctx = NULL;
    repl->shared->session_id = 1;

    // Set up: Insert text into input buffer
    const char *test_text = "Test without DB";
    for (const char *p = test_text; *p; p++) {
        res_t r = ik_byte_array_append(repl->input_buffer->text, (uint8_t)*p);
        ck_assert(is_ok(&r));
    }

    // Create newline action
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};

    // Process newline action (should skip DB persistence)
    res_t result = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&result));

    // Verify the user message was still added to conversation
    ck_assert_uint_eq(repl->conversation->message_count, 1);
    ck_assert_str_eq(repl->conversation->messages[0]->kind, "user");
    ck_assert_str_eq(repl->conversation->messages[0]->content, test_text);

    // No DB operation should have occurred, so no error logged
    fflush(repl->shared->db_debug_pipe->write_end);
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(db_debug_pipe_fds[0], &readfds);
    struct timeval timeout = {0, 0};
    int ready = select(db_debug_pipe_fds[0] + 1, &readfds, NULL, NULL, &timeout);
    ck_assert_int_eq(ready, 0);
}

END_TEST

static Suite *repl_actions_db_error_suite(void)
{
    Suite *s = suite_create("REPL Actions DB Error Basic");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_db_message_insert_error);
    tcase_add_test(tc_core, test_db_message_insert_success);
    tcase_add_test(tc_core, test_db_message_insert_error_no_debug_pipe);
    tcase_add_test(tc_core, test_message_submission_no_db_ctx);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_actions_db_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
