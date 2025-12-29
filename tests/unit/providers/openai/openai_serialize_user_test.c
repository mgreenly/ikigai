/**
 * @file openai_serialize_user_test.c
 * @brief Unit tests for OpenAI user message serialization
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/serialize.h"
#include "providers/provider.h"
#include "vendor/yyjson/yyjson.h"
#include "openai_serialize_user_test.h"

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
 * User Message Tests
 * ================================================================ */

START_TEST(test_serialize_user_message_single_text) {
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "Hello world");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);
    ck_assert(yyjson_mut_is_obj(val));

    yyjson_mut_val *role = yyjson_mut_obj_get(val, "role");
    ck_assert_ptr_nonnull(role);
    ck_assert_str_eq(yyjson_mut_get_str(role), "user");

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert_str_eq(yyjson_mut_get_str(content), "Hello world");

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_user_message_multiple_text_blocks)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 3;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 3);

    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "First");

    msg->content_blocks[1].type = IK_CONTENT_TEXT;
    msg->content_blocks[1].data.text.text = talloc_strdup(msg, "Second");

    msg->content_blocks[2].type = IK_CONTENT_TEXT;
    msg->content_blocks[2].data.text.text = talloc_strdup(msg, "Third");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert_str_eq(yyjson_mut_get_str(content), "First\n\nSecond\n\nThird");

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_user_message_empty_content)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 0;
    msg->content_blocks = NULL;

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert_str_eq(yyjson_mut_get_str(content), "");

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_user_message_text_and_thinking)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 3;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 3);

    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "First");

    msg->content_blocks[1].type = IK_CONTENT_THINKING;
    msg->content_blocks[1].data.thinking.text = talloc_strdup(msg, "Think");

    msg->content_blocks[2].type = IK_CONTENT_TEXT;
    msg->content_blocks[2].data.text.text = talloc_strdup(msg, "Second");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert_str_eq(yyjson_mut_get_str(content), "First\n\nSecond");

    yyjson_mut_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

Suite *openai_serialize_user_suite(void)
{
    Suite *s = suite_create("OpenAI Serialize User Messages");

    TCase *tc_user = tcase_create("User Messages");
    tcase_set_timeout(tc_user, 30);
    tcase_add_unchecked_fixture(tc_user, setup, teardown);
    tcase_add_test(tc_user, test_serialize_user_message_single_text);
    tcase_add_test(tc_user, test_serialize_user_message_multiple_text_blocks);
    tcase_add_test(tc_user, test_serialize_user_message_empty_content);
    tcase_add_test(tc_user, test_serialize_user_message_text_and_thinking);
    suite_add_tcase(s, tc_user);

    return s;
}