#include <check.h>
#include <talloc.h>
#include <signal.h>
#include "../../../src/error.h"
#include "../../test_utils.h"

// Helper function that returns success
static res_t helper_success(TALLOC_CTX *ctx)
{
    int *value = talloc(ctx, int);
    *value = 42;
    return OK(value);
}

// Helper function that returns error
static res_t helper_error(TALLOC_CTX *ctx)
{
    return ERR(ctx, INVALID_ARG, "Test error message");
}

// Helper function using CHECK macro
static res_t helper_propagate(TALLOC_CTX *ctx, bool should_fail)
{
    res_t res;

    if (should_fail) {
        res = helper_error(ctx);
    }else {
        res = helper_success(ctx);
    }

    CHECK(res);

    // Should only reach here if res was OK
    return res;
}

// Helper function using TRY macro - extracts ok value directly
static res_t helper_try_extract(TALLOC_CTX *ctx, bool should_fail)
{
    int *value;

    if (should_fail) {
        value = TRY(helper_error(ctx));
    } else {
        value = TRY(helper_success(ctx));
    }

    // If we reach here, TRY succeeded and extracted the value
    // Wrap it back in a result for testing
    return OK(value);
}

// Test OK() construction
START_TEST(test_ok_construction) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    int *value = talloc(ctx, int);
    *value = 123;

    res_t res = OK(value);

    ck_assert(is_ok(&res));
    ck_assert(!is_err(&res));
    ck_assert_ptr_eq(res.ok, value);
    ck_assert_int_eq(*(int *)res.ok, 123);

    talloc_free(ctx);
}

END_TEST
// Test ERR() construction
START_TEST(test_err_construction)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ERR(ctx, INVALID_ARG, "Test error: %d", 42);

    ck_assert(is_err(&res));
    ck_assert(!is_ok(&res));
    ck_assert_ptr_nonnull(res.err);
    ck_assert_int_eq(res.err->code, ERR_INVALID_ARG);
    ck_assert_str_eq(res.err->msg, "Test error: 42");
    ck_assert_ptr_nonnull(res.err->file);

    talloc_free(ctx);
}

END_TEST
// Test error message extraction
START_TEST(test_error_message)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Test with custom message
    res_t res = ERR(ctx, INVALID_ARG, "Custom message");

    const char *msg = error_message(res.err);
    ck_assert_str_eq(msg, "Custom message");

    err_code_t code = error_code(res.err);
    ck_assert_int_eq(code, ERR_INVALID_ARG);

    talloc_free(ctx);
}

END_TEST
// Test CHECK macro with success
START_TEST(test_try_success)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = helper_propagate(ctx, false);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(*(int *)res.ok, 42);

    talloc_free(ctx);
}

END_TEST
// Test CHECK macro with error propagation
START_TEST(test_try_error)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = helper_propagate(ctx, true);

    ck_assert(is_err(&res));
    ck_assert_str_eq(res.err->msg, "Test error message");

    talloc_free(ctx);
}

END_TEST
// Test TRY macro with success - extracts ok value
START_TEST(test_try_macro_success)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = helper_try_extract(ctx, false);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(res.ok);
    ck_assert_int_eq(*(int *)res.ok, 42);

    talloc_free(ctx);
}

END_TEST
// Test TRY macro with error - propagates error
START_TEST(test_try_macro_error)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = helper_try_extract(ctx, true);

    ck_assert(is_err(&res));
    ck_assert_str_eq(res.err->msg, "Test error message");

    talloc_free(ctx);
}

END_TEST
// Test talloc hierarchy - error freed with context
START_TEST(test_talloc_error_freed)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = helper_error(ctx);
    ck_assert(is_err(&res));

    // Error should be child of ctx
    TALLOC_CTX *err_parent = talloc_parent(res.err);
    ck_assert_ptr_eq(err_parent, ctx);

    // Freeing ctx should free the error
    talloc_free(ctx);
    // No assertion here - if talloc is working, no leak
}

END_TEST
// Test talloc hierarchy - success value freed with context
START_TEST(test_talloc_ok_freed)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = helper_success(ctx);
    ck_assert(is_ok(&res));

    // Value should be child of ctx
    TALLOC_CTX *val_parent = talloc_parent(res.ok);
    ck_assert_ptr_eq(val_parent, ctx);

    talloc_free(ctx);
}

END_TEST
// Test error formatting
START_TEST(test_error_fprintf)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = ERR(ctx, OUT_OF_RANGE, "Formatted error");

    // Capture output to memory buffer instead of stderr
    char buffer[256];
    FILE *memfile = fmemopen(buffer, sizeof(buffer), "w");
    ck_assert_ptr_nonnull(memfile);

    error_fprintf(memfile, res.err);
    fclose(memfile);

    // Verify the output contains expected text
    // Format is: "Error: <message> [<file>:<line>]"
    ck_assert_ptr_nonnull(strstr(buffer, "Error:"));
    ck_assert_ptr_nonnull(strstr(buffer, "Formatted error"));
    ck_assert_ptr_nonnull(strstr(buffer, "error_test.c"));

    talloc_free(ctx);
}

END_TEST
// Test nested contexts
START_TEST(test_nested_contexts)
{
    TALLOC_CTX *root = talloc_new(NULL);
    TALLOC_CTX *child = talloc_new(root);

    res_t res = ERR(child, INVALID_ARG, "Child error");

    ck_assert(is_err(&res));
    ck_assert_ptr_eq(talloc_parent(res.err), child);

    // Freeing root should free child and error
    talloc_free(root);
}

END_TEST
// Test error message with empty string (should fall back to error code string)
START_TEST(test_error_message_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create error with empty message string
    res_t res = ERR(ctx, INVALID_ARG, "");

    ck_assert(is_err(&res));
    // Empty message should fall back to error_code_str
    const char *msg = error_message(res.err);
    ck_assert_str_eq(msg, "Invalid argument");

    // Test with OUT_OF_RANGE
    res = ERR(ctx, OUT_OF_RANGE, "");
    msg = error_message(res.err);
    ck_assert_str_eq(msg, "Out of range");

    talloc_free(ctx);
}

END_TEST
// Test error fprintf with NULL file field in error
START_TEST(test_error_fprintf_null_file)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Manually create an error with NULL file field
    err_t *err = talloc_zero(ctx, err_t);
    err->code = ERR_INVALID_ARG;
    err->file = NULL;           // NULL file field
    err->line = 42;
    snprintf(err->msg, sizeof(err->msg), "Test error");

    char buffer[256];
    FILE *memfile = fmemopen(buffer, sizeof(buffer), "w");
    ck_assert_ptr_nonnull(memfile);

    error_fprintf(memfile, err);
    fclose(memfile);

    // Should print "unknown" for NULL file
    ck_assert_ptr_nonnull(strstr(buffer, "unknown"));
    ck_assert_ptr_nonnull(strstr(buffer, "Test error"));

    talloc_free(ctx);
}

END_TEST
// Test error code to string conversion
START_TEST(test_error_code_str)
{
    ck_assert_str_eq(error_code_str(OK), "OK");
    ck_assert_str_eq(error_code_str(ERR_INVALID_ARG), "Invalid argument");
    ck_assert_str_eq(error_code_str(ERR_OUT_OF_RANGE), "Out of range");
    ck_assert_str_eq(error_code_str(ERR_IO), "IO error");
    ck_assert_str_eq(error_code_str(ERR_PARSE), "Parse error");
    ck_assert_str_eq(error_code_str(ERR_DB_CONNECT), "Database connection error");
    ck_assert_str_eq(error_code_str(ERR_DB_MIGRATE), "Database migration error");
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test error code to string conversion with invalid code (should PANIC)
START_TEST(test_error_code_str_invalid)
{
    error_code_str((err_code_t)999);  // Invalid code - should PANIC
}

END_TEST
#endif

// Test suite setup
static Suite *error_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Error");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_ok_construction);
    tcase_add_test(tc_core, test_err_construction);
    tcase_add_test(tc_core, test_error_message);
    tcase_add_test(tc_core, test_try_success);
    tcase_add_test(tc_core, test_try_error);
    tcase_add_test(tc_core, test_try_macro_success);
    tcase_add_test(tc_core, test_try_macro_error);
    tcase_add_test(tc_core, test_talloc_error_freed);
    tcase_add_test(tc_core, test_talloc_ok_freed);
    tcase_add_test(tc_core, test_error_fprintf);
    tcase_add_test(tc_core, test_nested_contexts);
    tcase_add_test(tc_core, test_error_message_empty);
    tcase_add_test(tc_core, test_error_fprintf_null_file);
    tcase_add_test(tc_core, test_error_code_str);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_error_code_str_invalid, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = error_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
