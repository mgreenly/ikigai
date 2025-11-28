#include <check.h>
#include <talloc.h>
#include <string.h>
#include "openai/client.h"
#include "config.h"
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
 * Message creation tests
 */

START_TEST(test_message_create_valid) {
    res_t res = ik_openai_msg_create(ctx, "user", "Hello, world!");
    ck_assert(!res.is_err);

    ik_openai_msg_t *msg = res.ok;
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->role, "user");
    ck_assert_str_eq(msg->content, "Hello, world!");
}
END_TEST START_TEST(test_message_talloc_hierarchy)
{
    res_t res = ik_openai_msg_create(ctx, "assistant", "Hi there!");
    ck_assert(!res.is_err);

    ik_openai_msg_t *msg = res.ok;

    /* Message should be child of ctx */
    ck_assert_ptr_eq(talloc_parent(msg), ctx);

    /* Role and content should be children of message */
    ck_assert_ptr_eq(talloc_parent(msg->role), msg);
    ck_assert_ptr_eq(talloc_parent(msg->content), msg);
}

END_TEST
/*
 * Conversation tests
 */

START_TEST(test_conversation_create_empty)
{
    res_t res = ik_openai_conversation_create(ctx);
    ck_assert(!res.is_err);

    ik_openai_conversation_t *conv = res.ok;
    ck_assert_ptr_nonnull(conv);
    ck_assert_ptr_null(conv->messages);
    ck_assert_uint_eq(conv->message_count, 0);
}

END_TEST START_TEST(test_conversation_add_single_message)
{
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Test message");
    ck_assert(!msg_res.is_err);
    ik_openai_msg_t *msg = msg_res.ok;

    res_t add_res = ik_openai_conversation_add_msg(conv, msg);
    ck_assert(!add_res.is_err);

    ck_assert_uint_eq(conv->message_count, 1);
    ck_assert_ptr_nonnull(conv->messages);
    ck_assert_ptr_eq(conv->messages[0], msg);

    /* Message should now be child of conversation */
    ck_assert_ptr_eq(talloc_parent(msg), conv);
}

END_TEST START_TEST(test_conversation_add_multiple_messages)
{
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    /* Add user message */
    res_t msg1_res = ik_openai_msg_create(ctx, "user", "Question");
    ck_assert(!msg1_res.is_err);
    res_t add1_res = ik_openai_conversation_add_msg(conv, msg1_res.ok);
    ck_assert(!add1_res.is_err);

    /* Add assistant message */
    res_t msg2_res = ik_openai_msg_create(ctx, "assistant", "Answer");
    ck_assert(!msg2_res.is_err);
    res_t add2_res = ik_openai_conversation_add_msg(conv, msg2_res.ok);
    ck_assert(!add2_res.is_err);

    /* Add another user message */
    res_t msg3_res = ik_openai_msg_create(ctx, "user", "Follow-up");
    ck_assert(!msg3_res.is_err);
    res_t add3_res = ik_openai_conversation_add_msg(conv, msg3_res.ok);
    ck_assert(!add3_res.is_err);

    ck_assert_uint_eq(conv->message_count, 3);
    ck_assert_str_eq(conv->messages[0]->role, "user");
    ck_assert_str_eq(conv->messages[0]->content, "Question");
    ck_assert_str_eq(conv->messages[1]->role, "assistant");
    ck_assert_str_eq(conv->messages[1]->content, "Answer");
    ck_assert_str_eq(conv->messages[2]->role, "user");
    ck_assert_str_eq(conv->messages[2]->content, "Follow-up");
}

END_TEST
/*
 * Request creation tests
 */

START_TEST(test_request_create_valid)
{
    /* Create test config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 2048;

    /* Create conversation */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    /* Create request */
    ik_openai_request_t *req = ik_openai_request_create(ctx, cfg, conv);
    ck_assert_ptr_nonnull(req);
    ck_assert_str_eq(req->model, "gpt-4-turbo");
    ck_assert(req->temperature == 0.7);
    ck_assert_int_eq(req->max_completion_tokens, 2048);
    ck_assert(req->stream == true);
    ck_assert_ptr_eq(req->conv, conv);
}

END_TEST
/*
 * Response creation tests
 */

START_TEST(test_response_create_valid)
{
    ik_openai_response_t *resp = ik_openai_response_create(ctx);
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_null(resp->content);
    ck_assert_ptr_null(resp->finish_reason);
    ck_assert_int_eq(resp->prompt_tokens, 0);
    ck_assert_int_eq(resp->completion_tokens, 0);
    ck_assert_int_eq(resp->total_tokens, 0);
}

END_TEST
/*
 * JSON serialization tests
 */

START_TEST(test_serialize_empty_conversation)
{
    /* Create test config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 2048;

    /* Create empty conversation */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    /* Create request */
    ik_openai_request_t *req = ik_openai_request_create(ctx, cfg, conv);
    ck_assert_ptr_nonnull(req);

    /* Serialize */
    char *json = ik_openai_serialize_request(ctx, req);
    ck_assert_ptr_nonnull(json);

    /* Parse JSON to verify structure */
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_is_obj(root));

    /* Check fields */
    yyjson_val *model = yyjson_obj_get(root, "model");
    ck_assert_str_eq(yyjson_get_str(model), "gpt-4-turbo");

    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert(yyjson_is_arr(messages));
    ck_assert_uint_eq(yyjson_arr_size(messages), 0);

    yyjson_val *temp = yyjson_obj_get(root, "temperature");
    ck_assert(yyjson_get_real(temp) == 0.7);

    yyjson_val *max_tokens = yyjson_obj_get(root, "max_completion_tokens");
    ck_assert_int_eq(yyjson_get_int(max_tokens), 2048);

    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert(yyjson_get_bool(stream) == true);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_with_messages)
{
    /* Create test config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");
    cfg->openai_temperature = 0.5;
    cfg->openai_max_completion_tokens = 1024;

    /* Create conversation with messages */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg1_res = ik_openai_msg_create(ctx, "user", "Hello!");
    ck_assert(!msg1_res.is_err);
    ik_openai_conversation_add_msg(conv, msg1_res.ok);

    res_t msg2_res = ik_openai_msg_create(ctx, "assistant", "Hi there!");
    ck_assert(!msg2_res.is_err);
    ik_openai_conversation_add_msg(conv, msg2_res.ok);

    /* Create request */
    ik_openai_request_t *req = ik_openai_request_create(ctx, cfg, conv);
    ck_assert_ptr_nonnull(req);

    /* Serialize */
    char *json = ik_openai_serialize_request(ctx, req);
    ck_assert_ptr_nonnull(json);

    /* Parse JSON to verify structure */
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert(yyjson_is_arr(messages));
    ck_assert_uint_eq(yyjson_arr_size(messages), 2);

    /* Check first message */
    yyjson_val *msg1 = yyjson_arr_get(messages, 0);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg1, "role")), "user");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg1, "content")), "Hello!");

    /* Check second message */
    yyjson_val *msg2 = yyjson_arr_get(messages, 1);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg2, "role")), "assistant");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(msg2, "content")), "Hi there!");

    yyjson_doc_free(doc);
}

END_TEST
/*
 * Wrapper function tests
 */

START_TEST(test_yyjson_doc_get_root_wrapper_null)
{
    /* Test NULL doc returns NULL */
    yyjson_val *root = yyjson_doc_get_root_wrapper(NULL);
    ck_assert_ptr_null(root);
}

END_TEST START_TEST(test_yyjson_doc_get_root_wrapper_valid)
{
    /* Test valid doc returns root */
    const char *json = "{\"test\": \"value\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root_wrapper(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_yyjson_arr_get_wrapper_null)
{
    /* Test NULL array returns NULL */
    yyjson_val *elem = yyjson_arr_get_wrapper(NULL, 0);
    ck_assert_ptr_null(elem);
}

END_TEST START_TEST(test_yyjson_arr_get_wrapper_valid)
{
    /* Test valid array returns element */
    const char *json = "[1, 2, 3]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *elem = yyjson_arr_get_wrapper(root, 1);
    ck_assert_ptr_nonnull(elem);
    ck_assert_int_eq(yyjson_get_int(elem), 2);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_yyjson_is_obj_wrapper_null)
{
    /* Test NULL val returns false */
    bool is_obj = yyjson_is_obj_wrapper(NULL);
    ck_assert(is_obj == false);
}

END_TEST START_TEST(test_yyjson_is_obj_wrapper_valid_obj)
{
    /* Test object returns true */
    const char *json = "{\"test\": \"value\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_obj = yyjson_is_obj_wrapper(root);
    ck_assert(is_obj == true);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_yyjson_is_obj_wrapper_not_obj)
{
    /* Test non-object returns false */
    const char *json = "[1, 2, 3]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    bool is_obj = yyjson_is_obj_wrapper(root);
    ck_assert(is_obj == false);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_get_message_at_index_valid)
{
    /* Test valid array access */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Test");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    ik_openai_msg_t *msg = get_message_at_index(conv->messages, 0);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->content, "Test");
}

END_TEST

/*
 * Test suite
 */

static Suite *openai_structures_suite(void)
{
    Suite *s = suite_create("OpenAI Structures");

    TCase *tc_message = tcase_create("Message");
    tcase_add_checked_fixture(tc_message, setup, teardown);
    tcase_add_test(tc_message, test_message_create_valid);
    tcase_add_test(tc_message, test_message_talloc_hierarchy);
    suite_add_tcase(s, tc_message);

    TCase *tc_conversation = tcase_create("Conversation");
    tcase_add_checked_fixture(tc_conversation, setup, teardown);
    tcase_add_test(tc_conversation, test_conversation_create_empty);
    tcase_add_test(tc_conversation, test_conversation_add_single_message);
    tcase_add_test(tc_conversation, test_conversation_add_multiple_messages);
    suite_add_tcase(s, tc_conversation);

    TCase *tc_request = tcase_create("Request");
    tcase_add_checked_fixture(tc_request, setup, teardown);
    tcase_add_test(tc_request, test_request_create_valid);
    suite_add_tcase(s, tc_request);

    TCase *tc_response = tcase_create("Response");
    tcase_add_checked_fixture(tc_response, setup, teardown);
    tcase_add_test(tc_response, test_response_create_valid);
    suite_add_tcase(s, tc_response);

    TCase *tc_json = tcase_create("JSON Serialization");
    tcase_add_checked_fixture(tc_json, setup, teardown);
    tcase_add_test(tc_json, test_serialize_empty_conversation);
    tcase_add_test(tc_json, test_serialize_with_messages);
    suite_add_tcase(s, tc_json);

    TCase *tc_wrappers = tcase_create("Wrapper Functions");
    tcase_add_checked_fixture(tc_wrappers, setup, teardown);
    tcase_add_test(tc_wrappers, test_yyjson_doc_get_root_wrapper_null);
    tcase_add_test(tc_wrappers, test_yyjson_doc_get_root_wrapper_valid);
    tcase_add_test(tc_wrappers, test_yyjson_arr_get_wrapper_null);
    tcase_add_test(tc_wrappers, test_yyjson_arr_get_wrapper_valid);
    tcase_add_test(tc_wrappers, test_yyjson_is_obj_wrapper_null);
    tcase_add_test(tc_wrappers, test_yyjson_is_obj_wrapper_valid_obj);
    tcase_add_test(tc_wrappers, test_yyjson_is_obj_wrapper_not_obj);
    tcase_add_test(tc_wrappers, test_get_message_at_index_valid);
    suite_add_tcase(s, tc_wrappers);

    return s;
}

int main(void)
{
    Suite *s = openai_structures_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
