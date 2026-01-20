#include "../../test_constants.h"
/**
 * @file message_tool_call_helpers.c
 * @brief Unit tests for message.c tool_call message handling
 *
 * Tests error paths and success cases for tool_call messages in ik_message_from_db_msg.
 */

#include "message_tool_call_helper.h"

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
    char *data_json = talloc_strdup(test_ctx, "{\"tool_name\":\"bash\",\"tool_args\":\"{}\"}");

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
// Test: Missing tool_name in tool_call
START_TEST(test_tool_call_missing_name) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"tool_args\":\"{}\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Missing tool_args in tool_call
START_TEST(test_tool_call_missing_arguments) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\"}"),
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
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":123,\"tool_name\":\"bash\",\"tool_args\":\"{}\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Invalid field type for tool_name (number instead of string)
START_TEST(test_tool_call_invalid_name_type) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{\"tool_call_id\":\"call_123\",\"tool_name\":456,\"tool_args\":\"{}\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST
// Test: Invalid field type for tool_args (number instead of string)
START_TEST(test_tool_call_invalid_arguments_type) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx,
                                   "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\",\"tool_args\":789}"),
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
        .data_json = talloc_strdup(test_ctx,
                                   "{\"tool_call_id\":\"call_123\",\"tool_name\":\"bash\",\"tool_args\":\"{}\"}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(out);
    ck_assert_int_eq(out->role, IK_ROLE_ASSISTANT);
    ck_assert_uint_eq(out->content_count, 1);
    ck_assert_int_eq(out->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
}

END_TEST

TCase *create_tool_call_tcase(void)
{
    TCase *tc = tcase_create("Tool Call");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_tool_call_json_array);
    tcase_add_test(tc, test_tool_call_json_null);
    tcase_add_test(tc, test_tool_call_missing_id);
    tcase_add_test(tc, test_tool_call_missing_name);
    tcase_add_test(tc, test_tool_call_missing_arguments);
    tcase_add_test(tc, test_tool_call_invalid_id_type);
    tcase_add_test(tc, test_tool_call_invalid_name_type);
    tcase_add_test(tc, test_tool_call_invalid_arguments_type);
    tcase_add_test(tc, test_tool_call_valid);

    return tc;
}
