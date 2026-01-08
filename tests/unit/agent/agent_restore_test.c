/**
 * @file agent_restore_test.c
 * @brief Tests for ik_agent_restore() function
 *
 * Tests the restoration of agent context from database row data.
 */

#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/db/agent.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>

// ========== ik_agent_restore() Tests ==========

// Test: ik_agent_restore() creates agent from DB row successfully
START_TEST(test_agent_restore_creates_from_db_row) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    // Create mock DB row
    ik_db_agent_row_t row = {0};
    row.uuid = talloc_strdup(ctx, "test-uuid-123456789012");
    row.name = talloc_strdup(ctx, "Test Agent");
    row.parent_uuid = talloc_strdup(ctx, "parent-uuid-12345678");
    row.fork_message_id = talloc_strdup(ctx, "42");
    row.created_at = 1234567890;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(ctx, shared, &row, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->scrollback);
    ck_assert_ptr_nonnull(agent->layer_cake);
    // Messages array starts empty (NULL) with count 0
    ck_assert_uint_eq(agent->message_count, 0);

    talloc_free(ctx);
}
END_TEST
// Test: ik_agent_restore() uses row UUID, not generated
START_TEST(test_agent_restore_uses_row_uuid_not_generated) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_db_agent_row_t row = {0};
    row.uuid = talloc_strdup(ctx, "test-uuid-123456789012");
    row.name = NULL;
    row.parent_uuid = NULL;
    row.fork_message_id = NULL;
    row.created_at = 1234567890;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(ctx, shared, &row, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_str_eq(agent->uuid, "test-uuid-123456789012");

    talloc_free(ctx);
}

END_TEST
// Test: ik_agent_restore() sets fork_message_id from row
START_TEST(test_agent_restore_sets_fork_message_id) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_db_agent_row_t row = {0};
    row.uuid = talloc_strdup(ctx, "test-uuid-fork-id-test");
    row.name = NULL;
    row.parent_uuid = talloc_strdup(ctx, "parent-uuid");
    row.fork_message_id = talloc_strdup(ctx, "42");
    row.created_at = 1234567890;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(ctx, shared, &row, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_int_eq(agent->fork_message_id, 42);

    talloc_free(ctx);
}

END_TEST
// Test: ik_agent_restore() sets parent_uuid from row
START_TEST(test_agent_restore_sets_parent_uuid) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_db_agent_row_t row = {0};
    row.uuid = talloc_strdup(ctx, "test-uuid-parent-test1");
    row.name = NULL;
    row.parent_uuid = talloc_strdup(ctx, "parent-uuid-456789012");
    row.fork_message_id = NULL;
    row.created_at = 1234567890;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(ctx, shared, &row, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_str_eq(agent->parent_uuid, "parent-uuid-456789012");

    talloc_free(ctx);
}

END_TEST
// Test: ik_agent_restore() sets created_at from row
START_TEST(test_agent_restore_sets_created_at) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_db_agent_row_t row = {0};
    row.uuid = talloc_strdup(ctx, "test-uuid-created-at-t");
    row.name = NULL;
    row.parent_uuid = NULL;
    row.fork_message_id = NULL;
    row.created_at = 1234567890;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(ctx, shared, &row, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_int_eq(agent->created_at, 1234567890);

    talloc_free(ctx);
}

END_TEST
// Test: ik_agent_restore() sets name from row if present
START_TEST(test_agent_restore_sets_name_if_present) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_db_agent_row_t row = {0};
    row.uuid = talloc_strdup(ctx, "test-uuid-name-present");
    row.name = talloc_strdup(ctx, "My Agent");
    row.parent_uuid = NULL;
    row.fork_message_id = NULL;
    row.created_at = 1234567890;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(ctx, shared, &row, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_str_eq(agent->name, "My Agent");

    talloc_free(ctx);
}

END_TEST
// Test: ik_agent_restore() sets name to NULL if not present in row
START_TEST(test_agent_restore_null_name_if_not_present) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_db_agent_row_t row = {0};
    row.uuid = talloc_strdup(ctx, "test-uuid-null-name-12");
    row.name = NULL;
    row.parent_uuid = NULL;
    row.fork_message_id = NULL;
    row.created_at = 1234567890;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(ctx, shared, &row, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->name);

    talloc_free(ctx);
}

END_TEST

static Suite *agent_restore_suite(void)
{
    Suite *s = suite_create("Agent Restore");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_test(tc_core, test_agent_restore_creates_from_db_row);
    tcase_add_test(tc_core, test_agent_restore_uses_row_uuid_not_generated);
    tcase_add_test(tc_core, test_agent_restore_sets_fork_message_id);
    tcase_add_test(tc_core, test_agent_restore_sets_parent_uuid);
    tcase_add_test(tc_core, test_agent_restore_sets_created_at);
    tcase_add_test(tc_core, test_agent_restore_sets_name_if_present);
    tcase_add_test(tc_core, test_agent_restore_null_name_if_not_present);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = agent_restore_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
