// ik_render_create() unit tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/render.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"

// Test: successful ik_render_create
START_TEST(test_render_create_success) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, 1, &render);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(render);
    ck_assert_int_eq(render->rows, 24);
    ck_assert_int_eq(render->cols, 80);
    ck_assert_int_eq(render->tty_fd, 1);

    talloc_free(ctx);
}
END_TEST
// Test: invalid dimensions (rows <= 0)
START_TEST(test_render_create_invalid_rows) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 0, 80, 1, &render);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);
    ck_assert_ptr_null(render);

    talloc_free(ctx);
}

END_TEST
// Test: invalid dimensions (cols <= 0)
START_TEST(test_render_create_invalid_cols) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 0, 1, &render);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);
    ck_assert_ptr_null(render);

    talloc_free(ctx);
}

END_TEST
// Test: invalid dimensions (negative rows)
START_TEST(test_render_create_negative_rows) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, -1, 80, 1, &render);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);
    ck_assert_ptr_null(render);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test: ik_render_create with NULL parent asserts
START_TEST(test_render_create_null_parent_asserts) {
    ik_render_ctx_t *render = NULL;
    ik_render_create(NULL, 24, 80, 1, &render);
}

END_TEST
// Test: ik_render_create with NULL ctx_out asserts
START_TEST(test_render_create_null_ctx_out_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_render_create(ctx, 24, 80, 1, NULL);
    talloc_free(ctx);
}

END_TEST
#endif

// Test suite
static Suite *create_suite(void)
{
    Suite *s = suite_create("RenderDirect_Create");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_render_create_success);
    tcase_add_test(tc_core, test_render_create_invalid_rows);
    tcase_add_test(tc_core, test_render_create_invalid_cols);
    tcase_add_test(tc_core, test_render_create_negative_rows);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    tcase_add_test_raise_signal(tc_core, test_render_create_null_parent_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_render_create_null_ctx_out_asserts, SIGABRT);
#endif

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = create_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
