/**
 * @file message_from_db_test.c
 * @brief Unit tests for message.c ik_message_from_db_msg function
 *
 * Tests all error paths in ik_message_from_db_msg including:
 * - Missing fields in tool_call data_json
 * - Invalid field types in tool_call data_json
 * - Missing fields in tool_result data_json
 * - Invalid field types in tool_result data_json
 * - Success field handling in tool_result
 */

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

// Test: JSON array instead of object for tool_call
START_TEST(test_tool_call_json_array) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "[]"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}
END_TEST
// Test: JSON null for tool_call
START_TEST(test_tool_call_json_null) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "null"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Missing tool_call_id in tool_call
START_TEST(test_tool_call_missing_id) {
    char *kind = talloc_strdup(test_ctx, "tool_call");
    char *data_json = talloc_strdup(test_ctx, "{\"name\":\"bash\",\"arguments\":\"{}\"}");

    ik_msg_t db_msg = {
        .kind = kind,
        .content = NULL,
        .data_json = data_json,
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Missing name in tool_call
START_TEST(test_tool_call_missing_name) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"arguments\":\"{}\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Missing arguments in tool_call
START_TEST(test_tool_call_missing_arguments) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"name\":\"bash\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Invalid field type for tool_call_id (number instead of string)
START_TEST(test_tool_call_invalid_id_type) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":123,\"name\":\"bash\",\"arguments\":\"{}\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Invalid field type for name (number instead of string)
START_TEST(test_tool_call_invalid_name_type) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"name\":456,\"arguments\":\"{}\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Invalid field type for arguments (number instead of string)
START_TEST(test_tool_call_invalid_arguments_type) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"name\":\"bash\",\"arguments\":789}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Valid tool_call succeeds
START_TEST(test_tool_call_valid) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"name\":\"bash\",\"arguments\":\"{}\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    ck_assert_int_eq(out->role, IK_ROLE_ASSISTANT);
}

END_TEST
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

static Suite *message_from_db_suite(void)
{
    Suite *s = suite_create("Message from DB");
    TCase *tc_tool_call = tcase_create("Tool Call");
    tcase_set_timeout(tc_tool_call, 30);
    TCase *tc_tool_result = tcase_create("Tool Result");
    tcase_set_timeout(tc_tool_result, 30);

    tcase_add_checked_fixture(tc_tool_call, setup, teardown);
    tcase_add_test(tc_tool_call, test_tool_call_json_array);
    tcase_add_test(tc_tool_call, test_tool_call_json_null);
    tcase_add_test(tc_tool_call, test_tool_call_missing_id);
    tcase_add_test(tc_tool_call, test_tool_call_missing_name);
    tcase_add_test(tc_tool_call, test_tool_call_missing_arguments);
    tcase_add_test(tc_tool_call, test_tool_call_invalid_id_type);
    tcase_add_test(tc_tool_call, test_tool_call_invalid_name_type);
    tcase_add_test(tc_tool_call, test_tool_call_invalid_arguments_type);
    tcase_add_test(tc_tool_call, test_tool_call_valid);

    tcase_add_checked_fixture(tc_tool_result, setup, teardown);
    tcase_add_test(tc_tool_result, test_tool_result_json_array);
    tcase_add_test(tc_tool_result, test_tool_result_json_null);
    tcase_add_test(tc_tool_result, test_tool_result_missing_id);
    tcase_add_test(tc_tool_result, test_tool_result_missing_output);
    tcase_add_test(tc_tool_result, test_tool_result_invalid_id_type);
    tcase_add_test(tc_tool_result, test_tool_result_invalid_output_type);
    tcase_add_test(tc_tool_result, test_tool_result_success_true);
    tcase_add_test(tc_tool_result, test_tool_result_success_false);
    tcase_add_test(tc_tool_result, test_tool_result_no_success_field);
    tcase_add_test(tc_tool_result, test_tool_kind_handled);
    tcase_add_test(tc_tool_result, test_tool_kind_missing_fields);

    suite_add_tcase(s, tc_tool_call);
    suite_add_tcase(s, tc_tool_result);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = message_from_db_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
