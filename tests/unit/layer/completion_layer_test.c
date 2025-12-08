#include "../../../src/layer_wrappers.h"
#include "../../../src/completion.h"
#include "../../../src/commands.h"
#include "../../../src/layer.h"
#include "../../../src/error.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

// Test: completion layer visibility when NULL
START_TEST(test_completion_layer_visibility_null)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_completion_t *completion = NULL;
    ik_layer_t *layer = ik_completion_layer_create(ctx, "completion", &completion);

    ck_assert(layer != NULL);
    ck_assert(!layer->is_visible(layer));

    talloc_free(ctx);
}
END_TEST

// Test: completion layer visibility when not NULL
START_TEST(test_completion_layer_visibility_not_null)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_completion_t *completion = ik_completion_create_for_commands(ctx, "/m");
    ik_layer_t *layer = ik_completion_layer_create(ctx, "completion", &completion);

    ck_assert(layer != NULL);
    ck_assert(completion != NULL);
    ck_assert(layer->is_visible(layer));

    talloc_free(ctx);
}
END_TEST

// Test: completion layer height when NULL
START_TEST(test_completion_layer_height_null)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_completion_t *completion = NULL;
    ik_layer_t *layer = ik_completion_layer_create(ctx, "completion", &completion);

    ck_assert(layer != NULL);
    ck_assert_uint_eq(layer->get_height(layer, 80), 0);

    talloc_free(ctx);
}
END_TEST

// Test: completion layer height equals number of candidates
START_TEST(test_completion_layer_height_matches_count)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_completion_t *completion = ik_completion_create_for_commands(ctx, "/m");
    ik_layer_t *layer = ik_completion_layer_create(ctx, "completion", &completion);

    ck_assert(layer != NULL);
    ck_assert(completion != NULL);
    ck_assert_uint_eq(layer->get_height(layer, 80), completion->count);

    talloc_free(ctx);
}
END_TEST

// Test: completion layer render with no completion
START_TEST(test_completion_layer_render_null)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_completion_t *completion = NULL;
    ik_layer_t *layer = ik_completion_layer_create(ctx, "completion", &completion);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);

    layer->render(layer, output, 80, 0, 0);
    ck_assert_uint_eq(output->size, 0);

    talloc_free(ctx);
}
END_TEST

// Test: completion layer render with single candidate
START_TEST(test_completion_layer_render_single)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_completion_t *completion = ik_completion_create_for_commands(ctx, "/clear");
    ik_layer_t *layer = ik_completion_layer_create(ctx, "completion", &completion);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);

    ck_assert(completion != NULL);
    ck_assert_uint_eq(completion->count, 1);

    layer->render(layer, output, 80, 0, 1);

    // Should contain the command name "clear"
    ck_assert_uint_gt(output->size, 0);
    ck_assert(strstr(output->data, "clear") != NULL);

    // Should contain the description
    size_t cmd_count;
    const ik_command_t *commands = ik_cmd_get_all(&cmd_count);
    for (size_t i = 0; i < cmd_count; i++) {
        if (strcmp(commands[i].name, "clear") == 0) {
            ck_assert(strstr(output->data, commands[i].description) != NULL);
            break;
        }
    }

    talloc_free(ctx);
}
END_TEST

// Test: completion layer render with multiple candidates
START_TEST(test_completion_layer_render_multiple)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_completion_t *completion = ik_completion_create_for_commands(ctx, "/m");
    ik_layer_t *layer = ik_completion_layer_create(ctx, "completion", &completion);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);

    ck_assert(completion != NULL);
    ck_assert_uint_gt(completion->count, 0);

    layer->render(layer, output, 80, 0, completion->count);

    // Should contain at least one command name
    ck_assert_uint_gt(output->size, 0);

    // Each candidate should be rendered on its own line (with \r\n)
    size_t newline_count = 0;
    for (size_t i = 0; i < output->size - 1; i++) {
        if (output->data[i] == '\r' && output->data[i + 1] == '\n') {
            newline_count++;
        }
    }
    ck_assert_uint_eq(newline_count, completion->count);

    talloc_free(ctx);
}
END_TEST

// Test: completion layer selection highlight with ANSI reverse video and bold
START_TEST(test_completion_layer_selection_highlight)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_completion_t *completion = ik_completion_create_for_commands(ctx, "/m");
    ik_layer_t *layer = ik_completion_layer_create(ctx, "completion", &completion);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);

    ck_assert(completion != NULL);
    ck_assert_uint_gt(completion->count, 0);

    // Render the completions
    layer->render(layer, output, 80, 0, completion->count);

    // Should contain ANSI reverse video + bold escape code "\x1b[7;1m"
    ck_assert(strstr(output->data, "\x1b[7;1m") != NULL);

    // Should contain ANSI reset code "\x1b[0m"
    ck_assert(strstr(output->data, "\x1b[0m") != NULL);

    talloc_free(ctx);
}
END_TEST

// Test: completion layer selection highlight moves with current selection
START_TEST(test_completion_layer_selection_highlight_moves)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_completion_t *completion = ik_completion_create_for_commands(ctx, "/m");
    ck_assert(completion != NULL);
    ck_assert_uint_gt(completion->count, 1); // Need at least 2 candidates

    ik_layer_t *layer = ik_completion_layer_create(ctx, "completion", &completion);

    // Render with first selection
    ik_output_buffer_t *output1 = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output1, 80, 0, completion->count);

    // Move to next selection
    ik_completion_next(completion);

    // Render with second selection
    ik_output_buffer_t *output2 = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output2, 80, 0, completion->count);

    // The outputs should be different (highlight moved)
    ck_assert(output1->size != output2->size ||
              memcmp(output1->data, output2->data, output1->size) != 0);

    talloc_free(ctx);
}
END_TEST

// Test: highlight follows Tab cycling through all candidates
START_TEST(test_highlight_follows_current)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_completion_t *completion = ik_completion_create_for_commands(ctx, "/m");
    ck_assert(completion != NULL);
    ck_assert_uint_gt(completion->count, 1);

    ik_layer_t *layer = ik_completion_layer_create(ctx, "completion", &completion);

    // Get first candidate
    const char *first_candidate = completion->candidates[0];

    // Render with current = 0
    ik_output_buffer_t *output0 = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output0, 80, 0, completion->count);

    // First candidate should be highlighted (after reverse video + bold code)
    // Find the pattern: \x1b[7;1m + spaces + first_candidate + spaces
    char *highlight_pattern = talloc_asprintf(ctx, "\x1b[7;1m  %s", first_candidate);
    ck_assert(strstr(output0->data, highlight_pattern) != NULL);

    // Move to next (current = 1)
    ik_completion_next(completion);
    const char *second_candidate = completion->candidates[1];

    // Render again
    ik_output_buffer_t *output1 = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output1, 80, 0, completion->count);

    // Second candidate should now be highlighted
    char *highlight_pattern2 = talloc_asprintf(ctx, "\x1b[7;1m  %s", second_candidate);
    ck_assert(strstr(output1->data, highlight_pattern2) != NULL);

    // First candidate should NOT be highlighted in output1
    ck_assert(strstr(output1->data, highlight_pattern) == NULL);

    talloc_free(ctx);
}
END_TEST

// Test: highlight cycles correctly through all candidates and wraps
START_TEST(test_highlight_cycles_correctly)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_completion_t *completion = ik_completion_create_for_commands(ctx, "/m");
    ck_assert(completion != NULL);
    ck_assert_uint_ge(completion->count, 2);  // Should match at least "mark" and "model"

    size_t total_count = completion->count;
    ik_layer_t *layer = ik_completion_layer_create(ctx, "completion", &completion);

    // Tab through all candidates
    for (size_t i = 0; i < total_count; i++) {
        ck_assert_uint_eq(completion->current, i);

        // Render and verify the correct candidate is highlighted
        ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);
        layer->render(layer, output, 80, 0, completion->count);

        const char *expected_candidate = completion->candidates[i];
        char *highlight_pattern = talloc_asprintf(ctx, "\x1b[7;1m  %s", expected_candidate);
        ck_assert_msg(strstr(output->data, highlight_pattern) != NULL,
                      "Candidate %s not highlighted at position %zu", expected_candidate, i);

        // Move to next
        if (i < total_count - 1) {
            ik_completion_next(completion);
        }
    }

    // Now at last candidate (total_count - 1)
    ck_assert_uint_eq(completion->current, total_count - 1);

    // Tab one more time - should wrap to first
    ik_completion_next(completion);
    ck_assert_uint_eq(completion->current, 0);

    // Render and verify first candidate is highlighted again
    ik_output_buffer_t *output_wrapped = ik_output_buffer_create(ctx, 1024);
    layer->render(layer, output_wrapped, 80, 0, completion->count);

    const char *first_candidate = completion->candidates[0];
    char *highlight_pattern_wrapped = talloc_asprintf(ctx, "\x1b[7;1m  %s", first_candidate);
    ck_assert(strstr(output_wrapped->data, highlight_pattern_wrapped) != NULL);

    talloc_free(ctx);
}
END_TEST

// Test: completion layer render formatting (padding and alignment)
START_TEST(test_completion_layer_render_formatting)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_completion_t *completion = ik_completion_create_for_commands(ctx, "/m");
    ik_layer_t *layer = ik_completion_layer_create(ctx, "completion", &completion);
    ik_output_buffer_t *output = ik_output_buffer_create(ctx, 1024);

    ck_assert(completion != NULL);

    layer->render(layer, output, 80, 0, completion->count);

    // Should contain leading spaces (padding)
    ck_assert_uint_gt(output->size, 0);
    ck_assert(output->data[0] == ' ' || output->data[0] == '\x1b');

    talloc_free(ctx);
}
END_TEST

static Suite *completion_layer_suite(void)
{
    Suite *s = suite_create("Completion Layer");

    TCase *tc_visibility = tcase_create("Visibility");
    tcase_add_test(tc_visibility, test_completion_layer_visibility_null);
    tcase_add_test(tc_visibility, test_completion_layer_visibility_not_null);
    suite_add_tcase(s, tc_visibility);

    TCase *tc_height = tcase_create("Height");
    tcase_add_test(tc_height, test_completion_layer_height_null);
    tcase_add_test(tc_height, test_completion_layer_height_matches_count);
    suite_add_tcase(s, tc_height);

    TCase *tc_render = tcase_create("Render");
    tcase_add_test(tc_render, test_completion_layer_render_null);
    tcase_add_test(tc_render, test_completion_layer_render_single);
    tcase_add_test(tc_render, test_completion_layer_render_multiple);
    tcase_add_test(tc_render, test_completion_layer_render_formatting);
    suite_add_tcase(s, tc_render);

    TCase *tc_highlight = tcase_create("Selection Highlight");
    tcase_add_test(tc_highlight, test_completion_layer_selection_highlight);
    tcase_add_test(tc_highlight, test_completion_layer_selection_highlight_moves);
    tcase_add_test(tc_highlight, test_highlight_follows_current);
    tcase_add_test(tc_highlight, test_highlight_cycles_correctly);
    suite_add_tcase(s, tc_highlight);

    return s;
}

int main(void)
{
    Suite *s = completion_layer_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
