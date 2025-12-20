// Tests for separator layer navigation context display
#include "../../../src/layer_wrappers.h"
#include "../../../src/error.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

START_TEST(test_separator_layer_nav_context_with_parent) {
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
END_TEST START_TEST(test_separator_layer_nav_context_root_agent)
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

END_TEST START_TEST(test_separator_layer_nav_context_siblings)
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

END_TEST START_TEST(test_separator_layer_nav_context_no_siblings)
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

END_TEST START_TEST(test_separator_layer_nav_context_with_children)
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

END_TEST START_TEST(test_separator_layer_nav_context_no_children)
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

END_TEST START_TEST(test_separator_layer_nav_context_uuid_truncation)
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

END_TEST START_TEST(test_separator_layer_full_width_with_nav_context)
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

static Suite *separator_layer_nav_suite(void)
{
    Suite *s = suite_create("Separator Layer Navigation");

    TCase *tc_nav = tcase_create("Navigation Context");
    tcase_add_test(tc_nav, test_separator_layer_nav_context_with_parent);
    tcase_add_test(tc_nav, test_separator_layer_nav_context_root_agent);
    tcase_add_test(tc_nav, test_separator_layer_nav_context_siblings);
    tcase_add_test(tc_nav, test_separator_layer_nav_context_no_siblings);
    tcase_add_test(tc_nav, test_separator_layer_nav_context_with_children);
    tcase_add_test(tc_nav, test_separator_layer_nav_context_no_children);
    tcase_add_test(tc_nav, test_separator_layer_nav_context_uuid_truncation);
    tcase_add_test(tc_nav, test_separator_layer_full_width_with_nav_context);
    suite_add_tcase(s, tc_nav);

    return s;
}

int main(void)
{
    Suite *s = separator_layer_nav_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
