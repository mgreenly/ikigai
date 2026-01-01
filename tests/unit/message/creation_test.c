#include "message.h"
#include "msg.h"
#include "providers/provider.h"
#include "../../test_utils.h"
#include "vendor/yyjson/yyjson.h"

#include <check.h>
#include <stdlib.h>
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
    test_ctx = NULL;
}

START_TEST(test_message_create_text_user) {
    ik_message_t *msg = ik_message_create_text(test_ctx, IK_ROLE_USER, "Hello");

    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->role, IK_ROLE_USER);
    ck_assert_uint_eq(msg->content_count, 1);
    ck_assert_ptr_nonnull(msg->content_blocks);
    ck_assert_int_eq(msg->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(msg->content_blocks[0].data.text.text, "Hello");
    ck_assert_ptr_null(msg->provider_metadata);
}
END_TEST

START_TEST(test_message_create_text_assistant) {
    ik_message_t *msg = ik_message_create_text(test_ctx, IK_ROLE_ASSISTANT, "World");

    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->role, IK_ROLE_ASSISTANT);
    ck_assert_uint_eq(msg->content_count, 1);
    ck_assert_ptr_nonnull(msg->content_blocks);
    ck_assert_int_eq(msg->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(msg->content_blocks[0].data.text.text, "World");
}

END_TEST

START_TEST(test_message_create_tool_call) {
    ik_message_t *msg = ik_message_create_tool_call(test_ctx, "call_123", "grep", "{\"pattern\":\"test\"}");

    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->role, IK_ROLE_ASSISTANT);
    ck_assert_uint_eq(msg->content_count, 1);
    ck_assert_ptr_nonnull(msg->content_blocks);
    ck_assert_int_eq(msg->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(msg->content_blocks[0].data.tool_call.id, "call_123");
    ck_assert_str_eq(msg->content_blocks[0].data.tool_call.name, "grep");
    ck_assert_str_eq(msg->content_blocks[0].data.tool_call.arguments, "{\"pattern\":\"test\"}");
}

END_TEST

START_TEST(test_message_create_tool_result) {
    ik_message_t *msg = ik_message_create_tool_result(test_ctx, "call_123", "result data", false);

    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->role, IK_ROLE_TOOL);
    ck_assert_uint_eq(msg->content_count, 1);
    ck_assert_ptr_nonnull(msg->content_blocks);
    ck_assert_int_eq(msg->content_blocks[0].type, IK_CONTENT_TOOL_RESULT);
    ck_assert_str_eq(msg->content_blocks[0].data.tool_result.tool_call_id, "call_123");
    ck_assert_str_eq(msg->content_blocks[0].data.tool_result.content, "result data");
    ck_assert(!msg->content_blocks[0].data.tool_result.is_error);
}

END_TEST

START_TEST(test_message_from_db_msg_user) {
    char kind[] = "user";
    char content[] = "Hello world";
    ik_msg_t db_msg = {
        .id = 1,
        .kind = kind,
        .content = content,
        .data_json = NULL
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->role, IK_ROLE_USER);
    ck_assert_uint_eq(msg->content_count, 1);
    ck_assert_str_eq(msg->content_blocks[0].data.text.text, "Hello world");
}

END_TEST

START_TEST(test_message_from_db_msg_tool_call) {
    char kind[] = "tool_call";
    char content[] = "grep(pattern=\"test\")";
    char data_json[] =
        "{\"tool_call_id\":\"call_456\",\"name\":\"grep\",\"arguments\":\"{\\\"pattern\\\":\\\"test\\\"}\"}";
    ik_msg_t db_msg = {
        .id = 2,
        .kind = kind,
        .content = content,
        .data_json = data_json
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->role, IK_ROLE_ASSISTANT);
    ck_assert_uint_eq(msg->content_count, 1);
    ck_assert_int_eq(msg->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(msg->content_blocks[0].data.tool_call.id, "call_456");
    ck_assert_str_eq(msg->content_blocks[0].data.tool_call.name, "grep");
    ck_assert_str_eq(msg->content_blocks[0].data.tool_call.arguments, "{\"pattern\":\"test\"}");
}

END_TEST

START_TEST(test_message_from_db_msg_tool_result) {
    char kind[] = "tool_result";
    char content[] = "3 files found";
    char data_json[] = "{\"tool_call_id\":\"call_456\",\"output\":\"file1.c\\nfile2.c\\nfile3.c\",\"success\":true}";
    ik_msg_t db_msg = {
        .id = 3,
        .kind = kind,
        .content = content,
        .data_json = data_json
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->role, IK_ROLE_TOOL);
    ck_assert_uint_eq(msg->content_count, 1);
    ck_assert_int_eq(msg->content_blocks[0].type, IK_CONTENT_TOOL_RESULT);
    ck_assert_str_eq(msg->content_blocks[0].data.tool_result.tool_call_id, "call_456");
    ck_assert_str_eq(msg->content_blocks[0].data.tool_result.content, "file1.c\nfile2.c\nfile3.c");
    ck_assert(!msg->content_blocks[0].data.tool_result.is_error);
}

END_TEST

START_TEST(test_message_from_db_msg_system) {
    char kind[] = "system";
    char content[] = "You are a helpful assistant";
    ik_msg_t db_msg = {
        .id = 4,
        .kind = kind,
        .content = content,
        .data_json = NULL
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_ok(&res));
    ck_assert_ptr_null(msg);  // System messages return NULL
}

END_TEST

START_TEST(test_message_from_db_msg_invalid_json) {
    char kind[] = "tool_call";
    char content[] = "invalid";
    char data_json[] = "{invalid json";
    ik_msg_t db_msg = {
        .id = 5,
        .kind = kind,
        .content = content,
        .data_json = data_json
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_err(&res));
}

END_TEST

START_TEST(test_message_from_db_msg_user_missing_content) {
    char kind[] = "user";
    ik_msg_t db_msg = {
        .id = 6,
        .kind = kind,
        .content = NULL,
        .data_json = NULL
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_err(&res));
}

END_TEST

START_TEST(test_message_from_db_msg_assistant_missing_content) {
    char kind[] = "assistant";
    ik_msg_t db_msg = {
        .id = 7,
        .kind = kind,
        .content = NULL,
        .data_json = NULL
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_err(&res));
}

END_TEST

START_TEST(test_message_from_db_msg_tool_call_missing_data_json) {
    char kind[] = "tool_call";
    char content[] = "Some tool call";
    ik_msg_t db_msg = {
        .id = 8,
        .kind = kind,
        .content = content,
        .data_json = NULL
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_err(&res));
}

END_TEST

START_TEST(test_message_from_db_msg_tool_call_invalid_field_types) {
    char kind[] = "tool_call";
    char content[] = "Tool call";
    char data_json[] = "{\"tool_call_id\":123,\"name\":\"test\",\"arguments\":\"{}\"}";
    ik_msg_t db_msg = {
        .id = 9,
        .kind = kind,
        .content = content,
        .data_json = data_json
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_err(&res));
}

END_TEST

START_TEST(test_message_from_db_msg_tool_result_missing_data_json) {
    char kind[] = "tool_result";
    char content[] = "Result";
    ik_msg_t db_msg = {
        .id = 10,
        .kind = kind,
        .content = content,
        .data_json = NULL
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_err(&res));
}

END_TEST

START_TEST(test_message_from_db_msg_tool_result_invalid_json) {
    char kind[] = "tool_result";
    char content[] = "Result";
    char data_json[] = "{invalid}";
    ik_msg_t db_msg = {
        .id = 11,
        .kind = kind,
        .content = content,
        .data_json = data_json
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_err(&res));
}

END_TEST

START_TEST(test_message_from_db_msg_tool_result_invalid_field_types) {
    char kind[] = "tool_result";
    char content[] = "Result";
    char data_json[] = "{\"tool_call_id\":123,\"output\":\"result\"}";
    ik_msg_t db_msg = {
        .id = 12,
        .kind = kind,
        .content = content,
        .data_json = data_json
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_err(&res));
}

END_TEST

START_TEST(test_message_from_db_msg_unknown_kind) {
    char kind[] = "unknown_kind";
    char content[] = "Test";
    ik_msg_t db_msg = {
        .id = 13,
        .kind = kind,
        .content = content,
        .data_json = NULL
    };

    ik_message_t *msg = NULL;
    res_t res = ik_message_from_db_msg(test_ctx, &db_msg, &msg);

    ck_assert(is_err(&res));
}

END_TEST

static Suite *creation_suite(void)
{
    Suite *s = suite_create("Message Creation");

    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, 30);
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_message_create_text_user);
    tcase_add_test(tc, test_message_create_text_assistant);
    tcase_add_test(tc, test_message_create_tool_call);
    tcase_add_test(tc, test_message_create_tool_result);
    tcase_add_test(tc, test_message_from_db_msg_user);
    tcase_add_test(tc, test_message_from_db_msg_tool_call);
    tcase_add_test(tc, test_message_from_db_msg_tool_result);
    tcase_add_test(tc, test_message_from_db_msg_system);
    tcase_add_test(tc, test_message_from_db_msg_invalid_json);
    tcase_add_test(tc, test_message_from_db_msg_user_missing_content);
    tcase_add_test(tc, test_message_from_db_msg_assistant_missing_content);
    tcase_add_test(tc, test_message_from_db_msg_tool_call_missing_data_json);
    tcase_add_test(tc, test_message_from_db_msg_tool_call_invalid_field_types);
    tcase_add_test(tc, test_message_from_db_msg_tool_result_missing_data_json);
    tcase_add_test(tc, test_message_from_db_msg_tool_result_invalid_json);
    tcase_add_test(tc, test_message_from_db_msg_tool_result_invalid_field_types);
    tcase_add_test(tc, test_message_from_db_msg_unknown_kind);

    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = creation_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
