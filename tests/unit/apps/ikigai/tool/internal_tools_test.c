/**
 * @file internal_tools_test.c
 * @brief Unit tests for internal tool registration (fork, kill, send, wait)
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

// Test: ik_internal_tools_register registers all four internal tools
START_TEST(test_register_all_tools) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    ik_internal_tools_register(registry);

    ck_assert_uint_eq(registry->count, 4);

    // Verify fork tool
    ik_tool_registry_entry_t *fork_entry = ik_tool_registry_lookup(registry, "fork");
    ck_assert_ptr_nonnull(fork_entry);
    ck_assert_str_eq(fork_entry->name, "fork");
    ck_assert_ptr_null(fork_entry->path);
    ck_assert_int_eq(fork_entry->type, IK_TOOL_INTERNAL);
    ck_assert_ptr_nonnull(fork_entry->handler);
    ck_assert_ptr_nonnull(fork_entry->on_complete);
    ck_assert_ptr_nonnull(fork_entry->schema_doc);
    ck_assert_ptr_nonnull(fork_entry->schema_root);

    // Verify kill tool
    ik_tool_registry_entry_t *kill_entry = ik_tool_registry_lookup(registry, "kill");
    ck_assert_ptr_nonnull(kill_entry);
    ck_assert_str_eq(kill_entry->name, "kill");
    ck_assert_int_eq(kill_entry->type, IK_TOOL_INTERNAL);
    ck_assert_ptr_nonnull(kill_entry->handler);
    ck_assert_ptr_nonnull(kill_entry->on_complete);

    // Verify send tool
    ik_tool_registry_entry_t *send_entry = ik_tool_registry_lookup(registry, "send");
    ck_assert_ptr_nonnull(send_entry);
    ck_assert_str_eq(send_entry->name, "send");
    ck_assert_int_eq(send_entry->type, IK_TOOL_INTERNAL);
    ck_assert_ptr_nonnull(send_entry->handler);
    ck_assert_ptr_null(send_entry->on_complete);

    // Verify wait tool
    ik_tool_registry_entry_t *wait_entry = ik_tool_registry_lookup(registry, "wait");
    ck_assert_ptr_nonnull(wait_entry);
    ck_assert_str_eq(wait_entry->name, "wait");
    ck_assert_int_eq(wait_entry->type, IK_TOOL_INTERNAL);
    ck_assert_ptr_nonnull(wait_entry->handler);
    ck_assert_ptr_null(wait_entry->on_complete);
}

END_TEST

// Test: fork schema contains expected fields
START_TEST(test_fork_schema_fields) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    ik_internal_tools_register(registry);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "fork");
    ck_assert_ptr_nonnull(entry);

    // Verify schema has name field
    yyjson_val *name_val = yyjson_obj_get(entry->schema_root, "name");
    ck_assert_ptr_nonnull(name_val);
    ck_assert_str_eq(yyjson_get_str(name_val), "fork");

    // Verify schema has description field
    yyjson_val *desc_val = yyjson_obj_get(entry->schema_root, "description");
    ck_assert_ptr_nonnull(desc_val);

    // Verify schema has parameters field
    yyjson_val *params_val = yyjson_obj_get(entry->schema_root, "parameters");
    ck_assert_ptr_nonnull(params_val);

    // Verify required parameters
    yyjson_val *required_val = yyjson_obj_get(params_val, "required");
    ck_assert_ptr_nonnull(required_val);
    ck_assert(yyjson_is_arr(required_val));
    ck_assert_uint_eq(yyjson_arr_size(required_val), 2);
}

END_TEST

// Test: kill schema contains expected fields
START_TEST(test_kill_schema_fields) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    ik_internal_tools_register(registry);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "kill");
    ck_assert_ptr_nonnull(entry);

    // Verify schema has name field
    yyjson_val *name_val = yyjson_obj_get(entry->schema_root, "name");
    ck_assert_ptr_nonnull(name_val);
    ck_assert_str_eq(yyjson_get_str(name_val), "kill");

    // Verify required parameters
    yyjson_val *params_val = yyjson_obj_get(entry->schema_root, "parameters");
    ck_assert_ptr_nonnull(params_val);
    yyjson_val *required_val = yyjson_obj_get(params_val, "required");
    ck_assert_ptr_nonnull(required_val);
    ck_assert(yyjson_is_arr(required_val));
    ck_assert_uint_eq(yyjson_arr_size(required_val), 1);
}

END_TEST

// Test: send schema contains expected fields
START_TEST(test_send_schema_fields) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    ik_internal_tools_register(registry);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "send");
    ck_assert_ptr_nonnull(entry);

    // Verify schema has name field
    yyjson_val *name_val = yyjson_obj_get(entry->schema_root, "name");
    ck_assert_ptr_nonnull(name_val);
    ck_assert_str_eq(yyjson_get_str(name_val), "send");

    // Verify required parameters
    yyjson_val *params_val = yyjson_obj_get(entry->schema_root, "parameters");
    ck_assert_ptr_nonnull(params_val);
    yyjson_val *required_val = yyjson_obj_get(params_val, "required");
    ck_assert_ptr_nonnull(required_val);
    ck_assert(yyjson_is_arr(required_val));
    ck_assert_uint_eq(yyjson_arr_size(required_val), 2);
}

END_TEST

// Test: wait schema contains expected fields
START_TEST(test_wait_schema_fields) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    ik_internal_tools_register(registry);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "wait");
    ck_assert_ptr_nonnull(entry);

    // Verify schema has name field
    yyjson_val *name_val = yyjson_obj_get(entry->schema_root, "name");
    ck_assert_ptr_nonnull(name_val);
    ck_assert_str_eq(yyjson_get_str(name_val), "wait");

    // Verify required parameters
    yyjson_val *params_val = yyjson_obj_get(entry->schema_root, "parameters");
    ck_assert_ptr_nonnull(params_val);
    yyjson_val *required_val = yyjson_obj_get(params_val, "required");
    ck_assert_ptr_nonnull(required_val);
    ck_assert(yyjson_is_arr(required_val));
    ck_assert_uint_eq(yyjson_arr_size(required_val), 1);
}

END_TEST

// Test: registering twice overwrites cleanly
START_TEST(test_register_twice_overwrites) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    ik_internal_tools_register(registry);
    ck_assert_uint_eq(registry->count, 4);

    // Register again - should override, not duplicate
    ik_internal_tools_register(registry);
    ck_assert_uint_eq(registry->count, 4);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "fork");
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

    tcase_add_test(tc_core, test_register_all_tools);
    tcase_add_test(tc_core, test_fork_schema_fields);
    tcase_add_test(tc_core, test_kill_schema_fields);
    tcase_add_test(tc_core, test_send_schema_fields);
    tcase_add_test(tc_core, test_wait_schema_fields);
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
