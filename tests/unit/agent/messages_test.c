/**
 * @file agent_messages_test.c
 * @brief Unit tests for agent message management functions
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/agent.h"
#include "../../../src/message.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Helper Functions
 * ================================================================ */

static ik_agent_ctx_t *create_test_agent(TALLOC_CTX *ctx)
{
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    agent->message_count = 0;
    agent->message_capacity = 0;
    agent->messages = NULL;
    return agent;
}

static ik_message_t *create_text_message(TALLOC_CTX *ctx, ik_role_t role, const char *text)
{
    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    msg->role = role;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, text);
    msg->provider_metadata = NULL;
    return msg;
}

/* ================================================================
 * Add Message Tests
 * ================================================================ */

START_TEST(test_add_message_basic) {
    ik_agent_ctx_t *agent = create_test_agent(test_ctx);
    ik_message_t *msg = create_text_message(test_ctx, IK_ROLE_USER, "Hello");

    res_t r = ik_agent_add_message(agent, msg);

    ck_assert(!is_err(&r));
    ck_assert_uint_eq(agent->message_count, 1);
    ck_assert_ptr_eq(agent->messages[0], msg);
}
END_TEST

START_TEST(test_add_multiple_messages) {
    ik_agent_ctx_t *agent = create_test_agent(test_ctx);

    for (int32_t i = 0; i < 20; i++) {
        ik_message_t *msg = create_text_message(test_ctx, IK_ROLE_USER, "Test");
        res_t r = ik_agent_add_message(agent, msg);
        ck_assert(!is_err(&r));
    }

    ck_assert_uint_eq(agent->message_count, 20);
    ck_assert(agent->message_capacity >= 20);
}

END_TEST
/* ================================================================
 * Clear Messages Tests
 * ================================================================ */

START_TEST(test_clear_messages_empty) {
    ik_agent_ctx_t *agent = create_test_agent(test_ctx);

    ik_agent_clear_messages(agent);

    ck_assert_uint_eq(agent->message_count, 0);
    ck_assert_uint_eq(agent->message_capacity, 0);
    ck_assert_ptr_null(agent->messages);
}

END_TEST

START_TEST(test_clear_messages_with_data) {
    ik_agent_ctx_t *agent = create_test_agent(test_ctx);
    ik_message_t *msg = create_text_message(test_ctx, IK_ROLE_USER, "Hello");
    ik_agent_add_message(agent, msg);

    ik_agent_clear_messages(agent);

    ck_assert_uint_eq(agent->message_count, 0);
    ck_assert_uint_eq(agent->message_capacity, 0);
    ck_assert_ptr_null(agent->messages);
}

END_TEST
/* ================================================================
 * Clone Messages Tests
 * ================================================================ */

START_TEST(test_clone_messages_empty) {
    ik_agent_ctx_t *src = create_test_agent(test_ctx);
    ik_agent_ctx_t *dest = create_test_agent(test_ctx);

    res_t r = ik_agent_clone_messages(dest, src);

    ck_assert(!is_err(&r));
    ck_assert_uint_eq(dest->message_count, 0);
}

END_TEST

START_TEST(test_clone_messages_text) {
    ik_agent_ctx_t *src = create_test_agent(test_ctx);
    ik_agent_ctx_t *dest = create_test_agent(test_ctx);

    ik_message_t *msg = create_text_message(test_ctx, IK_ROLE_USER, "Hello");
    ik_agent_add_message(src, msg);

    res_t r = ik_agent_clone_messages(dest, src);

    ck_assert(!is_err(&r));
    ck_assert_uint_eq(dest->message_count, 1);
    ck_assert_int_eq(dest->messages[0]->role, IK_ROLE_USER);
    ck_assert_uint_eq(dest->messages[0]->content_count, 1);
    ck_assert_int_eq(dest->messages[0]->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(dest->messages[0]->content_blocks[0].data.text.text, "Hello");
}

END_TEST

START_TEST(test_clone_messages_thinking) {
    ik_agent_ctx_t *src = create_test_agent(test_ctx);
    ik_agent_ctx_t *dest = create_test_agent(test_ctx);

    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_THINKING;
    msg->content_blocks[0].data.thinking.text = talloc_strdup(msg, "Let me think...");
    msg->content_blocks[0].data.thinking.signature = NULL;
    msg->provider_metadata = NULL;

    ik_agent_add_message(src, msg);

    res_t r = ik_agent_clone_messages(dest, src);

    ck_assert(!is_err(&r));
    ck_assert_uint_eq(dest->message_count, 1);
    ck_assert_int_eq(dest->messages[0]->content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(dest->messages[0]->content_blocks[0].data.thinking.text, "Let me think...");
}

END_TEST

START_TEST(test_clone_thinking_with_signature) {
    ik_agent_ctx_t *src = create_test_agent(test_ctx);
    ik_agent_ctx_t *dest = create_test_agent(test_ctx);

    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_THINKING;
    msg->content_blocks[0].data.thinking.text = talloc_strdup(msg, "Let me analyze...");
    msg->content_blocks[0].data.thinking.signature = talloc_strdup(msg, "EqQBCgIYAhIM...");
    msg->provider_metadata = NULL;

    ik_agent_add_message(src, msg);

    res_t r = ik_agent_clone_messages(dest, src);

    ck_assert(!is_err(&r));
    ck_assert_uint_eq(dest->message_count, 1);
    ck_assert_int_eq(dest->messages[0]->content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(dest->messages[0]->content_blocks[0].data.thinking.text, "Let me analyze...");
    ck_assert_str_eq(dest->messages[0]->content_blocks[0].data.thinking.signature, "EqQBCgIYAhIM...");
}

END_TEST

START_TEST(test_clone_redacted_thinking) {
    ik_agent_ctx_t *src = create_test_agent(test_ctx);
    ik_agent_ctx_t *dest = create_test_agent(test_ctx);

    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_REDACTED_THINKING;
    msg->content_blocks[0].data.redacted_thinking.data = talloc_strdup(msg, "EmwKAhgBEgy...");
    msg->provider_metadata = NULL;

    ik_agent_add_message(src, msg);

    res_t r = ik_agent_clone_messages(dest, src);

    ck_assert(!is_err(&r));
    ck_assert_uint_eq(dest->message_count, 1);
    ck_assert_int_eq(dest->messages[0]->content_blocks[0].type, IK_CONTENT_REDACTED_THINKING);
    ck_assert_str_eq(dest->messages[0]->content_blocks[0].data.redacted_thinking.data, "EmwKAhgBEgy...");
}

END_TEST

START_TEST(test_clone_messages_tool_call) {
    ik_agent_ctx_t *src = create_test_agent(test_ctx);
    ik_agent_ctx_t *dest = create_test_agent(test_ctx);

    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    msg->content_blocks[0].data.tool_call.id = talloc_strdup(msg, "call_123");
    msg->content_blocks[0].data.tool_call.name = talloc_strdup(msg, "test_tool");
    msg->content_blocks[0].data.tool_call.arguments = talloc_strdup(msg, "{\"arg\":\"value\"}");
    msg->provider_metadata = NULL;

    ik_agent_add_message(src, msg);

    res_t r = ik_agent_clone_messages(dest, src);

    ck_assert(!is_err(&r));
    ck_assert_uint_eq(dest->message_count, 1);
    ck_assert_int_eq(dest->messages[0]->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(dest->messages[0]->content_blocks[0].data.tool_call.id, "call_123");
    ck_assert_str_eq(dest->messages[0]->content_blocks[0].data.tool_call.name, "test_tool");
    ck_assert_str_eq(dest->messages[0]->content_blocks[0].data.tool_call.arguments, "{\"arg\":\"value\"}");
}

END_TEST

START_TEST(test_clone_messages_tool_result) {
    ik_agent_ctx_t *src = create_test_agent(test_ctx);
    ik_agent_ctx_t *dest = create_test_agent(test_ctx);

    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_TOOL;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    msg->content_blocks[0].data.tool_result.tool_call_id = talloc_strdup(msg, "call_123");
    msg->content_blocks[0].data.tool_result.content = talloc_strdup(msg, "Result data");
    msg->content_blocks[0].data.tool_result.is_error = false;
    msg->provider_metadata = NULL;

    ik_agent_add_message(src, msg);

    res_t r = ik_agent_clone_messages(dest, src);

    ck_assert(!is_err(&r));
    ck_assert_uint_eq(dest->message_count, 1);
    ck_assert_int_eq(dest->messages[0]->content_blocks[0].type, IK_CONTENT_TOOL_RESULT);
    ck_assert_str_eq(dest->messages[0]->content_blocks[0].data.tool_result.tool_call_id, "call_123");
    ck_assert_str_eq(dest->messages[0]->content_blocks[0].data.tool_result.content, "Result data");
    ck_assert(!dest->messages[0]->content_blocks[0].data.tool_result.is_error);
}

END_TEST

START_TEST(test_clone_messages_with_provider_metadata) {
    ik_agent_ctx_t *src = create_test_agent(test_ctx);
    ik_agent_ctx_t *dest = create_test_agent(test_ctx);

    ik_message_t *msg = create_text_message(test_ctx, IK_ROLE_ASSISTANT, "Response");
    msg->provider_metadata = talloc_strdup(msg, "{\"usage\":{\"tokens\":100}}");
    ik_agent_add_message(src, msg);

    res_t r = ik_agent_clone_messages(dest, src);

    ck_assert(!is_err(&r));
    ck_assert_uint_eq(dest->message_count, 1);
    ck_assert_ptr_nonnull(dest->messages[0]->provider_metadata);
    ck_assert_str_eq(dest->messages[0]->provider_metadata, "{\"usage\":{\"tokens\":100}}");
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *agent_messages_suite(void)
{
    Suite *s = suite_create("Agent Messages");

    TCase *tc_add = tcase_create("Add Messages");
    tcase_set_timeout(tc_add, 30);
    tcase_add_unchecked_fixture(tc_add, setup, teardown);
    tcase_add_test(tc_add, test_add_message_basic);
    tcase_add_test(tc_add, test_add_multiple_messages);
    suite_add_tcase(s, tc_add);

    TCase *tc_clear = tcase_create("Clear Messages");
    tcase_set_timeout(tc_clear, 30);
    tcase_add_unchecked_fixture(tc_clear, setup, teardown);
    tcase_add_test(tc_clear, test_clear_messages_empty);
    tcase_add_test(tc_clear, test_clear_messages_with_data);
    suite_add_tcase(s, tc_clear);

    TCase *tc_clone = tcase_create("Clone Messages");
    tcase_set_timeout(tc_clone, 30);
    tcase_add_unchecked_fixture(tc_clone, setup, teardown);
    tcase_add_test(tc_clone, test_clone_messages_empty);
    tcase_add_test(tc_clone, test_clone_messages_text);
    tcase_add_test(tc_clone, test_clone_messages_thinking);
    tcase_add_test(tc_clone, test_clone_thinking_with_signature);
    tcase_add_test(tc_clone, test_clone_redacted_thinking);
    tcase_add_test(tc_clone, test_clone_messages_tool_call);
    tcase_add_test(tc_clone, test_clone_messages_tool_result);
    tcase_add_test(tc_clone, test_clone_messages_with_provider_metadata);
    suite_add_tcase(s, tc_clone);

    return s;
}

int main(void)
{
    Suite *s = agent_messages_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
