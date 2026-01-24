#include "../../test_constants.h"
// Tests for spinner layer wrapper
#include "../../../src/layer_wrappers.h"
#include "../../../src/error.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

START_TEST(test_spinner_layer_create_and_visibility) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_spinner_state_t state = {.frame_index = 0, .visible = true};
    ik_layer_t *layer = ik_spinner_layer_create(ctx, "spinner", &state);

    ck_assert(layer != NULL);
    ck_assert_str_eq(layer->name, "spinner");
    ck_assert(layer->is_visible(layer) == true);

    // Change visibility
    state.visible = false;
    ck_assert(layer->is_visible(layer) == false);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_spinner_layer_height) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_spinner_state_t state = {.frame_index = 0, .visible = true};
    ik_layer_t *layer = ik_spinner_layer_create(ctx, "spinner", &state);

    // Spinner is always 1 row when visible
    ck_assert_uint_eq(layer->get_height(layer, 80), 1);
    ck_assert_uint_eq(layer->get_height(layer, 40), 1);
    ck_assert_uint_eq(layer->get_height(layer, 200), 1);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_spinner_get_frame_cycles) {
    ik_spinner_state_t state = {.frame_index = 0, .visible = true};

    // Test cycling through all 10 frames
    ck_assert_str_eq(ik_spinner_get_frame(&state), "⠋");
    state.frame_index = 1;
    ck_assert_str_eq(ik_spinner_get_frame(&state), "⠙");
    state.frame_index = 2;
    ck_assert_str_eq(ik_spinner_get_frame(&state), "⠹");
    state.frame_index = 3;
    ck_assert_str_eq(ik_spinner_get_frame(&state), "⠸");
    state.frame_index = 4;
    ck_assert_str_eq(ik_spinner_get_frame(&state), "⠼");
    state.frame_index = 5;
    ck_assert_str_eq(ik_spinner_get_frame(&state), "⠴");
    state.frame_index = 6;
    ck_assert_str_eq(ik_spinner_get_frame(&state), "⠦");
    state.frame_index = 7;
    ck_assert_str_eq(ik_spinner_get_frame(&state), "⠧");
    state.frame_index = 8;
    ck_assert_str_eq(ik_spinner_get_frame(&state), "⠇");
    state.frame_index = 9;
    ck_assert_str_eq(ik_spinner_get_frame(&state), "⠏");

    // Test wrapping (frame_index beyond array should wrap)
    state.frame_index = 10;
    ck_assert_str_eq(ik_spinner_get_frame(&state), "⠋");
}

END_TEST

START_TEST(test_spinner_advance) {
    ik_spinner_state_t state = {.frame_index = 0, .visible = true};

    // Advance through all frames
    ck_assert_uint_eq(state.frame_index, 0);
    ik_spinner_advance(&state);
    ck_assert_uint_eq(state.frame_index, 1);
    ik_spinner_advance(&state);
    ck_assert_uint_eq(state.frame_index, 2);
    ik_spinner_advance(&state);
    ck_assert_uint_eq(state.frame_index, 3);
    ik_spinner_advance(&state);
    ck_assert_uint_eq(state.frame_index, 4);
    ik_spinner_advance(&state);
    ck_assert_uint_eq(state.frame_index, 5);
    ik_spinner_advance(&state);
    ck_assert_uint_eq(state.frame_index, 6);
    ik_spinner_advance(&state);
    ck_assert_uint_eq(state.frame_index, 7);
    ik_spinner_advance(&state);
    ck_assert_uint_eq(state.frame_index, 8);
    ik_spinner_advance(&state);
    ck_assert_uint_eq(state.frame_index, 9);

    // Test wrapping
    ik_spinner_advance(&state);
    ck_assert_uint_eq(state.frame_index, 0);
}

END_TEST

START_TEST(test_spinner_layer_render_frame0) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_spinner_state_t state = {.frame_index = 0, .visible = true};
    ik_layer_t *layer = ik_spinner_layer_create(ctx, "spinner", &state);

    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

    // Render spinner
    layer->render(layer, output, 80, 0, 1);

    // Should be "⠋ Waiting for response...\x1b[K\r\n"
    const char *expected = "⠋ Waiting for response...\x1b[K\r\n";
    ck_assert_uint_eq(output->size, strlen(expected));
    ck_assert_int_eq(memcmp(output->data, expected, strlen(expected)), 0);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_spinner_layer_render_all_frames) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_spinner_state_t state = {.frame_index = 0, .visible = true};
    ik_layer_t *layer = ik_spinner_layer_create(ctx, "spinner", &state);

    const char *expected_frames[] = {
        "⠋ Waiting for response...\x1b[K\r\n",
        "⠙ Waiting for response...\x1b[K\r\n",
        "⠹ Waiting for response...\x1b[K\r\n",
        "⠸ Waiting for response...\x1b[K\r\n",
        "⠼ Waiting for response...\x1b[K\r\n",
        "⠴ Waiting for response...\x1b[K\r\n",
        "⠦ Waiting for response...\x1b[K\r\n",
        "⠧ Waiting for response...\x1b[K\r\n",
        "⠇ Waiting for response...\x1b[K\r\n",
        "⠏ Waiting for response...\x1b[K\r\n"
    };

    for (size_t i = 0; i < 10; i++) {
        state.frame_index = i;

        ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

        layer->render(layer, output, 80, 0, 1);

        ck_assert_uint_eq(output->size, strlen(expected_frames[i]));
        ck_assert_int_eq(memcmp(output->data, expected_frames[i], strlen(expected_frames[i])), 0);
    }

    talloc_free(ctx);
}

END_TEST

START_TEST(test_spinner_animation_sequence) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_spinner_state_t state = {.frame_index = 0, .visible = true};
    ik_layer_t *layer = ik_spinner_layer_create(ctx, "spinner", &state);

    // Simulate animation: advance and render multiple times
    const char *braille_frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};

    for (size_t i = 0; i < 20; i++) {  // Two full cycles
        ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

        layer->render(layer, output, 80, 0, 1);

        // Verify frame is cycling correctly (braille characters are 3 bytes UTF-8)
        const char *expected_frame = braille_frames[i % 10];
        ck_assert_int_eq(memcmp(output->data, expected_frame, strlen(expected_frame)), 0);

        ik_spinner_advance(&state);
    }

    // After 20 advances, should be back to frame 0
    ck_assert_uint_eq(state.frame_index, 0);

    talloc_free(ctx);
}

END_TEST

static Suite *spinner_layer_suite(void)
{
    Suite *s = suite_create("Spinner Layer");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_test(tc_core, test_spinner_layer_create_and_visibility);
    tcase_add_test(tc_core, test_spinner_layer_height);
    tcase_add_test(tc_core, test_spinner_get_frame_cycles);
    tcase_add_test(tc_core, test_spinner_advance);
    tcase_add_test(tc_core, test_spinner_layer_render_frame0);
    tcase_add_test(tc_core, test_spinner_layer_render_all_frames);
    tcase_add_test(tc_core, test_spinner_animation_sequence);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = spinner_layer_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/layer/spinner_layer_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
