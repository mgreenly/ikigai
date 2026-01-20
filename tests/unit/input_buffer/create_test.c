/**
 * @file create_test.c
 * @brief Unit tests for input_buffer creation, clear, and get_text operations
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/input_buffer/core.h"
#include "../../test_utils_helper.h"

/* Test: Create input_buffer */
START_TEST(test_create) {
    void *ctx = talloc_new(NULL);

    /* Create input_buffer */
    ik_input_buffer_t *input_buffer = ik_input_buffer_create(ctx);
    ck_assert_ptr_nonnull(input_buffer);

    /* Verify text buffer is empty */
    size_t len = 0;
    (void)ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 0);

    /* Verify cursor at position 0 */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: Get text */
START_TEST(test_get_text) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buffer = ik_input_buffer_create(ctx);

    /* Manually add some data */
    const uint8_t test_data[] = {'h', 'e', 'l', 'l', 'o'};
    for (size_t i = 0; i < 5; i++) {
        ik_byte_array_append(input_buffer->text, test_data[i]);
    }

    /* Get text */
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 5);
    ck_assert_mem_eq(text, "hello", 5);

    talloc_free(ctx);
}

END_TEST
/* Test: Clear input_buffer */
START_TEST(test_clear) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buffer = ik_input_buffer_create(ctx);

    /* Manually add some data to test clearing */
    const uint8_t test_data[] = {'h', 'e', 'l', 'l', 'o'};
    for (size_t i = 0; i < 5; i++) {
        ik_byte_array_append(input_buffer->text, test_data[i]);
    }
    input_buffer->cursor_byte_offset = 3;

    /* Clear the input_buffer */
    ik_input_buffer_clear(input_buffer);

    /* Verify empty */
    size_t len = 0;
    (void)ik_input_buffer_get_text(input_buffer, &len);
    ck_assert_uint_eq(len, 0);

    /* Verify cursor at 0 */
    ck_assert_uint_eq(input_buffer->cursor_byte_offset, 0);

    talloc_free(ctx);
}

END_TEST

static Suite *input_buffer_create_suite(void)
{
    Suite *s = suite_create("Input Buffer Create");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Normal tests */
    tcase_add_test(tc_core, test_create);
    tcase_add_test(tc_core, test_get_text);
    tcase_add_test(tc_core, test_clear);

    suite_add_tcase(s, tc_core);
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
