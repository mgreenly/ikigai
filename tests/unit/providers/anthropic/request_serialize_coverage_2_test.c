/**
 * @file request_serialize_coverage_test_2.c
 * @brief Coverage tests for Anthropic request serialization - Part 2: Messages & Roles
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/request_serialize.h"
#include "providers/provider.h"
#include "wrapper_json.h"

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
 * Mock Overrides
 * ================================================================ */

static bool mock_yyjson_mut_obj_fail = false;
static bool mock_yyjson_mut_obj_add_str_fail = false;
static bool mock_yyjson_mut_arr_fail = false;
static bool mock_yyjson_mut_arr_add_val_fail = false;
static bool mock_yyjson_mut_obj_add_val_fail = false;

yyjson_mut_val *yyjson_mut_obj_(yyjson_mut_doc *doc)
{
    if (mock_yyjson_mut_obj_fail) {
        return NULL;
    }
    return yyjson_mut_obj(doc);
}

bool yyjson_mut_obj_add_str_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                             const char *key, const char *val)
{
    if (mock_yyjson_mut_obj_add_str_fail) {
        return false;
    }
    return yyjson_mut_obj_add_str(doc, obj, key, val);
}

yyjson_mut_val *yyjson_mut_arr_(yyjson_mut_doc *doc)
{
    if (mock_yyjson_mut_arr_fail) {
        return NULL;
    }
    return yyjson_mut_arr(doc);
}

bool yyjson_mut_arr_add_val_(yyjson_mut_val *arr, yyjson_mut_val *val)
{
    if (mock_yyjson_mut_arr_add_val_fail) {
        return false;
    }
    return yyjson_mut_arr_add_val(arr, val);
}

bool yyjson_mut_obj_add_val_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                             const char *key, yyjson_mut_val *val)
{
    if (mock_yyjson_mut_obj_add_val_fail) {
        return false;
    }
    return yyjson_mut_obj_add_val(doc, obj, key, val);
}

/* ================================================================
 * Helper Functions
 * ================================================================ */

static void reset_mocks(void)
{
    mock_yyjson_mut_obj_fail = false;
    mock_yyjson_mut_obj_add_str_fail = false;
    mock_yyjson_mut_arr_fail = false;
    mock_yyjson_mut_arr_add_val_fail = false;
    mock_yyjson_mut_obj_add_val_fail = false;
}

/* ================================================================
 * Message Content Serialization - OOM Tests
 * ================================================================ */

START_TEST(test_serialize_message_content_single_text_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);

    ik_message_t message;
    message.content_count = 1;
    ik_content_block_t blocks[1];
    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = talloc_strdup(test_ctx, "Hello");
    message.content_blocks = blocks;

    mock_yyjson_mut_obj_add_str_fail = true;

    bool result = ik_anthropic_serialize_message_content(doc, msg_obj, &message);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_message_content_arr_alloc_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);

    ik_message_t message;
    message.content_count = 2;
    ik_content_block_t blocks[2];
    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = talloc_strdup(test_ctx, "Hello");
    blocks[1].type = IK_CONTENT_TEXT;
    blocks[1].data.text.text = talloc_strdup(test_ctx, "World");
    message.content_blocks = blocks;

    mock_yyjson_mut_arr_fail = true;

    bool result = ik_anthropic_serialize_message_content(doc, msg_obj, &message);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_message_content_add_val_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);

    ik_message_t message;
    message.content_count = 2;
    ik_content_block_t blocks[2];
    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = talloc_strdup(test_ctx, "Hello");
    blocks[1].type = IK_CONTENT_TEXT;
    blocks[1].data.text.text = talloc_strdup(test_ctx, "World");
    message.content_blocks = blocks;

    mock_yyjson_mut_obj_add_val_fail = true;

    bool result = ik_anthropic_serialize_message_content(doc, msg_obj, &message);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Role Mapping Tests
 * ================================================================ */

START_TEST(test_role_to_string_default) {
    const char *role = ik_anthropic_role_to_string((ik_role_t)999);

    ck_assert_str_eq(role, "user");
}
END_TEST

/* ================================================================
 * Message Serialization - OOM Tests
 * ================================================================ */

START_TEST(test_serialize_messages_arr_alloc_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);

    ik_request_t req;
    req.message_count = 1;
    ik_message_t messages[1];
    messages[0].role = IK_ROLE_USER;
    messages[0].content_count = 1;
    ik_content_block_t blocks[1];
    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = talloc_strdup(test_ctx, "Hello");
    messages[0].content_blocks = blocks;
    req.messages = messages;

    mock_yyjson_mut_arr_fail = true;

    bool result = ik_anthropic_serialize_messages(doc, root, &req);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_messages_msg_obj_alloc_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);

    ik_request_t req;
    req.message_count = 1;
    ik_message_t messages[1];
    messages[0].role = IK_ROLE_USER;
    messages[0].content_count = 1;
    ik_content_block_t blocks[1];
    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = talloc_strdup(test_ctx, "Hello");
    messages[0].content_blocks = blocks;
    req.messages = messages;

    mock_yyjson_mut_obj_fail = true;

    bool result = ik_anthropic_serialize_messages(doc, root, &req);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_messages_role_add_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);

    ik_request_t req;
    req.message_count = 1;
    ik_message_t messages[1];
    messages[0].role = IK_ROLE_USER;
    messages[0].content_count = 1;
    ik_content_block_t blocks[1];
    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = talloc_strdup(test_ctx, "Hello");
    messages[0].content_blocks = blocks;
    req.messages = messages;

    mock_yyjson_mut_obj_add_str_fail = true;

    bool result = ik_anthropic_serialize_messages(doc, root, &req);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_messages_arr_add_msg_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);

    ik_request_t req;
    req.message_count = 1;
    ik_message_t messages[1];
    messages[0].role = IK_ROLE_USER;
    messages[0].content_count = 1;
    ik_content_block_t blocks[1];
    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = talloc_strdup(test_ctx, "Hello");
    messages[0].content_blocks = blocks;
    req.messages = messages;

    mock_yyjson_mut_arr_add_val_fail = true;

    bool result = ik_anthropic_serialize_messages(doc, root, &req);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_messages_add_to_root_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);

    ik_request_t req;
    req.message_count = 1;
    ik_message_t messages[1];
    messages[0].role = IK_ROLE_USER;
    messages[0].content_count = 1;
    ik_content_block_t blocks[1];
    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = talloc_strdup(test_ctx, "Hello");
    messages[0].content_blocks = blocks;
    req.messages = messages;

    mock_yyjson_mut_obj_add_val_fail = true;

    bool result = ik_anthropic_serialize_messages(doc, root, &req);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *request_serialize_coverage_suite_2(void)
{
    Suite *s = suite_create("Anthropic Request Serialize Coverage - Part 2");

    TCase *tc_message = tcase_create("Message Content OOM");
    tcase_set_timeout(tc_message, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_message, setup, teardown);
    tcase_add_test(tc_message, test_serialize_message_content_single_text_fail);
    tcase_add_test(tc_message, test_serialize_message_content_arr_alloc_fail);
    tcase_add_test(tc_message, test_serialize_message_content_add_val_fail);
    suite_add_tcase(s, tc_message);

    TCase *tc_role = tcase_create("Role Mapping");
    tcase_set_timeout(tc_role, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_role, setup, teardown);
    tcase_add_test(tc_role, test_role_to_string_default);
    suite_add_tcase(s, tc_role);

    TCase *tc_serialize = tcase_create("Message Serialization OOM");
    tcase_set_timeout(tc_serialize, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_serialize, setup, teardown);
    tcase_add_test(tc_serialize, test_serialize_messages_arr_alloc_fail);
    tcase_add_test(tc_serialize, test_serialize_messages_msg_obj_alloc_fail);
    tcase_add_test(tc_serialize, test_serialize_messages_role_add_fail);
    tcase_add_test(tc_serialize, test_serialize_messages_arr_add_msg_fail);
    tcase_add_test(tc_serialize, test_serialize_messages_add_to_root_fail);
    suite_add_tcase(s, tc_serialize);

    return s;
}

int main(void)
{
    Suite *s = request_serialize_coverage_suite_2();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
