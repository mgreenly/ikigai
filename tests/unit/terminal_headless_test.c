#include <check.h>
#include <stdlib.h>
#include <talloc.h>

#include "shared/terminal.h"

// ik_term_init_headless returns valid context with canned values
START_TEST(test_headless_init_returns_context)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = ik_term_init_headless(ctx);

    ck_assert_ptr_nonnull(term);
    ck_assert_int_eq(term->tty_fd, -1);
    ck_assert_int_eq(term->screen_rows, 50);
    ck_assert_int_eq(term->screen_cols, 100);
    ck_assert(!term->csi_u_supported);

    talloc_free(ctx);
}
END_TEST

// ik_term_init_headless context is child of parent
START_TEST(test_headless_init_talloc_parent)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = ik_term_init_headless(ctx);

    ck_assert_ptr_eq(talloc_parent(term), ctx);

    talloc_free(ctx);
}
END_TEST

// ik_term_cleanup with headless context does not crash
START_TEST(test_headless_cleanup_safe)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = ik_term_init_headless(ctx);

    // Should not crash or attempt I/O with tty_fd == -1
    ik_term_cleanup(term);

    talloc_free(ctx);
}
END_TEST

// ik_term_get_size with headless context returns stored values
START_TEST(test_headless_get_size)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_term_ctx_t *term = ik_term_init_headless(ctx);

    int rows = 0;
    int cols = 0;
    res_t result = ik_term_get_size(term, &rows, &cols);

    ck_assert(is_ok(&result));
    ck_assert_int_eq(rows, 50);
    ck_assert_int_eq(cols, 100);

    talloc_free(ctx);
}
END_TEST

static Suite *terminal_headless_suite(void)
{
    Suite *s = suite_create("terminal_headless");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_headless_init_returns_context);
    tcase_add_test(tc_core, test_headless_init_talloc_parent);
    tcase_add_test(tc_core, test_headless_cleanup_safe);
    tcase_add_test(tc_core, test_headless_get_size);
    suite_add_tcase(s, tc_core);

    return s;
}

int32_t main(void)
{
    Suite *s = terminal_headless_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/terminal_headless_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
