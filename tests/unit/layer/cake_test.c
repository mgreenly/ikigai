#include "../../test_constants.h"
#include "../../../src/layer.h"
#include "../../../src/error.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

// Mock layer implementations for testing
static bool always_visible(const ik_layer_t *layer)
{
    (void)layer;
    return true;
}

static bool never_visible(const ik_layer_t *layer)
{
    (void)layer;
    return false;
}

static size_t fixed_height_5(const ik_layer_t *layer, size_t width)
{
    (void)layer;
    (void)width;
    return 5;
}

static size_t fixed_height_10(const ik_layer_t *layer, size_t width)
{
    (void)layer;
    (void)width;
    return 10;
}

static void render_simple(const ik_layer_t *layer,
                          ik_output_buffer_t *output,
                          size_t width,
                          size_t start_row,
                          size_t row_count)
{
    (void)layer;
    (void)width;
    (void)start_row;
    (void)row_count;
    ik_output_buffer_append(output, "X", 1);
}

START_TEST(test_layer_cake_create) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake = ik_layer_cake_create(ctx, 24);

    ck_assert(cake != NULL);
    ck_assert(cake->layers != NULL);
    ck_assert_uint_eq(cake->layer_count, 0);
    ck_assert_uint_gt(cake->layer_capacity, 0);
    ck_assert_uint_eq(cake->viewport_row, 0);
    ck_assert_uint_eq(cake->viewport_height, 24);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_layer_cake_add_layer_single) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 24);

    ik_layer_t *layer = ik_layer_create(cake, "test", NULL, always_visible, fixed_height_5, render_simple);

    res_t res = ik_layer_cake_add_layer(cake, layer);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cake->layer_count, 1);
    ck_assert_ptr_eq(cake->layers[0], layer);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_add_layer_multiple) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 24);

    ik_layer_t *layer1 = ik_layer_create(cake, "layer1", NULL, always_visible, fixed_height_5, render_simple);
    ik_layer_t *layer2 = ik_layer_create(cake, "layer2", NULL, always_visible, fixed_height_10, render_simple);

    ik_layer_cake_add_layer(cake, layer1);
    ik_layer_cake_add_layer(cake, layer2);

    ck_assert_uint_eq(cake->layer_count, 2);
    ck_assert_ptr_eq(cake->layers[0], layer1);
    ck_assert_ptr_eq(cake->layers[1], layer2);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_add_layer_grows_array) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 24);

    // Add more layers than initial capacity to force growth
    for (size_t i = 0; i < 10; i++) {
        ik_layer_t *layer = ik_layer_create(cake, "layer", NULL, always_visible, fixed_height_5, render_simple);
        ik_layer_cake_add_layer(cake, layer);
    }

    ck_assert_uint_eq(cake->layer_count, 10);
    ck_assert_uint_ge(cake->layer_capacity, 10);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_get_total_height_all_visible) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 24);

    ik_layer_t *layer1 = ik_layer_create(cake, "layer1", NULL, always_visible, fixed_height_5, render_simple);
    ik_layer_t *layer2 = ik_layer_create(cake, "layer2", NULL, always_visible, fixed_height_10, render_simple);

    ik_layer_cake_add_layer(cake, layer1);
    ik_layer_cake_add_layer(cake, layer2);

    size_t total = ik_layer_cake_get_total_height(cake, 80);
    ck_assert_uint_eq(total, 15); // 5 + 10

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_get_total_height_some_invisible) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 24);

    ik_layer_t *layer1 = ik_layer_create(cake, "layer1", NULL, always_visible, fixed_height_5, render_simple);
    ik_layer_t *layer2 = ik_layer_create(cake, "layer2", NULL, never_visible, fixed_height_10, render_simple);
    ik_layer_t *layer3 = ik_layer_create(cake, "layer3", NULL, always_visible, fixed_height_5, render_simple);

    ik_layer_cake_add_layer(cake, layer1);
    ik_layer_cake_add_layer(cake, layer2);
    ik_layer_cake_add_layer(cake, layer3);

    size_t total = ik_layer_cake_get_total_height(cake, 80);
    ck_assert_uint_eq(total, 10); // 5 + 0 (invisible) + 5

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_get_total_height_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 24);

    size_t total = ik_layer_cake_get_total_height(cake, 80);
    ck_assert_uint_eq(total, 0);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_render_simple) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 24);

    ik_layer_t *layer = ik_layer_create(cake, "layer", NULL, always_visible, fixed_height_5, render_simple);
    ik_layer_cake_add_layer(cake, layer);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    ik_layer_cake_render(cake, output, 80);
    ck_assert_uint_eq(output->size, 1);
    ck_assert_int_eq(output->data[0], 'X');

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_render_multiple_layers) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 24);

    ik_layer_t *layer1 = ik_layer_create(cake, "layer1", NULL, always_visible, fixed_height_5, render_simple);
    ik_layer_t *layer2 = ik_layer_create(cake, "layer2", NULL, always_visible, fixed_height_10, render_simple);

    ik_layer_cake_add_layer(cake, layer1);
    ik_layer_cake_add_layer(cake, layer2);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    ik_layer_cake_render(cake, output, 80);
    ck_assert_uint_eq(output->size, 2); // One 'X' from each layer

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_render_skips_invisible) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 24);

    ik_layer_t *layer1 = ik_layer_create(cake, "layer1", NULL, always_visible, fixed_height_5, render_simple);
    ik_layer_t *layer2 = ik_layer_create(cake, "layer2", NULL, never_visible, fixed_height_10, render_simple);

    ik_layer_cake_add_layer(cake, layer1);
    ik_layer_cake_add_layer(cake, layer2);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    ik_layer_cake_render(cake, output, 80);
    ck_assert_uint_eq(output->size, 1); // Only layer1

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_render_viewport_clipping_top) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 10); // Small viewport
    cake->viewport_row = 3;                // Start viewport at row 3

    ik_layer_t *layer = ik_layer_create(cake, "layer", NULL, always_visible, fixed_height_10, render_simple);
    ik_layer_cake_add_layer(cake, layer);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    ik_layer_cake_render(cake, output, 80);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_render_viewport_clipping_bottom) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 5); // Small viewport

    ik_layer_t *layer = ik_layer_create(cake, "layer", NULL, always_visible, fixed_height_10, render_simple);
    ik_layer_cake_add_layer(cake, layer);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    ik_layer_cake_render(cake, output, 80);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_render_early_exit) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 5); // Small viewport

    // Add multiple layers, but viewport can only show first layer
    ik_layer_t *layer1 = ik_layer_create(cake, "layer1", NULL, always_visible, fixed_height_10, render_simple);
    ik_layer_t *layer2 = ik_layer_create(cake, "layer2", NULL, always_visible, fixed_height_10, render_simple);
    ik_layer_cake_add_layer(cake, layer1);
    ik_layer_cake_add_layer(cake, layer2);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    ik_layer_cake_render(cake, output, 80);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_render_layer_outside_viewport) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 5); // Viewport: rows 0-4
    cake->viewport_row = 20;               // Viewport at rows 20-24

    // Layer at rows 0-9 (completely before viewport)
    ik_layer_t *layer = ik_layer_create(cake, "layer", NULL, always_visible, fixed_height_10, render_simple);
    ik_layer_cake_add_layer(cake, layer);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    ik_layer_cake_render(cake, output, 80);
    ck_assert_uint_eq(output->size, 0); // Nothing rendered

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_render_layer_after_viewport) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 5); // Viewport: rows 0-4

    // Add a layer at rows 10-19 (completely after viewport)
    ik_layer_t *layer1 = ik_layer_create(cake, "layer1", NULL, always_visible, fixed_height_10, render_simple);
    // This layer starts at row 10, which is >= viewport_end (5)

    // Add a first layer to offset the second layer
    ik_layer_t *layer0 = ik_layer_create(cake, "layer0", NULL, always_visible, fixed_height_10, render_simple);
    ik_layer_cake_add_layer(cake, layer0);
    ik_layer_cake_add_layer(cake, layer1);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    ik_layer_cake_render(cake, output, 80);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_cake_render_layer_ends_at_viewport_start) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_cake_t *cake;
    cake = ik_layer_cake_create(ctx, 5);
    cake->viewport_row = 10; // Viewport at rows 10-14

    // Layer at rows 0-9, ends exactly at viewport start
    ik_layer_t *layer = ik_layer_create(cake, "layer", NULL, always_visible, fixed_height_10, render_simple);
    ik_layer_cake_add_layer(cake, layer);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    ik_layer_cake_render(cake, output, 80);
    ck_assert_uint_eq(output->size, 0); // Nothing rendered

    talloc_free(ctx);
}

END_TEST

static Suite *layer_cake_suite(void)
{
    Suite *s = suite_create("Layer Cake");

    TCase *tc_create = tcase_create("Create");
    tcase_set_timeout(tc_create, IK_TEST_TIMEOUT);
    tcase_add_test(tc_create, test_layer_cake_create);
    suite_add_tcase(s, tc_create);

    TCase *tc_add = tcase_create("Add Layer");
    tcase_set_timeout(tc_add, IK_TEST_TIMEOUT);
    tcase_add_test(tc_add, test_layer_cake_add_layer_single);
    tcase_add_test(tc_add, test_layer_cake_add_layer_multiple);
    tcase_add_test(tc_add, test_layer_cake_add_layer_grows_array);
    suite_add_tcase(s, tc_add);

    TCase *tc_height = tcase_create("Total Height");
    tcase_set_timeout(tc_height, IK_TEST_TIMEOUT);
    tcase_add_test(tc_height, test_layer_cake_get_total_height_all_visible);
    tcase_add_test(tc_height, test_layer_cake_get_total_height_some_invisible);
    tcase_add_test(tc_height, test_layer_cake_get_total_height_empty);
    suite_add_tcase(s, tc_height);

    TCase *tc_render = tcase_create("Render");
    tcase_set_timeout(tc_render, IK_TEST_TIMEOUT);
    tcase_add_test(tc_render, test_layer_cake_render_simple);
    tcase_add_test(tc_render, test_layer_cake_render_multiple_layers);
    tcase_add_test(tc_render, test_layer_cake_render_skips_invisible);
    tcase_add_test(tc_render, test_layer_cake_render_viewport_clipping_top);
    tcase_add_test(tc_render, test_layer_cake_render_viewport_clipping_bottom);
    tcase_add_test(tc_render, test_layer_cake_render_early_exit);
    tcase_add_test(tc_render, test_layer_cake_render_layer_outside_viewport);
    tcase_add_test(tc_render, test_layer_cake_render_layer_after_viewport);
    tcase_add_test(tc_render, test_layer_cake_render_layer_ends_at_viewport_start);
    suite_add_tcase(s, tc_render);

    return s;
}

int main(void)
{
    Suite *s = layer_cake_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
