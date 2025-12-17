// Tests for separator layer wrapper
#include "../../../src/layer_wrappers.h"
#include "../../../src/error.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

START_TEST(test_separator_layer_create_and_visibility) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    ck_assert(layer != NULL);
    ck_assert_str_eq(layer->name, "sep");
    ck_assert(layer->is_visible(layer) == true);

    // Change visibility
    visible = false;
    ck_assert(layer->is_visible(layer) == false);

    talloc_free(ctx);
}
END_TEST START_TEST(test_separator_layer_height)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Separator is always 1 row
    ck_assert_uint_eq(layer->get_height(layer, 80), 1);
    ck_assert_uint_eq(layer->get_height(layer, 40), 1);
    ck_assert_uint_eq(layer->get_height(layer, 200), 1);

    talloc_free(ctx);
}

END_TEST START_TEST(test_separator_layer_render)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    // Render separator at width 10
    layer->render(layer, output, 10, 0, 1);

    // Should be 10 box-drawing chars (3 bytes each) + \r\n = 32 bytes
    ck_assert_uint_eq(output->size, 32);
    // Expected: 10 * (0xE2 0x94 0x80) + \r\n
    const uint8_t expected[] = {0xE2, 0x94, 0x80, 0xE2, 0x94, 0x80, 0xE2, 0x94, 0x80, 0xE2, 0x94, 0x80,
                                0xE2, 0x94, 0x80, 0xE2, 0x94, 0x80, 0xE2, 0x94, 0x80, 0xE2, 0x94, 0x80,
                                0xE2, 0x94, 0x80, 0xE2, 0x94, 0x80, '\r', '\n'};
    ck_assert_int_eq(memcmp(output->data, expected, 32), 0);

    talloc_free(ctx);
}

END_TEST START_TEST(test_separator_layer_render_various_widths)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Test width 5
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);
    layer->render(layer, output, 5, 0, 1);
    // Should be 5 box-drawing chars (3 bytes each) + \r\n = 17 bytes
    ck_assert_uint_eq(output->size, 17); // 5 * 3 + 2
    // Expected: 5 * (0xE2 0x94 0x80) + \r\n
    const uint8_t expected[] = {0xE2, 0x94, 0x80, 0xE2, 0x94, 0x80, 0xE2, 0x94, 0x80,
                                0xE2, 0x94, 0x80, 0xE2, 0x94, 0x80, '\r', '\n'};
    ck_assert_int_eq(memcmp(output->data, expected, 17), 0);

    talloc_free(ctx);
}

END_TEST START_TEST(test_separator_layer_render_unicode_box_drawing)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    // Render separator at width 3
    // Each box-drawing character is 3 bytes (0xE2 0x94 0x80), so 3 chars = 9 bytes + 2 for \r\n = 11 bytes total
    layer->render(layer, output, 3, 0, 1);

    ck_assert_uint_eq(output->size, 11);
    // Expected: 3 box-drawing characters (3 bytes each) + \r\n = 11 bytes
    const uint8_t expected[] = {0xE2, 0x94, 0x80, 0xE2, 0x94, 0x80, 0xE2, 0x94, 0x80, '\r', '\n'};
    ck_assert_int_eq(memcmp(output->data, expected, 11), 0);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_separator_layer_debug_info_microseconds)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set up debug info with render time < 1000us
    size_t viewport_offset = 5;
    size_t viewport_row = 2;
    size_t viewport_height = 10;
    size_t document_height = 20;
    uint64_t render_elapsed_us = 500; // Less than 1000us - should display in microseconds

    ik_separator_layer_set_debug(layer, &viewport_offset, &viewport_row,
                                 &viewport_height, &document_height, &render_elapsed_us);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 80, 0, 1);

    // Should contain "t=500us" (microseconds format)
    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    ck_assert(strstr(output_str, "t=500us") != NULL);
    ck_assert(strstr(output_str, "off=5") != NULL);
    ck_assert(strstr(output_str, "row=2") != NULL);
    ck_assert(strstr(output_str, "h=10") != NULL);
    ck_assert(strstr(output_str, "doc=20") != NULL);
    // sb_rows = doc - 3 = 17
    ck_assert(strstr(output_str, "sb=17") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_debug_info_milliseconds)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set up debug info with render time >= 1000us
    size_t viewport_offset = 3;
    size_t viewport_row = 1;
    size_t viewport_height = 8;
    size_t document_height = 15;
    uint64_t render_elapsed_us = 2500; // >= 1000us - should display in milliseconds

    ik_separator_layer_set_debug(layer, &viewport_offset, &viewport_row,
                                 &viewport_height, &document_height, &render_elapsed_us);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 80, 0, 1);

    // Should contain "t=2.5ms" (milliseconds format)
    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    ck_assert(strstr(output_str, "t=2.5ms") != NULL);
    ck_assert(strstr(output_str, "off=3") != NULL);
    ck_assert(strstr(output_str, "row=1") != NULL);
    ck_assert(strstr(output_str, "h=8") != NULL);
    ck_assert(strstr(output_str, "doc=15") != NULL);
    // sb_rows = doc - 3 = 12
    ck_assert(strstr(output_str, "sb=12") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_debug_info_small_document)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set up debug info with document_height < 3 (should result in sb_rows = 0)
    size_t viewport_offset = 0;
    size_t viewport_row = 0;
    size_t viewport_height = 10;
    size_t document_height = 2; // Less than 3
    uint64_t render_elapsed_us = 100;

    ik_separator_layer_set_debug(layer, &viewport_offset, &viewport_row,
                                 &viewport_height, &document_height, &render_elapsed_us);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 80, 0, 1);

    // Should contain "sb=0" since doc < 3
    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    ck_assert(strstr(output_str, "sb=0") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_debug_info_null_render_elapsed)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set debug info but leave render_elapsed_us as NULL
    size_t viewport_offset = 1;
    size_t viewport_row = 0;
    size_t viewport_height = 5;
    size_t document_height = 10;

    ik_separator_layer_set_debug(layer, &viewport_offset, &viewport_row,
                                 &viewport_height, &document_height, NULL);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 80, 0, 1);

    // Should contain "t=0us" when render_elapsed_us is NULL
    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    ck_assert(strstr(output_str, "t=0us") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_debug_info_null_document_height)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set debug info but leave document_height as NULL
    size_t viewport_offset = 1;
    size_t viewport_row = 0;
    size_t viewport_height = 5;
    uint64_t render_elapsed_us = 100;

    ik_separator_layer_set_debug(layer, &viewport_offset, &viewport_row,
                                 &viewport_height, NULL, &render_elapsed_us);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 80, 0, 1);

    // Should contain "doc=0" and "sb=0" when document_height is NULL
    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    ck_assert(strstr(output_str, "doc=0") != NULL);
    ck_assert(strstr(output_str, "sb=0") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_nav_context_with_parent)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set navigation context with parent
    const char *parent_uuid = "abc123def456";
    const char *current_uuid = "xyz789ghi012";
    ik_separator_layer_set_nav_context(layer, parent_uuid, NULL, current_uuid, NULL, 0);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 80, 0, 1);

    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    // Should contain truncated parent UUID "↑abc123..." (first 6 chars)
    ck_assert(strstr(output_str, "\xE2\x86\x91" "abc123...") != NULL); // ↑ is U+2191
    // Should contain current UUID in brackets (first 6 chars)
    ck_assert(strstr(output_str, "[xyz789...]") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_nav_context_root_agent)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set navigation context for root agent (no parent)
    const char *current_uuid = "root123456";
    ik_separator_layer_set_nav_context(layer, NULL, NULL, current_uuid, NULL, 0);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 80, 0, 1);

    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    // Should contain grayed "-" for parent (dim color: ESC[2m)
    ck_assert(strstr(output_str, "\x1b[2m\xE2\x86\x91-\x1b[0m") != NULL);
    // Should contain current UUID
    ck_assert(strstr(output_str, "[root12...]") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_nav_context_siblings)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set navigation context with siblings
    const char *prev_uuid = "prev123456";
    const char *current_uuid = "curr789012";
    const char *next_uuid = "next345678";
    ik_separator_layer_set_nav_context(layer, NULL, prev_uuid, current_uuid, next_uuid, 0);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 80, 0, 1);

    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    // Should contain previous sibling "←prev12..." (first 6 chars of "prev123456")
    ck_assert(strstr(output_str, "\xE2\x86\x90" "prev12...") != NULL); // ← is U+2190
    // Should contain next sibling "→next34..." (first 6 chars of "next345678")
    ck_assert(strstr(output_str, "\xE2\x86\x92" "next34...") != NULL); // → is U+2192

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_nav_context_no_siblings)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set navigation context without siblings
    const char *current_uuid = "only123456";
    ik_separator_layer_set_nav_context(layer, NULL, NULL, current_uuid, NULL, 0);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 80, 0, 1);

    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    // Should contain grayed "-" for prev sibling
    ck_assert(strstr(output_str, "\x1b[2m\xE2\x86\x90-\x1b[0m") != NULL);
    // Should contain grayed "-" for next sibling
    ck_assert(strstr(output_str, "\x1b[2m\xE2\x86\x92-\x1b[0m") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_nav_context_with_children)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set navigation context with children
    const char *current_uuid = "parent12345";
    ik_separator_layer_set_nav_context(layer, NULL, NULL, current_uuid, NULL, 3);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 80, 0, 1);

    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    // Should contain child count "↓3"
    ck_assert(strstr(output_str, "\xE2\x86\x93" "3") != NULL); // ↓ is U+2193
    // Should contain current UUID (first 6 chars of "parent12345")
    ck_assert(strstr(output_str, "[parent...]") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_nav_context_no_children)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set navigation context without children
    const char *current_uuid = "leaf123456";
    ik_separator_layer_set_nav_context(layer, NULL, NULL, current_uuid, NULL, 0);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 80, 0, 1);

    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    // Should contain grayed "-" for children
    ck_assert(strstr(output_str, "\x1b[2m\xE2\x86\x93-\x1b[0m") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_nav_context_uuid_truncation)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Test UUID truncation (first 6 chars + "...")
    const char *parent_uuid = "1234567890abcdef";
    const char *current_uuid = "fedcba0987654321";
    ik_separator_layer_set_nav_context(layer, parent_uuid, NULL, current_uuid, NULL, 0);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 80, 0, 1);

    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    // Parent should be "123456..."
    ck_assert(strstr(output_str, "123456...") != NULL);
    // Current should be "fedcba..."
    ck_assert(strstr(output_str, "[fedcba...]") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_nav_context_with_debug_info)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set both navigation context and debug info
    const char *current_uuid = "test123456";
    ik_separator_layer_set_nav_context(layer, NULL, NULL, current_uuid, NULL, 0);

    size_t viewport_offset = 5;
    size_t viewport_row = 2;
    size_t viewport_height = 10;
    size_t document_height = 20;
    uint64_t render_elapsed_us = 500;
    ik_separator_layer_set_debug(layer, &viewport_offset, &viewport_row,
                                 &viewport_height, &document_height, &render_elapsed_us);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output, 120, 0, 1);

    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);
    // Should contain both nav context and debug info
    // Current UUID "test123456" truncated to first 6 chars: "test12"
    ck_assert(strstr(output_str, "[test12...]") != NULL);
    ck_assert(strstr(output_str, "off=5") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_separator_layer_full_width_with_nav_context)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    bool visible = true;
    ik_layer_t *layer = ik_separator_layer_create(ctx, "sep", &visible);

    // Set navigation context with all indicators dimmed (to test ANSI codes)
    const char *current_uuid = "abc123def456";
    ik_separator_layer_set_nav_context(layer, NULL, NULL, current_uuid, NULL, 0);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
    size_t width = 80;
    layer->render(layer, output, width, 0, 1);

    // Count visual width excluding ANSI codes
    char *output_str = talloc_strndup(ctx, (const char *)output->data, output->size);

    // Remove trailing \r\n for width calculation
    size_t content_len = output->size;
    if (content_len >= 2 && output_str[content_len - 2] == '\r' && output_str[content_len - 1] == '\n') {
        content_len -= 2;
    }

    // Calculate visual width (excluding ANSI escape codes)
    size_t visual_width = 0;
    bool in_escape = false;
    for (size_t i = 0; i < content_len; i++) {
        if (output_str[i] == '\x1b') {
            in_escape = true;
            continue;
        }
        if (in_escape) {
            if ((output_str[i] >= 'A' && output_str[i] <= 'Z') ||
                (output_str[i] >= 'a' && output_str[i] <= 'z')) {
                in_escape = false;
            }
            continue;
        }
        // Count UTF-8 characters: box-drawing and arrows are 3 bytes each = 1 column
        if ((unsigned char)output_str[i] == 0xE2) {
            // Start of 3-byte UTF-8 sequence
            visual_width++;
            i += 2; // Skip next 2 bytes
        } else {
            visual_width++;
        }
    }

    // The visual width should equal the terminal width
    ck_assert_uint_eq(visual_width, width);

    talloc_free(ctx);
}
END_TEST

static Suite *separator_layer_suite(void)
{
    Suite *s = suite_create("Separator Layer");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_separator_layer_create_and_visibility);
    tcase_add_test(tc_core, test_separator_layer_height);
    tcase_add_test(tc_core, test_separator_layer_render);
    tcase_add_test(tc_core, test_separator_layer_render_various_widths);
    tcase_add_test(tc_core, test_separator_layer_render_unicode_box_drawing);
    tcase_add_test(tc_core, test_separator_layer_debug_info_microseconds);
    tcase_add_test(tc_core, test_separator_layer_debug_info_milliseconds);
    tcase_add_test(tc_core, test_separator_layer_debug_info_small_document);
    tcase_add_test(tc_core, test_separator_layer_debug_info_null_render_elapsed);
    tcase_add_test(tc_core, test_separator_layer_debug_info_null_document_height);
    tcase_add_test(tc_core, test_separator_layer_nav_context_with_parent);
    tcase_add_test(tc_core, test_separator_layer_nav_context_root_agent);
    tcase_add_test(tc_core, test_separator_layer_nav_context_siblings);
    tcase_add_test(tc_core, test_separator_layer_nav_context_no_siblings);
    tcase_add_test(tc_core, test_separator_layer_nav_context_with_children);
    tcase_add_test(tc_core, test_separator_layer_nav_context_no_children);
    tcase_add_test(tc_core, test_separator_layer_nav_context_uuid_truncation);
    tcase_add_test(tc_core, test_separator_layer_nav_context_with_debug_info);
    tcase_add_test(tc_core, test_separator_layer_full_width_with_nav_context);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = separator_layer_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
