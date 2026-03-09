#include "tests/test_constants.h"
/**
 * @file anthropic_request_tool_cache_test.c
 * @brief Unit tests for tool cache_control in Anthropic request serialization
 *
 * Verifies that the last tool definition receives cache_control: {"type": "ephemeral"}
 * and that non-last tools do not.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/anthropic/request.h"
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
    req->model = talloc_strdup(req, "claude-3-5-sonnet-20241022");
    req->max_output_tokens = 1024;
    req->thinking.level = IK_THINKING_MIN;

    req->message_count = 1;
    req->messages = talloc_array(req, ik_message_t, 1);
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks = talloc_array(req, ik_content_block_t, 1);
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello");

    return req;
}

static void add_tool(ik_request_t *req, const char *name, const char *description)
{
    size_t idx = req->tool_count;
    req->tool_count++;
    req->tools = talloc_realloc(req, req->tools, ik_tool_def_t, (unsigned)req->tool_count);
    req->tools[idx].name = talloc_strdup(req, name);
    req->tools[idx].description = talloc_strdup(req, description);
    req->tools[idx].parameters = talloc_strdup(req, "{\"type\":\"object\",\"properties\":{}}");
    req->tools[idx].strict = false;
}

/* ================================================================
 * Tool Cache Control Tests
 * ================================================================ */

START_TEST(test_zero_tools_no_tools_array) {
    ik_request_t *req = create_basic_request(test_ctx);
    /* no tools added */

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_null(tools);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_single_tool_has_cache_control) {
    ik_request_t *req = create_basic_request(test_ctx);
    add_tool(req, "my_tool", "A test tool");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert_uint_eq(yyjson_arr_size(tools), 1);

    yyjson_val *tool = yyjson_arr_get_first(tools);
    ck_assert_ptr_nonnull(tool);

    yyjson_val *cc = yyjson_obj_get(tool, "cache_control");
    ck_assert_ptr_nonnull(cc);

    yyjson_val *cc_type = yyjson_obj_get(cc, "type");
    ck_assert_ptr_nonnull(cc_type);
    ck_assert_str_eq(yyjson_get_str(cc_type), "ephemeral");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_multiple_tools_only_last_has_cache_control) {
    ik_request_t *req = create_basic_request(test_ctx);
    add_tool(req, "tool_a", "First tool");
    add_tool(req, "tool_b", "Second tool");
    add_tool(req, "tool_c", "Third tool");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert_uint_eq(yyjson_arr_size(tools), 3);

    /* First tool: no cache_control */
    yyjson_val *tool0 = yyjson_arr_get(tools, 0);
    ck_assert_ptr_nonnull(tool0);
    ck_assert_ptr_null(yyjson_obj_get(tool0, "cache_control"));

    /* Second tool: no cache_control */
    yyjson_val *tool1 = yyjson_arr_get(tools, 1);
    ck_assert_ptr_nonnull(tool1);
    ck_assert_ptr_null(yyjson_obj_get(tool1, "cache_control"));

    /* Last tool: has cache_control */
    yyjson_val *tool2 = yyjson_arr_get(tools, 2);
    ck_assert_ptr_nonnull(tool2);
    yyjson_val *cc = yyjson_obj_get(tool2, "cache_control");
    ck_assert_ptr_nonnull(cc);

    yyjson_val *cc_type = yyjson_obj_get(cc, "type");
    ck_assert_ptr_nonnull(cc_type);
    ck_assert_str_eq(yyjson_get_str(cc_type), "ephemeral");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_request_tool_cache_suite(void)
{
    Suite *s = suite_create("Anthropic Request - Tool Cache Control");

    TCase *tc = tcase_create("Tool Cache");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_zero_tools_no_tools_array);
    tcase_add_test(tc, test_single_tool_has_cache_control);
    tcase_add_test(tc, test_multiple_tools_only_last_has_cache_control);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = anthropic_request_tool_cache_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/anthropic/anthropic_request_tool_cache_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
