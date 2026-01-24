#include "../../../src/tool_executor.h"

#include "../../../src/error.h"
#include "../../../src/json_allocator.h"
#include "../../../src/paths.h"
#include "../../../src/tool_registry.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include "../../../src/vendor/yyjson/yyjson.h"

// Test fixture
static TALLOC_CTX *test_ctx;
static ik_tool_registry_t *registry;
static ik_paths_t *paths;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    registry = ik_tool_registry_create(test_ctx);

    // Create test directories
    mkdir("/tmp/bin", 0755);
    mkdir("/tmp/state", 0755);
    mkdir("/tmp/cache", 0755);

    setenv("IKIGAI_BIN_DIR", "/tmp/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/tmp/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/tmp/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/tmp/libexec/ikigai", 1);
    setenv("IKIGAI_CACHE_DIR", "/tmp/cache", 1);
    setenv("IKIGAI_STATE_DIR", "/tmp/state", 1);

    res_t paths_result = ik_paths_init(test_ctx, &paths);
    ck_assert(!is_err(&paths_result));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Test: NULL registry
START_TEST(test_null_registry) {
    char *result = ik_tool_execute_from_registry(test_ctx, NULL, paths, "agent1", "test_tool", "{}");
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_doc_free(doc);
}

END_TEST

// Test: Tool not found
START_TEST(test_tool_not_found) {
    char *result = ik_tool_execute_from_registry(test_ctx, registry, paths, "agent1", "nonexistent", "{}");
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_doc_free(doc);
}

END_TEST

static Suite *tool_executor_suite(void)
{
    Suite *s = suite_create("ToolExecutor");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_null_registry);
    tcase_add_test(tc_core, test_tool_not_found);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = tool_executor_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/tool_executor_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
