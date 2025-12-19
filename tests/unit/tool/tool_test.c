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

// Test: ik_tool_add_string_parameter adds parameter correctly
START_TEST(test_tool_add_string_param) {
    // Create yyjson document
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    // Create properties object
    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    ck_assert_ptr_nonnull(properties);

    // Call function under test
    ik_tool_add_string_parameter(doc, properties, "test_param", "Test description");

    // Verify parameter was added
    yyjson_mut_val *param = yyjson_mut_obj_get(properties, "test_param");
    ck_assert_ptr_nonnull(param);
    ck_assert(yyjson_mut_is_obj(param));

    // Verify type field
    yyjson_mut_val *type = yyjson_mut_obj_get(param, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert(yyjson_mut_is_str(type));
    ck_assert_str_eq(yyjson_mut_get_str(type), "string");

    // Verify description field
    yyjson_mut_val *description = yyjson_mut_obj_get(param, "description");
    ck_assert_ptr_nonnull(description);
    ck_assert(yyjson_mut_is_str(description));
    ck_assert_str_eq(yyjson_mut_get_str(description), "Test description");

    yyjson_mut_doc_free(doc);
}
END_TEST
// Test: ik_tool_build_glob_schema returns non-NULL and has correct structure
START_TEST(test_tool_build_glob_schema_structure)
{
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
START_TEST(test_tool_build_file_read_schema_structure)
{
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
START_TEST(test_tool_build_grep_schema_structure)
{
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
START_TEST(test_tool_build_file_write_schema_structure)
{
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
START_TEST(test_tool_build_bash_schema_structure)
{
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
START_TEST(test_tool_build_all)
{
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
// Test: ik_tool_call_create returns non-NULL struct
START_TEST(test_tool_call_create_returns_nonnull)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_abc123", "glob", "{\"pattern\": \"*.c\"}");

    ck_assert_ptr_nonnull(call);

    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST
// Test: ik_tool_call_create sets id correctly
START_TEST(test_tool_call_create_sets_id)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_abc123", "glob", "{\"pattern\": \"*.c\"}");

    ck_assert_ptr_nonnull(call);
    ck_assert_ptr_nonnull(call->id);
    ck_assert_str_eq(call->id, "call_abc123");

    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST
// Test: ik_tool_call_create sets name correctly
START_TEST(test_tool_call_create_sets_name)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_abc123", "glob", "{\"pattern\": \"*.c\"}");

    ck_assert_ptr_nonnull(call);
    ck_assert_ptr_nonnull(call->name);
    ck_assert_str_eq(call->name, "glob");

    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST
// Test: ik_tool_call_create sets arguments correctly
START_TEST(test_tool_call_create_sets_arguments)
{
    const char *args = "{\"pattern\": \"*.c\", \"path\": \"src/\"}";
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_abc123", "glob", args);

    ck_assert_ptr_nonnull(call);
    ck_assert_ptr_nonnull(call->arguments);
    ck_assert_str_eq(call->arguments, args);

    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST
// Test: ik_tool_call_create works with NULL parent context (talloc root)
START_TEST(test_tool_call_create_null_parent)
{
    ik_tool_call_t *call = ik_tool_call_create(NULL, "call_xyz", "file_read", "{\"path\": \"/tmp/test\"}");

    ck_assert_ptr_nonnull(call);
    ck_assert_str_eq(call->id, "call_xyz");
    ck_assert_str_eq(call->name, "file_read");
    ck_assert_str_eq(call->arguments, "{\"path\": \"/tmp/test\"}");

    talloc_free(call);
}

END_TEST START_TEST(test_tool_truncate_output_null)
{
    ck_assert_ptr_null(ik_tool_truncate_output(ctx, NULL, 1024));
    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST START_TEST(test_tool_truncate_output_empty)
{
    char *result = ik_tool_truncate_output(ctx, "", 1024);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "");
    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST START_TEST(test_tool_truncate_output_under_limit)
{
    char *result = ik_tool_truncate_output(ctx, "Hello, World!", 100);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "Hello, World!");
    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST START_TEST(test_tool_truncate_output_at_limit)
{
    char *result = ik_tool_truncate_output(ctx, "12345", 5);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "12345");
    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST START_TEST(test_tool_truncate_output_over_limit)
{
    const char *output = "This is a very long string that exceeds the limit";
    char *result = ik_tool_truncate_output(ctx, output, 10);
    ck_assert_ptr_nonnull(result);
    ck_assert(strncmp(result, "This is a ", 10) == 0);
    ck_assert(strstr(result, "[Output truncated:") != NULL);
    ck_assert(strstr(result, "showing first 10 of") != NULL);
    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST START_TEST(test_tool_truncate_output_zero_limit)
{
    char *result = ik_tool_truncate_output(ctx, "test", 0);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "[Output truncated:") != NULL);
    talloc_free(ctx);
    ctx = talloc_new(NULL);
}

END_TEST

// Test: ik_tool_param_def_t struct exists and is usable
START_TEST(test_tool_param_def_struct_exists)
{
    ik_tool_param_def_t param = {
        .name = "test_param",
        .description = "Test description",
        .required = true
    };
    ck_assert_str_eq(param.name, "test_param");
    ck_assert_str_eq(param.description, "Test description");
    ck_assert(param.required);
}
END_TEST

// Test: ik_tool_schema_def_t struct exists and is usable
START_TEST(test_tool_schema_def_struct_exists)
{
    static const ik_tool_param_def_t params[] = {
        {"pattern", "Pattern to match", true},
        {"path", "Base path", false}
    };

    ik_tool_schema_def_t schema = {
        .name = "test_tool",
        .description = "Test tool description",
        .params = params,
        .param_count = 2
    };

    ck_assert_str_eq(schema.name, "test_tool");
    ck_assert_str_eq(schema.description, "Test tool description");
    ck_assert_ptr_eq(schema.params, params);
    ck_assert_uint_eq(schema.param_count, 2);
}
END_TEST

// Test suite
static Suite *tool_suite(void)
{
    Suite *s = suite_create("Tool");

    TCase *tc_helper = tcase_create("Helper Functions");
    tcase_add_checked_fixture(tc_helper, setup, teardown);
    tcase_add_test(tc_helper, test_tool_add_string_param);
    suite_add_tcase(s, tc_helper);

    TCase *tc_glob = tcase_create("Glob Schema");
    tcase_add_checked_fixture(tc_glob, setup, teardown);
    tcase_add_test(tc_glob, test_tool_build_glob_schema_structure);
    suite_add_tcase(s, tc_glob);

    TCase *tc_file_read = tcase_create("File Read Schema");
    tcase_add_checked_fixture(tc_file_read, setup, teardown);
    tcase_add_test(tc_file_read, test_tool_build_file_read_schema_structure);
    suite_add_tcase(s, tc_file_read);

    TCase *tc_grep = tcase_create("Grep Schema");
    tcase_add_checked_fixture(tc_grep, setup, teardown);
    tcase_add_test(tc_grep, test_tool_build_grep_schema_structure);
    suite_add_tcase(s, tc_grep);

    TCase *tc_file_write = tcase_create("File Write Schema");
    tcase_add_checked_fixture(tc_file_write, setup, teardown);
    tcase_add_test(tc_file_write, test_tool_build_file_write_schema_structure);
    suite_add_tcase(s, tc_file_write);

    TCase *tc_bash = tcase_create("Bash Schema");
    tcase_add_checked_fixture(tc_bash, setup, teardown);
    tcase_add_test(tc_bash, test_tool_build_bash_schema_structure);
    suite_add_tcase(s, tc_bash);

    TCase *tc_all = tcase_create("Build All");
    tcase_add_checked_fixture(tc_all, setup, teardown);
    tcase_add_test(tc_all, test_tool_build_all);
    suite_add_tcase(s, tc_all);

    TCase *tc_call = tcase_create("Tool Call");
    tcase_add_checked_fixture(tc_call, setup, teardown);
    tcase_add_test(tc_call, test_tool_call_create_returns_nonnull);
    tcase_add_test(tc_call, test_tool_call_create_sets_id);
    tcase_add_test(tc_call, test_tool_call_create_sets_name);
    tcase_add_test(tc_call, test_tool_call_create_sets_arguments);
    tcase_add_test(tc_call, test_tool_call_create_null_parent);
    suite_add_tcase(s, tc_call);

    TCase *tc_truncate = tcase_create("Truncate Output");
    tcase_add_checked_fixture(tc_truncate, setup, teardown);
    tcase_add_test(tc_truncate, test_tool_truncate_output_null);
    tcase_add_test(tc_truncate, test_tool_truncate_output_empty);
    tcase_add_test(tc_truncate, test_tool_truncate_output_under_limit);
    tcase_add_test(tc_truncate, test_tool_truncate_output_at_limit);
    tcase_add_test(tc_truncate, test_tool_truncate_output_over_limit);
    tcase_add_test(tc_truncate, test_tool_truncate_output_zero_limit);
    suite_add_tcase(s, tc_truncate);

    TCase *tc_param_def = tcase_create("Parameter Definition");
    tcase_add_checked_fixture(tc_param_def, setup, teardown);
    tcase_add_test(tc_param_def, test_tool_param_def_struct_exists);
    suite_add_tcase(s, tc_param_def);

    TCase *tc_schema_def = tcase_create("Schema Definition");
    tcase_add_checked_fixture(tc_schema_def, setup, teardown);
    tcase_add_test(tc_schema_def, test_tool_schema_def_struct_exists);
    suite_add_tcase(s, tc_schema_def);

    return s;
}

int main(void)
{
    Suite *s = tool_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
