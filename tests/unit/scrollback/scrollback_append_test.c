#include <check.h>
#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/scrollback.h"
#include "../../test_utils_helper.h"

// Test: Append a single line to scrollback
START_TEST(test_scrollback_append_single_line) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    const char *line = "hello world";
    res_t res = ik_scrollback_append_line(sb, line, strlen(line));
    ck_assert(is_ok(&res));

    // Verify count increased
    ck_assert_uint_eq(sb->count, 1);

    // Verify text was stored
    ck_assert_uint_eq(sb->text_offsets[0], 0);
    ck_assert_uint_eq(sb->text_lengths[0], strlen(line));
    ck_assert_mem_eq(sb->text_buffer, line, strlen(line));
    ck_assert_uint_eq(sb->buffer_used, strlen(line) + 1);  // +1 for null terminator

    // Verify layout was calculated (11 chars / 80 width = 1 physical line)
    ck_assert_uint_eq(sb->layouts[0].display_width, 11);
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 1);
    ck_assert_uint_eq(sb->total_physical_lines, 1);

    talloc_free(ctx);
}
END_TEST
// Test: Append multiple lines
START_TEST(test_scrollback_append_multiple_lines) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Append first line
    const char *line1 = "first";
    res_t res = ik_scrollback_append_line(sb, line1, strlen(line1));
    ck_assert(is_ok(&res));

    // Append second line
    const char *line2 = "second";
    res = ik_scrollback_append_line(sb, line2, strlen(line2));
    ck_assert(is_ok(&res));

    // Append third line
    const char *line3 = "third";
    res = ik_scrollback_append_line(sb, line3, strlen(line3));
    ck_assert(is_ok(&res));

    // Verify count
    ck_assert_uint_eq(sb->count, 3);

    // Verify first line
    ck_assert_uint_eq(sb->text_offsets[0], 0);
    ck_assert_uint_eq(sb->text_lengths[0], 5);
    ck_assert_mem_eq(sb->text_buffer + sb->text_offsets[0], "first", 5);

    // Verify second line (offset includes null terminator from first line)
    ck_assert_uint_eq(sb->text_offsets[1], 6);  // 5 + 1 for null terminator
    ck_assert_uint_eq(sb->text_lengths[1], 6);
    ck_assert_mem_eq(sb->text_buffer + sb->text_offsets[1], "second", 6);

    // Verify third line (offset includes null terminators from first two lines)
    ck_assert_uint_eq(sb->text_offsets[2], 13);  // 6 + 6 + 1
    ck_assert_uint_eq(sb->text_lengths[2], 5);
    ck_assert_mem_eq(sb->text_buffer + sb->text_offsets[2], "third", 5);

    // Verify buffer_used (includes 3 null terminators)
    ck_assert_uint_eq(sb->buffer_used, 19);  // 5 + 1 + 6 + 1 + 5 + 1

    // Verify total physical lines
    ck_assert_uint_eq(sb->total_physical_lines, 3);

    talloc_free(ctx);
}

END_TEST
// Test: Append UTF-8 content with various widths
START_TEST(test_scrollback_append_utf8_content) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Line with emoji (2 width each)
    const char *line1 = "Hello 😀 World 🎉";
    res_t res = ik_scrollback_append_line(sb, line1, strlen(line1));
    ck_assert(is_ok(&res));

    // Verify display width: "Hello " (6) + emoji (2) + " World " (7) + emoji (2) = 17
    ck_assert_uint_eq(sb->layouts[0].display_width, 17);
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 1);

    // Line with CJK characters (2 width each)
    const char *line2 = "日本語";  // 3 chars × 2 width = 6
    res = ik_scrollback_append_line(sb, line2, strlen(line2));
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(sb->layouts[1].display_width, 6);
    ck_assert_uint_eq(sb->layouts[1].physical_lines, 1);

    talloc_free(ctx);
}

END_TEST
// Test: Long line that wraps multiple times
START_TEST(test_scrollback_append_long_line) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // 160 character line should wrap to 2 physical lines
    char long_line[161];
    memset(long_line, 'a', 160);
    long_line[160] = '\0';

    res_t res = ik_scrollback_append_line(sb, long_line, 160);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(sb->layouts[0].display_width, 160);
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 2);  // 160 / 80 = 2
    ck_assert_uint_eq(sb->total_physical_lines, 2);

    talloc_free(ctx);
}

END_TEST
// Test: Empty line
START_TEST(test_scrollback_append_empty_line) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    res_t res = ik_scrollback_append_line(sb, "", 0);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(sb->count, 1);
    ck_assert_uint_eq(sb->text_lengths[0], 0);
    ck_assert_uint_eq(sb->layouts[0].display_width, 0);
    ck_assert_uint_eq(sb->layouts[0].physical_lines, 1);  // Empty line still takes 1 physical line

    talloc_free(ctx);
}

END_TEST
// Test: Array growth when capacity is exceeded
START_TEST(test_scrollback_array_growth) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    size_t initial_capacity = sb->capacity;
    ck_assert_uint_eq(initial_capacity, 16);  // From INITIAL_LINE_CAPACITY

    // Append 17 lines to trigger growth
    for (size_t i = 0; i < 17; i++) {
        char line[32];
        snprintf(line, sizeof(line), "line %" PRIu64, (uint64_t)i);
        res_t res = ik_scrollback_append_line(sb, line, strlen(line));
        ck_assert(is_ok(&res));
    }

    // Verify capacity doubled
    ck_assert_uint_eq(sb->count, 17);
    ck_assert_uint_eq(sb->capacity, 32);  // Doubled from 16

    // Verify all lines are accessible
    ck_assert_uint_eq(sb->text_lengths[0], 6);  // "line 0"
    ck_assert_uint_eq(sb->text_lengths[16], 7); // "line 16"

    talloc_free(ctx);
}

END_TEST
// Test: Text buffer growth when capacity is exceeded
START_TEST(test_scrollback_buffer_growth) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    size_t initial_buffer_capacity = sb->buffer_capacity;
    ck_assert_uint_eq(initial_buffer_capacity, 1024);  // From INITIAL_BUFFER_CAPACITY

    // Append lines with 100 chars each until we exceed 1024 bytes
    char long_line[101];
    memset(long_line, 'x', 100);
    long_line[100] = '\0';

    for (size_t i = 0; i < 11; i++) {  // 11 × 100 = 1100 > 1024
        res_t res = ik_scrollback_append_line(sb, long_line, 100);
        ck_assert(is_ok(&res));
    }

    // Verify buffer capacity grew (11 lines * (100 bytes + 1 null) = 1111)
    ck_assert_uint_eq(sb->count, 11);
    ck_assert_uint_eq(sb->buffer_used, 1111);  // 11 * (100 + 1)
    ck_assert_uint_ge(sb->buffer_capacity, 2048);  // Doubled from 1024

    talloc_free(ctx);
}

END_TEST
// Test: Buffer growth requiring multiple doublings
START_TEST(test_scrollback_buffer_multiple_doublings) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Append a very large line that requires multiple doublings
    // Initial capacity: 1024
    // Need 3000 bytes: requires doubling to 2048, then 4096
    char huge_line[3001];
    memset(huge_line, 'y', 3000);
    huge_line[3000] = '\0';

    res_t res = ik_scrollback_append_line(sb, huge_line, 3000);
    ck_assert(is_ok(&res));

    // Verify buffer grew with multiple doublings (3000 bytes + 1 null = 3001)
    ck_assert_uint_eq(sb->count, 1);
    ck_assert_uint_eq(sb->buffer_used, 3001);  // 3000 + 1 for null terminator
    ck_assert_uint_ge(sb->buffer_capacity, 4096);  // 1024 → 2048 → 4096

    talloc_free(ctx);
}

END_TEST
// Test: Invalid UTF-8 sequence handling
START_TEST(test_scrollback_append_invalid_utf8) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Create a line with invalid UTF-8 sequence
    // 0xFF is not a valid UTF-8 start byte
    const char invalid_utf8[] = "Hello\xFFWorld";
    res_t res = ik_scrollback_append_line(sb, invalid_utf8, 11);

    // Should succeed but treat invalid byte as width 1
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(sb->count, 1);
    // "Hello" (5) + invalid byte (1) + "World" (5) = 11
    ck_assert_uint_eq(sb->layouts[0].display_width, 11);

    talloc_free(ctx);
}

END_TEST
// Test: Control characters (negative width from utf8proc_charwidth)
START_TEST(test_scrollback_append_control_chars) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // Append line with control characters
    // NULL (0x00), backspace (0x08), ESC (0x1B) all have negative width from utf8proc
    const char control_line[] = "Hello\x00\x08\x1BWorld";
    res_t res = ik_scrollback_append_line(sb, control_line, 14);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(sb->count, 1);
    // "Hello" (5) + NULL (0) + backspace (0) + ESC (0) + "World" (5) = 10
    ck_assert_uint_eq(sb->layouts[0].display_width, 10);

    talloc_free(ctx);
}

END_TEST
// Test: Line with trailing newline
START_TEST(test_scrollback_append_trailing_newline) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // "A\n" should be 2 physical rows (A + empty line)
    const char *line = "A\n";
    res_t res = ik_scrollback_append_line(sb, line, 2);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(sb->layouts[0].physical_lines, 2);

    talloc_free(ctx);
}

END_TEST
// Test: Line with just newline
START_TEST(test_scrollback_append_just_newline) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // "\n" should be 1 physical row (one empty line)
    const char *line = "\n";
    res_t res = ik_scrollback_append_line(sb, line, 1);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(sb->layouts[0].physical_lines, 1);

    talloc_free(ctx);
}

END_TEST
// Test: Line with multiple newlines
START_TEST(test_scrollback_append_multiple_newlines) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // "\n\n" should be 2 physical rows (two empty lines)
    const char *line = "\n\n";
    res_t res = ik_scrollback_append_line(sb, line, 2);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(sb->layouts[0].physical_lines, 2);

    talloc_free(ctx);
}

END_TEST
// Test: Line with content and multiple trailing newlines
START_TEST(test_scrollback_append_content_multiple_newlines) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // "A\n\n" should be 3 physical rows (A + two empty lines)
    const char *line = "A\n\n";
    res_t res = ik_scrollback_append_line(sb, line, 3);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(sb->layouts[0].physical_lines, 3);

    talloc_free(ctx);
}

END_TEST
// Test: Line with newline followed by control character
START_TEST(test_scrollback_append_newline_control_char) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *sb = ik_scrollback_create(ctx, 80);

    // "A\n\x00" should be 1 physical row (control char has width 0, no trailing line)
    const char line[] = "A\n\x00";
    res_t res = ik_scrollback_append_line(sb, line, 3);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(sb->layouts[0].physical_lines, 1);

    talloc_free(ctx);
}

END_TEST

static Suite *scrollback_append_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Scrollback Append");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_scrollback_append_single_line);
    tcase_add_test(tc_core, test_scrollback_append_multiple_lines);
    tcase_add_test(tc_core, test_scrollback_append_utf8_content);
    tcase_add_test(tc_core, test_scrollback_append_long_line);
    tcase_add_test(tc_core, test_scrollback_append_empty_line);
    tcase_add_test(tc_core, test_scrollback_array_growth);
    tcase_add_test(tc_core, test_scrollback_buffer_growth);
    tcase_add_test(tc_core, test_scrollback_buffer_multiple_doublings);
    tcase_add_test(tc_core, test_scrollback_append_invalid_utf8);
    tcase_add_test(tc_core, test_scrollback_append_control_chars);
    tcase_add_test(tc_core, test_scrollback_append_trailing_newline);
    tcase_add_test(tc_core, test_scrollback_append_just_newline);
    tcase_add_test(tc_core, test_scrollback_append_multiple_newlines);
    tcase_add_test(tc_core, test_scrollback_append_content_multiple_newlines);
    tcase_add_test(tc_core, test_scrollback_append_newline_control_char);

    suite_add_tcase(s, tc_core);
    return s;
}

int32_t main(void)
{
    int32_t number_failed;
    Suite *s;
    SRunner *sr;

    s = scrollback_append_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
