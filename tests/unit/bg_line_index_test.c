#include <check.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/bg_line_index.h"
#include "shared/error.h"

START_TEST(test_create_destroy)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    ck_assert_ptr_nonnull(idx);
    ck_assert_ptr_nonnull(idx->offsets);
    ck_assert_int_eq(idx->count, 0);
    ck_assert_int_gt(idx->capacity, 0);
    ck_assert_int_eq(idx->cursor, 0);
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_destroy_null)
{
    bg_line_index_destroy(NULL);
}
END_TEST

START_TEST(test_count_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    ck_assert_int_eq(bg_line_index_count(idx), 0);
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_append_no_newlines)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t data[] = "hello world";
    bg_line_index_append(idx, data, sizeof(data) - 1);
    ck_assert_int_eq(bg_line_index_count(idx), 0);
    ck_assert_int_eq(idx->cursor, (off_t)(sizeof(data) - 1));
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_append_single_newline)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t data[] = "hello\n";
    bg_line_index_append(idx, data, sizeof(data) - 1);
    ck_assert_int_eq(bg_line_index_count(idx), 1);
    ck_assert_int_eq(idx->offsets[0], 5);
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_append_multiple_lines)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t data[] = "line1\nline2\nline3\n";
    bg_line_index_append(idx, data, sizeof(data) - 1);
    ck_assert_int_eq(bg_line_index_count(idx), 3);
    ck_assert_int_eq(idx->offsets[0], 5);
    ck_assert_int_eq(idx->offsets[1], 11);
    ck_assert_int_eq(idx->offsets[2], 17);
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_append_no_trailing_newline)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t data[] = "line1\npartial";
    bg_line_index_append(idx, data, sizeof(data) - 1);
    ck_assert_int_eq(bg_line_index_count(idx), 1);
    ck_assert_int_eq(idx->cursor, (off_t)(sizeof(data) - 1));
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_append_consecutive_newlines)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t data[] = "a\n\nb\n";
    bg_line_index_append(idx, data, sizeof(data) - 1);
    ck_assert_int_eq(bg_line_index_count(idx), 3);
    ck_assert_int_eq(idx->offsets[0], 1);
    ck_assert_int_eq(idx->offsets[1], 2);
    ck_assert_int_eq(idx->offsets[2], 4);
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_append_null_and_zero)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t data[] = "x\n";
    bg_line_index_append(idx, NULL, 10);
    bg_line_index_append(idx, data, 0);
    ck_assert_int_eq(bg_line_index_count(idx), 0);
    ck_assert_int_eq(idx->cursor, 0);
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_single_byte_appends)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const char *text = "AB\nCD\n";
    for (size_t i = 0; text[i] != '\0'; i++)
        bg_line_index_append(idx, (const uint8_t *)&text[i], 1);
    ck_assert_int_eq(bg_line_index_count(idx), 2);
    ck_assert_int_eq(idx->offsets[0], 2);
    ck_assert_int_eq(idx->offsets[1], 5);
    ck_assert_int_eq(idx->cursor, 6);
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_large_single_append)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const int nlines = 10000;
    const size_t line_len = 2; /* "X\n" */
    size_t total = (size_t)nlines * line_len;
    uint8_t *buf = talloc_array(ctx, uint8_t, (unsigned int)total);
    ck_assert_ptr_nonnull(buf);
    for (int i = 0; i < nlines; i++) {
        buf[i * 2]     = 'X';
        buf[i * 2 + 1] = '\n';
    }
    bg_line_index_append(idx, buf, total);
    ck_assert_int_eq(bg_line_index_count(idx), nlines);
    ck_assert_int_eq(idx->cursor, (off_t)total);
    ck_assert_int_eq(idx->offsets[0], 1);
    ck_assert_int_eq(idx->offsets[nlines - 1], (off_t)((nlines - 1) * 2 + 1));
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_get_range_single_line)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t data[] = "hello\n";
    bg_line_index_append(idx, data, sizeof(data) - 1);
    off_t off = 0;
    size_t len = 0;
    res_t r = bg_line_index_get_range(idx, 1, 1, &off, &len);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(off, 0);
    ck_assert_uint_eq(len, 6);
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_get_range_multiple_lines)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t data[] = "line1\nline2\nline3\n";
    bg_line_index_append(idx, data, sizeof(data) - 1);
    off_t off = 0;
    size_t len = 0;

    res_t r = bg_line_index_get_range(idx, 1, 1, &off, &len);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(off, 0);
    ck_assert_uint_eq(len, 6);

    r = bg_line_index_get_range(idx, 2, 2, &off, &len);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(off, 6);
    ck_assert_uint_eq(len, 6);

    r = bg_line_index_get_range(idx, 1, 3, &off, &len);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(off, 0);
    ck_assert_uint_eq(len, 18);

    r = bg_line_index_get_range(idx, 2, 3, &off, &len);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(off, 6);
    ck_assert_uint_eq(len, 12);

    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_get_range_consecutive_newlines)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t data[] = "a\n\nb\n";
    bg_line_index_append(idx, data, sizeof(data) - 1);
    off_t off = 0;
    size_t len = 0;

    res_t r = bg_line_index_get_range(idx, 1, 1, &off, &len);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(off, 0);
    ck_assert_uint_eq(len, 2);

    r = bg_line_index_get_range(idx, 2, 2, &off, &len);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(off, 2);
    ck_assert_uint_eq(len, 1);

    r = bg_line_index_get_range(idx, 3, 3, &off, &len);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(off, 3);
    ck_assert_uint_eq(len, 2);

    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_get_range_boundary_first_last)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t data[] = "first\nsecond\nthird\n";
    bg_line_index_append(idx, data, sizeof(data) - 1);
    off_t off = 0;
    size_t len = 0;

    res_t r = bg_line_index_get_range(idx, 1, 1, &off, &len);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(off, 0);
    ck_assert_uint_eq(len, 6);

    r = bg_line_index_get_range(idx, 3, 3, &off, &len);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(off, 13);
    ck_assert_uint_eq(len, 6);

    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_get_range_out_of_range)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t data[] = "hello\n";
    bg_line_index_append(idx, data, sizeof(data) - 1);
    off_t off = 0;
    size_t len = 0;

    /* empty index */
    bg_line_index_t *empty = bg_line_index_create(ctx);
    res_t r = bg_line_index_get_range(empty, 1, 1, &off, &len);
    ck_assert(is_err(&r));
    ck_assert_int_eq(error_code(r.err), ERR_OUT_OF_RANGE);
    bg_line_index_destroy(empty);

    /* start_line = 0 */
    r = bg_line_index_get_range(idx, 0, 1, &off, &len);
    ck_assert(is_err(&r));
    ck_assert_int_eq(error_code(r.err), ERR_OUT_OF_RANGE);

    /* end exceeds count */
    r = bg_line_index_get_range(idx, 1, 2, &off, &len);
    ck_assert(is_err(&r));
    ck_assert_int_eq(error_code(r.err), ERR_OUT_OF_RANGE);

    /* start > end */
    bg_line_index_append(idx, (const uint8_t *)"x\n", 2);
    r = bg_line_index_get_range(idx, 2, 1, &off, &len);
    ck_assert(is_err(&r));
    ck_assert_int_eq(error_code(r.err), ERR_OUT_OF_RANGE);

    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_get_range_null_args)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t data[] = "hello\n";
    bg_line_index_append(idx, data, sizeof(data) - 1);
    off_t off = 0;
    size_t len = 0;

    res_t r = bg_line_index_get_range(NULL, 1, 1, &off, &len);
    ck_assert(is_err(&r));
    ck_assert_int_eq(error_code(r.err), ERR_INVALID_ARG);

    r = bg_line_index_get_range(idx, 1, 1, NULL, &len);
    ck_assert(is_err(&r));
    ck_assert_int_eq(error_code(r.err), ERR_INVALID_ARG);

    r = bg_line_index_get_range(idx, 1, 1, &off, NULL);
    ck_assert(is_err(&r));
    ck_assert_int_eq(error_code(r.err), ERR_INVALID_ARG);

    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_dynamic_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t line[] = "data\n";
    for (int i = 0; i < 10000; i++)
        bg_line_index_append(idx, line, sizeof(line) - 1);
    ck_assert_int_eq(bg_line_index_count(idx), 10000);
    ck_assert_int_ge(idx->capacity, 10000);
    for (int i = 0; i < 10000; i++)
        ck_assert_int_eq(idx->offsets[i], (off_t)(i * 5 + 4));
    off_t off = 0;
    size_t len = 0;
    res_t r = bg_line_index_get_range(idx, 5000, 5000, &off, &len);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(off, (off_t)(4999 * 5));
    ck_assert_uint_eq(len, 5);
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_multi_chunk_offsets)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_line_index_t *idx = bg_line_index_create(ctx);
    const uint8_t chunk1[] = "aaa\nbbb\n";
    const uint8_t chunk2[] = "ccc\nddd\n";
    bg_line_index_append(idx, chunk1, sizeof(chunk1) - 1);
    bg_line_index_append(idx, chunk2, sizeof(chunk2) - 1);
    ck_assert_int_eq(bg_line_index_count(idx), 4);
    ck_assert_int_eq(idx->offsets[0], 3);
    ck_assert_int_eq(idx->offsets[1], 7);
    ck_assert_int_eq(idx->offsets[2], 11);
    ck_assert_int_eq(idx->offsets[3], 15);
    off_t off = 0;
    size_t len = 0;
    res_t r = bg_line_index_get_range(idx, 2, 3, &off, &len);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(off, 4);
    ck_assert_uint_eq(len, 8);
    bg_line_index_destroy(idx);
    talloc_free(ctx);
}
END_TEST

static Suite *bg_line_index_suite(void)
{
    Suite *s = suite_create("bg_line_index");

    TCase *tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_create_destroy);
    tcase_add_test(tc_lifecycle, test_destroy_null);
    suite_add_tcase(s, tc_lifecycle);

    TCase *tc_count = tcase_create("Count");
    tcase_add_test(tc_count, test_count_empty);
    suite_add_tcase(s, tc_count);

    TCase *tc_append = tcase_create("Append");
    tcase_add_test(tc_append, test_append_no_newlines);
    tcase_add_test(tc_append, test_append_single_newline);
    tcase_add_test(tc_append, test_append_multiple_lines);
    tcase_add_test(tc_append, test_append_no_trailing_newline);
    tcase_add_test(tc_append, test_append_consecutive_newlines);
    tcase_add_test(tc_append, test_append_null_and_zero);
    tcase_add_test(tc_append, test_single_byte_appends);
    tcase_add_test(tc_append, test_large_single_append);
    suite_add_tcase(s, tc_append);

    TCase *tc_range = tcase_create("GetRange");
    tcase_add_test(tc_range, test_get_range_single_line);
    tcase_add_test(tc_range, test_get_range_multiple_lines);
    tcase_add_test(tc_range, test_get_range_consecutive_newlines);
    tcase_add_test(tc_range, test_get_range_boundary_first_last);
    tcase_add_test(tc_range, test_get_range_out_of_range);
    tcase_add_test(tc_range, test_get_range_null_args);
    suite_add_tcase(s, tc_range);

    TCase *tc_growth = tcase_create("Growth");
    tcase_add_test(tc_growth, test_dynamic_growth);
    tcase_add_test(tc_growth, test_multi_chunk_offsets);
    suite_add_tcase(s, tc_growth);

    return s;
}

int32_t main(void)
{
    Suite *s = bg_line_index_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/bg_line_index_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
