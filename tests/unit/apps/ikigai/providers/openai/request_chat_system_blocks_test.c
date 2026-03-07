#include "tests/test_constants.h"
/**
 * @file request_chat_system_blocks_test.c
 * @brief Unit tests for OpenAI Chat Completions system block serialization
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/openai/request.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/provider_types.h"
#include "vendor/yyjson/yyjson.h"

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

static ik_request_t *create_basic_request(TALLOC_CTX *ctx)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(req, "gpt-4o");
    req->max_output_tokens = 1024;

    req->message_count = 1;
    req->messages = talloc_array(req, ik_message_t, 1);
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks = talloc_array(req, ik_content_block_t, 1);
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello");

    return req;
}

/* Count messages with a specific role in the messages array */
static size_t count_messages_with_role(yyjson_val *messages, const char *role)
{
    size_t count = 0;
    size_t idx, max;
    yyjson_val *msg;
    yyjson_arr_foreach(messages, idx, max, msg) {
        yyjson_val *r = yyjson_obj_get(msg, "role");
        if (r && yyjson_is_str(r) && strcmp(yyjson_get_str(r), role) == 0) {
            count++;
        }
    }
    return count;
}

/* Get the Nth message with a given role */
static yyjson_val *get_message_with_role(yyjson_val *messages, const char *role, size_t n)
{
    size_t found = 0;
    size_t idx, max;
    yyjson_val *msg;
    yyjson_arr_foreach(messages, idx, max, msg) {
        yyjson_val *r = yyjson_obj_get(msg, "role");
        if (r && yyjson_is_str(r) && strcmp(yyjson_get_str(r), role) == 0) {
            if (found == n) return msg;
            found++;
        }
    }
    return NULL;
}

/* ================================================================
 * Backward compatibility: system_prompt used when no blocks
 * ================================================================ */

START_TEST(test_fallback_to_system_prompt_when_no_blocks) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = talloc_strdup(req, "Legacy prompt");
    req->system_block_count = 0;
    req->system_blocks = NULL;

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages);
    ck_assert(yyjson_is_arr(messages));

    /* Should have one system message + one user message */
    ck_assert_uint_eq(count_messages_with_role(messages, "system"), 1);
    yyjson_val *sys_msg = get_message_with_role(messages, "system", 0);
    ck_assert_ptr_nonnull(sys_msg);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(sys_msg, "content")), "Legacy prompt");

    /* System message must come before user message */
    yyjson_val *first = yyjson_arr_get_first(messages);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(first, "role")), "system");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Single system block serialized as system role message
 * ================================================================ */

START_TEST(test_single_system_block) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 1;
    req->system_blocks = talloc_array(req, ik_system_block_t, 1);
    req->system_blocks[0].text = talloc_strdup(req, "You are helpful.");
    req->system_blocks[0].cacheable = false;

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages);

    ck_assert_uint_eq(count_messages_with_role(messages, "system"), 1);
    yyjson_val *sys_msg = get_message_with_role(messages, "system", 0);
    ck_assert_ptr_nonnull(sys_msg);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(sys_msg, "content")), "You are helpful.");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Multiple system blocks each become a separate system message
 * ================================================================ */

START_TEST(test_multiple_system_blocks) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 3;
    req->system_blocks = talloc_array(req, ik_system_block_t, 3);
    req->system_blocks[0].text = talloc_strdup(req, "Block one.");
    req->system_blocks[0].cacheable = true;
    req->system_blocks[1].text = talloc_strdup(req, "Block two.");
    req->system_blocks[1].cacheable = false;
    req->system_blocks[2].text = talloc_strdup(req, "Block three.");
    req->system_blocks[2].cacheable = true;

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages);

    /* Three system messages in order */
    ck_assert_uint_eq(count_messages_with_role(messages, "system"), 3);

    yyjson_val *m0 = get_message_with_role(messages, "system", 0);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(m0, "content")), "Block one.");

    yyjson_val *m1 = get_message_with_role(messages, "system", 1);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(m1, "content")), "Block two.");

    yyjson_val *m2 = get_message_with_role(messages, "system", 2);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(m2, "content")), "Block three.");

    /* All system messages precede the user message */
    yyjson_val *first_non_system = NULL;
    size_t idx2, max2;
    yyjson_val *msg2;
    yyjson_arr_foreach(messages, idx2, max2, msg2) {
        yyjson_val *role_val = yyjson_obj_get(msg2, "role");
        if (role_val && strcmp(yyjson_get_str(role_val), "system") != 0) {
            first_non_system = msg2;
            break;
        }
    }
    ck_assert_ptr_nonnull(first_non_system);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(first_non_system, "role")), "user");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * system_blocks take priority over system_prompt when both set
 * ================================================================ */

START_TEST(test_system_blocks_override_system_prompt) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = talloc_strdup(req, "Legacy prompt");
    req->system_block_count = 1;
    req->system_blocks = talloc_array(req, ik_system_block_t, 1);
    req->system_blocks[0].text = talloc_strdup(req, "New block.");
    req->system_blocks[0].cacheable = false;

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages);

    /* Only one system message and it uses the block text, not legacy prompt */
    ck_assert_uint_eq(count_messages_with_role(messages, "system"), 1);
    yyjson_val *sys_msg = get_message_with_role(messages, "system", 0);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(sys_msg, "content")), "New block.");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * No system content: no system message added
 * ================================================================ */

START_TEST(test_no_system_content) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = NULL;
    req->system_block_count = 0;
    req->system_blocks = NULL;

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages);

    ck_assert_uint_eq(count_messages_with_role(messages, "system"), 0);

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_chat_system_blocks_suite(void)
{
    Suite *s = suite_create("OpenAI Chat System Blocks");

    TCase *tc = tcase_create("SystemBlocks");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_fallback_to_system_prompt_when_no_blocks);
    tcase_add_test(tc, test_single_system_block);
    tcase_add_test(tc, test_multiple_system_blocks);
    tcase_add_test(tc, test_system_blocks_override_system_prompt);
    tcase_add_test(tc, test_no_system_content);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = openai_chat_system_blocks_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/request_chat_system_blocks_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
