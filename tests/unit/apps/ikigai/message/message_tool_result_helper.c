#include "tests/test_constants.h"
/**
 * @file message_tool_result_helpers.c
 * @brief Unit tests for message.c tool_result message handling
 *
 * Tests error paths and success cases for tool_result messages in ik_message_from_db_msg.
 */

#include "message_tool_result_helper.h"

#include "apps/ikigai/message.h"

#include "shared/error.h"
#include "apps/ikigai/msg.h"
#include "shared/wrapper.h"
#include "vendor/yyjson/yyjson.h"

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

// Test: JSON array instead of object for tool_result
START_TEST(test_tool_result_json_array) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_result"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "[]"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: JSON null for tool_result
START_TEST(test_tool_result_json_null) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_result"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "null"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Missing tool_call_id in tool_result
START_TEST(test_tool_result_missing_id) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_result"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"output\":\"result\",\"success\":true}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Missing output in tool_result
START_TEST(test_tool_result_missing_output) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_result"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"success\":true}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Invalid field type for tool_call_id in tool_result (number instead of string)
START_TEST(test_tool_result_invalid_id_type) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_result"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":123,\"output\":\"result\",\"success\":true}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Invalid field type for output in tool_result (number instead of string)
START_TEST(test_tool_result_invalid_output_type) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_result"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"output\":456,\"success\":true}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Valid tool_result with success=true
START_TEST(test_tool_result_success_true) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_result"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"output\":\"result\",\"success\":true}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    ck_assert_int_eq(out->role, IK_ROLE_TOOL);
}

END_TEST
// Test: Valid tool_result with success=false
START_TEST(test_tool_result_success_false) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_result"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx,
                                   "{\"tool_call_id\":\"call_123\",\"output\":\"error occurred\",\"success\":false}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    ck_assert_int_eq(out->role, IK_ROLE_TOOL);
}

END_TEST
// Test: Valid tool_result without success field (defaults to false)
START_TEST(test_tool_result_no_success_field) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_result"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"output\":\"result\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    ck_assert_int_eq(out->role, IK_ROLE_TOOL);
}

END_TEST
// Test: "tool" kind is handled same as "tool_result"
START_TEST(test_tool_kind_handled) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"output\":\"result\",\"success\":true}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    ck_assert_int_eq(out->role, IK_ROLE_TOOL);
}

END_TEST
// Test: "tool" kind with missing fields
START_TEST(test_tool_kind_missing_fields) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"output\":\"result\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST

TCase *create_tool_result_tcase(void)
{
    TCase *tc = tcase_create("Tool Result");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_tool_result_json_array);
    tcase_add_test(tc, test_tool_result_json_null);
    tcase_add_test(tc, test_tool_result_missing_id);
    tcase_add_test(tc, test_tool_result_missing_output);
    tcase_add_test(tc, test_tool_result_invalid_id_type);
    tcase_add_test(tc, test_tool_result_invalid_output_type);
    tcase_add_test(tc, test_tool_result_success_true);
    tcase_add_test(tc, test_tool_result_success_false);
    tcase_add_test(tc, test_tool_result_no_success_field);
    tcase_add_test(tc, test_tool_kind_handled);
    tcase_add_test(tc, test_tool_kind_missing_fields);

    return tc;
}
