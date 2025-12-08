#include "../../../src/shared.h"

#include "../../../src/error.h"
#include "../../../src/config.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Test that ik_shared_ctx_init() succeeds
START_TEST(test_shared_ctx_init_success)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);

    talloc_free(ctx);
}
END_TEST

// Test that shared_ctx is allocated under provided parent
START_TEST(test_shared_ctx_parent_allocation)
{
    TALLOC_CTX *parent = talloc_new(NULL);
    ck_assert_ptr_nonnull(parent);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(parent, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(parent, cfg, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);

    // Verify parent relationship
    TALLOC_CTX *actual_parent = talloc_parent(shared);
    ck_assert_ptr_eq(actual_parent, parent);

    talloc_free(parent);
}
END_TEST

// Test that shared_ctx can be freed via talloc_free
START_TEST(test_shared_ctx_can_be_freed)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);

    // Free shared context directly
    int result = talloc_free(shared);
    ck_assert_int_eq(result, 0);  // talloc_free returns 0 on success

    talloc_free(ctx);
}
END_TEST

// Test that shared_ctx stores cfg pointer
START_TEST(test_shared_ctx_stores_cfg)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_eq(shared->cfg, cfg);

    talloc_free(ctx);
}
END_TEST

// Test that shared_ctx->cfg is accessible
START_TEST(test_shared_ctx_cfg_accessible)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create cfg with specific value for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_model = talloc_strdup(cfg, "test-model");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(shared->cfg);
    ck_assert_str_eq(shared->cfg->openai_model, "test-model");

    talloc_free(ctx);
}
END_TEST

static Suite *shared_suite(void)
{
    Suite *s = suite_create("Shared Context");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_shared_ctx_init_success);
    tcase_add_test(tc_core, test_shared_ctx_parent_allocation);
    tcase_add_test(tc_core, test_shared_ctx_can_be_freed);
    tcase_add_test(tc_core, test_shared_ctx_stores_cfg);
    tcase_add_test(tc_core, test_shared_ctx_cfg_accessible);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = shared_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
