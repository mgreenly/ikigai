/**
 * @file switch_test.c
 * @brief Unit tests for agent switching with state save/restore
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"
#include "../../helpers/test_contexts.h"

// Test fixture data
static ik_repl_ctx_t *repl = NULL;
static ik_shared_ctx_t *shared = NULL;
static TALLOC_CTX *test_ctx = NULL;

// Setup function - runs before each test
static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    // Create minimal repl context for testing
    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Initialize agent array
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;
}

// Teardown function - runs after each test
static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
    repl = NULL;
    shared = NULL;
}

// Helper: Create minimal agent with input buffer and scrollback
static ik_agent_ctx_t *create_test_agent(const char *uuid)
{
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    agent->uuid = talloc_strdup(agent, uuid);
    ck_assert_ptr_nonnull(agent->uuid);

    // Create input buffer
    agent->input_buffer = ik_input_buffer_create(agent);
    ck_assert_ptr_nonnull(agent->input_buffer);

    // Initialize viewport state
    agent->viewport_offset = 0;

    return agent;
}

// Test: Switch to different agent succeeds
START_TEST(test_switch_to_different_agent) {
    ik_agent_ctx_t *agent_a = create_test_agent("agent-a-uuid");
    ik_agent_ctx_t *agent_b = create_test_agent("agent-b-uuid");

    repl->current = agent_a;

    res_t result = ik_repl_switch_agent(repl, agent_b);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, agent_b);
}
END_TEST
// Test: Switch to NULL returns error
START_TEST(test_switch_to_null_returns_error)
{
    ik_agent_ctx_t *agent_a = create_test_agent("agent-a-uuid");
    repl->current = agent_a;

    res_t result = ik_repl_switch_agent(repl, NULL);
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
    ck_assert_ptr_eq(repl->current, agent_a);  // Current unchanged
    talloc_free(result.err);
}

END_TEST
// Test: Switch to same agent is no-op
START_TEST(test_switch_to_same_agent_is_noop)
{
    ik_agent_ctx_t *agent_a = create_test_agent("agent-a-uuid");
    repl->current = agent_a;

    res_t result = ik_repl_switch_agent(repl, agent_a);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, agent_a);  // Current unchanged
}

END_TEST
// Test: Input buffer preserved on outgoing agent
START_TEST(test_input_buffer_preserved_on_outgoing)
{
    ik_agent_ctx_t *agent_a = create_test_agent("agent-a-uuid");
    ik_agent_ctx_t *agent_b = create_test_agent("agent-b-uuid");

    repl->current = agent_a;

    // Type some text into agent A
    const char *text = "Hello from agent A";
    res_t set_result = ik_input_buffer_set_text(agent_a->input_buffer, text, strlen(text));
    ck_assert(is_ok(&set_result));

    // Switch to agent B
    res_t result = ik_repl_switch_agent(repl, agent_b);
    ck_assert(is_ok(&result));

    // Agent A's input buffer should still have the text
    size_t len;
    const char *preserved = ik_input_buffer_get_text(agent_a->input_buffer, &len);
    ck_assert_ptr_nonnull(preserved);
    ck_assert_uint_eq(len, strlen(text));
    ck_assert_int_eq(memcmp(preserved, text, len), 0);
}

END_TEST
// Test: Input buffer restored on incoming agent
START_TEST(test_input_buffer_restored_on_incoming)
{
    ik_agent_ctx_t *agent_a = create_test_agent("agent-a-uuid");
    ik_agent_ctx_t *agent_b = create_test_agent("agent-b-uuid");

    // Set up agent B with some text
    const char *text_b = "Agent B's text";
    res_t set_result = ik_input_buffer_set_text(agent_b->input_buffer, text_b, strlen(text_b));
    ck_assert(is_ok(&set_result));

    // Start on agent A
    repl->current = agent_a;

    // Switch to agent B
    res_t result = ik_repl_switch_agent(repl, agent_b);
    ck_assert(is_ok(&result));

    // Current agent should now be B
    ck_assert_ptr_eq(repl->current, agent_b);

    // Agent B's input buffer should have its text
    size_t len;
    const char *restored = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_ptr_nonnull(restored);
    ck_assert_uint_eq(len, strlen(text_b));
    ck_assert_int_eq(memcmp(restored, text_b, len), 0);
}

END_TEST
// Test: Cursor position preserved and restored
START_TEST(test_cursor_position_preserved)
{
    ik_agent_ctx_t *agent_a = create_test_agent("agent-a-uuid");
    ik_agent_ctx_t *agent_b = create_test_agent("agent-b-uuid");

    repl->current = agent_a;

    // Type text and move cursor
    const char *text = "Hello world";
    res_t set_result = ik_input_buffer_set_text(agent_a->input_buffer, text, strlen(text));
    ck_assert(is_ok(&set_result));

    // Move cursor to position 5 (after "Hello")
    for (int i = 0; i < 6; i++) {
        res_t move_result = ik_input_buffer_cursor_left(agent_a->input_buffer);
        ck_assert(is_ok(&move_result));
    }

    // Get cursor position before switch
    size_t byte_before, grapheme_before;
    res_t pos_result = ik_input_buffer_get_cursor_position(agent_a->input_buffer, &byte_before, &grapheme_before);
    ck_assert(is_ok(&pos_result));

    // Switch to agent B and back
    res_t switch_result = ik_repl_switch_agent(repl, agent_b);
    ck_assert(is_ok(&switch_result));
    switch_result = ik_repl_switch_agent(repl, agent_a);
    ck_assert(is_ok(&switch_result));

    // Cursor position should be preserved
    size_t byte_after, grapheme_after;
    pos_result = ik_input_buffer_get_cursor_position(agent_a->input_buffer, &byte_after, &grapheme_after);
    ck_assert(is_ok(&pos_result));

    ck_assert_uint_eq(byte_before, byte_after);
    ck_assert_uint_eq(grapheme_before, grapheme_after);
}

END_TEST
// Test: Viewport offset preserved and restored
START_TEST(test_viewport_offset_preserved)
{
    ik_agent_ctx_t *agent_a = create_test_agent("agent-a-uuid");
    ik_agent_ctx_t *agent_b = create_test_agent("agent-b-uuid");

    repl->current = agent_a;

    // Set viewport offset on agent A
    agent_a->viewport_offset = 42;

    // Switch to agent B
    res_t result = ik_repl_switch_agent(repl, agent_b);
    ck_assert(is_ok(&result));

    // Agent A should still have its viewport offset
    ck_assert_uint_eq(agent_a->viewport_offset, 42);

    // Switch back to agent A
    result = ik_repl_switch_agent(repl, agent_a);
    ck_assert(is_ok(&result));

    // Viewport offset should be restored
    ck_assert_uint_eq(repl->current->viewport_offset, 42);
}

END_TEST
// Test: repl->current updated after switch
START_TEST(test_repl_current_updated)
{
    ik_agent_ctx_t *agent_a = create_test_agent("agent-a-uuid");
    ik_agent_ctx_t *agent_b = create_test_agent("agent-b-uuid");
    ik_agent_ctx_t *agent_c = create_test_agent("agent-c-uuid");

    // Start on agent A
    repl->current = agent_a;
    ck_assert_ptr_eq(repl->current, agent_a);

    // Switch to B
    res_t result = ik_repl_switch_agent(repl, agent_b);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, agent_b);

    // Switch to C
    result = ik_repl_switch_agent(repl, agent_c);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, agent_c);

    // Switch back to A
    result = ik_repl_switch_agent(repl, agent_a);
    ck_assert(is_ok(&result));
    ck_assert_ptr_eq(repl->current, agent_a);
}

END_TEST
// Test: Complex scenario - typing in A, switch to B, type, switch back to A
START_TEST(test_typing_preserved_across_switches)
{
    ik_agent_ctx_t *agent_a = create_test_agent("agent-a-uuid");
    ik_agent_ctx_t *agent_b = create_test_agent("agent-b-uuid");

    repl->current = agent_a;

    // Type in agent A
    const char *text_a = "Agent A text";
    res_t set_result = ik_input_buffer_set_text(agent_a->input_buffer, text_a, strlen(text_a));
    ck_assert(is_ok(&set_result));

    // Switch to agent B
    res_t result = ik_repl_switch_agent(repl, agent_b);
    ck_assert(is_ok(&result));

    // Type in agent B
    const char *text_b = "Agent B text";
    set_result = ik_input_buffer_set_text(agent_b->input_buffer, text_b, strlen(text_b));
    ck_assert(is_ok(&set_result));

    // Switch back to agent A
    result = ik_repl_switch_agent(repl, agent_a);
    ck_assert(is_ok(&result));

    // Agent A's text should be intact
    size_t len;
    const char *preserved = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_ptr_nonnull(preserved);
    ck_assert_uint_eq(len, strlen(text_a));
    ck_assert_int_eq(memcmp(preserved, text_a, len), 0);

    // Switch back to B
    result = ik_repl_switch_agent(repl, agent_b);
    ck_assert(is_ok(&result));

    // Agent B's text should be intact
    preserved = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_ptr_nonnull(preserved);
    ck_assert_uint_eq(len, strlen(text_b));
    ck_assert_int_eq(memcmp(preserved, text_b, len), 0);
}

END_TEST

// Create test suite
static Suite *repl_switch_suite(void)
{
    Suite *s = suite_create("Agent Switch");

    TCase *tc_basic = tcase_create("Basic Switch");
    tcase_set_timeout(tc_basic, 30);
    tcase_set_timeout(tc_basic, 30);
    tcase_set_timeout(tc_basic, 30);
    tcase_set_timeout(tc_basic, 30);
    tcase_set_timeout(tc_basic, 30);
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_switch_to_different_agent);
    tcase_add_test(tc_basic, test_switch_to_null_returns_error);
    tcase_add_test(tc_basic, test_switch_to_same_agent_is_noop);
    suite_add_tcase(s, tc_basic);

    TCase *tc_state = tcase_create("State Preservation");
    tcase_set_timeout(tc_state, 30);
    tcase_set_timeout(tc_state, 30);
    tcase_set_timeout(tc_state, 30);
    tcase_set_timeout(tc_state, 30);
    tcase_set_timeout(tc_state, 30);
    tcase_add_checked_fixture(tc_state, setup, teardown);
    tcase_add_test(tc_state, test_input_buffer_preserved_on_outgoing);
    tcase_add_test(tc_state, test_input_buffer_restored_on_incoming);
    tcase_add_test(tc_state, test_cursor_position_preserved);
    tcase_add_test(tc_state, test_viewport_offset_preserved);
    suite_add_tcase(s, tc_state);

    TCase *tc_current = tcase_create("Current Update");
    tcase_set_timeout(tc_current, 30);
    tcase_set_timeout(tc_current, 30);
    tcase_set_timeout(tc_current, 30);
    tcase_set_timeout(tc_current, 30);
    tcase_set_timeout(tc_current, 30);
    tcase_add_checked_fixture(tc_current, setup, teardown);
    tcase_add_test(tc_current, test_repl_current_updated);
    suite_add_tcase(s, tc_current);

    TCase *tc_complex = tcase_create("Complex Scenarios");
    tcase_set_timeout(tc_complex, 30);
    tcase_set_timeout(tc_complex, 30);
    tcase_set_timeout(tc_complex, 30);
    tcase_set_timeout(tc_complex, 30);
    tcase_set_timeout(tc_complex, 30);
    tcase_add_checked_fixture(tc_complex, setup, teardown);
    tcase_add_test(tc_complex, test_typing_preserved_across_switches);
    suite_add_tcase(s, tc_complex);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_switch_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
