#include "../../test_constants.h"
#include "../../../src/db/message.h"
#include "../../../src/db/replay.h"
#include "../../../src/msg.h"
#include <check.h>
#include <talloc.h>
#include <string.h>
#include "vendor/yyjson/yyjson.h"

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
}

/*
 * Basic creation tests
 */

START_TEST(test_tool_result_message_create_returns_nonnull) {
    ik_msg_t *msg = ik_msg_create_tool_result(
        ctx,
        "call_abc123",
        "glob",
        "src/main.c\nsrc/config.c\nsrc/repl.c",
        true,
        "3 files found"
        );
    ck_assert_ptr_nonnull(msg);
}
END_TEST

START_TEST(test_tool_result_message_kind_is_tool_result) {
    ik_msg_t *msg = ik_msg_create_tool_result(
        ctx,
        "call_abc123",
        "glob",
        "src/main.c\nsrc/config.c\nsrc/repl.c",
        true,
        "3 files found"
        );
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->kind, "tool_result");
}

END_TEST

START_TEST(test_tool_result_message_content_is_summary) {
    ik_msg_t *msg = ik_msg_create_tool_result(
        ctx,
        "call_abc123",
        "glob",
        "src/main.c\nsrc/config.c\nsrc/repl.c",
        true,
        "3 files found"
        );
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->content, "3 files found");
}

END_TEST

START_TEST(test_tool_result_message_data_json_contains_tool_call_id) {
    ik_msg_t *msg = ik_msg_create_tool_result(
        ctx,
        "call_abc123",
        "glob",
        "src/main.c\nsrc/config.c\nsrc/repl.c",
        true,
        "3 files found"
        );
    ck_assert_ptr_nonnull(msg);
    ck_assert_ptr_nonnull(msg->data_json);
    ck_assert_ptr_nonnull(strstr(msg->data_json, "call_abc123"));
}

END_TEST

START_TEST(test_tool_result_message_data_json_contains_name) {
    ik_msg_t *msg = ik_msg_create_tool_result(
        ctx,
        "call_abc123",
        "glob",
        "src/main.c\nsrc/config.c\nsrc/repl.c",
        true,
        "3 files found"
        );
    ck_assert_ptr_nonnull(msg);
    ck_assert_ptr_nonnull(msg->data_json);
    ck_assert_ptr_nonnull(strstr(msg->data_json, "glob"));
}

END_TEST

START_TEST(test_tool_result_message_data_json_contains_output) {
    ik_msg_t *msg = ik_msg_create_tool_result(
        ctx,
        "call_abc123",
        "glob",
        "src/main.c\nsrc/config.c\nsrc/repl.c",
        true,
        "3 files found"
        );
    ck_assert_ptr_nonnull(msg);
    ck_assert_ptr_nonnull(msg->data_json);
    ck_assert_ptr_nonnull(strstr(msg->data_json, "src/main.c"));
}

END_TEST

START_TEST(test_tool_result_message_data_json_contains_success) {
    ik_msg_t *msg = ik_msg_create_tool_result(
        ctx,
        "call_abc123",
        "glob",
        "src/main.c\nsrc/config.c\nsrc/repl.c",
        true,
        "3 files found"
        );
    ck_assert_ptr_nonnull(msg);
    ck_assert_ptr_nonnull(msg->data_json);
    // Check for "success":true (allowing for variations in JSON formatting)
    ck_assert_ptr_nonnull(strstr(msg->data_json, "success"));
    ck_assert_ptr_nonnull(strstr(msg->data_json, "true"));
}

END_TEST
/*
 * Talloc hierarchy tests
 */

START_TEST(test_tool_result_message_talloc_hierarchy) {
    ik_msg_t *msg = ik_msg_create_tool_result(
        ctx,
        "call_abc123",
        "glob",
        "src/main.c\nsrc/config.c\nsrc/repl.c",
        true,
        "3 files found"
        );
    ck_assert_ptr_nonnull(msg);

    /* Message should be child of ctx */
    ck_assert_ptr_eq(talloc_parent(msg), ctx);

    /* Strings should be children of message */
    ck_assert_ptr_eq(talloc_parent(msg->kind), msg);
    ck_assert_ptr_eq(talloc_parent(msg->content), msg);
    ck_assert_ptr_eq(talloc_parent(msg->data_json), msg);
}

END_TEST
/*
 * Different content strings
 */

START_TEST(test_tool_result_message_with_different_summary) {
    ik_msg_t *msg = ik_msg_create_tool_result(
        ctx,
        "call_xyz789",
        "file_read",
        "file contents here",
        true,
        "File read successfully"
        );
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->content, "File read successfully");
    ck_assert_str_eq(msg->kind, "tool_result");
}

END_TEST

START_TEST(test_tool_result_message_success_false) {
    ik_msg_t *msg = ik_msg_create_tool_result(
        ctx,
        "call_error123",
        "bash",
        "Permission denied",
        false,
        "Command failed"
        );
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->content, "Command failed");
    ck_assert_ptr_nonnull(msg->data_json);
    ck_assert_ptr_nonnull(strstr(msg->data_json, "false"));
}

END_TEST

/*
 * Test suite
 */

static Suite *tool_result_message_suite(void)
{
    Suite *s = suite_create("Tool Result Message Creation");

    TCase *tc_basic = tcase_create("Basic Creation");
    tcase_set_timeout(tc_basic, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_tool_result_message_create_returns_nonnull);
    tcase_add_test(tc_basic, test_tool_result_message_kind_is_tool_result);
    tcase_add_test(tc_basic, test_tool_result_message_content_is_summary);
    suite_add_tcase(s, tc_basic);

    TCase *tc_data_json = tcase_create("Data JSON Content");
    tcase_set_timeout(tc_data_json, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_data_json, setup, teardown);
    tcase_add_test(tc_data_json, test_tool_result_message_data_json_contains_tool_call_id);
    tcase_add_test(tc_data_json, test_tool_result_message_data_json_contains_name);
    tcase_add_test(tc_data_json, test_tool_result_message_data_json_contains_output);
    tcase_add_test(tc_data_json, test_tool_result_message_data_json_contains_success);
    suite_add_tcase(s, tc_data_json);

    TCase *tc_hierarchy = tcase_create("Talloc Hierarchy");
    tcase_set_timeout(tc_hierarchy, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_hierarchy, setup, teardown);
    tcase_add_test(tc_hierarchy, test_tool_result_message_talloc_hierarchy);
    suite_add_tcase(s, tc_hierarchy);

    TCase *tc_variants = tcase_create("Variant Inputs");
    tcase_set_timeout(tc_variants, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_variants, setup, teardown);
    tcase_add_test(tc_variants, test_tool_result_message_with_different_summary);
    tcase_add_test(tc_variants, test_tool_result_message_success_false);
    suite_add_tcase(s, tc_variants);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = tool_result_message_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
