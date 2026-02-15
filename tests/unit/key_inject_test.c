#include <check.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/key_inject.h"
#include "shared/error.h"

// Init/destroy lifecycle
START_TEST(test_init_destroy)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_key_inject_buf_t *buf = ik_key_inject_init(ctx);

    ck_assert_ptr_nonnull(buf);
    ck_assert_ptr_nonnull(buf->data);
    ck_assert_uint_gt(buf->capacity, 0);
    ck_assert_uint_eq(buf->len, 0);
    ck_assert_uint_eq(buf->read_pos, 0);

    ik_key_inject_destroy(buf);
    talloc_free(ctx);
}
END_TEST

// Append and drain single byte
START_TEST(test_append_drain_single)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_key_inject_buf_t *buf = ik_key_inject_init(ctx);

    res_t res = ik_key_inject_append(buf, "A", 1);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(buf->len, 1);

    char byte;
    bool drained = ik_key_inject_drain(buf, &byte);
    ck_assert(drained);
    ck_assert_int_eq(byte, 'A');

    // Buffer should be reset after full drain
    ck_assert_uint_eq(buf->len, 0);
    ck_assert_uint_eq(buf->read_pos, 0);

    ik_key_inject_destroy(buf);
    talloc_free(ctx);
}
END_TEST

// Append multiple bytes, drain in order
START_TEST(test_append_drain_multiple)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_key_inject_buf_t *buf = ik_key_inject_init(ctx);

    const char *data = "ABCD";
    res_t res = ik_key_inject_append(buf, data, 4);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(buf->len, 4);

    char byte;
    for (size_t i = 0; i < 4; i++) {
        bool drained = ik_key_inject_drain(buf, &byte);
        ck_assert(drained);
        ck_assert_int_eq(byte, data[i]);
    }

    // Buffer should be reset
    ck_assert_uint_eq(buf->len, 0);
    ck_assert_uint_eq(buf->read_pos, 0);

    ik_key_inject_destroy(buf);
    talloc_free(ctx);
}
END_TEST

// Drain from empty buffer returns false
START_TEST(test_drain_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_key_inject_buf_t *buf = ik_key_inject_init(ctx);

    char byte;
    bool drained = ik_key_inject_drain(buf, &byte);
    ck_assert(!drained);

    ik_key_inject_destroy(buf);
    talloc_free(ctx);
}
END_TEST

// Multiple appends queue behind each other
START_TEST(test_multiple_appends)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_key_inject_buf_t *buf = ik_key_inject_init(ctx);

    res_t res = ik_key_inject_append(buf, "AB", 2);
    ck_assert(is_ok(&res));
    res = ik_key_inject_append(buf, "CD", 2);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(buf->len, 4);

    char byte;
    ck_assert(ik_key_inject_drain(buf, &byte));
    ck_assert_int_eq(byte, 'A');
    ck_assert(ik_key_inject_drain(buf, &byte));
    ck_assert_int_eq(byte, 'B');
    ck_assert(ik_key_inject_drain(buf, &byte));
    ck_assert_int_eq(byte, 'C');
    ck_assert(ik_key_inject_drain(buf, &byte));
    ck_assert_int_eq(byte, 'D');

    ik_key_inject_destroy(buf);
    talloc_free(ctx);
}
END_TEST

// Pending count
START_TEST(test_pending)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_key_inject_buf_t *buf = ik_key_inject_init(ctx);

    ck_assert_uint_eq(ik_key_inject_pending(buf), 0);

    ik_key_inject_append(buf, "ABCD", 4);
    ck_assert_uint_eq(ik_key_inject_pending(buf), 4);

    char byte;
    ik_key_inject_drain(buf, &byte);
    ck_assert_uint_eq(ik_key_inject_pending(buf), 3);

    ik_key_inject_drain(buf, &byte);
    ik_key_inject_drain(buf, &byte);
    ck_assert_uint_eq(ik_key_inject_pending(buf), 1);

    ik_key_inject_drain(buf, &byte);
    ck_assert_uint_eq(ik_key_inject_pending(buf), 0);

    ik_key_inject_destroy(buf);
    talloc_free(ctx);
}
END_TEST

// Unescape: printable passthrough
START_TEST(test_unescape_passthrough)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *input = "hello";
    char *output;
    size_t output_len;

    res_t res = ik_key_inject_unescape(ctx, input, 5, &output, &output_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(output_len, 5);
    ck_assert_mem_eq(output, "hello", 5);

    talloc_free(ctx);
}
END_TEST

// Unescape: \r → 0x0D
START_TEST(test_unescape_r)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *input = "\\r";
    char *output;
    size_t output_len;

    res_t res = ik_key_inject_unescape(ctx, input, 2, &output, &output_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(output_len, 1);
    ck_assert_int_eq((unsigned char)output[0], 0x0D);

    talloc_free(ctx);
}
END_TEST

// Unescape: \n → 0x0A
START_TEST(test_unescape_n)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *input = "\\n";
    char *output;
    size_t output_len;

    res_t res = ik_key_inject_unescape(ctx, input, 2, &output, &output_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(output_len, 1);
    ck_assert_int_eq((unsigned char)output[0], 0x0A);

    talloc_free(ctx);
}
END_TEST

// Unescape: \t → 0x09
START_TEST(test_unescape_t)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *input = "\\t";
    char *output;
    size_t output_len;

    res_t res = ik_key_inject_unescape(ctx, input, 2, &output, &output_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(output_len, 1);
    ck_assert_int_eq((unsigned char)output[0], 0x09);

    talloc_free(ctx);
}
END_TEST

// Unescape: \\ → 0x5C
START_TEST(test_unescape_backslash)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *input = "\\\\";
    char *output;
    size_t output_len;

    res_t res = ik_key_inject_unescape(ctx, input, 2, &output, &output_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(output_len, 1);
    ck_assert_int_eq((unsigned char)output[0], 0x5C);

    talloc_free(ctx);
}
END_TEST

// Unescape: \x1b → 0x1B
START_TEST(test_unescape_x1b)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *input = "\\x1b";
    char *output;
    size_t output_len;

    res_t res = ik_key_inject_unescape(ctx, input, 4, &output, &output_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(output_len, 1);
    ck_assert_int_eq((unsigned char)output[0], 0x1B);

    talloc_free(ctx);
}
END_TEST

// Unescape: \x7f → 0x7F
START_TEST(test_unescape_x7f)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *input = "\\x7f";
    char *output;
    size_t output_len;

    res_t res = ik_key_inject_unescape(ctx, input, 4, &output, &output_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(output_len, 1);
    ck_assert_int_eq((unsigned char)output[0], 0x7F);

    talloc_free(ctx);
}
END_TEST

// Unescape: mixed ("hi\r" → 0x68 0x69 0x0D)
START_TEST(test_unescape_mixed)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *input = "hi\\r";
    char *output;
    size_t output_len;

    res_t res = ik_key_inject_unescape(ctx, input, 4, &output, &output_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(output_len, 3);
    ck_assert_int_eq((unsigned char)output[0], 0x68);
    ck_assert_int_eq((unsigned char)output[1], 0x69);
    ck_assert_int_eq((unsigned char)output[2], 0x0D);

    talloc_free(ctx);
}
END_TEST

// Unescape: \x1b[A → 0x1B 0x5B 0x41 (arrow up sequence)
START_TEST(test_unescape_arrow_up)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *input = "\\x1b[A";
    char *output;
    size_t output_len;

    res_t res = ik_key_inject_unescape(ctx, input, 6, &output, &output_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(output_len, 3);
    ck_assert_int_eq((unsigned char)output[0], 0x1B);
    ck_assert_int_eq((unsigned char)output[1], 0x5B);
    ck_assert_int_eq((unsigned char)output[2], 0x41);

    talloc_free(ctx);
}
END_TEST

// Buffer growth test
START_TEST(test_buffer_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_key_inject_buf_t *buf = ik_key_inject_init(ctx);

    // Append more than initial capacity
    char large_data[1024];
    memset(large_data, 'X', sizeof(large_data));

    res_t res = ik_key_inject_append(buf, large_data, sizeof(large_data));
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(buf->len, sizeof(large_data));
    ck_assert_uint_ge(buf->capacity, sizeof(large_data));

    // Verify data integrity
    for (size_t i = 0; i < sizeof(large_data); i++) {
        char byte;
        bool drained = ik_key_inject_drain(buf, &byte);
        ck_assert(drained);
        ck_assert_int_eq(byte, 'X');
    }

    ik_key_inject_destroy(buf);
    talloc_free(ctx);
}
END_TEST

// Test NULL pointer handling
START_TEST(test_null_handling)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_key_inject_buf_t *buf = ik_key_inject_init(ctx);

    // drain with NULL buffer
    char byte;
    ck_assert(!ik_key_inject_drain(NULL, &byte));

    // drain with NULL out_byte
    ck_assert(!ik_key_inject_drain(buf, NULL));

    // append with NULL buffer
    res_t res = ik_key_inject_append(NULL, "test", 4);
    ck_assert(is_err(&res));

    // append with NULL data
    res = ik_key_inject_append(buf, NULL, 4);
    ck_assert(is_err(&res));

    // pending with NULL
    ck_assert_uint_eq(ik_key_inject_pending(NULL), 0);

    // unescape with NULL input
    char *output;
    size_t output_len;
    res = ik_key_inject_unescape(ctx, NULL, 5, &output, &output_len);
    ck_assert(is_err(&res));

    // unescape with NULL out
    res = ik_key_inject_unescape(ctx, "test", 4, NULL, &output_len);
    ck_assert(is_err(&res));

    // unescape with NULL out_len
    res = ik_key_inject_unescape(ctx, "test", 4, &output, NULL);
    ck_assert(is_err(&res));

    ik_key_inject_destroy(buf);
    talloc_free(ctx);
}
END_TEST

// Test destroy with NULL
START_TEST(test_destroy_null)
{
    ik_key_inject_destroy(NULL);  // Should not crash
}
END_TEST

static Suite *key_inject_suite(void)
{
    Suite *s = suite_create("key_inject");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_init_destroy);
    tcase_add_test(tc_core, test_append_drain_single);
    tcase_add_test(tc_core, test_append_drain_multiple);
    tcase_add_test(tc_core, test_drain_empty);
    tcase_add_test(tc_core, test_multiple_appends);
    tcase_add_test(tc_core, test_pending);
    tcase_add_test(tc_core, test_buffer_growth);
    tcase_add_test(tc_core, test_null_handling);
    tcase_add_test(tc_core, test_destroy_null);
    suite_add_tcase(s, tc_core);

    TCase *tc_unescape = tcase_create("Unescape");
    tcase_add_test(tc_unescape, test_unescape_passthrough);
    tcase_add_test(tc_unescape, test_unescape_r);
    tcase_add_test(tc_unescape, test_unescape_n);
    tcase_add_test(tc_unescape, test_unescape_t);
    tcase_add_test(tc_unescape, test_unescape_backslash);
    tcase_add_test(tc_unescape, test_unescape_x1b);
    tcase_add_test(tc_unescape, test_unescape_x7f);
    tcase_add_test(tc_unescape, test_unescape_mixed);
    tcase_add_test(tc_unescape, test_unescape_arrow_up);
    suite_add_tcase(s, tc_unescape);

    return s;
}

int32_t main(void)
{
    Suite *s = key_inject_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/key_inject_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
