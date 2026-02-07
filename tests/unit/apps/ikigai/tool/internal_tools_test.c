/**
 * @file internal_tools_test.c
 * @brief Unit tests for internal tool registration and noop handler
 */

#include "tests/test_constants.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/internal_tools.h"
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

// Test: ik_internal_tools_register registers the noop tool
START_TEST(test_register_noop_tool) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    ik_internal_tools_register(registry);

    ck_assert_uint_eq(registry->count, 1);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "noop");
    ck_assert_ptr_nonnull(entry);
    ck_assert_str_eq(entry->name, "noop");
    ck_assert_ptr_null(entry->path);
    ck_assert_int_eq(entry->type, IK_TOOL_INTERNAL);
    ck_assert_ptr_nonnull(entry->handler);
    ck_assert_ptr_null(entry->on_complete);
    ck_assert_ptr_nonnull(entry->schema_doc);
    ck_assert_ptr_nonnull(entry->schema_root);
}

END_TEST

// Test: noop handler returns correct JSON
START_TEST(test_noop_handler_returns_ok) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    ik_internal_tools_register(registry);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "noop");
    ck_assert_ptr_nonnull(entry);
    ck_assert_ptr_nonnull(entry->handler);

    // Create a minimal agent context
    ik_agent_ctx_t agent = {0};

    // Call the handler directly
    char *result = entry->handler(test_ctx, &agent, "{}");
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "{\"ok\": true}");

    talloc_free(result);
}

END_TEST

// Test: noop schema contains expected fields
START_TEST(test_noop_schema_fields) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    ik_internal_tools_register(registry);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "noop");
    ck_assert_ptr_nonnull(entry);

    // Verify schema has name field
    yyjson_val *name_val = yyjson_obj_get(entry->schema_root, "name");
    ck_assert_ptr_nonnull(name_val);
    ck_assert_str_eq(yyjson_get_str(name_val), "noop");

    // Verify schema has description field
    yyjson_val *desc_val = yyjson_obj_get(entry->schema_root, "description");
    ck_assert_ptr_nonnull(desc_val);

    // Verify schema has parameters field
    yyjson_val *params_val = yyjson_obj_get(entry->schema_root, "parameters");
    ck_assert_ptr_nonnull(params_val);
}

END_TEST

// Test: noop handler with different arguments (ignored)
START_TEST(test_noop_handler_ignores_arguments) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    ik_internal_tools_register(registry);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "noop");
    ck_assert_ptr_nonnull(entry);

    ik_agent_ctx_t agent = {0};

    // Call with different arguments - should still return ok
    char *result = entry->handler(test_ctx, &agent, "{\"foo\": \"bar\"}");
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "{\"ok\": true}");

    talloc_free(result);
}

END_TEST

// Test: registering twice overwrites cleanly
START_TEST(test_register_twice_overwrites) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    ik_internal_tools_register(registry);
    ck_assert_uint_eq(registry->count, 1);

    // Register again - should override, not duplicate
    ik_internal_tools_register(registry);
    ck_assert_uint_eq(registry->count, 1);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "noop");
    ck_assert_ptr_nonnull(entry);
    ck_assert_int_eq(entry->type, IK_TOOL_INTERNAL);
}

END_TEST

static Suite *internal_tools_suite(void)
{
    Suite *s = suite_create("InternalTools");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_register_noop_tool);
    tcase_add_test(tc_core, test_noop_handler_returns_ok);
    tcase_add_test(tc_core, test_noop_schema_fields);
    tcase_add_test(tc_core, test_noop_handler_ignores_arguments);
    tcase_add_test(tc_core, test_register_twice_overwrites);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = internal_tools_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/tool/internal_tools_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
