#include <check.h>
#include <talloc.h>
#include <string.h>
#include "openai/client.h"
#include "openai/tool_choice.h"
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
    ik_msg_t *msg = ik_openai_msg_create(ctx, "user", "Hello, world!");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->kind, "user");
    ck_assert_str_eq(msg->content, "Hello, world!");
}
END_TEST START_TEST(test_message_talloc_hierarchy)
{
    ik_msg_t *msg = ik_openai_msg_create(ctx, "assistant", "Hi there!");

    /* Message should be child of ctx */
    ck_assert_ptr_eq(talloc_parent(msg), ctx);

    /* Role and content should be children of message */
    ck_assert_ptr_eq(talloc_parent(msg->kind), msg);
    ck_assert_ptr_eq(talloc_parent(msg->content), msg);
}

END_TEST
/*
 * Conversation tests
 */

START_TEST(test_conversation_create_empty)
{
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);
    ck_assert_ptr_nonnull(conv);
    ck_assert_ptr_null(conv->messages);
    ck_assert_uint_eq(conv->message_count, 0);
}

END_TEST START_TEST(test_conversation_add_single_message)
{
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg = ik_openai_msg_create(ctx, "user", "Test message");

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
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    /* Add user message */
    ik_msg_t *msg1 = ik_openai_msg_create(ctx, "user", "Question");
    res_t add1_res = ik_openai_conversation_add_msg(conv, msg1);
    ck_assert(!add1_res.is_err);

    /* Add assistant message */
    ik_msg_t *msg2 = ik_openai_msg_create(ctx, "assistant", "Answer");
    res_t add2_res = ik_openai_conversation_add_msg(conv, msg2);
    ck_assert(!add2_res.is_err);

    /* Add another user message */
    ik_msg_t *msg3 = ik_openai_msg_create(ctx, "user", "Follow-up");
    res_t add3_res = ik_openai_conversation_add_msg(conv, msg3);
    ck_assert(!add3_res.is_err);

    ck_assert_uint_eq(conv->message_count, 3);
    ck_assert_str_eq(conv->messages[0]->kind, "user");
    ck_assert_str_eq(conv->messages[0]->content, "Question");
    ck_assert_str_eq(conv->messages[1]->kind, "assistant");
    ck_assert_str_eq(conv->messages[1]->content, "Answer");
    ck_assert_str_eq(conv->messages[2]->kind, "user");
    ck_assert_str_eq(conv->messages[2]->content, "Follow-up");
}

END_TEST
/*
 * Request creation tests
 */

START_TEST(test_request_create_valid)
{
    /* Create test config */
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 2048;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

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

END_TEST START_TEST(test_get_message_at_index_valid)
{
    /* Test valid array access */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_created = ik_openai_msg_create(ctx, "user", "Test");
    ik_openai_conversation_add_msg(conv, msg_created);

    ik_msg_t *msg = ik_openai_get_message_at_index(conv->messages, 0);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg->content, "Test");
}

END_TEST START_TEST(test_serialize_with_tools_and_tool_choice)
{
    /* Create test config */
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4o-mini");
    cfg->openai_temperature = 1.0;
    cfg->openai_max_completion_tokens = 4096;

    /* Create conversation with one message */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg1 = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg1);

    /* Create request */
    ik_openai_request_t *req = ik_openai_request_create(ctx, cfg, conv);
    ck_assert_ptr_nonnull(req);

    /* Serialize with tool_choice auto */
    ik_tool_choice_t choice_auto = ik_tool_choice_auto();
    char *json = ik_openai_serialize_request(ctx, req, choice_auto);
    ck_assert_ptr_nonnull(json);

    /* Parse JSON to verify structure */
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_is_obj(root));

    /* Verify tools array exists and is an array */
    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_is_arr(tools));

    /* Verify tools array has 5 elements */
    ck_assert_uint_eq(yyjson_arr_size(tools), 5);

    /* Verify tool_choice field exists with value "auto" */
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    ck_assert_str_eq(yyjson_get_str(tool_choice), "auto");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_with_tool_choice_none)
{
    /* Create test config */
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4o-mini");
    cfg->openai_temperature = 1.0;
    cfg->openai_max_completion_tokens = 4096;

    /* Create conversation with one message */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg2 = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg2);

    /* Create request */
    ik_openai_request_t *req = ik_openai_request_create(ctx, cfg, conv);
    ck_assert_ptr_nonnull(req);

    /* Serialize with tool_choice none */
    ik_tool_choice_t choice_none = ik_tool_choice_none();
    char *json = ik_openai_serialize_request(ctx, req, choice_none);
    ck_assert_ptr_nonnull(json);

    /* Parse JSON to verify structure */
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_is_obj(root));

    /* Verify tool_choice field exists with value "none" */
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    ck_assert_str_eq(yyjson_get_str(tool_choice), "none");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_with_tool_choice_required)
{
    /* Create test config */
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4o-mini");
    cfg->openai_temperature = 1.0;
    cfg->openai_max_completion_tokens = 4096;

    /* Create conversation with one message */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg3 = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg3);

    /* Create request */
    ik_openai_request_t *req = ik_openai_request_create(ctx, cfg, conv);
    ck_assert_ptr_nonnull(req);

    /* Serialize with tool_choice required */
    ik_tool_choice_t choice_required = ik_tool_choice_required();
    char *json = ik_openai_serialize_request(ctx, req, choice_required);
    ck_assert_ptr_nonnull(json);

    /* Parse JSON to verify structure */
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_is_obj(root));

    /* Verify tool_choice field exists with value "required" */
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    ck_assert_str_eq(yyjson_get_str(tool_choice), "required");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_with_tool_choice_specific)
{
    /* Create test config */
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4o-mini");
    cfg->openai_temperature = 1.0;
    cfg->openai_max_completion_tokens = 4096;

    /* Create conversation with one message */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg4 = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg4);

    /* Create request */
    ik_openai_request_t *req = ik_openai_request_create(ctx, cfg, conv);
    ck_assert_ptr_nonnull(req);

    /* Serialize with tool_choice specific "glob" */
    ik_tool_choice_t choice_specific = ik_tool_choice_specific(ctx, "glob");
    char *json = ik_openai_serialize_request(ctx, req, choice_specific);
    ck_assert_ptr_nonnull(json);

    /* Parse JSON to verify structure */
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_is_obj(root));

    /* Verify tool_choice field exists and is an object */
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    ck_assert(yyjson_is_obj(tool_choice));

    /* Verify tool_choice has "type": "function" */
    yyjson_val *type = yyjson_obj_get(tool_choice, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "function");

    /* Verify tool_choice has "function" object */
    yyjson_val *function = yyjson_obj_get(tool_choice, "function");
    ck_assert_ptr_nonnull(function);
    ck_assert(yyjson_is_obj(function));

    /* Verify function has "name": "glob" */
    yyjson_val *name = yyjson_obj_get(function, "name");
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(yyjson_get_str(name), "glob");

    yyjson_doc_free(doc);
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
    tcase_add_test(tc_json, test_serialize_with_tools_and_tool_choice);
    tcase_add_test(tc_json, test_serialize_with_tool_choice_none);
    tcase_add_test(tc_json, test_serialize_with_tool_choice_required);
    tcase_add_test(tc_json, test_serialize_with_tool_choice_specific);
    suite_add_tcase(s, tc_json);

    TCase *tc_wrappers = tcase_create("Wrapper Functions");
    tcase_add_checked_fixture(tc_wrappers, setup, teardown);
    tcase_add_test(tc_wrappers, test_yyjson_doc_get_root_wrapper_null);
    tcase_add_test(tc_wrappers, test_yyjson_doc_get_root_wrapper_valid);
    tcase_add_test(tc_wrappers, test_yyjson_arr_get_wrapper_null);
    tcase_add_test(tc_wrappers, test_yyjson_arr_get_wrapper_valid);
    tcase_add_test(tc_wrappers, test_yyjson_is_obj_wrapper_null);
    tcase_add_test(tc_wrappers, test_yyjson_is_obj_wrapper_valid_obj);
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
