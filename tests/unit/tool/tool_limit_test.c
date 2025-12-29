#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/error.h"
#include "../../../src/tool.h"
#include "../../test_utils.h"
#include "vendor/yyjson/yyjson.h"

// Test fixtures
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

// Test: Add limit metadata to tool result JSON
START_TEST(test_add_limit_metadata_basic) {
    // Create a simple tool result JSON
    const char *result_json = "{\"output\": \"file.c\", \"count\": 1}";
    int32_t max_tool_turns = 3;

    // Call function to add limit metadata
    char *result = ik_tool_result_add_limit_metadata(ctx, result_json, max_tool_turns);
    ck_assert_ptr_nonnull(result);

    // Parse result and verify metadata was added
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_is_obj(root));

    // Verify original fields preserved
    yyjson_val *output = yyjson_obj_get(root, "output");
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(yyjson_get_str(output), "file.c");

    yyjson_val *count = yyjson_obj_get(root, "count");
    ck_assert_ptr_nonnull(count);
    ck_assert_int_eq(yyjson_get_int(count), 1);

    // Verify limit_reached field
    yyjson_val *limit_reached = yyjson_obj_get(root, "limit_reached");
    ck_assert_ptr_nonnull(limit_reached);
    ck_assert(yyjson_get_bool(limit_reached) == true);

    // Verify limit_message field
    yyjson_val *limit_message = yyjson_obj_get(root, "limit_message");
    ck_assert_ptr_nonnull(limit_message);
    const char *msg = yyjson_get_str(limit_message);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Tool call limit reached (3). Stopping tool loop.");

    yyjson_doc_free(doc);
}
END_TEST
// Test: Handle NULL input
START_TEST(test_add_limit_metadata_null_input)
{
    int32_t max_tool_turns = 3;

    char *result = ik_tool_result_add_limit_metadata(ctx, NULL, max_tool_turns);
    ck_assert_ptr_null(result);
}

END_TEST
// Test: Handle malformed JSON input
START_TEST(test_add_limit_metadata_malformed_json)
{
    const char *bad_json = "{invalid json}";
    int32_t max_tool_turns = 3;

    char *result = ik_tool_result_add_limit_metadata(ctx, bad_json, max_tool_turns);
    ck_assert_ptr_null(result);
}

END_TEST
// Test: Handle empty JSON string
START_TEST(test_add_limit_metadata_empty_json)
{
    const char *empty_json = "";
    int32_t max_tool_turns = 3;

    char *result = ik_tool_result_add_limit_metadata(ctx, empty_json, max_tool_turns);
    ck_assert_ptr_null(result);
}

END_TEST
// Test: Handle JSON array (not object)
START_TEST(test_add_limit_metadata_json_array)
{
    const char *array_json = "[1, 2, 3]";
    int32_t max_tool_turns = 3;

    char *result = ik_tool_result_add_limit_metadata(ctx, array_json, max_tool_turns);
    ck_assert_ptr_null(result);
}

END_TEST
// Test: Different max_tool_turns value
START_TEST(test_add_limit_metadata_different_limit)
{
    const char *result_json = "{\"output\": \"test\"}";
    int32_t max_tool_turns = 5;

    char *result = ik_tool_result_add_limit_metadata(ctx, result_json, max_tool_turns);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *limit_message = yyjson_obj_get(root, "limit_message");
    ck_assert_ptr_nonnull(limit_message);
    ck_assert_str_eq(yyjson_get_str(limit_message), "Tool call limit reached (5). Stopping tool loop.");

    yyjson_doc_free(doc);
}

END_TEST
// Test: Complex JSON with nested structures
START_TEST(test_add_limit_metadata_complex_json)
{
    const char *result_json = "{\"output\": \"file1.c\\nfile2.c\", \"count\": 2, \"nested\": {\"key\": \"value\"}}";
    int32_t max_tool_turns = 3;

    char *result = ik_tool_result_add_limit_metadata(ctx, result_json, max_tool_turns);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify all original fields preserved
    yyjson_val *output = yyjson_obj_get(root, "output");
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(yyjson_get_str(output), "file1.c\nfile2.c");

    yyjson_val *count = yyjson_obj_get(root, "count");
    ck_assert_ptr_nonnull(count);
    ck_assert_int_eq(yyjson_get_int(count), 2);

    yyjson_val *nested = yyjson_obj_get(root, "nested");
    ck_assert_ptr_nonnull(nested);
    ck_assert(yyjson_is_obj(nested));

    // Verify limit metadata added
    yyjson_val *limit_reached = yyjson_obj_get(root, "limit_reached");
    ck_assert_ptr_nonnull(limit_reached);
    ck_assert(yyjson_get_bool(limit_reached) == true);

    yyjson_val *limit_message = yyjson_obj_get(root, "limit_message");
    ck_assert_ptr_nonnull(limit_message);
    ck_assert_str_eq(yyjson_get_str(limit_message), "Tool call limit reached (3). Stopping tool loop.");

    yyjson_doc_free(doc);
}

END_TEST

/*
 * Test suite
 */

static Suite *tool_limit_suite(void)
{
    Suite *s = suite_create("Tool Limit Metadata");

    TCase *tc_basic = tcase_create("Basic Tests");
    tcase_set_timeout(tc_basic, 30);
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_add_limit_metadata_basic);
    tcase_add_test(tc_basic, test_add_limit_metadata_null_input);
    tcase_add_test(tc_basic, test_add_limit_metadata_malformed_json);
    tcase_add_test(tc_basic, test_add_limit_metadata_empty_json);
    tcase_add_test(tc_basic, test_add_limit_metadata_json_array);
    tcase_add_test(tc_basic, test_add_limit_metadata_different_limit);
    tcase_add_test(tc_basic, test_add_limit_metadata_complex_json);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = tool_limit_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
