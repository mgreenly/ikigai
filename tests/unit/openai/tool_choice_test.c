#include <check.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/openai/tool_choice.h"
#include "../../../src/json_allocator.h"
#include "../../test_utils.h"
#include "vendor/yyjson/yyjson.h"

/*
 * Test tool_choice type and helper functions
 *
 * Tests for creating and serializing tool_choice values:
 * - "auto" mode
 * - "none" mode
 * - "required" mode
 * - specific tool mode (e.g., {"type": "function", "function": {"name": "glob"}})
 */

// Test creating tool_choice for "auto" mode
START_TEST(test_tool_choice_auto) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_tool_choice_t choice = ik_tool_choice_auto();

    ck_assert_int_eq(choice.mode, IK_TOOL_CHOICE_AUTO);
    ck_assert_ptr_null(choice.tool_name);

    talloc_free(ctx);
}
END_TEST
// Test creating tool_choice for "none" mode
START_TEST(test_tool_choice_none)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_tool_choice_t choice = ik_tool_choice_none();

    ck_assert_int_eq(choice.mode, IK_TOOL_CHOICE_NONE);
    ck_assert_ptr_null(choice.tool_name);

    talloc_free(ctx);
}

END_TEST
// Test creating tool_choice for "required" mode
START_TEST(test_tool_choice_required)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_tool_choice_t choice = ik_tool_choice_required();

    ck_assert_int_eq(choice.mode, IK_TOOL_CHOICE_REQUIRED);
    ck_assert_ptr_null(choice.tool_name);

    talloc_free(ctx);
}

END_TEST
// Test creating tool_choice for specific tool
START_TEST(test_tool_choice_specific)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_tool_choice_t choice = ik_tool_choice_specific(ctx, "glob");

    ck_assert_int_eq(choice.mode, IK_TOOL_CHOICE_SPECIFIC);
    ck_assert_ptr_nonnull(choice.tool_name);
    ck_assert_str_eq(choice.tool_name, "glob");

    talloc_free(ctx);
}

END_TEST
// Test serializing tool_choice auto mode to JSON
START_TEST(test_serialize_tool_choice_auto)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create yyjson document with talloc allocator
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    ck_assert_ptr_nonnull(doc);

    // Create root object
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    ck_assert_ptr_nonnull(root);
    yyjson_mut_doc_set_root(doc, root);

    // Serialize tool_choice auto mode
    ik_tool_choice_t choice = ik_tool_choice_auto();
    ik_tool_choice_serialize(doc, root, "tool_choice", choice);

    // Convert to JSON string
    char *json_str = yyjson_mut_write(doc, 0, NULL);
    ck_assert_ptr_nonnull(json_str);

    // Parse JSON to verify structure
    yyjson_doc *parsed_doc = yyjson_read(json_str, strlen(json_str), 0);
    ck_assert_ptr_nonnull(parsed_doc);

    yyjson_val *parsed_root = yyjson_doc_get_root(parsed_doc);
    ck_assert(yyjson_is_obj(parsed_root));

    // Verify tool_choice field is string "auto"
    yyjson_val *tool_choice_val = yyjson_obj_get(parsed_root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice_val);
    ck_assert(yyjson_is_str(tool_choice_val));
    ck_assert_str_eq(yyjson_get_str(tool_choice_val), "auto");

    yyjson_doc_free(parsed_doc);
    free(json_str);
    talloc_free(ctx);
}

END_TEST
// Test serializing tool_choice none mode to JSON
START_TEST(test_serialize_tool_choice_none)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create yyjson document with talloc allocator
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    ck_assert_ptr_nonnull(doc);

    // Create root object
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    ck_assert_ptr_nonnull(root);
    yyjson_mut_doc_set_root(doc, root);

    // Serialize tool_choice none mode
    ik_tool_choice_t choice = ik_tool_choice_none();
    ik_tool_choice_serialize(doc, root, "tool_choice", choice);

    // Convert to JSON string
    char *json_str = yyjson_mut_write(doc, 0, NULL);
    ck_assert_ptr_nonnull(json_str);

    // Parse JSON to verify structure
    yyjson_doc *parsed_doc = yyjson_read(json_str, strlen(json_str), 0);
    ck_assert_ptr_nonnull(parsed_doc);

    yyjson_val *parsed_root = yyjson_doc_get_root(parsed_doc);
    ck_assert(yyjson_is_obj(parsed_root));

    // Verify tool_choice field is string "none"
    yyjson_val *tool_choice_val = yyjson_obj_get(parsed_root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice_val);
    ck_assert(yyjson_is_str(tool_choice_val));
    ck_assert_str_eq(yyjson_get_str(tool_choice_val), "none");

    yyjson_doc_free(parsed_doc);
    free(json_str);
    talloc_free(ctx);
}

END_TEST
// Test serializing tool_choice required mode to JSON
START_TEST(test_serialize_tool_choice_required)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create yyjson document with talloc allocator
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    ck_assert_ptr_nonnull(doc);

    // Create root object
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    ck_assert_ptr_nonnull(root);
    yyjson_mut_doc_set_root(doc, root);

    // Serialize tool_choice required mode
    ik_tool_choice_t choice = ik_tool_choice_required();
    ik_tool_choice_serialize(doc, root, "tool_choice", choice);

    // Convert to JSON string
    char *json_str = yyjson_mut_write(doc, 0, NULL);
    ck_assert_ptr_nonnull(json_str);

    // Parse JSON to verify structure
    yyjson_doc *parsed_doc = yyjson_read(json_str, strlen(json_str), 0);
    ck_assert_ptr_nonnull(parsed_doc);

    yyjson_val *parsed_root = yyjson_doc_get_root(parsed_doc);
    ck_assert(yyjson_is_obj(parsed_root));

    // Verify tool_choice field is string "required"
    yyjson_val *tool_choice_val = yyjson_obj_get(parsed_root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice_val);
    ck_assert(yyjson_is_str(tool_choice_val));
    ck_assert_str_eq(yyjson_get_str(tool_choice_val), "required");

    yyjson_doc_free(parsed_doc);
    free(json_str);
    talloc_free(ctx);
}

END_TEST
// Test serializing tool_choice specific tool mode to JSON
START_TEST(test_serialize_tool_choice_specific)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create yyjson document with talloc allocator
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    ck_assert_ptr_nonnull(doc);

    // Create root object
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    ck_assert_ptr_nonnull(root);
    yyjson_mut_doc_set_root(doc, root);

    // Serialize tool_choice specific mode for "glob" tool
    ik_tool_choice_t choice = ik_tool_choice_specific(ctx, "glob");
    ik_tool_choice_serialize(doc, root, "tool_choice", choice);

    // Convert to JSON string
    char *json_str = yyjson_mut_write(doc, 0, NULL);
    ck_assert_ptr_nonnull(json_str);

    // Parse JSON to verify structure
    yyjson_doc *parsed_doc = yyjson_read(json_str, strlen(json_str), 0);
    ck_assert_ptr_nonnull(parsed_doc);

    yyjson_val *parsed_root = yyjson_doc_get_root(parsed_doc);
    ck_assert(yyjson_is_obj(parsed_root));

    // Verify tool_choice field is an object
    yyjson_val *tool_choice_val = yyjson_obj_get(parsed_root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice_val);
    ck_assert(yyjson_is_obj(tool_choice_val));

    // Verify type field is "function"
    yyjson_val *type_val = yyjson_obj_get(tool_choice_val, "type");
    ck_assert_ptr_nonnull(type_val);
    ck_assert(yyjson_is_str(type_val));
    ck_assert_str_eq(yyjson_get_str(type_val), "function");

    // Verify function object exists
    yyjson_val *function_val = yyjson_obj_get(tool_choice_val, "function");
    ck_assert_ptr_nonnull(function_val);
    ck_assert(yyjson_is_obj(function_val));

    // Verify function.name is "glob"
    yyjson_val *name_val = yyjson_obj_get(function_val, "name");
    ck_assert_ptr_nonnull(name_val);
    ck_assert(yyjson_is_str(name_val));
    ck_assert_str_eq(yyjson_get_str(name_val), "glob");

    yyjson_doc_free(parsed_doc);
    free(json_str);
    talloc_free(ctx);
}

END_TEST

// Test suite setup
static Suite *tool_choice_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Tool Choice");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_tool_choice_auto);
    tcase_add_test(tc_core, test_tool_choice_none);
    tcase_add_test(tc_core, test_tool_choice_required);
    tcase_add_test(tc_core, test_tool_choice_specific);
    tcase_add_test(tc_core, test_serialize_tool_choice_auto);
    tcase_add_test(tc_core, test_serialize_tool_choice_none);
    tcase_add_test(tc_core, test_serialize_tool_choice_required);
    tcase_add_test(tc_core, test_serialize_tool_choice_specific);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = tool_choice_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
