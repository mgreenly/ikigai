/**
 * @file agent_restore_core_tests.c
 * @brief Core test functions for agent restore functionality
 */

#include "agent_restore_core_tests.h"
#include "agent_restore_test_helpers.h"

#include "../../../src/repl/agent_restore.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/message.h"
#include "../../../src/scrollback.h"

#include <check.h>
#include <talloc.h>

// Helper macro to skip test if DB not available
#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// Test: restore_agents queries running agents from DB
START_TEST(test_restore_agents_queries_running_agents) {
    SKIP_IF_NO_DB();

    // Insert Agent 0 (root)
    insert_agent("agent0-test-restore1", NULL, 1000, 0);
    insert_message("agent0-test-restore1", "clear", NULL);

    // Insert child agents
    insert_agent("child1-test-restore1", "agent0-test-restore1", 2000, 0);
    insert_agent("child2-test-restore1", "agent0-test-restore1", 3000, 0);

    // Create repl and override Agent 0's UUID
    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-test-restore1");

    // Call restore_agents
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify all running agents are restored
    ck_assert_uint_eq(repl->agent_count, 3);
}
END_TEST

// Test: restore_agents sorts by created_at (oldest first)
START_TEST(test_restore_agents_sorts_by_created_at) {
    SKIP_IF_NO_DB();

    // Insert root
    insert_agent("root-sort-test-12345", NULL, 1000, 0);
    insert_message("root-sort-test-12345", "clear", NULL);

    // Insert children out of order (newer first, then older)
    insert_agent("newer-child-sort-te", "root-sort-test-12345", 3000, 0);
    insert_agent("older-child-sort-te", "root-sort-test-12345", 2000, 0);

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "root-sort-test-12345");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify 3 agents total
    ck_assert_uint_eq(repl->agent_count, 3);

    // Verify sorting: older child should be restored before newer
    // Agent 0 is at index 0, older-child should be at 1, newer-child at 2
    ck_assert_str_eq(repl->agents[1]->uuid, "older-child-sort-te");
    ck_assert_str_eq(repl->agents[2]->uuid, "newer-child-sort-te");
}
END_TEST

// Test: restore_agents skips Agent 0 (parent_uuid=NULL)
START_TEST(test_restore_agents_skips_none_restores_all_running) {
    SKIP_IF_NO_DB();

    // Insert Agent 0
    insert_agent("agent0-skip-test-12", NULL, 1000, 0);
    insert_message("agent0-skip-test-12", "clear", NULL);

    // Insert child
    insert_agent("child1-skip-test-12", "agent0-skip-test-12", 2000, 0);

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-skip-test-12");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Agent 0 should not be duplicated, child should be added
    ck_assert_uint_eq(repl->agent_count, 2);
}
END_TEST

// Test: restore_agents handles Agent 0 specially
START_TEST(test_restore_agents_handles_agent0_specially) {
    SKIP_IF_NO_DB();

    // Insert Agent 0 with some history
    insert_agent("agent0-special-test", NULL, 1000, 0);
    insert_message("agent0-special-test", "clear", NULL);
    insert_message("agent0-special-test", "user", "Hello");
    insert_message("agent0-special-test", "assistant", "Hi there");

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-special-test");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Agent 0 should not be duplicated
    ck_assert_uint_eq(repl->agent_count, 1);

    // Agent 0's conversation should have messages
    ck_assert_uint_ge(repl->current->message_count, 2);
}
END_TEST

// Test: restore_agents populates conversation from replay
START_TEST(test_restore_agents_populates_conversation) {
    SKIP_IF_NO_DB();

    // Insert Agent 0
    insert_agent("agent0-conv-test-12", NULL, 1000, 0);
    insert_message("agent0-conv-test-12", "clear", NULL);
    insert_message("agent0-conv-test-12", "user", "Test message");
    insert_message("agent0-conv-test-12", "assistant", "Response");

    // Get fork point
    int64_t fork_id = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "agent0-conv-test-12", &fork_id);
    ck_assert(is_ok(&res));

    // Insert child with its own messages
    insert_agent("child1-conv-test-12", "agent0-conv-test-12", 2000, fork_id);
    insert_message("child1-conv-test-12", "user", "Child message");

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-conv-test-12");

    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify child exists and has conversation
    ck_assert_uint_eq(repl->agent_count, 2);
    ik_agent_ctx_t *child = repl->agents[1];
    ck_assert_uint_ge(child->message_count, 0);

    // Child should have parent's messages plus its own
    ck_assert_uint_ge(child->message_count, 3);
}
END_TEST

// Test: restore_agents populates scrollback from replay
START_TEST(test_restore_agents_populates_scrollback) {
    SKIP_IF_NO_DB();

    // Insert Agent 0 with messages
    insert_agent("agent0-scroll-test1", NULL, 1000, 0);
    insert_message("agent0-scroll-test1", "clear", NULL);
    insert_message("agent0-scroll-test1", "user", "User input");
    insert_message("agent0-scroll-test1", "assistant", "AI response");

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-scroll-test1");

    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Scrollback should have content
    ck_assert_ptr_nonnull(repl->current->scrollback);
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(line_count, 0);
}
END_TEST

// Test: restore_agents handles mark events (marks are stored in DB but not processed by agent_replay)
// Note: The agent_replay module returns raw messages without processing marks.
// Mark processing happens at a higher level when needed.
START_TEST(test_restore_agents_handles_mark_events) {
    SKIP_IF_NO_DB();

    // Insert Agent 0 with mark events
    insert_agent("agent0-marks-test12", NULL, 1000, 0);
    insert_message("agent0-marks-test12", "clear", NULL);
    insert_message("agent0-marks-test12", "user", "Before mark");
    // Insert mark event
    res_t res = ik_db_message_insert(db, session_id, "agent0-marks-test12",
                                     "mark", NULL, "{\"label\":\"checkpoint1\"}");
    ck_assert(is_ok(&res));
    insert_message("agent0-marks-test12", "user", "After mark");

    ik_repl_ctx_t *repl = create_test_repl();
    talloc_free(repl->current->uuid);
    repl->current->uuid = talloc_strdup(repl->current, "agent0-marks-test12");

    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    // Verify restore succeeded (mark events in DB are valid)
    ck_assert_uint_eq(repl->agent_count, 1);
}
END_TEST
