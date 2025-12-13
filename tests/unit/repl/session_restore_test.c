#include <check.h>
#include "../../../src/agent.h"
#include <talloc.h>
#include <string.h>

#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/db/session.h"
#include "../../../src/db/message.h"
#include "../../../src/db/replay.h"
#include "../../../src/openai/client.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"
#include "../../../src/msg.h"
#include "../../test_utils.h"

static int64_t mock_active_session_id = 0;
static int64_t mock_created_session_id = 1;
static ik_replay_context_t *mock_replay_context = NULL;
static int mock_message_insert_call_count = 0;
static char *mock_inserted_kind[10];
static char *mock_inserted_content[10];

res_t ik_repl_restore_session(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx, ik_cfg_t *cfg);
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx;
    *session_id_out = mock_active_session_id;
    return OK(NULL);
}

// Mock ik_db_session_create
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx;
    *session_id_out = mock_created_session_id;
    return OK(NULL);
}

// Mock ik_db_messages_load
res_t ik_db_messages_load(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id)
{
    (void)db_ctx;
    (void)session_id;

    // Return the pre-configured mock replay context
    if (mock_replay_context == NULL) {
        // Empty context if not set
        ik_replay_context_t *empty = talloc_zero_(ctx, sizeof(ik_replay_context_t));
        empty->messages = NULL;
        empty->count = 0;
        empty->capacity = 0;
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
    (void)data_json;

    // Record the call
    if (mock_message_insert_call_count < 10) {
        mock_inserted_kind[mock_message_insert_call_count] = strdup(kind);
        mock_inserted_content[mock_message_insert_call_count] = content ? strdup(content) : NULL;
        mock_message_insert_call_count++;
    }

    return OK(NULL);
}

// Wrapper mocks (pass-through to real implementations)
MOCKABLE res_t ik_msg_from_db_(void *parent, const void *db_msg) {
    return ik_msg_from_db(parent, (const ik_message_t *)db_msg);
}
MOCKABLE res_t ik_openai_conversation_add_msg_(void *conv, void *msg) {
    return ik_openai_conversation_add_msg((ik_openai_conversation_t *)conv, (ik_msg_t *)msg);
}

static void reset_mocks(void)
{
    // Reset session mocks
    mock_active_session_id = 0;
    mock_created_session_id = 1;

    // Reset messages load mock
    mock_replay_context = NULL;

    // Reset message insert mock
    for (int i = 0; i < mock_message_insert_call_count; i++) {
        free(mock_inserted_kind[i]);
        if (mock_inserted_content[i]) free(mock_inserted_content[i]);
    }
    mock_message_insert_call_count = 0;
}

static ik_repl_ctx_t *create_test_repl(TALLOC_CTX *ctx)
{
    ik_repl_ctx_t *repl = talloc_zero_(ctx, sizeof(ik_repl_ctx_t));
    ik_shared_ctx_t *shared = talloc_zero_(ctx, sizeof(ik_shared_ctx_t));
    shared->cfg = talloc_zero_(ctx, sizeof(ik_cfg_t));
    repl->shared = shared;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero_(repl, sizeof(ik_agent_ctx_t));
    repl->current = agent;

    repl->current->scrollback = ik_scrollback_create(repl, 80);
    repl->shared->session_id = 0;
    repl->conversation = ik_openai_conversation_create(repl).ok;
    return repl;
}

static ik_db_ctx_t *create_test_db_ctx(TALLOC_CTX *ctx)
{
    return talloc_zero_(ctx, sizeof(ik_db_ctx_t));
}

static ik_replay_context_t *create_mock_replay_context(TALLOC_CTX *ctx, int message_count)
{
    ik_replay_context_t *replay_ctx = talloc_zero_(ctx, sizeof(ik_replay_context_t));
    replay_ctx->capacity = (size_t)message_count;
    replay_ctx->count = (size_t)message_count;
    replay_ctx->messages = talloc_array_(ctx, sizeof(ik_message_t *), (size_t)message_count);
    return replay_ctx;
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

/* Test: No active session - creates new session */
START_TEST(test_restore_no_active_session_creates_new) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);


    mock_active_session_id = 0;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(repl->shared->session_id, mock_created_session_id);

    talloc_free(ctx);
}
END_TEST
/* Test: No active session - writes clear event */
START_TEST(test_restore_no_active_session_writes_clear)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);


    mock_active_session_id = 0;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    ck_assert_int_ge(mock_message_insert_call_count, 1);
    ck_assert_str_eq(mock_inserted_kind[0], "clear");

    talloc_free(ctx);
}

END_TEST
/* Test: No active session with system message */
START_TEST(test_restore_no_active_session_writes_system_message)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->openai_system_message = talloc_strdup_(ctx, "You are a helpful assistant");


    mock_active_session_id = 0;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(mock_message_insert_call_count, 2);
    ck_assert_str_eq(mock_inserted_kind[0], "clear");
    ck_assert_str_eq(mock_inserted_kind[1], "system");
    ck_assert_str_eq(mock_inserted_content[1], "You are a helpful assistant");

    talloc_free(ctx);
}

END_TEST
/* Test: No active session without system message */
START_TEST(test_restore_no_active_session_no_system_message)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->openai_system_message = NULL;


    mock_active_session_id = 0;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(mock_message_insert_call_count, 1);
    ck_assert_str_eq(mock_inserted_kind[0], "clear");

    talloc_free(ctx);
}

END_TEST
/* Test: No active session - scrollback empty */
START_TEST(test_restore_no_active_session_scrollback_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->openai_system_message = NULL; // No system message


    mock_active_session_id = 0;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)ik_scrollback_get_line_count(repl->current->scrollback), 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Bug 6 - system message in scrollback */
START_TEST(test_restore_new_session_system_message_in_scrollback)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->openai_system_message = talloc_strdup_(ctx, "You are a helpful assistant");

    // No active session - will create new one
    mock_active_session_id = 0;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    // System message should be in scrollback (Bug 6 fix) - with blank line = 2 lines
    ck_assert_int_eq((int)ik_scrollback_get_line_count(repl->current->scrollback), 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Active session found - loads session ID */
START_TEST(test_restore_active_session_loads_id)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);


    mock_active_session_id = 42;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(repl->shared->session_id, 42);

    talloc_free(ctx);
}

END_TEST
/* Test: Active session with messages - populates scrollback */
START_TEST(test_restore_active_session_populates_scrollback)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Create mock replay context with 2 messages
    ik_replay_context_t *replay_ctx = create_mock_replay_context(ctx, 2);
    replay_ctx->messages[0] = create_mock_message(ctx, "user", "Hello");
    replay_ctx->messages[1] = create_mock_message(ctx, "assistant", "Hi there!");
    mock_replay_context = replay_ctx;


    mock_active_session_id = 42;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)ik_scrollback_get_line_count(repl->current->scrollback), 4);

    talloc_free(ctx);
}

END_TEST
/* Test: Active session with no messages - scrollback empty */
START_TEST(test_restore_active_session_no_messages)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Empty replay context (no messages)
    ik_replay_context_t *replay_ctx = create_mock_replay_context(ctx, 0);
    mock_replay_context = replay_ctx;


    mock_active_session_id = 42;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)ik_scrollback_get_line_count(repl->current->scrollback), 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Active session - does not write new events */
START_TEST(test_restore_active_session_no_event_writes)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->openai_system_message = talloc_strdup_(ctx, "You are helpful");


    mock_active_session_id = 42;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    // Should not write any events for existing session
    ck_assert_int_eq(mock_message_insert_call_count, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Multiple clears - only after last */
START_TEST(test_restore_multiple_clears_only_after_last)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Create replay context: [user1, assistant1, clear, user2]
    // Should only see user2 in scrollback
    ik_replay_context_t *replay_ctx = create_mock_replay_context(ctx, 1);
    replay_ctx->messages[0] = create_mock_message(ctx, "user", "Message after clear");
    mock_replay_context = replay_ctx;


    mock_active_session_id = 42;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)ik_scrollback_get_line_count(repl->current->scrollback), 2);

    talloc_free(ctx);
}

END_TEST

/* Test: Event render handles each kind */
START_TEST(test_restore_active_session_empty_string_content_skipped)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Create mock replay context with different event types
    // Event renderer handles each kind: user/assistant/system render content,
    // clear renders nothing, mark renders "/mark [label]"
    ik_replay_context_t *replay_ctx = create_mock_replay_context(ctx, 3);
    replay_ctx->messages[0] = create_mock_message(ctx, "user", "Hello");
    replay_ctx->messages[1] = create_mock_message(ctx, "clear", ""); // Clear renders nothing
    replay_ctx->messages[2] = create_mock_message(ctx, "rewind", NULL); // Rewind renders nothing
    mock_replay_context = replay_ctx;


    mock_active_session_id = 42;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    // User message renders (with blank line), clear/rewind don't render visible content
    ck_assert_int_eq((int)ik_scrollback_get_line_count(repl->current->scrollback), 2);

    talloc_free(ctx);
}

END_TEST

/* Test: Active session - conversation rebuilt */
START_TEST(test_restore_rebuilds_conversation)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Replay: user, assistant, clear, user, mark
    ik_replay_context_t *replay_ctx = create_mock_replay_context(ctx, 5);
    replay_ctx->messages[0] = create_mock_message(ctx, "user", "Hello");
    replay_ctx->messages[1] = create_mock_message(ctx, "assistant", "Hi");
    replay_ctx->messages[2] = create_mock_message(ctx, "clear", NULL);
    replay_ctx->messages[3] = create_mock_message(ctx, "user", "Second");
    replay_ctx->messages[4] = create_mock_message(ctx, "mark", NULL);
    mock_replay_context = replay_ctx;
    mock_active_session_id = 42;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)repl->conversation->message_count, 3);
    ck_assert_str_eq(repl->conversation->messages[0]->kind, "user");
    ck_assert_str_eq(repl->conversation->messages[1]->kind, "assistant");
    ck_assert_str_eq(repl->conversation->messages[2]->kind, "user");

    talloc_free(ctx);
}

END_TEST

static Suite *session_restore_suite(void)
{
    Suite *s = suite_create("Session Restoration");
    TCase *tc_new = tcase_create("New Session");
    tcase_add_test(tc_new, test_restore_no_active_session_creates_new);
    tcase_add_test(tc_new, test_restore_no_active_session_writes_clear);
    tcase_add_test(tc_new, test_restore_no_active_session_writes_system_message);
    tcase_add_test(tc_new, test_restore_no_active_session_no_system_message);
    tcase_add_test(tc_new, test_restore_no_active_session_scrollback_empty);
    tcase_add_test(tc_new, test_restore_new_session_system_message_in_scrollback);
    suite_add_tcase(s, tc_new);
    TCase *tc_existing = tcase_create("Existing Session");
    tcase_add_test(tc_existing, test_restore_active_session_loads_id);
    tcase_add_test(tc_existing, test_restore_active_session_populates_scrollback);
    tcase_add_test(tc_existing, test_restore_active_session_no_messages);
    tcase_add_test(tc_existing, test_restore_active_session_no_event_writes);
    tcase_add_test(tc_existing, test_restore_active_session_empty_string_content_skipped);
    tcase_add_test(tc_existing, test_restore_rebuilds_conversation);
    suite_add_tcase(s, tc_existing);
    TCase *tc_clears = tcase_create("Multiple Clears");
    tcase_add_test(tc_clears, test_restore_multiple_clears_only_after_last);
    suite_add_tcase(s, tc_clears);
    return s;
}

int main(void)
{
    Suite *s = session_restore_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
