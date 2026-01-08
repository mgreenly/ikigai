#include <check.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/scrollback.h"

// Test 1: Column 0 returns byte offset 0
START_TEST(test_byte_offset_at_col_zero) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(sb, "Hello World", 11);

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 0, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);

    talloc_free(ctx);
}
END_TEST
// Test 2: Column 5 of ASCII text returns byte 5
START_TEST(test_byte_offset_ascii) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(sb, "Hello World", 11);

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 5, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 5);  // " World" starts at byte 5

    talloc_free(ctx);
}

END_TEST
// Test 3: UTF-8 multi-byte characters (e is 2 bytes, 1 column)
START_TEST(test_byte_offset_utf8_multibyte) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    // "cafe" = 5 bytes: c(1) a(1) f(1) e(2) = 4 display cols
    ik_scrollback_append_line(sb, "caf\xc3\xa9", 5);

    size_t byte_offset = 0;
    // Column 4 is after "cafe" (4 display cols), but byte offset is 5 (after e)
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 4, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 5);

    talloc_free(ctx);
}

END_TEST
// Test 4: Wide characters (CJK: 3 bytes, 2 columns each)
START_TEST(test_byte_offset_wide_chars) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    // (nihon) = 9 bytes, 6 display cols (each char is 3 bytes, 2 cols)
    ik_scrollback_append_line(sb, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", 9);

    size_t byte_offset = 0;
    // Column 2 should be at byte 3 (after first CJK char)
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 2, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 3);

    // Column 4 should be at byte 6 (after second CJK char)
    res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 4, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 6);

    talloc_free(ctx);
}

END_TEST
// Test 5: ANSI escape sequences are skipped (0 display width)
START_TEST(test_byte_offset_with_ansi) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    // "\x1b[31mHello\x1b[0m" = red "Hello" reset
    // ANSI: 5 bytes, Hello: 5 bytes, ANSI: 4 bytes = 14 total bytes, 5 display cols
    ik_scrollback_append_line(sb, "\x1b[31mHello\x1b[0m", 14);

    size_t byte_offset = 0;
    // Column 0 should skip ANSI and point to 'H' at byte 5
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 0, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);  // Start of text (ANSI is part of output)

    // Column 3 ("llo") - after "Hel" (3 cols), byte 8 (skip 5 ANSI + 3 chars)
    res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 3, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 8);

    talloc_free(ctx);
}

END_TEST
// Test 6: Column beyond text length returns end of text
START_TEST(test_byte_offset_beyond_text) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(sb, "Short", 5);

    size_t byte_offset = 0;
    // Column 100 is way beyond "Short" (5 cols)
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 100, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 5);  // End of text

    talloc_free(ctx);
}

END_TEST
// Test 7: Empty line returns 0
START_TEST(test_byte_offset_empty_line) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(sb, "", 0);

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 0, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test 8: Invalid line index returns error
START_TEST(test_byte_offset_invalid_line) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    ik_scrollback_append_line(sb, "Test", 4);

    size_t byte_offset = 0;
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 5, 0, &byte_offset);
    ck_assert(is_err(&res));

    talloc_free(ctx);
}

END_TEST
// Test 9: Mixed content (ASCII + ANSI + UTF-8)
START_TEST(test_byte_offset_mixed_content) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);
    // "Hi \x1b[1mWorld\x1b[0m" = "Hi " + ANSI(4) + "World" + ANSI(4)
    // Bytes: 3 + 4 + 5 + 4 = 16, Display: 8 cols
    ik_scrollback_append_line(sb, "Hi \x1b[1mWorld\x1b[0m", 16);

    size_t byte_offset = 0;
    // Column 3 = after "Hi ", should be at byte 7 (3 + 4 ANSI)
    res_t res = ik_scrollback_get_byte_offset_at_display_col(sb, 0, 3, &byte_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 7);

    talloc_free(ctx);
}

END_TEST

static Suite *scrollback_byte_offset_suite(void)
{
    Suite *s = suite_create("Scrollback Byte Offset");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_byte_offset_at_col_zero);
    tcase_add_test(tc_core, test_byte_offset_ascii);
    tcase_add_test(tc_core, test_byte_offset_utf8_multibyte);
    tcase_add_test(tc_core, test_byte_offset_wide_chars);
    tcase_add_test(tc_core, test_byte_offset_with_ansi);
    tcase_add_test(tc_core, test_byte_offset_beyond_text);
    tcase_add_test(tc_core, test_byte_offset_empty_line);
    tcase_add_test(tc_core, test_byte_offset_invalid_line);
    tcase_add_test(tc_core, test_byte_offset_mixed_content);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = scrollback_byte_offset_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
