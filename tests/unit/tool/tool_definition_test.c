#include "../../test_constants.h"
#include <check.h>
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

// Test: ik_tool_param_def_t struct exists and is usable
START_TEST(test_tool_param_def_struct_exists) {
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
START_TEST(test_tool_schema_def_struct_exists) {
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
// Test: ik_tool_build_schema_from_def basic functionality
START_TEST(test_tool_build_schema_from_def_basic) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    static const ik_tool_param_def_t params[] = {
        {"pattern", "Glob pattern", true},
        {"path", "Base directory", false}
    };

    ik_tool_schema_def_t def = {
        .name = "test_glob",
        .description = "Test glob tool",
        .params = params,
        .param_count = 2
    };

    yyjson_mut_val *schema = ik_tool_build_schema_from_def(doc, &def);
    verify_schema_basics(schema, "test_glob");

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
// Test: All schema definitions produce correct output (completeness check)
START_TEST(test_schema_definitions_complete) {
    // Verify all 5 tool schemas can be built and have expected tool names
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    const char *expected_names[] = {"glob", "file_read", "grep", "file_write", "bash"};
    yyjson_mut_val *arr = ik_tool_build_all(doc);
    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 5);

    for (size_t i = 0; i < 5; i++) {
        yyjson_mut_val *schema = yyjson_mut_arr_get(arr, i);
        yyjson_mut_val *function = yyjson_mut_obj_get(schema, "function");
        yyjson_mut_val *name = yyjson_mut_obj_get(function, "name");
        ck_assert_str_eq(yyjson_mut_get_str(name), expected_names[i]);
    }

    yyjson_mut_doc_free(doc);
}

END_TEST

// Test suite
static Suite *tool_definition_suite(void)
{
    Suite *s = suite_create("Tool Definition");

    TCase *tc_param_def = tcase_create("Parameter Definition");
    tcase_set_timeout(tc_param_def, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_param_def, setup, teardown);
    tcase_add_test(tc_param_def, test_tool_param_def_struct_exists);
    suite_add_tcase(s, tc_param_def);

    TCase *tc_schema_def = tcase_create("Schema Definition");
    tcase_set_timeout(tc_schema_def, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_schema_def, setup, teardown);
    tcase_add_test(tc_schema_def, test_tool_schema_def_struct_exists);
    suite_add_tcase(s, tc_schema_def);

    TCase *tc_build_from_def = tcase_create("Build Schema From Def");
    tcase_set_timeout(tc_build_from_def, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_build_from_def, setup, teardown);
    tcase_add_test(tc_build_from_def, test_tool_build_schema_from_def_basic);
    tcase_add_test(tc_build_from_def, test_schema_definitions_complete);
    suite_add_tcase(s, tc_build_from_def);

    return s;
}

int main(void)
{
    Suite *s = tool_definition_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
