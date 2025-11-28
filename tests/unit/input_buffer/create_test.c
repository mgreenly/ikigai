/**
 * @file create_test.c
 * @brief Unit tests for input_buffer creation, clear, and get_text operations
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input_buffer.h"
#include "../../test_utils.h"

/* Test: Create input_buffer */
START_TEST(test_create) {
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    /* Create input_buffer */
    res_t res = ik_input_buffer_create(ctx, &input_buffer);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(input_buffer);

    /* Verify text buffer is empty */
    char *text = NULL;
    size_t len = 0;
    res = ik_input_buffer_get_text(input_buffer, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 0);

    /* Verify cursor at position 0 */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: Get text */
START_TEST(test_get_text)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    ik_input_buffer_create(ctx, &input_buffer);

    /* Manually add some data */
    const uint8_t test_data[] = {'h', 'e', 'l', 'l', 'o'};
    for (size_t i = 0; i < 5; i++) {
        ik_byte_array_append(input_buffer->text, test_data[i]);
    }

    /* Get text */
    char *text = NULL;
    size_t len = 0;
    res_t res = ik_input_buffer_get_text(input_buffer, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 5);
    ck_assert_mem_eq(text, "hello", 5);

    talloc_free(ctx);
}

END_TEST
/* Test: Clear input_buffer */
START_TEST(test_clear)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;

    ik_input_buffer_create(ctx, &input_buffer);

    /* Manually add some data to test clearing */
    const uint8_t test_data[] = {'h', 'e', 'l', 'l', 'o'};
    for (size_t i = 0; i < 5; i++) {
        ik_byte_array_append(input_buffer->text, test_data[i]);
    }
    input_buffer->cursor_byte_offset = 3;

    /* Clear the input_buffer */
    ik_input_buffer_clear(input_buffer);

    /* Verify empty */
    char *text = NULL;
    size_t len = 0;
    ik_input_buffer_get_text(input_buffer, &text, &len);
    ck_assert_uint_eq(len, 0);

    /* Verify cursor at 0 */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: NULL parameter assertions */
START_TEST(test_create_null_input_buffer_out_asserts)
{
    void *ctx = talloc_new(NULL);

    /* input_buffer_out cannot be NULL - should abort */
    ik_input_buffer_create(ctx, NULL);

    talloc_free(ctx);
}

END_TEST START_TEST(test_get_text_null_input_buffer_asserts)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    char *text = NULL;
    size_t len = 0;

    ik_input_buffer_create(ctx, &input_buffer);

    /* input_buffer cannot be NULL */
    ik_input_buffer_get_text(NULL, &text, &len);

    talloc_free(ctx);
}

END_TEST START_TEST(test_get_text_null_text_out_asserts)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    size_t len = 0;

    ik_input_buffer_create(ctx, &input_buffer);

    /* text_out cannot be NULL */
    ik_input_buffer_get_text(input_buffer, NULL, &len);

    talloc_free(ctx);
}

END_TEST START_TEST(test_get_text_null_len_out_asserts)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buffer = NULL;
    char *text = NULL;

    ik_input_buffer_create(ctx, &input_buffer);

    /* len_out cannot be NULL */
    ik_input_buffer_get_text(input_buffer, &text, NULL);

    talloc_free(ctx);
}

END_TEST START_TEST(test_clear_null_input_buffer_asserts)
{
    /* input_buffer cannot be NULL - should abort */
    ik_input_buffer_clear(NULL);
}

END_TEST

static Suite *input_buffer_create_suite(void)
{
    Suite *s = suite_create("Input Buffer Create");
    TCase *tc_core = tcase_create("Core");
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind

    /* Normal tests */
    tcase_add_test(tc_core, test_create);
    tcase_add_test(tc_core, test_get_text);
    tcase_add_test(tc_core, test_clear);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_assertions, test_create_null_input_buffer_out_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_get_text_null_input_buffer_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_get_text_null_text_out_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_get_text_null_len_out_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_clear_null_input_buffer_asserts, SIGABRT);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_assertions);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = input_buffer_create_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
