#include "agent.h"
/**
 * @file repl_exact_user_scenario_test.c
 * @brief Test exact user scenario: 5-row terminal with A, B, C, D in scrollback
 */

#include <check.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/render.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

// Mock write() to capture output
static char mock_output[16384];
static size_t mock_output_len = 0;

ssize_t posix_write_(int fd, const void *buf, size_t count);

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    if (mock_output_len + count < sizeof(mock_output)) {
        memcpy(mock_output + mock_output_len, buf, count);
        mock_output_len += count;
    }
    return (ssize_t)count;
}

static void reset_mock(void)
{
    mock_output_len = 0;
    memset(mock_output, 0, sizeof(mock_output));
}

/**
 * Test: Exact user scenario
 *
 * Terminal: 5 rows
 * Initial scrollback: A, B, C, D (4 lines)
 * At bottom: shows B, C, D, separator, input buffer
 * After Page Up: should show A, B, C, D, separator (input buffer off-screen)
 */
START_TEST(test_exact_user_scenario) {
    reset_mock();
    void *ctx = talloc_new(NULL);
    res_t res;

    // Terminal: 5 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 5;
    term->screen_cols = 80;
    term->tty_fd = 1;

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 5, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL at bottom (offset=0)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    shared->render = render_ctx;

    // Create agent context for display state
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    repl->current = agent;

    // Initialize mutex (required by render_frame)
    pthread_mutex_init(&repl->current->tool_thread_mutex, NULL);

    // Use agent's input buffer
    ik_input_buffer_ensure_layout(agent->input_buffer, 80);

    // Add scrollback A, B, C, D to agent's scrollback
    res = ik_scrollback_append_line(agent->scrollback, "A", 1);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(agent->scrollback, "B", 1);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(agent->scrollback, "C", 1);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(agent->scrollback, "D", 1);
    ck_assert(is_ok(&res));

    agent->viewport_offset = 0;

    // Document: 4 scrollback + 1 (upper sep) + 1 input + 1 (lower sep) = 7 rows
    // Terminal: 5 rows
    // At bottom (offset=0): shows C, D, separator, input buffer, lower separator (A, B off-screen top)

    fprintf(stderr, "\n=== User Scenario: At Bottom ===\n");

    // Render at bottom
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    fprintf(stderr, "Output at bottom:\n%s\n", mock_output);
    fprintf(stderr, "Contains C: %s\n", strstr(mock_output, "C") ? "YES" : "NO");
    fprintf(stderr, "Contains D: %s\n", strstr(mock_output, "D") ? "YES" : "NO");

    // At bottom: should see C, D, separator, input buffer, lower separator (A and B are off-screen top)
    ck_assert_ptr_ne(strstr(mock_output, "C"), NULL);
    ck_assert_ptr_ne(strstr(mock_output, "D"), NULL);

    // Now press Page Up
    reset_mock();
    agent->viewport_offset = 5;

    fprintf(stderr, "\n=== After Page Up ===\n");

    // Render after Page Up
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    fprintf(stderr, "Output after Page Up:\n%s\n", mock_output);
    fprintf(stderr, "Contains A: %s\n", strstr(mock_output, "A") ? "YES" : "NO");
    fprintf(stderr, "Contains B: %s\n", strstr(mock_output, "B") ? "YES" : "NO");
    fprintf(stderr, "Contains D: %s\n", strstr(mock_output, "D") ? "YES" : "NO");

    // After Page Up, should show A, B, C, D, separator (rows 0-4)
    // Input buffer is off-screen (row 5)
    ck_assert_ptr_ne(strstr(mock_output, "A"), NULL);
    ck_assert_ptr_ne(strstr(mock_output, "B"), NULL);
    ck_assert_ptr_ne(strstr(mock_output, "C"), NULL);
    ck_assert_ptr_ne(strstr(mock_output, "D"), NULL);

    talloc_free(ctx);
}
END_TEST

/* Create test suite */
static Suite *exact_scenario_suite(void)
{
    Suite *s = suite_create("Exact User Scenario");

    TCase *tc_scenario = tcase_create("Scenario");
    tcase_set_timeout(tc_scenario, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_scenario, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_scenario, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_scenario, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_scenario, IK_TEST_TIMEOUT);
    tcase_add_test(tc_scenario, test_exact_user_scenario);
    suite_add_tcase(s, tc_scenario);

    return s;
}

int main(void)
{
    Suite *s = exact_scenario_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
