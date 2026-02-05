#include "apps/ikigai/agent.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/shared.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <stdlib.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;
static ik_shared_ctx_t *shared;
static ik_agent_ctx_t *agent;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    shared = talloc_zero(test_ctx, ik_shared_ctx_t);

    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
    shared = NULL;
    agent = NULL;
}

START_TEST(test_agent_add_message_single) {
    ik_message_t *msg = ik_message_create_text(test_ctx, IK_ROLE_USER, "Hello");

    res_t res = ik_agent_add_message(agent, msg);

    ck_assert(is_ok(&res));
    ck_assert_uint_eq(agent->message_count, 1);
    ck_assert_ptr_eq(agent->messages[0], msg);
}
END_TEST

START_TEST(test_agent_add_message_growth) {
    // Add 20 messages to test capacity growth
    for (int i = 0; i < 20; i++) {
        char text[32];
        snprintf(text, sizeof(text), "Message %d", i);
        ik_message_t *msg = ik_message_create_text(test_ctx, IK_ROLE_USER, text);

        res_t res = ik_agent_add_message(agent, msg);
        ck_assert(is_ok(&res));
    }

    ck_assert_uint_eq(agent->message_count, 20);
    ck_assert(agent->message_capacity >= 20);

    // Verify all messages are present
    for (int i = 0; i < 20; i++) {
        char expected[32];
        snprintf(expected, sizeof(expected), "Message %d", i);
        ck_assert_str_eq(agent->messages[i]->content_blocks[0].data.text.text, expected);
    }
}

END_TEST

START_TEST(test_agent_clear_messages) {
    // Add some messages
    ik_message_t *msg1 = ik_message_create_text(test_ctx, IK_ROLE_USER, "Hello");
    ik_message_t *msg2 = ik_message_create_text(test_ctx, IK_ROLE_ASSISTANT, "World");

    ik_agent_add_message(agent, msg1);
    ik_agent_add_message(agent, msg2);

    ck_assert_uint_eq(agent->message_count, 2);

    // Clear messages
    ik_agent_clear_messages(agent);

    ck_assert_uint_eq(agent->message_count, 0);
    ck_assert_uint_eq(agent->message_capacity, 0);
    ck_assert_ptr_null(agent->messages);
}

END_TEST

START_TEST(test_agent_clone_messages) {
    // Create source agent with messages
    ik_message_t *msg1 = ik_message_create_text(test_ctx, IK_ROLE_USER, "First");
    ik_message_t *msg2 = ik_message_create_tool_call(test_ctx, "call_1", "grep", "{\"pattern\":\"test\"}");
    ik_message_t *msg3 = ik_message_create_tool_result(test_ctx, "call_1", "result", false);

    ik_agent_add_message(agent, msg1);
    ik_agent_add_message(agent, msg2);
    ik_agent_add_message(agent, msg3);

    // Create destination agent
    ik_agent_ctx_t *dest_agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &dest_agent);
    ck_assert(is_ok(&res));

    // Clone messages
    res = ik_agent_clone_messages(dest_agent, agent);
    ck_assert(is_ok(&res));

    // Verify count
    ck_assert_uint_eq(dest_agent->message_count, 3);

    // Verify deep copy - different pointers
    ck_assert_ptr_ne(dest_agent->messages[0], agent->messages[0]);
    ck_assert_ptr_ne(dest_agent->messages[1], agent->messages[1]);
    ck_assert_ptr_ne(dest_agent->messages[2], agent->messages[2]);

    // Verify content is identical
    ck_assert_str_eq(dest_agent->messages[0]->content_blocks[0].data.text.text, "First");
    ck_assert_str_eq(dest_agent->messages[1]->content_blocks[0].data.tool_call.name, "grep");
    ck_assert_str_eq(dest_agent->messages[2]->content_blocks[0].data.tool_result.content, "result");
}

END_TEST

static Suite *message_management_suite(void)
{
    Suite *s = suite_create("Agent Message Management");

    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_agent_add_message_single);
    tcase_add_test(tc, test_agent_add_message_growth);
    tcase_add_test(tc, test_agent_clear_messages);
    tcase_add_test(tc, test_agent_clone_messages);

    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = message_management_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/agent/message_management_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
