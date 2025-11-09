#include <check.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <talloc.h>
#include <unistd.h>
#include "../../src/repl.h"
#include "../test_utils.h"

// Test: REPL initialization creates all components
//
// NOTE: This test is skipped when no TTY is available (e.g., CI environment).
// The functionality is still tested via assertion and OOM tests below.
START_TEST(test_repl_init) {
    // Check if TTY is available
    int32_t tty_fd = open("/dev/tty", O_RDWR | O_NOCTTY);
    if (tty_fd < 0) {
        // No TTY available - skip test silently
        return;
    }
    close(tty_fd);

    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Initialize REPL
    res_t result = ik_repl_init(ctx, &repl);

    // Verify successful initialization
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(repl);
    ck_assert_ptr_nonnull(repl->term);
    ck_assert_ptr_nonnull(repl->render);
    ck_assert_ptr_nonnull(repl->workspace);
    ck_assert_ptr_nonnull(repl->input_parser);
    ck_assert(!repl->quit);

    // Cleanup
    ik_repl_cleanup(repl);
    talloc_free(ctx);
}
END_TEST

// Test: REPL initialization with NULL parent
START_TEST(test_repl_init_null_parent) {
    ik_repl_ctx_t *repl = NULL;
    (void)ik_repl_init(NULL, &repl);
}
END_TEST

// Test: REPL initialization with NULL out pointer
START_TEST(test_repl_init_null_out) {
    void *ctx = talloc_new(NULL);
    (void)ik_repl_init(ctx, NULL);
    talloc_free(ctx);
}
END_TEST

// Test: REPL initialization OOM scenarios
//
// NOTE: This test only covers OOM during REPL context allocation.
// Full coverage of error paths requires a TTY and is achieved via test_repl_init
// when run in a terminal environment.
START_TEST(test_repl_init_oom) {
    void *ctx = talloc_new(NULL);

    // Test OOM during repl context allocation
    oom_test_fail_next_alloc();
    ik_repl_ctx_t *repl = NULL;
    res_t result = ik_repl_init(ctx, &repl);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(repl);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST

static Suite *repl_suite(void) {
    Suite *s = suite_create("REPL");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_repl_init);
    tcase_add_test(tc_core, test_repl_init_oom);
    suite_add_tcase(s, tc_core);

    TCase *tc_assertions = tcase_create("Assertions");
    tcase_add_test_raise_signal(tc_assertions, test_repl_init_null_parent, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_repl_init_null_out, SIGABRT);
    suite_add_tcase(s, tc_assertions);

    return s;
}

int32_t main(void) {
    Suite *s = repl_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
