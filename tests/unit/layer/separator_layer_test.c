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
