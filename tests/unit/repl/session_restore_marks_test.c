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

// Mock state for ik_db_session_get_active
static bool mock_session_get_active_should_fail = false;
static int64_t mock_active_session_id = 0;

// Mock state for ik_db_messages_load
static bool mock_messages_load_should_fail = false;
static ik_replay_context_t *mock_replay_context = NULL;

// Forward declarations
res_t ik_repl_restore_session(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx, ik_cfg_t *cfg);

static TALLOC_CTX *mock_err_ctx = NULL;

// Mock ik_db_session_get_active
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx;

    if (mock_session_get_active_should_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, DB_CONNECT, "Mock session get active failure");
    }

    *session_id_out = mock_active_session_id;
    return OK(NULL);
}

// Mock ik_db_session_create (not used in these tests, but needed for linking)
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx;
    *session_id_out = 1;
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

// Mock ik_db_message_insert (not used in these tests, but needed for linking)
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
    mock_session_get_active_should_fail = false;
    mock_active_session_id = 0;

    // Reset messages load mock
    mock_messages_load_should_fail = false;
    mock_replay_context = NULL;

    // Clean up error context
    if (mock_err_ctx) {
        talloc_free(mock_err_ctx);
        mock_err_ctx = NULL;
    }
}

static ik_repl_ctx_t *create_test_repl(TALLOC_CTX *ctx)
{
    ik_repl_ctx_t *repl = talloc_zero_(ctx, sizeof(ik_repl_ctx_t));

    // Create shared context with test config
    ik_shared_ctx_t *shared = talloc_zero_(ctx, sizeof(ik_shared_ctx_t));
    shared->cfg = talloc_zero_(ctx, sizeof(ik_cfg_t));
    repl->shared = shared;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero_(repl, sizeof(ik_agent_ctx_t));
    repl->current = agent;

    repl->current->scrollback = ik_scrollback_create(repl, 80);
    repl->shared->session_id = 0;
    repl->marks = NULL;
    repl->mark_count = 0;

    // Create empty conversation
    res_t conv_res = ik_openai_conversation_create(repl);
    repl->conversation = conv_res.ok;

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

/* Test: Session with marks - rebuilds mark stack */
START_TEST(test_restore_session_with_marks_rebuilds_stack) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Create replay context with 3 messages and 2 marks
    ik_replay_context_t *replay_ctx = talloc_zero_(ctx, sizeof(ik_replay_context_t));
    replay_ctx->capacity = 3;
    replay_ctx->count = 3;
    replay_ctx->messages = talloc_array_(ctx, sizeof(ik_message_t *), 3);
    replay_ctx->messages[0] = create_mock_message(ctx, "user", "First message");
    replay_ctx->messages[1] = create_mock_message(ctx, "mark", NULL); // Mark 1
    replay_ctx->messages[2] = create_mock_message(ctx, "user", "Second message");

    // Set up mark stack with 2 marks
    replay_ctx->mark_stack.capacity = 2;
    replay_ctx->mark_stack.count = 2;
    replay_ctx->mark_stack.marks = talloc_array_(ctx, sizeof(ik_replay_mark_t), 2);

    // Mark 1: at message index 1, labeled "checkpoint-1"
    replay_ctx->mark_stack.marks[0].message_id = 101;
    replay_ctx->mark_stack.marks[0].context_idx = 1;
    replay_ctx->mark_stack.marks[0].label = talloc_strdup_(ctx, "checkpoint-1");

    // Mark 2: at message index 2, no label
    replay_ctx->mark_stack.marks[1].message_id = 102;
    replay_ctx->mark_stack.marks[1].context_idx = 2;
    replay_ctx->mark_stack.marks[1].label = NULL;

    mock_replay_context = replay_ctx;
    mock_active_session_id = 42;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));

    // Verify mark stack was rebuilt
    ck_assert_int_eq((int)repl->mark_count, 2);
    ck_assert_ptr_nonnull(repl->marks);

    // Verify first mark
    ck_assert_int_eq((int)repl->marks[0]->message_index, 1);
    ck_assert_ptr_nonnull(repl->marks[0]->label);
    ck_assert_str_eq(repl->marks[0]->label, "checkpoint-1");

    // Verify second mark
    ck_assert_int_eq((int)repl->marks[1]->message_index, 2);
    ck_assert_ptr_null(repl->marks[1]->label);

    talloc_free(ctx);
}
END_TEST
/* Test: Session with no marks - mark stack remains empty */
START_TEST(test_restore_session_no_marks_stack_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Create replay context with messages but no marks
    ik_replay_context_t *replay_ctx = talloc_zero_(ctx, sizeof(ik_replay_context_t));
    replay_ctx->capacity = 2;
    replay_ctx->count = 2;
    replay_ctx->messages = talloc_array_(ctx, sizeof(ik_message_t *), 2);
    replay_ctx->messages[0] = create_mock_message(ctx, "user", "Hello");
    replay_ctx->messages[1] = create_mock_message(ctx, "assistant", "Hi");

    // Empty mark stack
    replay_ctx->mark_stack.capacity = 0;
    replay_ctx->mark_stack.count = 0;
    replay_ctx->mark_stack.marks = NULL;

    mock_replay_context = replay_ctx;
    mock_active_session_id = 42;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));

    // Verify mark stack is empty
    ck_assert_int_eq((int)repl->mark_count, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Session with single labeled mark */
START_TEST(test_restore_session_single_labeled_mark)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Create replay context with 1 mark
    ik_replay_context_t *replay_ctx = talloc_zero_(ctx, sizeof(ik_replay_context_t));
    replay_ctx->capacity = 1;
    replay_ctx->count = 1;
    replay_ctx->messages = talloc_array_(ctx, sizeof(ik_message_t *), 1);
    replay_ctx->messages[0] = create_mock_message(ctx, "mark", NULL);

    // Set up mark stack
    replay_ctx->mark_stack.capacity = 1;
    replay_ctx->mark_stack.count = 1;
    replay_ctx->mark_stack.marks = talloc_array_(ctx, sizeof(ik_replay_mark_t), 1);
    replay_ctx->mark_stack.marks[0].message_id = 100;
    replay_ctx->mark_stack.marks[0].context_idx = 0;
    replay_ctx->mark_stack.marks[0].label = talloc_strdup_(ctx, "test-mark");

    mock_replay_context = replay_ctx;
    mock_active_session_id = 42;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)repl->mark_count, 1);
    ck_assert_str_eq(repl->marks[0]->label, "test-mark");

    talloc_free(ctx);
}

END_TEST
/* Test: Session with unlabeled mark */
START_TEST(test_restore_session_unlabeled_mark)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_repl_ctx_t *repl = create_test_repl(ctx);
    ik_db_ctx_t *db_ctx = create_test_db_ctx(ctx);
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Create replay context with unlabeled mark
    ik_replay_context_t *replay_ctx = talloc_zero_(ctx, sizeof(ik_replay_context_t));
    replay_ctx->capacity = 1;
    replay_ctx->count = 1;
    replay_ctx->messages = talloc_array_(ctx, sizeof(ik_message_t *), 1);
    replay_ctx->messages[0] = create_mock_message(ctx, "mark", NULL);

    // Set up mark stack with NULL label
    replay_ctx->mark_stack.capacity = 1;
    replay_ctx->mark_stack.count = 1;
    replay_ctx->mark_stack.marks = talloc_array_(ctx, sizeof(ik_replay_mark_t), 1);
    replay_ctx->mark_stack.marks[0].message_id = 100;
    replay_ctx->mark_stack.marks[0].context_idx = 0;
    replay_ctx->mark_stack.marks[0].label = NULL;

    mock_replay_context = replay_ctx;
    mock_active_session_id = 42;

    res_t res = ik_repl_restore_session(repl, db_ctx, cfg);

    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)repl->mark_count, 1);
    ck_assert_ptr_null(repl->marks[0]->label);

    talloc_free(ctx);
}

END_TEST

static Suite *session_restore_marks_suite(void)
{
    Suite *s = suite_create("Session Restoration - Marks");
    TCase *tc_marks = tcase_create("Mark Stack Restoration");
    tcase_add_test(tc_marks, test_restore_session_with_marks_rebuilds_stack);
    tcase_add_test(tc_marks, test_restore_session_no_marks_stack_empty);
    tcase_add_test(tc_marks, test_restore_session_single_labeled_mark);
    tcase_add_test(tc_marks, test_restore_session_unlabeled_mark);
    suite_add_tcase(s, tc_marks);
    return s;
}

int main(void)
{
    Suite *s = session_restore_marks_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
