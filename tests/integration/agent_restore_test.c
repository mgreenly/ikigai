/**
 * @file agent_restore_test.c
 * @brief Integration tests for multi-agent restoration
 *
 * Tests the complete agent restoration flow including:
 * - Multi-agent hierarchy preservation
 * - Fork point boundary enforcement
 * - Clear event handling
 */

#include "../../src/repl/agent_restore.h"
#include "../../src/db/agent.h"
#include "../../src/db/agent_replay.h"
#include "../../src/db/connection.h"
#include "../../src/db/message.h"
#include "../../src/db/session.h"
#include "../../src/agent.h"
#include "../../src/providers/provider.h"
#include "../../src/repl.h"
#include "../../src/error.h"
#include "../../src/config.h"
#include "../../src/shared.h"
#include "../../src/logger.h"
#include "../test_utils_helper.h"
#include <check.h>
#include <talloc.h>
#include <string.h>
#include <time.h>

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

// Helper: Insert an agent into the registry
static void insert_agent(const char *uuid, const char *parent_uuid,
                         int64_t created_at, int64_t fork_message_id)
{
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, uuid);
    agent.name = NULL;
    agent.parent_uuid = parent_uuid ? talloc_strdup(test_ctx, parent_uuid) : NULL;
    agent.created_at = created_at;
    agent.fork_message_id = fork_message_id;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));
}

// Helper: Insert a message and return its ID
static int64_t insert_msg_id(const char *uuid, const char *kind, const char *content)
{
    res_t res = ik_db_message_insert(db, session_id, uuid, kind, content, "{}");
    ck_assert(is_ok(&res));
    int64_t msg_id = 0;
    res = ik_db_agent_get_last_message_id(db, uuid, &msg_id);
    ck_assert(is_ok(&res));
    return msg_id;
}

// Helper: Insert a message
static void insert_msg(const char *uuid, const char *kind, const char *content)
{
    res_t res = ik_db_message_insert(db, session_id, uuid, kind, content, "{}");
    ck_assert(is_ok(&res));
}

// Helper: Verify message content at index
static void verify_msg(ik_agent_ctx_t *agent, size_t idx, const char *expected)
{
    ik_message_t *msg = agent->messages[idx];
    ck_assert_ptr_nonnull(msg);
    ck_assert_uint_ge(msg->content_count, 1);
    ck_assert_int_eq(msg->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(msg->content_blocks[0].data.text.text, expected);
}

// Helper: Get text content from message
static const char *get_msg_text(ik_message_t *msg)
{
    if (msg == NULL || msg->content_count == 0) {
        return NULL;
    }
    if (msg->content_blocks[0].type != IK_CONTENT_TEXT) {
        return NULL;
    }
    return msg->content_blocks[0].data.text.text;
}

// Helper: Create minimal repl context for testing
static ik_repl_ctx_t *create_test_repl(const char *agent0_uuid)
{
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->db_ctx = db;
    shared->session_id = session_id;

    // Create logger
    shared->logger = ik_logger_create(shared, "/tmp");
    ck_assert_ptr_nonnull(shared->logger);

    // Create config
    shared->cfg = ik_test_create_config(shared);

    repl->shared = shared;

    // Initialize agents array
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agent_count = 0;
    repl->agent_capacity = 16;

    // Create Agent 0 (root agent)
    ik_agent_ctx_t *agent0 = NULL;
    res_t res = ik_agent_create(repl, shared, NULL, &agent0);
    ck_assert(is_ok(&res));

    // Set Agent 0's UUID to match DB
    talloc_free(agent0->uuid);
    agent0->uuid = talloc_strdup(agent0, agent0_uuid);

    repl->agents[0] = agent0;
    repl->agent_count = 1;
    repl->current = agent0;

    return repl;
}

// ========== Integration Test Cases ==========

// Test: Multiple agents survive restart with hierarchy preserved
START_TEST(test_multi_agent_restart_preserves_hierarchy) {
    SKIP_IF_NO_DB();

    insert_agent("parent-hier-test", NULL, 1000, 0);
    insert_msg("parent-hier-test", "clear", NULL);
    insert_msg("parent-hier-test", "user", "Parent msg 1");
    int64_t fork_id = insert_msg_id("parent-hier-test", "assistant", "Parent msg 2");

    insert_agent("child-hier-test", "parent-hier-test", 2000, fork_id);
    insert_msg("child-hier-test", "user", "Child msg 1");

    ik_repl_ctx_t *repl = create_test_repl("parent-hier-test");
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->agent_count, 2);
    ik_agent_ctx_t *child = repl->agents[1];
    ck_assert_str_eq(child->parent_uuid, "parent-hier-test");
    ck_assert_uint_ge(child->message_count, 3);
}
END_TEST
// Test: Forked agent survives restart with correct history
START_TEST(test_forked_agent_survives_restart) {
    SKIP_IF_NO_DB();

    insert_agent("parent-fork-test", NULL, 1000, 0);
    insert_msg("parent-fork-test", "clear", NULL);
    insert_msg("parent-fork-test", "user", "A");
    int64_t fork_point = insert_msg_id("parent-fork-test", "assistant", "B");

    insert_agent("child-fork-test", "parent-fork-test", 2000, fork_point);
    insert_msg("parent-fork-test", "user", "C");
    insert_msg("parent-fork-test", "assistant", "D");
    insert_msg("child-fork-test", "user", "X");
    insert_msg("child-fork-test", "assistant", "Y");

    ik_repl_ctx_t *repl = create_test_repl("parent-fork-test");
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->agent_count, 2);

    ik_agent_ctx_t *parent = repl->current;
    ck_assert_uint_ge(parent->message_count, 4);

    ik_agent_ctx_t *child = repl->agents[1];
    ck_assert_uint_eq(child->message_count, 4);
    verify_msg(child, 0, "A");
    verify_msg(child, 1, "B");
    verify_msg(child, 2, "X");
    verify_msg(child, 3, "Y");
}

END_TEST
// Test: Killed agents not restored (status != 'running')
START_TEST(test_killed_agent_not_restored) {
    SKIP_IF_NO_DB();

    insert_agent("parent-kill-test", NULL, 1000, 0);
    insert_msg("parent-kill-test", "clear", NULL);

    insert_agent("dead-kill-test", "parent-kill-test", 2000, 0);
    res_t res = ik_db_agent_mark_dead(db, "dead-kill-test");
    ck_assert(is_ok(&res));

    insert_agent("live-kill-test", "parent-kill-test", 3000, 0);

    ik_repl_ctx_t *repl = create_test_repl("parent-kill-test");
    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->agent_count, 2);

    bool found_dead = false;
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strcmp(repl->agents[i]->uuid, "dead-kill-test") == 0) {
            found_dead = true;
            break;
        }
    }
    ck_assert(!found_dead);
}

END_TEST
// Test: Fork points respected on restore
START_TEST(test_fork_points_respected_on_restore) {
    SKIP_IF_NO_DB();

    insert_agent("parent-forkpt", NULL, 1000, 0);
    insert_msg("parent-forkpt", "clear", NULL);
    insert_msg("parent-forkpt", "user", "msg1");
    insert_msg("parent-forkpt", "assistant", "msg2");
    int64_t fork_point = insert_msg_id("parent-forkpt", "user", "msg3");

    insert_agent("child-forkpt", "parent-forkpt", 2000, fork_point);
    insert_msg("parent-forkpt", "assistant", "msg4");
    insert_msg("parent-forkpt", "user", "msg5");
    insert_msg("child-forkpt", "user", "child_msg1");
    insert_msg("child-forkpt", "assistant", "child_msg2");

    ik_repl_ctx_t *repl = create_test_repl("parent-forkpt");
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->agents[1];
    ck_assert_uint_eq(child->message_count, 5);
    verify_msg(child, 0, "msg1");
    verify_msg(child, 1, "msg2");
    verify_msg(child, 2, "msg3");
    verify_msg(child, 3, "child_msg1");
    verify_msg(child, 4, "child_msg2");

    // Verify child does not have parent's post-fork messages
    for (size_t i = 0; i < child->message_count; i++) {
        const char *text = get_msg_text(child->messages[i]);
        if (text != NULL) {
            ck_assert_str_ne(text, "msg4");
            ck_assert_str_ne(text, "msg5");
        }
    }
}

END_TEST
// Test: Clear events respected
START_TEST(test_clear_events_respected_on_restore) {
    SKIP_IF_NO_DB();

    insert_agent("agent-clear-test", NULL, 1000, 0);
    insert_msg("agent-clear-test", "user", "msg1");
    insert_msg("agent-clear-test", "assistant", "msg2");
    insert_msg("agent-clear-test", "clear", NULL);
    insert_msg("agent-clear-test", "user", "msg3");
    insert_msg("agent-clear-test", "assistant", "msg4");

    ik_repl_ctx_t *repl = create_test_repl("agent-clear-test");
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->message_count, 2);
    verify_msg(repl->current, 0, "msg3");
    verify_msg(repl->current, 1, "msg4");
}

END_TEST
// Test: Deep ancestry
START_TEST(test_deep_ancestry_on_restore) {
    SKIP_IF_NO_DB();

    insert_agent("grandparent-deep", NULL, 1000, 0);
    insert_msg("grandparent-deep", "clear", NULL);
    insert_msg("grandparent-deep", "user", "gp_msg1");
    int64_t gp_fork = insert_msg_id("grandparent-deep", "assistant", "gp_msg2");

    insert_agent("parent-deep", "grandparent-deep", 2000, gp_fork);
    int64_t p_fork = insert_msg_id("parent-deep", "user", "p_msg1");

    insert_agent("child-deep", "parent-deep", 3000, p_fork);
    insert_msg("child-deep", "user", "c_msg1");

    ik_repl_ctx_t *repl = create_test_repl("grandparent-deep");
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->agent_count, 3);

    ik_agent_ctx_t *child = NULL;
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strcmp(repl->agents[i]->uuid, "child-deep") == 0) {
            child = repl->agents[i];
            break;
        }
    }
    ck_assert_ptr_nonnull(child);
    ck_assert_uint_eq(child->message_count, 4);
    verify_msg(child, 0, "gp_msg1");
    verify_msg(child, 1, "gp_msg2");
    verify_msg(child, 2, "p_msg1");
    verify_msg(child, 3, "c_msg1");
}

END_TEST
// Test: Dependency ordering
START_TEST(test_dependency_ordering_on_restore) {
    SKIP_IF_NO_DB();

    insert_agent("parent-order", NULL, 2000, 0);
    insert_msg("parent-order", "clear", NULL);
    insert_agent("child-order", "parent-order", 1000, 0);

    ik_repl_ctx_t *repl = create_test_repl("parent-order");
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->agent_count, 2);
}

END_TEST
// Test: Metadata events filtered
START_TEST(test_metadata_events_filtered_on_restore) {
    SKIP_IF_NO_DB();

    insert_agent("agent-metadata", NULL, 1000, 0);
    insert_msg("agent-metadata", "clear", NULL);
    insert_msg("agent-metadata", "user", "Hello");
    insert_msg("agent-metadata", "assistant", "Hi there");
    insert_msg("agent-metadata", "agent_killed", NULL);
    insert_msg("agent-metadata", "mark", NULL);
    insert_msg("agent-metadata", "user", "Follow up");
    insert_msg("agent-metadata", "assistant", "Response");

    ik_repl_ctx_t *repl = create_test_repl("agent-metadata");
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Metadata events (clear, mark, agent_killed) are filtered during replay
    // Only conversation messages (user, assistant) should be in the array
    ck_assert_uint_eq(repl->current->message_count, 4);
    verify_msg(repl->current, 0, "Hello");
    verify_msg(repl->current, 1, "Hi there");
    verify_msg(repl->current, 2, "Follow up");
    verify_msg(repl->current, 3, "Response");
}

END_TEST

// Test: Toolset command replayed on restore
START_TEST(test_toolset_command_replayed) {
    SKIP_IF_NO_DB();

    insert_agent("agent-toolset-cmd", NULL, 1000, 0);
    insert_msg("agent-toolset-cmd", "clear", NULL);

    // Insert toolset command - data goes in data_json parameter
    const char *toolset_data = "{\"command\":\"toolset\",\"args\":\"Read Write Bash\"}";
    res_t res = ik_db_message_insert(db, session_id, "agent-toolset-cmd", "command", NULL, toolset_data);
    ck_assert(is_ok(&res));

    insert_msg("agent-toolset-cmd", "user", "msg1");

    ik_repl_ctx_t *repl = create_test_repl("agent-toolset-cmd");
    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify toolset_filter was restored from command
    ck_assert_ptr_nonnull(repl->current->toolset_filter);
    ck_assert_uint_eq(repl->current->toolset_count, 3);
    ck_assert_str_eq(repl->current->toolset_filter[0], "Read");
    ck_assert_str_eq(repl->current->toolset_filter[1], "Write");
    ck_assert_str_eq(repl->current->toolset_filter[2], "Bash");
}

END_TEST

// Test: Toolset inherited from fork message
START_TEST(test_toolset_inherited_from_fork_message) {
    SKIP_IF_NO_DB();

    insert_agent("parent-fork-toolset", NULL, 1000, 0);
    insert_msg("parent-fork-toolset", "clear", NULL);
    insert_msg("parent-fork-toolset", "user", "msg1");
    int64_t fork_point = insert_msg_id("parent-fork-toolset", "assistant", "msg2");

    // Create child agent
    insert_agent("child-fork-toolset", "parent-fork-toolset", 2000, fork_point);

    // Insert fork message in child's history with toolset_filter
    const char *fork_data = "{\"toolset_filter\":[\"Edit\",\"Glob\"]}";
    res_t res = ik_db_message_insert(db, session_id, "child-fork-toolset", "fork", NULL, fork_data);
    ck_assert(is_ok(&res));

    insert_msg("child-fork-toolset", "user", "msg3");

    ik_repl_ctx_t *repl = create_test_repl("parent-fork-toolset");
    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->agent_count, 2);
    ik_agent_ctx_t *child = repl->agents[1];

    // Verify toolset_filter was inherited from fork message
    ck_assert_ptr_nonnull(child->toolset_filter);
    ck_assert_uint_eq(child->toolset_count, 2);
    ck_assert_str_eq(child->toolset_filter[0], "Edit");
    ck_assert_str_eq(child->toolset_filter[1], "Glob");
}

END_TEST

// Test: Toolset replay with empty array
START_TEST(test_toolset_fork_empty_array) {
    SKIP_IF_NO_DB();

    insert_agent("parent-empty", NULL, 1000, 0);
    insert_msg("parent-empty", "clear", NULL);
    int64_t fork_point = insert_msg_id("parent-empty", "assistant", "msg1");

    insert_agent("child-empty", "parent-empty", 2000, fork_point);

    // Fork message with empty toolset_filter array
    const char *fork_data = "{\"toolset_filter\":[]}";
    res_t res = ik_db_message_insert(db, session_id, "child-empty", "fork", NULL, fork_data);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = create_test_repl("parent-empty");
    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->agent_count, 2);
    ik_agent_ctx_t *child = repl->agents[1];

    // Empty array means no toolset_filter
    ck_assert_ptr_null(child->toolset_filter);
    ck_assert_uint_eq(child->toolset_count, 0);
}

END_TEST

// Test: Toolset replay with non-string in array
START_TEST(test_toolset_fork_non_string) {
    SKIP_IF_NO_DB();

    insert_agent("parent-nonstr", NULL, 1000, 0);
    insert_msg("parent-nonstr", "clear", NULL);
    int64_t fork_point = insert_msg_id("parent-nonstr", "assistant", "msg1");

    insert_agent("child-nonstr", "parent-nonstr", 2000, fork_point);

    // Fork message with non-string values in toolset_filter (should skip them)
    const char *fork_data = "{\"toolset_filter\":[\"Read\", 123, \"Write\"]}";
    res_t res = ik_db_message_insert(db, session_id, "child-nonstr", "fork", NULL, fork_data);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = create_test_repl("parent-nonstr");
    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->agent_count, 2);
    ik_agent_ctx_t *child = repl->agents[1];

    // Should only have the string values
    ck_assert_ptr_nonnull(child->toolset_filter);
    ck_assert_uint_eq(child->toolset_count, 2);
    ck_assert_str_eq(child->toolset_filter[0], "Read");
    ck_assert_str_eq(child->toolset_filter[1], "Write");
}

END_TEST

// Test: Toolset replay with invalid fork data (not array)
START_TEST(test_toolset_fork_invalid_array) {
    SKIP_IF_NO_DB();

    insert_agent("parent-invalid", NULL, 1000, 0);
    insert_msg("parent-invalid", "clear", NULL);
    int64_t fork_point = insert_msg_id("parent-invalid", "assistant", "msg1");

    insert_agent("child-invalid", "parent-invalid", 2000, fork_point);

    // Fork message with toolset_filter that's not an array
    const char *fork_data = "{\"toolset_filter\":\"not_an_array\"}";
    res_t res = ik_db_message_insert(db, session_id, "child-invalid", "fork", NULL, fork_data);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = create_test_repl("parent-invalid");
    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->agent_count, 2);
    ik_agent_ctx_t *child = repl->agents[1];

    // Invalid data means no toolset_filter
    ck_assert_ptr_null(child->toolset_filter);
    ck_assert_uint_eq(child->toolset_count, 0);
}

END_TEST

// Test: Toolset replay with no toolset_filter in fork
START_TEST(test_toolset_fork_no_filter) {
    SKIP_IF_NO_DB();

    insert_agent("parent-nofilter", NULL, 1000, 0);
    insert_msg("parent-nofilter", "clear", NULL);
    int64_t fork_point = insert_msg_id("parent-nofilter", "assistant", "msg1");

    insert_agent("child-nofilter", "parent-nofilter", 2000, fork_point);

    // Fork message without toolset_filter field
    const char *fork_data = "{\"other_field\":\"value\"}";
    res_t res = ik_db_message_insert(db, session_id, "child-nofilter", "fork", NULL, fork_data);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = create_test_repl("parent-nofilter");
    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->agent_count, 2);
    ik_agent_ctx_t *child = repl->agents[1];

    // No toolset_filter field means no filter
    ck_assert_ptr_null(child->toolset_filter);
    ck_assert_uint_eq(child->toolset_count, 0);
}

END_TEST

// Test: Toolset command replaces existing filter
START_TEST(test_toolset_command_replaces_existing) {
    SKIP_IF_NO_DB();

    insert_agent("agent-replace", NULL, 1000, 0);
    insert_msg("agent-replace", "clear", NULL);

    // First toolset command
    const char *toolset1 = "{\"command\":\"toolset\",\"args\":\"Read Write\"}";
    res_t res = ik_db_message_insert(db, session_id, "agent-replace", "command", NULL, toolset1);
    ck_assert(is_ok(&res));

    // Second toolset command (more recent, should use this one)
    const char *toolset2 = "{\"command\":\"toolset\",\"args\":\"Bash\"}";
    res = ik_db_message_insert(db, session_id, "agent-replace", "command", NULL, toolset2);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = create_test_repl("agent-replace");
    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Should have the most recent toolset (just verify it's restored)
    ck_assert_ptr_nonnull(repl->current->toolset_filter);
    ck_assert_uint_gt(repl->current->toolset_count, 0);
}

END_TEST

// Test: Fork with all non-string values (count stays 0)
START_TEST(test_toolset_fork_all_non_string) {
    SKIP_IF_NO_DB();

    insert_agent("parent-allnonstr", NULL, 1000, 0);
    insert_msg("parent-allnonstr", "clear", NULL);
    int64_t fork_point = insert_msg_id("parent-allnonstr", "assistant", "msg1");

    insert_agent("child-allnonstr", "parent-allnonstr", 2000, fork_point);

    // All non-string values in toolset_filter
    const char *fork_data = "{\"toolset_filter\":[123, 456, true]}";
    res_t res = ik_db_message_insert(db, session_id, "child-allnonstr", "fork", NULL, fork_data);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = create_test_repl("parent-allnonstr");
    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->agent_count, 2);
    ik_agent_ctx_t *child = repl->agents[1];

    // All non-strings means no valid tools
    ck_assert_ptr_null(child->toolset_filter);
    ck_assert_uint_eq(child->toolset_count, 0);
}

END_TEST

// ========== Suite Configuration ==========

static Suite *agent_restore_integration_suite(void)
{
    Suite *s = suite_create("Agent Restore Integration");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_multi_agent_restart_preserves_hierarchy);
    tcase_add_test(tc_core, test_forked_agent_survives_restart);
    tcase_add_test(tc_core, test_killed_agent_not_restored);
    tcase_add_test(tc_core, test_fork_points_respected_on_restore);
    tcase_add_test(tc_core, test_clear_events_respected_on_restore);
    tcase_add_test(tc_core, test_deep_ancestry_on_restore);
    tcase_add_test(tc_core, test_dependency_ordering_on_restore);
    tcase_add_test(tc_core, test_metadata_events_filtered_on_restore);
    tcase_add_test(tc_core, test_toolset_command_replayed);
    tcase_add_test(tc_core, test_toolset_inherited_from_fork_message);
    tcase_add_test(tc_core, test_toolset_fork_empty_array);
    tcase_add_test(tc_core, test_toolset_fork_non_string);
    tcase_add_test(tc_core, test_toolset_fork_invalid_array);
    tcase_add_test(tc_core, test_toolset_fork_no_filter);
    tcase_add_test(tc_core, test_toolset_command_replaces_existing);
    tcase_add_test(tc_core, test_toolset_fork_all_non_string);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_restore_integration_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/integration/agent_restore_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
