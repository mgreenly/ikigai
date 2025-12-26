/**
 * @file agent_restore_replay_test.c
 * @brief Tests for agent restore replay helpers
 *
 * Tests for replay-specific helpers that populate agent state
 * during restoration from database.
 */

#include "../../../src/repl/agent_restore_replay.h"
#include "../../../src/agent.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/agent_replay.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/logger.h"
#include "../../../src/msg.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../test_utils.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;

// Suite-level setup: Create and migrate database (runs once)
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
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

// Suite-level teardown: Destroy database (runs once)
static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

// Per-test setup: Connect and begin transaction
static void test_setup(void)
{
    if (!db_available) {
        test_ctx = NULL;
        db = NULL;
        return;
    }

    test_ctx = talloc_new(NULL);
    res_t res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        return;
    }

    res = ik_test_db_begin(db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        return;
    }

    // Create a session for tests
    res = ik_db_session_create(db, &session_id);
    if (is_err(&res)) {
        ik_test_db_rollback(db);
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

// Per-test teardown: Rollback and cleanup
static void test_teardown(void)
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

// Helper macro to skip test if DB not available
#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// Helper: Create minimal agent for testing
static ik_agent_ctx_t *create_test_agent(const char *uuid)
{
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    shared->db_ctx = db;
    shared->session_id = session_id;
    shared->logger = ik_logger_create(shared, "/tmp");
    ck_assert_ptr_nonnull(shared->logger);
    shared->cfg = ik_test_create_config(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));

    if (uuid != NULL) {
        talloc_free(agent->uuid);
        agent->uuid = talloc_strdup(agent, uuid);
    }

    return agent;
}

// Helper: Insert an agent into the registry
static void insert_agent(const char *uuid)
{
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, uuid);
    agent.name = NULL;
    agent.parent_uuid = NULL;
    agent.created_at = 1000;
    agent.fork_message_id = 0;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));
}

// Helper: Insert a message
static void insert_message(const char *agent_uuid, const char *kind,
                           const char *content, const char *data_json)
{
    res_t res = ik_db_message_insert(db, session_id, agent_uuid, kind,
                                     content, data_json);
    ck_assert(is_ok(&res));
}

// ========== Test Cases ==========

// Test: populate_scrollback with command replays model command
START_TEST(test_populate_scrollback_replays_model_command)
{
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-model-replay-1";
    insert_agent(agent_uuid);

    // Insert a /model command
    const char *data_json = "{\"command\":\"model\",\"args\":\"gpt-4o\"}";
    insert_message(agent_uuid, "command", NULL, data_json);

    // Create agent
    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    // Verify initial state - should use default provider
    ck_assert_ptr_null(agent->provider);
    ck_assert_ptr_null(agent->model);

    // Load replay context
    ik_replay_context_t *replay_ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(replay_ctx);

    // Populate scrollback (this also replays command effects)
    ik_agent_restore_populate_scrollback(agent, replay_ctx,
                                         agent->shared->logger);

    // Verify model command was replayed
    ck_assert_ptr_nonnull(agent->provider);
    ck_assert_ptr_nonnull(agent->model);
    ck_assert_str_eq(agent->provider, "openai");
    ck_assert_str_eq(agent->model, "gpt-4o");
}
END_TEST

// Test: model command with slash syntax (model/thinking)
START_TEST(test_model_command_with_slash_thinking)
{
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-model-slash-1";
    insert_agent(agent_uuid);

    // Insert a /model command with slash syntax
    const char *data_json = "{\"command\":\"model\",\"args\":\"claude-opus-4/extended\"}";
    insert_message(agent_uuid, "command", NULL, data_json);

    // Create agent
    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    // Load replay context
    ik_replay_context_t *replay_ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&res));

    // Populate scrollback
    ik_agent_restore_populate_scrollback(agent, replay_ctx,
                                         agent->shared->logger);

    // Verify model command was replayed (thinking part should be stripped)
    ck_assert_ptr_nonnull(agent->provider);
    ck_assert_ptr_nonnull(agent->model);
    ck_assert_str_eq(agent->provider, "anthropic");
    ck_assert_str_eq(agent->model, "claude-opus-4");

}
END_TEST

// Test: model command updates provider instance
START_TEST(test_model_command_invalidates_provider_instance)
{
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-model-provider-1";
    insert_agent(agent_uuid);

    // Insert two model commands
    const char *data_json1 = "{\"command\":\"model\",\"args\":\"gpt-4o\"}";
    insert_message(agent_uuid, "command", NULL, data_json1);
    const char *data_json2 = "{\"command\":\"model\",\"args\":\"claude-opus-4\"}";
    insert_message(agent_uuid, "command", NULL, data_json2);

    // Create agent
    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    // Set initial provider
    agent->provider = talloc_strdup(agent, "google");
    agent->model = talloc_strdup(agent, "gemini-2.0");

    // Verify provider_instance starts as NULL
    ck_assert_ptr_null(agent->provider_instance);

    // Load replay context
    ik_replay_context_t *replay_ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&res));

    // Populate scrollback
    ik_agent_restore_populate_scrollback(agent, replay_ctx,
                                         agent->shared->logger);

    // Verify final model command was replayed
    ck_assert_ptr_nonnull(agent->provider);
    ck_assert_ptr_nonnull(agent->model);
    ck_assert_str_eq(agent->provider, "anthropic");
    ck_assert_str_eq(agent->model, "claude-opus-4");

    // Provider instance should still be NULL (not yet loaded)
    ck_assert_ptr_null(agent->provider_instance);

}
END_TEST

// Test: command with NULL data_json is skipped
START_TEST(test_command_with_null_data_json)
{
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-null-data-1";
    insert_agent(agent_uuid);

    // Insert a command with NULL data_json
    insert_message(agent_uuid, "command", NULL, "{}");

    // Create agent
    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    // Load replay context
    ik_replay_context_t *replay_ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&res));

    // Manually set data_json to NULL to test that path
    if (replay_ctx->count > 0) {
        replay_ctx->messages[0]->data_json = NULL;
    }

    // Populate scrollback - should not crash
    ik_agent_restore_populate_scrollback(agent, replay_ctx,
                                         agent->shared->logger);

    // Agent state should be unchanged
    ck_assert_ptr_null(agent->provider);
    ck_assert_ptr_null(agent->model);

}
END_TEST

// Test: command with invalid JSON is skipped
START_TEST(test_command_with_invalid_json)
{
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-invalid-json-1";
    insert_agent(agent_uuid);

    // Insert a command with invalid JSON
    const char *data_json = "{invalid json}";
    insert_message(agent_uuid, "command", NULL, data_json);

    // Create agent
    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    // Load replay context
    ik_replay_context_t *replay_ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&res));

    // Populate scrollback - should not crash
    ik_agent_restore_populate_scrollback(agent, replay_ctx,
                                         agent->shared->logger);

    // Agent state should be unchanged
    ck_assert_ptr_null(agent->provider);
    ck_assert_ptr_null(agent->model);

}
END_TEST

// Test: command with NULL command name is skipped
START_TEST(test_command_with_null_command_name)
{
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-null-cmd-1";
    insert_agent(agent_uuid);

    // Insert a command with missing command name
    const char *data_json = "{\"args\":\"something\"}";
    insert_message(agent_uuid, "command", NULL, data_json);

    // Create agent
    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    // Load replay context
    ik_replay_context_t *replay_ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&res));

    // Populate scrollback - should not crash
    ik_agent_restore_populate_scrollback(agent, replay_ctx,
                                         agent->shared->logger);

    // Agent state should be unchanged
    ck_assert_ptr_null(agent->provider);
    ck_assert_ptr_null(agent->model);

}
END_TEST

// Test: non-model command is ignored
START_TEST(test_non_model_command_ignored)
{
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-other-cmd-1";
    insert_agent(agent_uuid);

    // Insert a different command (not /model)
    const char *data_json = "{\"command\":\"clear\"}";
    insert_message(agent_uuid, "command", NULL, data_json);

    // Create agent
    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    // Load replay context
    ik_replay_context_t *replay_ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&res));

    // Populate scrollback
    ik_agent_restore_populate_scrollback(agent, replay_ctx,
                                         agent->shared->logger);

    // Agent state should be unchanged
    ck_assert_ptr_null(agent->provider);
    ck_assert_ptr_null(agent->model);

}
END_TEST

// Test: model command with NULL args is skipped
START_TEST(test_model_command_with_null_args)
{
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-model-null-args";
    insert_agent(agent_uuid);

    // Insert a model command without args
    const char *data_json = "{\"command\":\"model\"}";
    insert_message(agent_uuid, "command", NULL, data_json);

    // Create agent
    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    // Load replay context
    ik_replay_context_t *replay_ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&res));

    // Populate scrollback
    ik_agent_restore_populate_scrollback(agent, replay_ctx,
                                         agent->shared->logger);

    // Agent state should be unchanged (NULL args means no change)
    ck_assert_ptr_null(agent->provider);
    ck_assert_ptr_null(agent->model);

}
END_TEST

// Test: populate_conversation adds user and assistant messages
START_TEST(test_populate_conversation_adds_messages)
{
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-conv-msgs-1";
    insert_agent(agent_uuid);

    // Insert conversation messages
    insert_message(agent_uuid, "user", "Hello", "{}");
    insert_message(agent_uuid, "assistant", "Hi there", "{}");

    // Create agent
    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    // Load replay context
    ik_replay_context_t *replay_ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&res));

    // Populate conversation
    ik_agent_restore_populate_conversation(agent, replay_ctx,
                                           agent->shared->logger);

    // Verify messages were added
    ck_assert_uint_ge(agent->message_count, 2);

}
END_TEST

// Test: populate_conversation skips non-conversation messages
START_TEST(test_populate_conversation_skips_commands)
{
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-conv-skip-1";
    insert_agent(agent_uuid);

    // Insert mix of messages
    insert_message(agent_uuid, "command", NULL, "{\"command\":\"clear\"}");
    insert_message(agent_uuid, "user", "Hello", "{}");
    insert_message(agent_uuid, "usage", NULL, "{}");

    // Create agent
    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    // Load replay context
    ik_replay_context_t *replay_ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&res));

    // Populate conversation
    ik_agent_restore_populate_conversation(agent, replay_ctx,
                                           agent->shared->logger);

    // Only conversation messages should be added (1 user message)
    ck_assert_uint_ge(agent->message_count, 1);

}
END_TEST

// Test: restore_marks with empty mark stack
START_TEST(test_restore_marks_empty_stack)
{
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-marks-empty-1";
    insert_agent(agent_uuid);

    // Create agent
    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    // Load replay context (no marks)
    ik_replay_context_t *replay_ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, agent_uuid, &replay_ctx);
    ck_assert(is_ok(&res));

    // Verify mark stack is empty
    ck_assert_uint_eq(replay_ctx->mark_stack.count, 0);

    // Restore marks - should do nothing
    ik_agent_restore_marks(agent, replay_ctx);

    // Verify agent marks are unchanged
    ck_assert_uint_eq(agent->mark_count, 0);

}
END_TEST

// ========== Suite Configuration ==========

static Suite *agent_restore_replay_suite(void)
{
    Suite *s = suite_create("Agent Restore Replay");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_populate_scrollback_replays_model_command);
    tcase_add_test(tc_core, test_model_command_with_slash_thinking);
    tcase_add_test(tc_core, test_model_command_invalidates_provider_instance);
    tcase_add_test(tc_core, test_command_with_null_data_json);
    tcase_add_test(tc_core, test_command_with_invalid_json);
    tcase_add_test(tc_core, test_command_with_null_command_name);
    tcase_add_test(tc_core, test_non_model_command_ignored);
    tcase_add_test(tc_core, test_model_command_with_null_args);
    tcase_add_test(tc_core, test_populate_conversation_adds_messages);
    tcase_add_test(tc_core, test_populate_conversation_skips_commands);
    tcase_add_test(tc_core, test_restore_marks_empty_stack);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_restore_replay_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
