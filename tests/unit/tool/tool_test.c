#include "tool.h"

#include "vendor/yyjson/yyjson.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

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

// Test: ik_tool_build_glob_schema returns non-NULL and has correct structure
START_TEST(test_tool_build_glob_schema_structure) {
    // Create yyjson document
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    // Call function under test
    yyjson_mut_val *schema = ik_tool_build_glob_schema(doc);

    // Verify returns non-NULL
    ck_assert_ptr_nonnull(schema);

    // Verify "type": "function"
    yyjson_mut_val *type = yyjson_mut_obj_get(schema, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert(yyjson_mut_is_str(type));
    ck_assert_str_eq(yyjson_mut_get_str(type), "function");

    // Verify "function" object exists
    yyjson_mut_val *function = yyjson_mut_obj_get(schema, "function");
    ck_assert_ptr_nonnull(function);
    ck_assert(yyjson_mut_is_obj(function));

    // Verify "function.name": "glob"
    yyjson_mut_val *name = yyjson_mut_obj_get(function, "name");
    ck_assert_ptr_nonnull(name);
    ck_assert(yyjson_mut_is_str(name));
    ck_assert_str_eq(yyjson_mut_get_str(name), "glob");

    // Verify "function.description" exists
    yyjson_mut_val *description = yyjson_mut_obj_get(function, "description");
    ck_assert_ptr_nonnull(description);
    ck_assert(yyjson_mut_is_str(description));

    // Verify "function.parameters" exists
    yyjson_mut_val *parameters = yyjson_mut_obj_get(function, "parameters");
    ck_assert_ptr_nonnull(parameters);
    ck_assert(yyjson_mut_is_obj(parameters));

    // Verify "function.parameters.type": "object"
    yyjson_mut_val *params_type = yyjson_mut_obj_get(parameters, "type");
    ck_assert_ptr_nonnull(params_type);
    ck_assert(yyjson_mut_is_str(params_type));
    ck_assert_str_eq(yyjson_mut_get_str(params_type), "object");

    // Verify "function.parameters.properties" exists
    yyjson_mut_val *properties = yyjson_mut_obj_get(parameters, "properties");
    ck_assert_ptr_nonnull(properties);
    ck_assert(yyjson_mut_is_obj(properties));

    // Verify "function.parameters.properties.pattern" exists
    yyjson_mut_val *pattern = yyjson_mut_obj_get(properties, "pattern");
    ck_assert_ptr_nonnull(pattern);
    ck_assert(yyjson_mut_is_obj(pattern));

    // Verify "function.parameters.properties.pattern.type": "string"
    yyjson_mut_val *pattern_type = yyjson_mut_obj_get(pattern, "type");
    ck_assert_ptr_nonnull(pattern_type);
    ck_assert(yyjson_mut_is_str(pattern_type));
    ck_assert_str_eq(yyjson_mut_get_str(pattern_type), "string");

    // Verify "function.parameters.properties.pattern.description" exists
    yyjson_mut_val *pattern_desc = yyjson_mut_obj_get(pattern, "description");
    ck_assert_ptr_nonnull(pattern_desc);
    ck_assert(yyjson_mut_is_str(pattern_desc));

    // Verify "function.parameters.properties.path" exists
    yyjson_mut_val *path = yyjson_mut_obj_get(properties, "path");
    ck_assert_ptr_nonnull(path);
    ck_assert(yyjson_mut_is_obj(path));

    // Verify "function.parameters.properties.path.type": "string"
    yyjson_mut_val *path_type = yyjson_mut_obj_get(path, "type");
    ck_assert_ptr_nonnull(path_type);
    ck_assert(yyjson_mut_is_str(path_type));
    ck_assert_str_eq(yyjson_mut_get_str(path_type), "string");

    // Verify "function.parameters.properties.path.description" exists
    yyjson_mut_val *path_desc = yyjson_mut_obj_get(path, "description");
    ck_assert_ptr_nonnull(path_desc);
    ck_assert(yyjson_mut_is_str(path_desc));

    // Verify "function.parameters.required" exists and is array with ["pattern"]
    yyjson_mut_val *required = yyjson_mut_obj_get(parameters, "required");
    ck_assert_ptr_nonnull(required);
    ck_assert(yyjson_mut_is_arr(required));
    ck_assert_uint_eq(yyjson_mut_arr_size(required), 1);

    yyjson_mut_val *required_0 = yyjson_mut_arr_get(required, 0);
    ck_assert_ptr_nonnull(required_0);
    ck_assert(yyjson_mut_is_str(required_0));
    ck_assert_str_eq(yyjson_mut_get_str(required_0), "pattern");

    yyjson_mut_doc_free(doc);
}
END_TEST

// Test suite
static Suite *tool_suite(void)
{
    Suite *s = suite_create("Tool");

    TCase *tc_glob = tcase_create("Glob Schema");
    tcase_add_checked_fixture(tc_glob, setup, teardown);
    tcase_add_test(tc_glob, test_tool_build_glob_schema_structure);
    suite_add_tcase(s, tc_glob);

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
