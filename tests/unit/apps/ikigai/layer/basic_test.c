#include "tests/test_constants.h"
#include "apps/ikigai/layer.h"
#include "shared/error.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

// Test helper: simple layer implementation
static bool test_layer_visible(const ik_layer_t *layer)
{
    (void)layer;
    return true;
}

static size_t test_layer_height(const ik_layer_t *layer, size_t width)
{
    (void)layer;
    (void)width;
    return 5; // Fixed height
}

static void test_layer_render(const ik_layer_t *layer,
                              ik_output_buffer_t *output,
                              size_t width,
                              size_t start_row,
                              size_t row_count)
{
    (void)layer;
    (void)width;
    (void)start_row;
    (void)row_count;
    ik_output_buffer_append(output, "test", 4);
}

START_TEST(test_output_buffer_create) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_output_buffer_t *buf = ik_output_buffer_create(ctx, 100);

    ck_assert(buf != NULL);
    ck_assert(buf->data != NULL);
    ck_assert_uint_eq(buf->size, 0);
    ck_assert_uint_eq(buf->capacity, 100);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_output_buffer_append_simple) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_output_buffer_t *buf = ik_output_buffer_create(ctx, 100);

    ik_output_buffer_append(buf, "hello", 5);
    ck_assert_uint_eq(buf->size, 5);
    ck_assert_int_eq(memcmp(buf->data, "hello", 5), 0);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_output_buffer_append_multiple) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_output_buffer_t *buf = ik_output_buffer_create(ctx, 100);

    ik_output_buffer_append(buf, "hello", 5);
    ik_output_buffer_append(buf, " ", 1);
    ik_output_buffer_append(buf, "world", 5);

    ck_assert_uint_eq(buf->size, 11);
    ck_assert_int_eq(memcmp(buf->data, "hello world", 11), 0);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_output_buffer_append_grow) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_output_buffer_t *buf = ik_output_buffer_create(ctx, 10); // Small initial capacity

    // Append data that exceeds initial capacity
    const char *data = "this is a long string that exceeds 10 bytes";
    size_t len = strlen(data);

    ik_output_buffer_append(buf, data, len);
    ck_assert_uint_eq(buf->size, len);
    ck_assert_uint_ge(buf->capacity, len);
    ck_assert_int_eq(memcmp(buf->data, data, len), 0);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_output_buffer_append_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_output_buffer_t *buf = ik_output_buffer_create(ctx, 100);

    ik_output_buffer_append(buf, "anything", 0);
    ck_assert_uint_eq(buf->size, 0);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_create) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    int dummy_data = 42;
    ik_layer_t *layer = ik_layer_create(ctx, "test_layer", &dummy_data,
                                        test_layer_visible, test_layer_height, test_layer_render);

    ck_assert(layer != NULL);
    ck_assert_str_eq(layer->name, "test_layer");
    ck_assert_ptr_eq(layer->data, &dummy_data);
    ck_assert_ptr_eq(layer->is_visible, test_layer_visible);
    ck_assert_ptr_eq(layer->get_height, test_layer_height);
    ck_assert_ptr_eq(layer->render, test_layer_render);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_layer_callbacks) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_layer_t *layer = ik_layer_create(ctx, "test", NULL, test_layer_visible, test_layer_height, test_layer_render);

    // Test is_visible callback
    bool visible = layer->is_visible(layer);
    ck_assert(visible == true);

    // Test get_height callback
    size_t height = layer->get_height(layer, 80);
    ck_assert_uint_eq(height, 5);

    // Test render callback
    ik_output_buffer_t *buf = ik_output_buffer_create(ctx, 100);
    layer->render(layer, buf, 80, 0, 5);
    ck_assert_uint_eq(buf->size, 4);
    ck_assert_int_eq(memcmp(buf->data, "test", 4), 0);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_output_buffer_grow_1_5x) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_output_buffer_t *buf = ik_output_buffer_create(ctx, 10);

    // Append 5 bytes (total 5, capacity 10)
    ik_output_buffer_append(buf, "hello", 5);

    // Append 7 more bytes (total 12, needs growth)
    // New capacity should be 15 (10 * 1.5), which is > 12
    ik_output_buffer_append(buf, " world!", 7);

    ck_assert_uint_eq(buf->size, 12);
    ck_assert_uint_eq(buf->capacity, 15);

    talloc_free(ctx);
}

END_TEST

static Suite *layer_suite(void)
{
    Suite *s = suite_create("Layer Basic");

    TCase *tc_output = tcase_create("Output Buffer");
    tcase_set_timeout(tc_output, IK_TEST_TIMEOUT);
    tcase_add_test(tc_output, test_output_buffer_create);
    tcase_add_test(tc_output, test_output_buffer_append_simple);
    tcase_add_test(tc_output, test_output_buffer_append_multiple);
    tcase_add_test(tc_output, test_output_buffer_append_grow);
    tcase_add_test(tc_output, test_output_buffer_append_empty);
    tcase_add_test(tc_output, test_output_buffer_grow_1_5x);
    suite_add_tcase(s, tc_output);

    TCase *tc_layer = tcase_create("Layer");
    tcase_set_timeout(tc_layer, IK_TEST_TIMEOUT);
    tcase_add_test(tc_layer, test_layer_create);
    tcase_add_test(tc_layer, test_layer_callbacks);
    suite_add_tcase(s, tc_layer);

    return s;
}

int main(void)
{
    Suite *s = layer_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/layer/basic_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
