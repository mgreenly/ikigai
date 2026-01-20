/**
 * @file cmd_fork_conversation_test.c
 * @brief Unit tests for /fork command - conversation inheritance
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../../src/message.h"
#include "../../../src/providers/provider.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <talloc.h>

// Mock posix_rename_ to prevent PANIC during logger rotation
int posix_rename_(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return 0;
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
    if (db != NULL && test_ctx != NULL) {
        ik_test_db_truncate_all(db);
    }

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

// Test: Child inherits parent conversation
START_TEST(test_fork_child_inherits_conversation) {
    // Add a message to parent's conversation before forking
    ik_message_t *msg = ik_message_create_text(test_ctx, IK_ROLE_USER, "Test message from parent");

    res_t add_res = ik_agent_add_message(repl->current, msg);
    ck_assert(is_ok(&add_res));
    ck_assert_uint_eq(repl->current->message_count, 1);

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Child should inherit parent's conversation
    ik_agent_ctx_t *child = repl->current;
    ck_assert_uint_eq(child->message_count, 1);

    // Verify the message content was copied
    ck_assert(child->messages[0]->role == IK_ROLE_USER);
    ck_assert(child->messages[0]->content_count > 0);
    ck_assert(child->messages[0]->content_blocks[0].type == IK_CONTENT_TEXT);
    ck_assert_str_eq(child->messages[0]->content_blocks[0].data.text.text, "Test message from parent");
}
END_TEST
// Test: Child post-fork messages are separate from parent
START_TEST(test_fork_child_post_fork_messages_separate) {
    // Add initial message to parent
    ik_message_t *parent_msg = ik_message_create_text(test_ctx, IK_ROLE_USER, "Parent message before fork");
    res_t add_res = ik_agent_add_message(repl->current, parent_msg);
    ck_assert(is_ok(&add_res));

    ik_agent_ctx_t *parent = repl->current;
    size_t parent_msg_count_before_fork = parent->message_count;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Add message to child's conversation (simulating post-fork message)
    ik_agent_ctx_t *child = repl->current;
    ik_message_t *child_msg = ik_message_create_text(test_ctx, IK_ROLE_USER, "Child message after fork");
    res_t child_add_res = ik_agent_add_message(child, child_msg);
    ck_assert(is_ok(&child_add_res));

    // Child should have the post-fork message
    ck_assert_uint_eq(child->message_count, 2);

    // Parent's conversation should remain unchanged
    ck_assert_uint_eq(parent->message_count, parent_msg_count_before_fork);
}

END_TEST

static Suite *cmd_fork_suite(void)
{
    Suite *s = suite_create("Fork Command Conversation");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_fork_child_inherits_conversation);
    tcase_add_test(tc, test_fork_child_post_fork_messages_separate);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = cmd_fork_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
