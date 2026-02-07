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

static Suite *tool_registry_sort_suite(void)
{
    Suite *s = suite_create("ToolRegistrySort");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_sort_empty);
    tcase_add_test(tc_core, test_sort_single);
    tcase_add_test(tc_core, test_sort_multiple);
    tcase_add_test(tc_core, test_sort_idempotent);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = tool_registry_sort_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/tool/tool_registry_sort_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
