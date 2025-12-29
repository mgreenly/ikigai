#include "tmp_ctx.h"

#include <check.h>
#include <talloc.h>

START_TEST(test_tmp_ctx_create_returns_non_null) {
    TALLOC_CTX *tmp = tmp_ctx_create();
    ck_assert_ptr_nonnull(tmp);
    talloc_free(tmp);
}
END_TEST START_TEST(test_tmp_ctx_can_allocate)
{
    TALLOC_CTX *tmp = tmp_ctx_create();
    ck_assert_ptr_nonnull(tmp);

    // Allocate something using the context
    char *str = talloc_strdup(tmp, "test string");
    ck_assert_ptr_nonnull(str);
    ck_assert_str_eq(str, "test string");

    talloc_free(tmp);
}

END_TEST START_TEST(test_tmp_ctx_can_be_freed)
{
    TALLOC_CTX *tmp = tmp_ctx_create();
    ck_assert_ptr_nonnull(tmp);

    // Free should not crash
    int32_t result = talloc_free(tmp);
    ck_assert_int_eq(result, 0);
}

END_TEST

static Suite *tmp_ctx_suite(void)
{
    Suite *s = suite_create("TmpCtx");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, 30);

    tcase_add_test(tc, test_tmp_ctx_create_returns_non_null);
    tcase_add_test(tc, test_tmp_ctx_can_allocate);
    tcase_add_test(tc, test_tmp_ctx_can_be_freed);

    suite_add_tcase(s, tc);
    return s;
}

int32_t main(void)
{
    Suite *s = tmp_ctx_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
