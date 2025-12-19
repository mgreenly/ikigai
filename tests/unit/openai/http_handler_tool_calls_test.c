#include <check.h>
#include <talloc.h>
#include <string.h>
#include <curl/curl.h>
#include "openai/client.h"
#include "config.h"
#include "tool.h"
#include "vendor/yyjson/yyjson.h"

/*
 * HTTP handler tool calls tests
 *
 * Tests tool call handling in http_handler.c including:
 * - Tool call extraction from SSE events (lines 162-176)
 * - Tool call accumulation across multiple chunks
 * - Tool call transfer to response structure (line 287)
 */

/* Forward declarations for curl wrapper functions */
CURL *curl_easy_init_(void);
void curl_easy_cleanup_(CURL *curl);
CURLcode curl_easy_perform_(CURL *curl);
const char *curl_easy_strerror_(CURLcode code);
struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string);
void curl_slist_free_all_(struct curl_slist *list);
CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val);

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;

/* Callback capture state */
typedef size_t (*curl_write_callback)(char *data, size_t size, size_t nmemb, void *userdata);
static curl_write_callback g_write_callback = NULL;
static void *g_write_data = NULL;

/* Mock response data */
static const char *mock_response_data = NULL;
static size_t mock_response_len = 0;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Reset callback capture state */
    g_write_callback = NULL;
    g_write_data = NULL;

    /* Reset mock response data */
    mock_response_data = NULL;
    mock_response_len = 0;
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
}

/* Mock libcurl functions */

CURL *curl_easy_init_(void)
{
    return curl_easy_init();
}

void curl_easy_cleanup_(CURL *curl)
{
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }
}

CURLcode curl_easy_perform_(CURL *curl)
{
    (void)curl;

    /* Invoke write callback with mock response */
    if (g_write_callback && mock_response_data) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        size_t result = g_write_callback(
            (char *)mock_response_data,
            1,
            mock_response_len,
            g_write_data
            );
#pragma GCC diagnostic pop
        if (result != mock_response_len) {
            return CURLE_WRITE_ERROR;
        }
    }

    return CURLE_OK;
}

const char *curl_easy_strerror_(CURLcode code)
{
    switch (code) {
        case CURLE_OK:
            return "No error";
        case CURLE_WRITE_ERROR:
            return "Write error";
        default:
            return "Unknown error";
    }
}

struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string)
{
    return curl_slist_append(list, string);
}

void curl_slist_free_all_(struct curl_slist *list)
{
    if (list != NULL) {
        curl_slist_free_all(list);
    }
}

CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    if (opt == CURLOPT_WRITEFUNCTION) {
        g_write_callback = (curl_write_callback)val;
    } else if (opt == CURLOPT_WRITEDATA) {
        g_write_data = (void *)val;
    }
#pragma GCC diagnostic pop

    return curl_easy_setopt(curl, opt, val);
}

/*
 * Test: Tool call in single chunk
 *
 * Tests lines 162-176: Tool call extraction and first chunk handling
 * Tests line 287: Tool call transfer to response structure
 * Tests canonical message conversion in ik_openai_chat_create
 */
START_TEST(test_tool_call_single_chunk) {
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(conv, "user", "Test");
    ck_assert(!msg_res.is_err);
    res_t add_res = ik_openai_conversation_add_msg(conv, msg_res.ok);
    ck_assert(!add_res.is_err);

    /* Set up mock response with tool call */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_abc123\",\"type\":\"function\",\"function\":{\"name\":\"glob\",\"arguments\":\"{\\\"pattern\\\": \\\"*.c\\\"}\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Execute request */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);

    /* Should succeed */
    ck_assert(!result.is_err);
    ik_msg_t *msg = result.ok;

    /* Should be canonical tool_call message */
    ck_assert_ptr_nonnull(msg->kind);
    ck_assert_str_eq(msg->kind, "tool_call");

    /* Should have human-readable content */
    ck_assert_ptr_nonnull(msg->content);

    /* Should have data_json with tool call details */
    ck_assert_ptr_nonnull(msg->data_json);

    /* Parse data_json to verify tool call data */
    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    const char *id = yyjson_get_str(yyjson_obj_get(root, "id"));
    ck_assert_ptr_nonnull(id);
    ck_assert_str_eq(id, "call_abc123");

    yyjson_val *func = yyjson_obj_get(root, "function");
    ck_assert_ptr_nonnull(func);

    const char *name = yyjson_get_str(yyjson_obj_get(func, "name"));
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(name, "glob");

    const char *arguments = yyjson_get_str(yyjson_obj_get(func, "arguments"));
    ck_assert_ptr_nonnull(arguments);
    ck_assert_str_eq(arguments, "{\"pattern\": \"*.c\"}");

    yyjson_doc_free(doc);
}
END_TEST
/*
 * Test: Tool call with streaming (multiple chunks)
 *
 * Tests lines 169-174: Tool call accumulation across multiple chunks
 * This tests the case where ctx->tool_call is already set and needs accumulation
 */
START_TEST(test_tool_call_streaming_multiple_chunks)
{
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(conv, "user", "Test");
    ck_assert(!msg_res.is_err);
    res_t add_res = ik_openai_conversation_add_msg(conv, msg_res.ok);
    ck_assert(!add_res.is_err);

    /* Set up mock response with tool call split across multiple chunks
     * First chunk contains id, name, and partial arguments
     * Second chunk contains additional arguments (streaming case)
     */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_xyz789\",\"type\":\"function\",\"function\":{\"name\":\"file_read\",\"arguments\":\"{\\\"pa\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"th\\\": \\\"tes\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"t.txt\\\"}\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Execute request */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);

    /* Should succeed */
    ck_assert(!result.is_err);
    ik_msg_t *msg = result.ok;

    /* Should be canonical tool_call message */
    ck_assert_ptr_nonnull(msg->kind);
    ck_assert_str_eq(msg->kind, "tool_call");

    /* Should have human-readable content */
    ck_assert_ptr_nonnull(msg->content);

    /* Should have data_json with tool call details */
    ck_assert_ptr_nonnull(msg->data_json);

    /* Parse data_json to verify tool call data with accumulated arguments */
    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    const char *id = yyjson_get_str(yyjson_obj_get(root, "id"));
    ck_assert_ptr_nonnull(id);
    ck_assert_str_eq(id, "call_xyz789");

    yyjson_val *func = yyjson_obj_get(root, "function");
    ck_assert_ptr_nonnull(func);

    const char *name = yyjson_get_str(yyjson_obj_get(func, "name"));
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(name, "file_read");

    const char *arguments = yyjson_get_str(yyjson_obj_get(func, "arguments"));
    ck_assert_ptr_nonnull(arguments);
    ck_assert_str_eq(arguments, "{\"path\": \"test.txt\"}");

    yyjson_doc_free(doc);
}

END_TEST
/*
 * Test: Tool call with no content
 *
 * Ensures that when there's no text content, only tool call data is present
 */
START_TEST(test_tool_call_no_content)
{
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(conv, "user", "Test");
    ck_assert(!msg_res.is_err);
    res_t add_res = ik_openai_conversation_add_msg(conv, msg_res.ok);
    ck_assert(!add_res.is_err);

    /* Set up mock response with tool call but no content */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_grep\",\"type\":\"function\",\"function\":{\"name\":\"grep\",\"arguments\":\"{\\\"pattern\\\": \\\"TODO\\\"}\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Execute request */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);

    /* Should succeed */
    ck_assert(!result.is_err);
    ik_msg_t *msg = result.ok;

    /* Should be canonical tool_call message */
    ck_assert_ptr_nonnull(msg->kind);
    ck_assert_str_eq(msg->kind, "tool_call");

    /* Should have human-readable content (generated summary) */
    ck_assert_ptr_nonnull(msg->content);

    /* Should have data_json with tool call details */
    ck_assert_ptr_nonnull(msg->data_json);

    /* Parse data_json to verify tool call data */
    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    const char *id = yyjson_get_str(yyjson_obj_get(root, "id"));
    ck_assert_ptr_nonnull(id);
    ck_assert_str_eq(id, "call_grep");

    yyjson_val *func = yyjson_obj_get(root, "function");
    ck_assert_ptr_nonnull(func);

    const char *name = yyjson_get_str(yyjson_obj_get(func, "name"));
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(name, "grep");

    const char *arguments = yyjson_get_str(yyjson_obj_get(func, "arguments"));
    ck_assert_ptr_nonnull(arguments);
    ck_assert_str_eq(arguments, "{\"pattern\": \"TODO\"}");

    yyjson_doc_free(doc);
}

END_TEST
/*
 * Test: Parse tool calls returns OK with NULL
 *
 * Tests line 163 branch 3: is_ok(&tool_res) is true but tool_res.ok is NULL
 * This occurs when an SSE event parses successfully but contains no tool call data
 * (e.g., a delta event with no tool_calls field)
 */
START_TEST(test_parse_tool_calls_ok_null)
{
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(conv, "user", "Test");
    ck_assert(!msg_res.is_err);
    res_t add_res = ik_openai_conversation_add_msg(conv, msg_res.ok);
    ck_assert(!add_res.is_err);

    /* Set up mock response with delta events that have no tool_calls
     * This triggers the OK(NULL) return path in ik_openai_parse_tool_calls
     */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Execute request */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);

    /* Should succeed */
    ck_assert(!result.is_err);
    ik_msg_t *msg = result.ok;

    /* Should be a regular assistant message (not a tool_call) */
    ck_assert_ptr_nonnull(msg->kind);
    ck_assert_str_eq(msg->kind, "assistant");

    /* Should have content */
    ck_assert_ptr_nonnull(msg->content);
    ck_assert_str_eq(msg->content, "Hello");
}

END_TEST
/*
 * Test: Parse tool calls returns error
 *
 * Tests line 163 branch 1: is_ok(&tool_res) is false
 * This occurs when ik_openai_parse_tool_calls returns an error (e.g., invalid JSON)
 * The http_write_callback should continue processing other events despite parse errors
 */
START_TEST(test_parse_tool_calls_error)
{
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-3.5-turbo");

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(conv, "user", "Test");
    ck_assert(!msg_res.is_err);
    res_t add_res = ik_openai_conversation_add_msg(conv, msg_res.ok);
    ck_assert(!add_res.is_err);

    /* Set up mock response with malformed JSON in the middle
     * This triggers the ERR return path in ik_openai_parse_tool_calls
     * The callback should continue processing and return the content from valid events
     */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n"
        "data: {INVALID JSON}\n\n"
        "data: {\"choices\":[{\"delta\":{\"content\":\" World\"}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n";
    mock_response_data = response;
    mock_response_len = strlen(response);

    /* Execute request */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);

    /* Should succeed despite the malformed JSON event */
    ck_assert(!result.is_err);
    ik_msg_t *msg = result.ok;

    /* Should be a regular assistant message */
    ck_assert_ptr_nonnull(msg->kind);
    ck_assert_str_eq(msg->kind, "assistant");

    /* Should have content from the valid events */
    ck_assert_ptr_nonnull(msg->content);
    ck_assert_str_eq(msg->content, "Hello World");
}

END_TEST

/*
 * Test suite
 */
static Suite *http_handler_tool_calls_suite(void)
{
    Suite *s = suite_create("HTTP Handler Tool Calls");

    TCase *tc_tool_calls = tcase_create("Tool Call Handling");
    tcase_add_checked_fixture(tc_tool_calls, setup, teardown);
    tcase_add_test(tc_tool_calls, test_tool_call_single_chunk);
    tcase_add_test(tc_tool_calls, test_tool_call_streaming_multiple_chunks);
    tcase_add_test(tc_tool_calls, test_tool_call_no_content);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_ok_null);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_error);
    suite_add_tcase(s, tc_tool_calls);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = http_handler_tool_calls_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
