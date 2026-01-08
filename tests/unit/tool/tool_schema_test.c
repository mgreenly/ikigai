#include <check.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/tool.h"

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

// Helper: Verify basic schema structure
static void verify_schema_basics(yyjson_mut_val *schema, const char *expected_name)
{
    ck_assert_ptr_nonnull(schema);

    yyjson_mut_val *type = yyjson_mut_obj_get(schema, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_mut_get_str(type), "function");

    yyjson_mut_val *function = yyjson_mut_obj_get(schema, "function");
    ck_assert_ptr_nonnull(function);

    yyjson_mut_val *name = yyjson_mut_obj_get(function, "name");
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(yyjson_mut_get_str(name), expected_name);

    yyjson_mut_val *description = yyjson_mut_obj_get(function, "description");
    ck_assert_ptr_nonnull(description);
}

// Helper: Get parameters object from schema
static yyjson_mut_val *get_parameters(yyjson_mut_val *schema)
{
    yyjson_mut_val *function = yyjson_mut_obj_get(schema, "function");
    ck_assert_ptr_nonnull(function);

    yyjson_mut_val *parameters = yyjson_mut_obj_get(function, "parameters");
    ck_assert_ptr_nonnull(parameters);

    yyjson_mut_val *params_type = yyjson_mut_obj_get(parameters, "type");
    ck_assert_ptr_nonnull(params_type);
    ck_assert_str_eq(yyjson_mut_get_str(params_type), "object");

    return parameters;
}

// Helper: Verify string parameter exists
static void verify_string_param(yyjson_mut_val *properties, const char *param_name)
{
    yyjson_mut_val *param = yyjson_mut_obj_get(properties, param_name);
    ck_assert_ptr_nonnull(param);

    yyjson_mut_val *type = yyjson_mut_obj_get(param, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_mut_get_str(type), "string");

    yyjson_mut_val *description = yyjson_mut_obj_get(param, "description");
    ck_assert_ptr_nonnull(description);
}

// Helper: Verify required array
static void verify_required(yyjson_mut_val *parameters, const char *required_params[], size_t count)
{
    yyjson_mut_val *required = yyjson_mut_obj_get(parameters, "required");
    ck_assert_ptr_nonnull(required);
    ck_assert(yyjson_mut_is_arr(required));
    ck_assert_uint_eq(yyjson_mut_arr_size(required), count);

    for (size_t i = 0; i < count; i++) {
        yyjson_mut_val *item = yyjson_mut_arr_get(required, i);
        ck_assert_ptr_nonnull(item);
        ck_assert_str_eq(yyjson_mut_get_str(item), required_params[i]);
    }
}

// Test: ik_tool_build_glob_schema returns non-NULL and has correct structure
START_TEST(test_tool_build_glob_schema_structure) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    yyjson_mut_val *schema = ik_tool_build_glob_schema(doc);
    verify_schema_basics(schema, "glob");

    yyjson_mut_val *parameters = get_parameters(schema);
    yyjson_mut_val *properties = yyjson_mut_obj_get(parameters, "properties");
    ck_assert_ptr_nonnull(properties);

    verify_string_param(properties, "pattern");
    verify_string_param(properties, "path");

    const char *required_params[] = {"pattern"};
    verify_required(parameters, required_params, 1);

    yyjson_mut_doc_free(doc);
}
END_TEST
// Test: ik_tool_build_file_read_schema returns non-NULL and has correct structure
START_TEST(test_tool_build_file_read_schema_structure) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    yyjson_mut_val *schema = ik_tool_build_file_read_schema(doc);
    verify_schema_basics(schema, "file_read");

    yyjson_mut_val *parameters = get_parameters(schema);
    yyjson_mut_val *properties = yyjson_mut_obj_get(parameters, "properties");
    ck_assert_ptr_nonnull(properties);

    verify_string_param(properties, "path");

    const char *required_params[] = {"path"};
    verify_required(parameters, required_params, 1);

    yyjson_mut_doc_free(doc);
}

END_TEST
// Test: ik_tool_build_grep_schema returns non-NULL and has correct structure
START_TEST(test_tool_build_grep_schema_structure) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    yyjson_mut_val *schema = ik_tool_build_grep_schema(doc);
    verify_schema_basics(schema, "grep");

    yyjson_mut_val *parameters = get_parameters(schema);
    yyjson_mut_val *properties = yyjson_mut_obj_get(parameters, "properties");
    ck_assert_ptr_nonnull(properties);

    verify_string_param(properties, "pattern");
    verify_string_param(properties, "path");
    verify_string_param(properties, "glob");

    const char *required_params[] = {"pattern"};
    verify_required(parameters, required_params, 1);

    yyjson_mut_doc_free(doc);
}

END_TEST
// Test: ik_tool_build_file_write_schema returns non-NULL and has correct structure
START_TEST(test_tool_build_file_write_schema_structure) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    yyjson_mut_val *schema = ik_tool_build_file_write_schema(doc);
    verify_schema_basics(schema, "file_write");

    yyjson_mut_val *parameters = get_parameters(schema);
    yyjson_mut_val *properties = yyjson_mut_obj_get(parameters, "properties");
    ck_assert_ptr_nonnull(properties);

    verify_string_param(properties, "path");
    verify_string_param(properties, "content");

    const char *required_params[] = {"path", "content"};
    verify_required(parameters, required_params, 2);

    yyjson_mut_doc_free(doc);
}

END_TEST
// Test: ik_tool_build_bash_schema returns non-NULL and has correct structure
START_TEST(test_tool_build_bash_schema_structure) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    yyjson_mut_val *schema = ik_tool_build_bash_schema(doc);
    verify_schema_basics(schema, "bash");

    yyjson_mut_val *parameters = get_parameters(schema);
    yyjson_mut_val *properties = yyjson_mut_obj_get(parameters, "properties");
    ck_assert_ptr_nonnull(properties);

    verify_string_param(properties, "command");

    const char *required_params[] = {"command"};
    verify_required(parameters, required_params, 1);

    yyjson_mut_doc_free(doc);
}

END_TEST
// Test: ik_tool_build_all returns array with all 5 tools
START_TEST(test_tool_build_all) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    yyjson_mut_val *arr = ik_tool_build_all(doc);

    // Verify returns non-NULL
    ck_assert_ptr_nonnull(arr);

    // Verify is array
    ck_assert(yyjson_mut_is_arr(arr));

    // Verify has exactly 5 elements
    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 5);

    // Verify first element has "function.name": "glob"
    yyjson_mut_val *first = yyjson_mut_arr_get(arr, 0);
    ck_assert_ptr_nonnull(first);
    yyjson_mut_val *function = yyjson_mut_obj_get(first, "function");
    ck_assert_ptr_nonnull(function);
    yyjson_mut_val *name = yyjson_mut_obj_get(function, "name");
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(yyjson_mut_get_str(name), "glob");

    // Verify second element has "function.name": "file_read"
    yyjson_mut_val *second = yyjson_mut_arr_get(arr, 1);
    ck_assert_ptr_nonnull(second);
    function = yyjson_mut_obj_get(second, "function");
    ck_assert_ptr_nonnull(function);
    name = yyjson_mut_obj_get(function, "name");
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(yyjson_mut_get_str(name), "file_read");

    // Verify third element has "function.name": "grep"
    yyjson_mut_val *third = yyjson_mut_arr_get(arr, 2);
    ck_assert_ptr_nonnull(third);
    function = yyjson_mut_obj_get(third, "function");
    ck_assert_ptr_nonnull(function);
    name = yyjson_mut_obj_get(function, "name");
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(yyjson_mut_get_str(name), "grep");

    // Verify fourth element has "function.name": "file_write"
    yyjson_mut_val *fourth = yyjson_mut_arr_get(arr, 3);
    ck_assert_ptr_nonnull(fourth);
    function = yyjson_mut_obj_get(fourth, "function");
    ck_assert_ptr_nonnull(function);
    name = yyjson_mut_obj_get(function, "name");
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(yyjson_mut_get_str(name), "file_write");

    // Verify fifth element has "function.name": "bash"
    yyjson_mut_val *fifth = yyjson_mut_arr_get(arr, 4);
    ck_assert_ptr_nonnull(fifth);
    function = yyjson_mut_obj_get(fifth, "function");
    ck_assert_ptr_nonnull(function);
    name = yyjson_mut_obj_get(function, "name");
    ck_assert_ptr_nonnull(name);
    ck_assert_str_eq(yyjson_mut_get_str(name), "bash");

    yyjson_mut_doc_free(doc);
}

END_TEST

// Test suite
static Suite *tool_schema_suite(void)
{
    Suite *s = suite_create("Tool Schema");

    TCase *tc_glob = tcase_create("Glob Schema");
    tcase_set_timeout(tc_glob, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_glob, setup, teardown);
    tcase_add_test(tc_glob, test_tool_build_glob_schema_structure);
    suite_add_tcase(s, tc_glob);

    TCase *tc_file_read = tcase_create("File Read Schema");
    tcase_set_timeout(tc_file_read, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_file_read, setup, teardown);
    tcase_add_test(tc_file_read, test_tool_build_file_read_schema_structure);
    suite_add_tcase(s, tc_file_read);

    TCase *tc_grep = tcase_create("Grep Schema");
    tcase_set_timeout(tc_grep, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_grep, setup, teardown);
    tcase_add_test(tc_grep, test_tool_build_grep_schema_structure);
    suite_add_tcase(s, tc_grep);

    TCase *tc_file_write = tcase_create("File Write Schema");
    tcase_set_timeout(tc_file_write, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_file_write, setup, teardown);
    tcase_add_test(tc_file_write, test_tool_build_file_write_schema_structure);
    suite_add_tcase(s, tc_file_write);

    TCase *tc_bash = tcase_create("Bash Schema");
    tcase_set_timeout(tc_bash, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_bash, setup, teardown);
    tcase_add_test(tc_bash, test_tool_build_bash_schema_structure);
    suite_add_tcase(s, tc_bash);

    TCase *tc_all = tcase_create("Build All");
    tcase_set_timeout(tc_all, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_all, setup, teardown);
    tcase_add_test(tc_all, test_tool_build_all);
    suite_add_tcase(s, tc_all);

    return s;
}

int main(void)
{
    Suite *s = tool_schema_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
