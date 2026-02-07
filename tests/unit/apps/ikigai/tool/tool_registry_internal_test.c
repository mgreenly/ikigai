#include "tests/test_constants.h"
#include "shared/json_allocator.h"
#include "apps/ikigai/tool_registry.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// Test fixture
static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Helper to create a simple schema document
static yyjson_doc *create_test_schema(const char *tool_name)
{
    char *json = talloc_asprintf(test_ctx, "{\"name\":\"%s\",\"description\":\"Test tool\"}", tool_name);
    yyjson_alc allocator = ik_make_talloc_allocator(test_ctx);
    yyjson_doc *doc = yyjson_read_opts(json, strlen(json), 0, &allocator, NULL);
    if (doc == NULL) {
        ck_abort_msg("Failed to parse test schema");
    }
    talloc_free(json);
    return doc;
}

// Dummy handler for internal tool tests
static char *dummy_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *arguments_json)
{
    (void)agent;
    (void)arguments_json;
    return talloc_strdup(ctx, "{\"ok\": true}");
}

// Dummy on_complete for internal tool tests
static void dummy_on_complete(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    (void)repl;
    (void)agent;
}

// Test: Sort empty registry
START_TEST(test_sort_empty) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    // Sort empty registry should be no-op
    ik_tool_registry_sort(registry);

    ck_assert_uint_eq(registry->count, 0);
}

END_TEST

// Test: Sort single entry
START_TEST(test_sort_single) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    yyjson_doc *schema = create_test_schema("bash");

    ik_tool_registry_add(registry, "bash", "/usr/bin/bash", schema);

    // Sort with single entry should be no-op
    ik_tool_registry_sort(registry);

    ck_assert_uint_eq(registry->count, 1);
    ck_assert_str_eq(registry->entries[0].name, "bash");
}

END_TEST

// Test: Sort multiple entries
START_TEST(test_sort_multiple) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    // Add tools in non-alphabetical order
    yyjson_doc *schema1 = create_test_schema("python");
    yyjson_doc *schema2 = create_test_schema("bash");
    yyjson_doc *schema3 = create_test_schema("node");
    yyjson_doc *schema4 = create_test_schema("grep");

    ik_tool_registry_add(registry, "python", "/usr/bin/python", schema1);
    ik_tool_registry_add(registry, "bash", "/usr/bin/bash", schema2);
    ik_tool_registry_add(registry, "node", "/usr/bin/node", schema3);
    ik_tool_registry_add(registry, "grep", "/usr/bin/grep", schema4);

    ck_assert_uint_eq(registry->count, 4);

    // Sort the registry
    ik_tool_registry_sort(registry);

    // Verify alphabetical order
    ck_assert_uint_eq(registry->count, 4);
    ck_assert_str_eq(registry->entries[0].name, "bash");
    ck_assert_str_eq(registry->entries[1].name, "grep");
    ck_assert_str_eq(registry->entries[2].name, "node");
    ck_assert_str_eq(registry->entries[3].name, "python");
}

END_TEST

// Test: Sort is idempotent
START_TEST(test_sort_idempotent) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    // Add tools in non-alphabetical order
    yyjson_doc *schema1 = create_test_schema("zebra");
    yyjson_doc *schema2 = create_test_schema("apple");
    yyjson_doc *schema3 = create_test_schema("mango");

    ik_tool_registry_add(registry, "zebra", "/usr/bin/zebra", schema1);
    ik_tool_registry_add(registry, "apple", "/usr/bin/apple", schema2);
    ik_tool_registry_add(registry, "mango", "/usr/bin/mango", schema3);

    // Sort twice
    ik_tool_registry_sort(registry);
    ik_tool_registry_sort(registry);

    // Verify alphabetical order is maintained
    ck_assert_uint_eq(registry->count, 3);
    ck_assert_str_eq(registry->entries[0].name, "apple");
    ck_assert_str_eq(registry->entries[1].name, "mango");
    ck_assert_str_eq(registry->entries[2].name, "zebra");
}

END_TEST

// Test: Add internal tool - new entry
START_TEST(test_add_internal_new) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    yyjson_doc *schema = create_test_schema("noop");

    res_t result = ik_tool_registry_add_internal(registry, "noop", schema, dummy_handler, dummy_on_complete);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq(registry->count, 1);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "noop");
    ck_assert_ptr_nonnull(entry);
    ck_assert_str_eq(entry->name, "noop");
    ck_assert_ptr_null(entry->path);
    ck_assert_ptr_nonnull(entry->schema_doc);
    ck_assert_ptr_nonnull(entry->schema_root);
    ck_assert_int_eq(entry->type, IK_TOOL_INTERNAL);
    ck_assert_ptr_eq(entry->handler, dummy_handler);
    ck_assert_ptr_eq(entry->on_complete, dummy_on_complete);
}

END_TEST

// Test: Add internal tool - override existing external tool
START_TEST(test_add_internal_override_external) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    yyjson_doc *schema1 = create_test_schema("mytool");
    yyjson_doc *schema2 = create_test_schema("mytool_internal");

    // First add as external
    ik_tool_registry_add(registry, "mytool", "/usr/bin/mytool", schema1);
    ck_assert_uint_eq(registry->count, 1);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "mytool");
    ck_assert_ptr_nonnull(entry);
    ck_assert_str_eq(entry->path, "/usr/bin/mytool");
    ck_assert_int_eq(entry->type, IK_TOOL_EXTERNAL);

    // Override with internal
    res_t result = ik_tool_registry_add_internal(registry, "mytool", schema2, dummy_handler, dummy_on_complete);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(registry->count, 1);  // Count stays the same

    entry = ik_tool_registry_lookup(registry, "mytool");
    ck_assert_ptr_nonnull(entry);
    ck_assert_ptr_null(entry->path);
    ck_assert_int_eq(entry->type, IK_TOOL_INTERNAL);
    ck_assert_ptr_eq(entry->handler, dummy_handler);
    ck_assert_ptr_eq(entry->on_complete, dummy_on_complete);
}

END_TEST

// Test: Add internal tool - override existing internal tool
START_TEST(test_add_internal_override_internal) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    yyjson_doc *schema1 = create_test_schema("noop");
    yyjson_doc *schema2 = create_test_schema("noop_v2");

    // Add internal tool
    ik_tool_registry_add_internal(registry, "noop", schema1, dummy_handler, NULL);
    ck_assert_uint_eq(registry->count, 1);

    // Override with another internal tool
    res_t result = ik_tool_registry_add_internal(registry, "noop", schema2, dummy_handler, dummy_on_complete);
    ck_assert(!is_err(&result));
    ck_assert_uint_eq(registry->count, 1);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "noop");
    ck_assert_ptr_nonnull(entry);
    ck_assert_ptr_null(entry->path);
    ck_assert_int_eq(entry->type, IK_TOOL_INTERNAL);
    ck_assert_ptr_eq(entry->on_complete, dummy_on_complete);
}

END_TEST

// Test: Add internal tool with NULL on_complete
START_TEST(test_add_internal_null_on_complete) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    yyjson_doc *schema = create_test_schema("noop");

    res_t result = ik_tool_registry_add_internal(registry, "noop", schema, dummy_handler, NULL);

    ck_assert(!is_err(&result));
    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "noop");
    ck_assert_ptr_nonnull(entry);
    ck_assert_ptr_eq(entry->handler, dummy_handler);
    ck_assert_ptr_null(entry->on_complete);
}

END_TEST

// Test: Clear registry with internal tools (path==NULL)
START_TEST(test_clear_with_internal_tools) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    yyjson_doc *schema1 = create_test_schema("bash");
    yyjson_doc *schema2 = create_test_schema("noop");

    ik_tool_registry_add(registry, "bash", "/usr/bin/bash", schema1);
    ik_tool_registry_add_internal(registry, "noop", schema2, dummy_handler, NULL);

    ck_assert_uint_eq(registry->count, 2);

    // This exercises the path==NULL branch on line 154 of tool_registry.c
    ik_tool_registry_clear(registry);

    ck_assert_uint_eq(registry->count, 0);
    ck_assert_ptr_null(ik_tool_registry_lookup(registry, "bash"));
    ck_assert_ptr_null(ik_tool_registry_lookup(registry, "noop"));
}

END_TEST

// Test: Sort with mixed external and internal tools
START_TEST(test_sort_with_internal_tools) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    yyjson_doc *schema1 = create_test_schema("python");
    yyjson_doc *schema2 = create_test_schema("noop");
    yyjson_doc *schema3 = create_test_schema("bash");

    ik_tool_registry_add(registry, "python", "/usr/bin/python", schema1);
    ik_tool_registry_add_internal(registry, "noop", schema2, dummy_handler, NULL);
    ik_tool_registry_add(registry, "bash", "/usr/bin/bash", schema3);

    ck_assert_uint_eq(registry->count, 3);

    ik_tool_registry_sort(registry);

    // Verify alphabetical order: bash, noop, python
    ck_assert_str_eq(registry->entries[0].name, "bash");
    ck_assert_str_eq(registry->entries[1].name, "noop");
    ck_assert_str_eq(registry->entries[2].name, "python");

    // Verify internal tool properties preserved after sort
    ik_tool_registry_entry_t *noop = ik_tool_registry_lookup(registry, "noop");
    ck_assert_ptr_nonnull(noop);
    ck_assert_ptr_null(noop->path);
    ck_assert_int_eq(noop->type, IK_TOOL_INTERNAL);
    ck_assert_ptr_eq(noop->handler, dummy_handler);
}

END_TEST

// Test: Build all tools array includes internal tools
START_TEST(test_build_all_with_internal_tools) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    yyjson_doc *schema1 = create_test_schema("bash");
    yyjson_doc *schema2 = create_test_schema("noop");

    ik_tool_registry_add(registry, "bash", "/usr/bin/bash", schema1);
    ik_tool_registry_add_internal(registry, "noop", schema2, dummy_handler, NULL);

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *tools_array = ik_tool_registry_build_all(registry, doc);

    ck_assert_ptr_nonnull(tools_array);
    ck_assert(yyjson_mut_is_arr(tools_array));
    ck_assert_uint_eq(yyjson_mut_arr_size(tools_array), 2);

    yyjson_mut_doc_free(doc);
}

END_TEST

static Suite *tool_registry_internal_suite(void)
{
    Suite *s = suite_create("ToolRegistryInternal");

    TCase *tc_sort = tcase_create("Sort");
    tcase_set_timeout(tc_sort, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_sort, setup, teardown);
    tcase_add_test(tc_sort, test_sort_empty);
    tcase_add_test(tc_sort, test_sort_single);
    tcase_add_test(tc_sort, test_sort_multiple);
    tcase_add_test(tc_sort, test_sort_idempotent);
    suite_add_tcase(s, tc_sort);

    TCase *tc_internal = tcase_create("Internal");
    tcase_set_timeout(tc_internal, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_internal, setup, teardown);
    tcase_add_test(tc_internal, test_add_internal_new);
    tcase_add_test(tc_internal, test_add_internal_override_external);
    tcase_add_test(tc_internal, test_add_internal_override_internal);
    tcase_add_test(tc_internal, test_add_internal_null_on_complete);
    tcase_add_test(tc_internal, test_clear_with_internal_tools);
    tcase_add_test(tc_internal, test_sort_with_internal_tools);
    tcase_add_test(tc_internal, test_build_all_with_internal_tools);
    suite_add_tcase(s, tc_internal);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = tool_registry_internal_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/tool/tool_registry_internal_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
