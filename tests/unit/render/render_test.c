/**
 * @file render_test.c
 * @brief Unit tests for render module creation
 */

#include <check.h>
#include <signal.h>
#include <string.h>
#include <talloc.h>
#include "../../../src/render.h"
#include "../../test_utils.h"

// Test: Create render context
START_TEST(test_render_create)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, &render);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(render);
    ck_assert_ptr_nonnull(render->vterm);
    ck_assert_ptr_nonnull(render->vscreen);
    ck_assert_int_eq(render->rows, 24);
    ck_assert_int_eq(render->cols, 80);

    talloc_free(ctx);
}
END_TEST

// Test: Create render context with different dimensions
START_TEST(test_render_create_custom_size)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 50, 120, &render);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(render);
    ck_assert_int_eq(render->rows, 50);
    ck_assert_int_eq(render->cols, 120);

    talloc_free(ctx);
}
END_TEST

// Test: OOM scenario - talloc fails
START_TEST(test_render_create_oom_talloc)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    oom_test_fail_next_alloc();

    res_t res = ik_render_create(ctx, 24, 80, &render);

    ck_assert(is_err(&res));
    ck_assert_ptr_null(render);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST

// Test: Destructor properly frees vterm
START_TEST(test_render_destructor)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, &render);
    ck_assert(is_ok(&res));

    // Free the context - destructor should be called
    // This test mainly ensures no crashes occur
    talloc_free(ctx);
}
END_TEST

// Test: NULL parameter assertions
// Test: Clear render context
START_TEST(test_render_clear)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, &render);
    ck_assert(is_ok(&res));

    // Clear should not crash
    ik_render_clear(render);

    talloc_free(ctx);
}
END_TEST

// Test: Write text to render context
START_TEST(test_render_write_text)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, &render);
    ck_assert(is_ok(&res));

    // Write some text
    const char *text = "hello world";
    res = ik_render_write_text(render, text, strlen(text));
    ck_assert(is_ok(&res));

    // Write empty string
    res = ik_render_write_text(render, "", 0);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

// Test: Set cursor position
START_TEST(test_render_set_cursor)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, &render);
    ck_assert(is_ok(&res));

    // Set cursor to various positions
    res = ik_render_set_cursor(render, 0, 0);
    ck_assert(is_ok(&res));

    res = ik_render_set_cursor(render, 10, 20);
    ck_assert(is_ok(&res));

    res = ik_render_set_cursor(render, 23, 79);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

// Test: Blit render context
START_TEST(test_render_blit)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;

    res_t res = ik_render_create(ctx, 24, 80, &render);
    ck_assert(is_ok(&res));

    // Blit should not crash (even with fake fd)
    res = ik_render_blit(render, 1);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

#ifdef ENABLE_ASSERT_TESTS
// Test: Clear with NULL parameter
START_TEST(test_render_clear_null)
{
    expect_sigabrt(ik_render_clear(NULL));
}
END_TEST

// Test: Write text with NULL render
START_TEST(test_render_write_text_null_render)
{
    const char *text = "hello";
    expect_sigabrt(ik_render_write_text(NULL, text, 5));
}
END_TEST

// Test: Set cursor with NULL render
START_TEST(test_render_set_cursor_null_render)
{
    expect_sigabrt(ik_render_set_cursor(NULL, 0, 0));
}
END_TEST

// Test: Blit with NULL render
START_TEST(test_render_blit_null_render)
{
    expect_sigabrt(ik_render_blit(NULL, 1));
}
END_TEST

START_TEST(test_render_create_null_output)
{
    void *ctx = talloc_new(NULL);
    expect_sigabrt(ik_render_create(ctx, 24, 80, NULL));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_render_create_invalid_rows)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;
    expect_sigabrt(ik_render_create(ctx, 0, 80, &render));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_render_create_invalid_cols)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;
    expect_sigabrt(ik_render_create(ctx, 24, 0, &render));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_render_create_negative_rows)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;
    expect_sigabrt(ik_render_create(ctx, -1, 80, &render));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_render_create_negative_cols)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render = NULL;
    expect_sigabrt(ik_render_create(ctx, 24, -1, &render));
    talloc_free(ctx);
}
END_TEST
#endif

static Suite *render_suite(void)
{
    Suite *s = suite_create("Render");

    TCase *tc_create = tcase_create("Create");
    tcase_add_test(tc_create, test_render_create);
    tcase_add_test(tc_create, test_render_create_custom_size);
    tcase_add_test(tc_create, test_render_create_oom_talloc);
    tcase_add_test(tc_create, test_render_destructor);
    suite_add_tcase(s, tc_create);

    TCase *tc_operations = tcase_create("Operations");
    tcase_add_test(tc_operations, test_render_clear);
    tcase_add_test(tc_operations, test_render_write_text);
    tcase_add_test(tc_operations, test_render_set_cursor);
    tcase_add_test(tc_operations, test_render_blit);
    suite_add_tcase(s, tc_operations);

#ifdef ENABLE_ASSERT_TESTS
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_add_test_raise_signal(tc_assertions, test_render_create_null_output, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_render_create_invalid_rows, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_render_create_invalid_cols, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_render_create_negative_rows, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_render_create_negative_cols, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_render_clear_null, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_render_write_text_null_render, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_render_set_cursor_null_render, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_render_blit_null_render, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = render_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
