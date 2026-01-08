#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Test agent_ctx is allocated under provided parent
START_TEST(test_agent_allocated_under_parent) {
    TALLOC_CTX *parent = talloc_new(NULL);
    ck_assert_ptr_nonnull(parent);

    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(parent, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);

    // Verify parent relationship
    TALLOC_CTX *actual_parent = talloc_parent(agent);
    ck_assert_ptr_eq(actual_parent, parent);

    talloc_free(parent);
}
END_TEST
// Test agent_ctx can be freed via talloc_free
START_TEST(test_agent_can_be_freed) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);

    // Free agent directly
    int result = talloc_free(agent);
    ck_assert_int_eq(result, 0);  // talloc_free returns 0 on success

    talloc_free(ctx);
}

END_TEST

static Suite *agent_lifecycle_suite(void)
{
    Suite *s = suite_create("Agent Lifecycle");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_test(tc_core, test_agent_allocated_under_parent);
    tcase_add_test(tc_core, test_agent_can_be_freed);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = agent_lifecycle_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
