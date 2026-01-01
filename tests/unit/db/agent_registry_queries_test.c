/**
 * @file agent_registry_queries_test.c
 * @brief Agent registry query tests
 *
 * Tests for agent registry query functions: get, list_running, get_children, get_parent.
 * Split from agent_registry_test.c to meet file size limits.
 */

#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../../src/agent.h"
#include "../../test_utils.h"
#include <check.h>
#include <libpq-fe.h>
#include <talloc.h>
#include <string.h>
#include <time.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;

// Suite-level setup: Create and migrate database (runs once)
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
    }
}

// Per-test teardown: Rollback and disconnect
static void test_teardown(void)
{
    if (db != NULL) {
        ik_test_db_rollback(db);
    }
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
    db = NULL;
}

// Skip macro for tests when DB not available
#define SKIP_IF_NO_DB() do { \
            if (!db_available) { \
                return; \
            } \
} while (0)

// ========== Query Tests ==========

// Test: get returns correct row for existing UUID
START_TEST(test_get_returns_correct_row) {
    SKIP_IF_NO_DB();

    // Insert an agent
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "get-test-uuid-123");
    agent.name = talloc_strdup(test_ctx, "Get Test Agent");
    agent.parent_uuid = NULL;
    agent.created_at = 1234567890;
    agent.fork_message_id = 42;

    res_t insert_res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&insert_res));

    // Get the agent
    ik_db_agent_row_t *row = NULL;
    res_t get_res = ik_db_agent_get(db, test_ctx, agent.uuid, &row);
    ck_assert(is_ok(&get_res));
    ck_assert(row != NULL);

    // Verify fields
    ck_assert_str_eq(row->uuid, "get-test-uuid-123");
    ck_assert_str_eq(row->name, "Get Test Agent");
    ck_assert(row->parent_uuid == NULL);
    ck_assert_str_eq(row->fork_message_id, "42");
    ck_assert_str_eq(row->status, "running");
    ck_assert_int_eq(row->created_at, 1234567890);
    ck_assert_int_eq(row->ended_at, 0);
}
END_TEST
// Test: get returns error for non-existent UUID
START_TEST(test_get_nonexistent_uuid) {
    SKIP_IF_NO_DB();

    ik_db_agent_row_t *row = NULL;
    res_t get_res = ik_db_agent_get(db, test_ctx, "nonexistent-uuid", &row);
    ck_assert(is_err(&get_res));
}

END_TEST
// Test: list_running returns only status='running' agents
START_TEST(test_list_running_only_running) {
    SKIP_IF_NO_DB();

    // Insert running agent
    ik_agent_ctx_t running = {0};
    running.uuid = talloc_strdup(test_ctx, "running-uuid-1");
    running.name = talloc_strdup(test_ctx, "Running Agent");
    running.parent_uuid = NULL;
    running.created_at = time(NULL);
    running.fork_message_id = 0;

    res_t running_res = ik_db_agent_insert(db, &running);
    ck_assert(is_ok(&running_res));

    // Insert another running agent
    ik_agent_ctx_t running2 = {0};
    running2.uuid = talloc_strdup(test_ctx, "running-uuid-2");
    running2.name = talloc_strdup(test_ctx, "Running Agent 2");
    running2.parent_uuid = NULL;
    running2.created_at = time(NULL);
    running2.fork_message_id = 0;

    res_t running2_res = ik_db_agent_insert(db, &running2);
    ck_assert(is_ok(&running2_res));

    // Insert dead agent
    ik_agent_ctx_t dead = {0};
    dead.uuid = talloc_strdup(test_ctx, "dead-uuid-1");
    dead.name = talloc_strdup(test_ctx, "Dead Agent");
    dead.parent_uuid = NULL;
    dead.created_at = time(NULL);
    dead.fork_message_id = 0;

    res_t dead_res = ik_db_agent_insert(db, &dead);
    ck_assert(is_ok(&dead_res));

    // Mark dead agent
    res_t mark_res = ik_db_agent_mark_dead(db, dead.uuid);
    ck_assert(is_ok(&mark_res));

    // List running agents
    ik_db_agent_row_t **rows = NULL;
    size_t count = 0;
    res_t list_res = ik_db_agent_list_running(db, test_ctx, &rows, &count);
    ck_assert(is_ok(&list_res));
    ck_assert_int_eq((int)count, 2);
    ck_assert(rows != NULL);

    // Verify only running agents returned
    bool found_running1 = false;
    bool found_running2 = false;
    bool found_dead = false;

    for (size_t i = 0; i < count; i++) {
        if (strcmp(rows[i]->uuid, "running-uuid-1") == 0) found_running1 = true;
        if (strcmp(rows[i]->uuid, "running-uuid-2") == 0) found_running2 = true;
        if (strcmp(rows[i]->uuid, "dead-uuid-1") == 0) found_dead = true;
    }

    ck_assert(found_running1);
    ck_assert(found_running2);
    ck_assert(!found_dead);
}

END_TEST
// Test: list_running excludes dead agents
START_TEST(test_list_running_excludes_dead) {
    SKIP_IF_NO_DB();

    // Insert and immediately kill an agent
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "killed-uuid");
    agent.name = talloc_strdup(test_ctx, "Killed Agent");
    agent.parent_uuid = NULL;
    agent.created_at = time(NULL);
    agent.fork_message_id = 0;

    res_t insert_res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&insert_res));

    res_t mark_res = ik_db_agent_mark_dead(db, agent.uuid);
    ck_assert(is_ok(&mark_res));

    // List running agents - should be empty
    ik_db_agent_row_t **rows = NULL;
    size_t count = 0;
    res_t list_res = ik_db_agent_list_running(db, test_ctx, &rows, &count);
    ck_assert(is_ok(&list_res));
    ck_assert_int_eq((int)count, 0);
}

END_TEST
// Test: get_children returns children ordered by created_at
START_TEST(test_get_children_ordered) {
    SKIP_IF_NO_DB();

    // Insert parent
    ik_agent_ctx_t parent = {0};
    parent.uuid = talloc_strdup(test_ctx, "parent-children-test");
    parent.name = talloc_strdup(test_ctx, "Parent");
    parent.parent_uuid = NULL;
    parent.created_at = time(NULL);
    parent.fork_message_id = 0;

    res_t parent_res = ik_db_agent_insert(db, &parent);
    ck_assert(is_ok(&parent_res));

    // Insert children with different created_at
    ik_agent_ctx_t child1 = {0};
    child1.uuid = talloc_strdup(test_ctx, "child-1");
    child1.name = talloc_strdup(test_ctx, "Child 1");
    child1.parent_uuid = talloc_strdup(test_ctx, "parent-children-test");
    child1.created_at = 1000;
    child1.fork_message_id = 10;

    res_t child1_res = ik_db_agent_insert(db, &child1);
    ck_assert(is_ok(&child1_res));

    ik_agent_ctx_t child2 = {0};
    child2.uuid = talloc_strdup(test_ctx, "child-2");
    child2.name = talloc_strdup(test_ctx, "Child 2");
    child2.parent_uuid = talloc_strdup(test_ctx, "parent-children-test");
    child2.created_at = 2000;
    child2.fork_message_id = 20;

    res_t child2_res = ik_db_agent_insert(db, &child2);
    ck_assert(is_ok(&child2_res));

    ik_agent_ctx_t child3 = {0};
    child3.uuid = talloc_strdup(test_ctx, "child-3");
    child3.name = talloc_strdup(test_ctx, "Child 3");
    child3.parent_uuid = talloc_strdup(test_ctx, "parent-children-test");
    child3.created_at = 1500;
    child3.fork_message_id = 15;

    res_t child3_res = ik_db_agent_insert(db, &child3);
    ck_assert(is_ok(&child3_res));

    // Get children
    ik_db_agent_row_t **rows = NULL;
    size_t count = 0;
    res_t get_res = ik_db_agent_get_children(db, test_ctx, parent.uuid, &rows, &count);
    ck_assert(is_ok(&get_res));
    ck_assert_int_eq((int)count, 3);

    // Verify ordered by created_at
    ck_assert_str_eq(rows[0]->uuid, "child-1");  // 1000
    ck_assert_str_eq(rows[1]->uuid, "child-3");  // 1500
    ck_assert_str_eq(rows[2]->uuid, "child-2");  // 2000
}

END_TEST
// Test: get_children returns empty for agent with no children
START_TEST(test_get_children_empty) {
    SKIP_IF_NO_DB();

    // Insert agent with no children
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, "childless-agent");
    agent.name = talloc_strdup(test_ctx, "Childless");
    agent.parent_uuid = NULL;
    agent.created_at = time(NULL);
    agent.fork_message_id = 0;

    res_t insert_res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&insert_res));

    // Get children
    ik_db_agent_row_t **rows = NULL;
    size_t count = 0;
    res_t get_res = ik_db_agent_get_children(db, test_ctx, agent.uuid, &rows, &count);
    ck_assert(is_ok(&get_res));
    ck_assert_int_eq((int)count, 0);
}

END_TEST
// Test: get_parent returns parent row for child agent
START_TEST(test_get_parent_returns_parent) {
    SKIP_IF_NO_DB();

    // Insert parent
    ik_agent_ctx_t parent = {0};
    parent.uuid = talloc_strdup(test_ctx, "parent-get-test");
    parent.name = talloc_strdup(test_ctx, "Parent Agent");
    parent.parent_uuid = NULL;
    parent.created_at = 1000;
    parent.fork_message_id = 0;

    res_t parent_res = ik_db_agent_insert(db, &parent);
    ck_assert(is_ok(&parent_res));

    // Insert child
    ik_agent_ctx_t child = {0};
    child.uuid = talloc_strdup(test_ctx, "child-get-parent-test");
    child.name = talloc_strdup(test_ctx, "Child Agent");
    child.parent_uuid = talloc_strdup(test_ctx, "parent-get-test");
    child.created_at = 2000;
    child.fork_message_id = 99;

    res_t child_res = ik_db_agent_insert(db, &child);
    ck_assert(is_ok(&child_res));

    // Get parent
    ik_db_agent_row_t *parent_row = NULL;
    res_t get_res = ik_db_agent_get_parent(db, test_ctx, child.uuid, &parent_row);
    ck_assert(is_ok(&get_res));
    ck_assert(parent_row != NULL);

    // Verify parent fields
    ck_assert_str_eq(parent_row->uuid, "parent-get-test");
    ck_assert_str_eq(parent_row->name, "Parent Agent");
}

END_TEST
// Test: get_parent returns NULL for root agent (no parent)
START_TEST(test_get_parent_null_for_root) {
    SKIP_IF_NO_DB();

    // Insert root agent
    ik_agent_ctx_t root = {0};
    root.uuid = talloc_strdup(test_ctx, "root-agent");
    root.name = talloc_strdup(test_ctx, "Root Agent");
    root.parent_uuid = NULL;
    root.created_at = time(NULL);
    root.fork_message_id = 0;

    res_t insert_res = ik_db_agent_insert(db, &root);
    ck_assert(is_ok(&insert_res));

    // Get parent - should be NULL
    ik_db_agent_row_t *parent_row = NULL;
    res_t get_res = ik_db_agent_get_parent(db, test_ctx, root.uuid, &parent_row);
    ck_assert(is_ok(&get_res));
    ck_assert(parent_row == NULL);
}

END_TEST
// Test: get_parent allows iterative chain walking
START_TEST(test_get_parent_chain_walking) {
    SKIP_IF_NO_DB();

    // Insert grandparent
    ik_agent_ctx_t grandparent = {0};
    grandparent.uuid = talloc_strdup(test_ctx, "grandparent");
    grandparent.name = talloc_strdup(test_ctx, "Grandparent");
    grandparent.parent_uuid = NULL;
    grandparent.created_at = 1000;
    grandparent.fork_message_id = 0;

    res_t gp_res = ik_db_agent_insert(db, &grandparent);
    ck_assert(is_ok(&gp_res));

    // Insert parent
    ik_agent_ctx_t parent = {0};
    parent.uuid = talloc_strdup(test_ctx, "parent-chain");
    parent.name = talloc_strdup(test_ctx, "Parent");
    parent.parent_uuid = talloc_strdup(test_ctx, "grandparent");
    parent.created_at = 2000;
    parent.fork_message_id = 10;

    res_t p_res = ik_db_agent_insert(db, &parent);
    ck_assert(is_ok(&p_res));

    // Insert child
    ik_agent_ctx_t child = {0};
    child.uuid = talloc_strdup(test_ctx, "child-chain");
    child.name = talloc_strdup(test_ctx, "Child");
    child.parent_uuid = talloc_strdup(test_ctx, "parent-chain");
    child.created_at = 3000;
    child.fork_message_id = 20;

    res_t c_res = ik_db_agent_insert(db, &child);
    ck_assert(is_ok(&c_res));

    // Walk chain: child -> parent -> grandparent -> NULL
    ik_db_agent_row_t *row1 = NULL;
    res_t res1 = ik_db_agent_get_parent(db, test_ctx, "child-chain", &row1);
    ck_assert(is_ok(&res1));
    ck_assert(row1 != NULL);
    ck_assert_str_eq(row1->uuid, "parent-chain");

    ik_db_agent_row_t *row2 = NULL;
    res_t res2 = ik_db_agent_get_parent(db, test_ctx, row1->uuid, &row2);
    ck_assert(is_ok(&res2));
    ck_assert(row2 != NULL);
    ck_assert_str_eq(row2->uuid, "grandparent");

    ik_db_agent_row_t *row3 = NULL;
    res_t res3 = ik_db_agent_get_parent(db, test_ctx, row2->uuid, &row3);
    ck_assert(is_ok(&res3));
    ck_assert(row3 == NULL);  // Root has no parent
}

END_TEST

// ========== Suite Configuration ==========

static Suite *agent_registry_queries_suite(void)
{
    Suite *s = suite_create("Agent Registry Queries");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_get_returns_correct_row);
    tcase_add_test(tc_core, test_get_nonexistent_uuid);
    tcase_add_test(tc_core, test_list_running_only_running);
    tcase_add_test(tc_core, test_list_running_excludes_dead);
    tcase_add_test(tc_core, test_get_children_ordered);
    tcase_add_test(tc_core, test_get_children_empty);
    tcase_add_test(tc_core, test_get_parent_returns_parent);
    tcase_add_test(tc_core, test_get_parent_null_for_root);
    tcase_add_test(tc_core, test_get_parent_chain_walking);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_registry_queries_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
