#include <check.h>
#include <talloc.h>

#include "../../../src/tmp_ctx.h"
#include "../../test_utils.h"

// Test that tmp_ctx_create returns non-NULL
START_TEST(test_tmp_ctx_create_not_null)
{
    TALLOC_CTX *tmp = tmp_ctx_create();
    ck_assert_ptr_nonnull(tmp);
    talloc_free(tmp);
}

END_TEST

// Test that we can allocate using the returned context
START_TEST(test_tmp_ctx_allocation)
{
    TALLOC_CTX *tmp = tmp_ctx_create();

    int32_t *value = talloc(tmp, int32_t);
    ck_assert_ptr_nonnull(value);

    *value = 42;
    ck_assert_int_eq(*value, 42);

    talloc_free(tmp);
}

END_TEST

// Test that we can free the context without errors
START_TEST(test_tmp_ctx_free)
{
    TALLOC_CTX *tmp = tmp_ctx_create();

    // Allocate something
    char *str = talloc_strdup(tmp, "test");
    ck_assert_ptr_nonnull(str);
    ck_assert_str_eq(str, "test");

    // Free should work without errors
    int32_t result = talloc_free(tmp);
    ck_assert_int_eq(result, 0);
}

END_TEST

// Test suite setup
static Suite *tmp_ctx_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("TmpCtx");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_tmp_ctx_create_not_null);
    tcase_add_test(tc_core, test_tmp_ctx_allocation);
    tcase_add_test(tc_core, test_tmp_ctx_free);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s;
    SRunner *sr;

    s = tmp_ctx_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
