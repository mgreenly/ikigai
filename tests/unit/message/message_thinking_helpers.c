/**
 * @file message_thinking_helpers.c
 * @brief Unit tests for message.c thinking block handling
 *
 * Tests thinking and redacted thinking blocks in tool_call messages.
 */

#include "message_thinking_helpers.h"

#include "../../../src/message.h"

#include "../../../src/error.h"
#include "../../../src/msg.h"
#include "../../../src/wrapper.h"
#include "../../../src/vendor/yyjson/yyjson.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Test: Tool call with thinking block
START_TEST(test_from_db_tool_call_with_thinking) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx,
            "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\",\"tool_args\":\"{}\","
            "\"thinking\":{\"text\":\"Let me analyze...\"}}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    ck_assert_int_eq(out->role, IK_ROLE_ASSISTANT);
    ck_assert_uint_eq(out->content_count, 2);
    ck_assert_int_eq(out->content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(out->content_blocks[0].data.thinking.text, "Let me analyze...");
    ck_assert_ptr_null(out->content_blocks[0].data.thinking.signature);
    ck_assert_int_eq(out->content_blocks[1].type, IK_CONTENT_TOOL_CALL);
}

END_TEST

// Test: Tool call with thinking and signature
START_TEST(test_from_db_tool_call_with_signature) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx,
            "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\",\"tool_args\":\"{}\","
            "\"thinking\":{\"text\":\"Think carefully...\",\"signature\":\"EqQBCgIYAhIM...\"}}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    ck_assert_uint_eq(out->content_count, 2);
    ck_assert_int_eq(out->content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(out->content_blocks[0].data.thinking.text, "Think carefully...");
    ck_assert_str_eq(out->content_blocks[0].data.thinking.signature, "EqQBCgIYAhIM...");
    ck_assert_int_eq(out->content_blocks[1].type, IK_CONTENT_TOOL_CALL);
}

END_TEST

// Test: Tool call with redacted thinking
START_TEST(test_from_db_tool_call_with_redacted) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx,
            "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\",\"tool_args\":\"{}\","
            "\"redacted_thinking\":{\"data\":\"EmwKAhgBEgy...\"}}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    ck_assert_uint_eq(out->content_count, 2);
    ck_assert_int_eq(out->content_blocks[0].type, IK_CONTENT_REDACTED_THINKING);
    ck_assert_str_eq(out->content_blocks[0].data.redacted_thinking.data, "EmwKAhgBEgy...");
    ck_assert_int_eq(out->content_blocks[1].type, IK_CONTENT_TOOL_CALL);
}

END_TEST

// Test: Tool call without thinking (backward compatibility)
START_TEST(test_from_db_tool_call_no_thinking) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx,
            "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\",\"tool_args\":\"{}\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    ck_assert_uint_eq(out->content_count, 1);
    ck_assert_int_eq(out->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
}

END_TEST

// Test: Tool call with empty thinking object (treated as no thinking)
START_TEST(test_from_db_tool_call_empty_thinking) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx,
            "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\",\"tool_args\":\"{}\","
            "\"thinking\":{}}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    // Empty thinking object has no text, so no thinking block created
    ck_assert_uint_eq(out->content_count, 1);
    ck_assert_int_eq(out->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
}

END_TEST

// Test: Tool call with both thinking and redacted thinking
START_TEST(test_from_db_tool_call_thinking_and_redacted) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx,
            "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\",\"tool_args\":\"{}\","
            "\"thinking\":{\"text\":\"My thinking...\",\"signature\":\"sig123\"},"
            "\"redacted_thinking\":{\"data\":\"redacted_data\"}}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    ck_assert_uint_eq(out->content_count, 3);
    ck_assert_int_eq(out->content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(out->content_blocks[0].data.thinking.text, "My thinking...");
    ck_assert_str_eq(out->content_blocks[0].data.thinking.signature, "sig123");
    ck_assert_int_eq(out->content_blocks[1].type, IK_CONTENT_REDACTED_THINKING);
    ck_assert_str_eq(out->content_blocks[1].data.redacted_thinking.data, "redacted_data");
    ck_assert_int_eq(out->content_blocks[2].type, IK_CONTENT_TOOL_CALL);
}

END_TEST

// Test: Tool call with thinking as non-object (should skip thinking block)
START_TEST(test_from_db_tool_call_thinking_not_object) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx,
            "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\",\"tool_args\":\"{}\","
            "\"thinking\":\"not an object\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    // thinking is not an object, so it should be skipped
    ck_assert_uint_eq(out->content_count, 1);
    ck_assert_int_eq(out->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
}

END_TEST

// Test: Tool call with redacted_thinking as non-object (should skip redacted block)
START_TEST(test_from_db_tool_call_redacted_not_object) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx,
            "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\",\"tool_args\":\"{}\","
            "\"redacted_thinking\":\"not an object\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    // redacted_thinking is not an object, so it should be skipped
    ck_assert_uint_eq(out->content_count, 1);
    ck_assert_int_eq(out->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
}

END_TEST

// Test: Tool call with thinking object but no text field (should skip thinking block)
START_TEST(test_from_db_tool_call_thinking_no_text) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx,
            "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\",\"tool_args\":\"{}\","
            "\"thinking\":{\"signature\":\"sig123\"}}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    // thinking object has no text, so no thinking block created
    ck_assert_uint_eq(out->content_count, 1);
    ck_assert_int_eq(out->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
}

END_TEST

// Test: Tool call with redacted_thinking object but no data field (should skip redacted block)
START_TEST(test_from_db_tool_call_redacted_no_data) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx,
            "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\",\"tool_args\":\"{}\","
            "\"redacted_thinking\":{\"other_field\":\"value\"}}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    // redacted_thinking object has no data, so no redacted block created
    ck_assert_uint_eq(out->content_count, 1);
    ck_assert_int_eq(out->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
}

END_TEST

TCase *create_thinking_tcase(void)
{
    TCase *tc = tcase_create("Thinking Blocks");
    tcase_set_timeout(tc, 30);

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_from_db_tool_call_with_thinking);
    tcase_add_test(tc, test_from_db_tool_call_with_signature);
    tcase_add_test(tc, test_from_db_tool_call_with_redacted);
    tcase_add_test(tc, test_from_db_tool_call_no_thinking);
    tcase_add_test(tc, test_from_db_tool_call_empty_thinking);
    tcase_add_test(tc, test_from_db_tool_call_thinking_and_redacted);
    tcase_add_test(tc, test_from_db_tool_call_thinking_not_object);
    tcase_add_test(tc, test_from_db_tool_call_redacted_not_object);
    tcase_add_test(tc, test_from_db_tool_call_thinking_no_text);
    tcase_add_test(tc, test_from_db_tool_call_redacted_no_data);

    return tc;
}