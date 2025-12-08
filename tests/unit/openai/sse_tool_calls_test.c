#include <check.h>
#include <talloc.h>
#include <string.h>
#include "openai/sse_parser.h"
#include "tool.h"
#include "error.h"

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
 * Tool calls parsing tests
 */

START_TEST(test_parse_tool_calls_returns_non_null_result) {
    /* SSE event with tool_calls in delta */
    const char *event =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_abc123\",\"type\":\"function\",\"function\":{\"name\":\"glob\",\"arguments\":\"{\\\"pattern\\\": \\\"*.c\\\", \\\"path\\\": \\\"src/\\\"}\"}}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_nonnull(res.ok);
}
END_TEST START_TEST(test_parse_tool_calls_extracts_id_correctly)
{
    const char *event =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_abc123\",\"type\":\"function\",\"function\":{\"name\":\"glob\",\"arguments\":\"{\\\"pattern\\\": \\\"*.c\\\"}\"}}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_nonnull(res.ok);

    ik_tool_call_t *tool_call = (ik_tool_call_t *)res.ok;
    ck_assert_str_eq(tool_call->id, "call_abc123");
}

END_TEST START_TEST(test_parse_tool_calls_extracts_function_name_correctly)
{
    const char *event =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_xyz789\",\"type\":\"function\",\"function\":{\"name\":\"file_read\",\"arguments\":\"{\\\"path\\\": \\\"test.txt\\\"}\"}}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_nonnull(res.ok);

    ik_tool_call_t *tool_call = (ik_tool_call_t *)res.ok;
    ck_assert_str_eq(tool_call->name, "file_read");
}

END_TEST START_TEST(test_parse_tool_calls_extracts_arguments_correctly)
{
    const char *event =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_123\",\"type\":\"function\",\"function\":{\"name\":\"grep\",\"arguments\":\"{\\\"pattern\\\": \\\"TODO\\\"}\"}}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_nonnull(res.ok);

    ik_tool_call_t *tool_call = (ik_tool_call_t *)res.ok;
    ck_assert_str_eq(tool_call->arguments, "{\"pattern\": \"TODO\"}");
}

END_TEST START_TEST(test_parse_tool_calls_returns_null_for_content_only)
{
    /* Delta with only content field, no tool_calls */
    const char *event = "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_handles_finish_reason_tool_calls)
{
    /* Event with finish_reason: "tool_calls" */
    const char *event = "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    /* finish_reason without tool_calls data returns NULL */
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_missing_data_prefix)
{
    const char *event = "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"id\":\"call_123\"}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(res.is_err);
    ck_assert(res.err->code == ERR_PARSE);
}

END_TEST START_TEST(test_parse_tool_calls_malformed_json)
{
    const char *event = "data: {\"malformed\"";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(res.is_err);
    ck_assert(res.err->code == ERR_PARSE);
}

END_TEST START_TEST(test_parse_tool_calls_done_marker)
{
    const char *event = "data: [DONE]";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_missing_choices)
{
    const char *event = "data: {\"other\":\"field\"}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_empty_choices)
{
    const char *event = "data: {\"choices\":[]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_missing_delta)
{
    const char *event = "data: {\"choices\":[{\"index\":0}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_empty_tool_calls_array)
{
    const char *event = "data: {\"choices\":[{\"delta\":{\"tool_calls\":[]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_missing_id)
{
    /* tool_call missing id field */
    const char *event =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"type\":\"function\",\"function\":{\"name\":\"glob\"}}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_missing_function)
{
    /* tool_call missing function object */
    const char *event =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_123\",\"type\":\"function\"}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_missing_function_name)
{
    /* function object missing name field */
    const char *event =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_123\",\"type\":\"function\",\"function\":{\"arguments\":\"{}\"}}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_missing_function_arguments)
{
    /* function object missing arguments field */
    const char *event =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_123\",\"type\":\"function\",\"function\":{\"name\":\"glob\"}}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_json_root_not_object)
{
    const char *event = "data: [\"not\", \"an\", \"object\"]";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(res.is_err);
    ck_assert(res.err->code == ERR_PARSE);
}

END_TEST START_TEST(test_parse_tool_calls_choice0_not_object)
{
    /* choices[0] is a string instead of object */
    const char *event = "data: {\"choices\":[\"not_an_object\"]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_tool_call_not_object)
{
    /* tool_calls[0] is a string instead of object */
    const char *event = "data: {\"choices\":[{\"delta\":{\"tool_calls\":[\"not_an_object\"]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_choices_not_array)
{
    /* choices exists but is not an array */
    const char *event = "data: {\"choices\":\"not_an_array\"}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_delta_null)
{
    /* delta is explicitly null */
    const char *event = "data: {\"choices\":[{\"delta\":null}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_tool_calls_not_array)
{
    /* tool_calls exists but is not an array */
    const char *event = "data: {\"choices\":[{\"delta\":{\"tool_calls\":\"not_an_array\"}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_tool_call_null)
{
    /* tool_calls[0] is explicitly null */
    const char *event = "data: {\"choices\":[{\"delta\":{\"tool_calls\":[null]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_id_not_string)
{
    /* id exists but is not a string */
    const char *event = "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"id\":123}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_function_not_object)
{
    /* function exists but is not an object */
    const char *event =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"id\":\"call_123\",\"function\":\"not_an_object\"}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_function_name_not_string)
{
    /* function.name exists but is not a string */
    const char *event =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"id\":\"call_123\",\"function\":{\"name\":123}}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_arguments_not_string)
{
    /* function.arguments exists but is not a string */
    const char *event =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"id\":\"call_123\",\"function\":{\"name\":\"glob\",\"arguments\":123}}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_tool_calls_streaming_chunk_without_id_and_name)
{
    /* Subsequent streaming chunk: has arguments but no id or name */
    const char *event =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"more args\"}}]}}]}";
    res_t res = ik_openai_parse_tool_calls(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_nonnull(res.ok);

    ik_tool_call_t *tool_call = (ik_tool_call_t *)res.ok;
    ck_assert_str_eq(tool_call->id, "");
    ck_assert_str_eq(tool_call->name, "");
    ck_assert_str_eq(tool_call->arguments, "more args");
}

END_TEST

/*
 * Test suite
 */

static Suite *openai_tool_calls_suite(void)
{
    Suite *s = suite_create("OpenAI Tool Calls");

    TCase *tc_tool_calls = tcase_create("Tool Calls Parsing");
    tcase_add_checked_fixture(tc_tool_calls, setup, teardown);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_returns_non_null_result);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_extracts_id_correctly);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_extracts_function_name_correctly);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_extracts_arguments_correctly);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_returns_null_for_content_only);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_handles_finish_reason_tool_calls);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_missing_data_prefix);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_malformed_json);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_done_marker);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_missing_choices);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_empty_choices);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_missing_delta);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_empty_tool_calls_array);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_missing_id);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_missing_function);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_missing_function_name);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_missing_function_arguments);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_json_root_not_object);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_choice0_not_object);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_tool_call_not_object);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_choices_not_array);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_delta_null);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_tool_calls_not_array);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_tool_call_null);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_id_not_string);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_function_not_object);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_function_name_not_string);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_arguments_not_string);
    tcase_add_test(tc_tool_calls, test_parse_tool_calls_streaming_chunk_without_id_and_name);
    suite_add_tcase(s, tc_tool_calls);

    return s;
}

int main(void)
{
    Suite *s = openai_tool_calls_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
