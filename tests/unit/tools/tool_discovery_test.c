#include "../../test_constants.h"
#include "../../../src/tool_discovery.h"

#include "../../../src/tool_registry.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

// Test fixture
static TALLOC_CTX *test_ctx;
static char test_dir[256];

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    // Create unique test directory
    snprintf(test_dir, sizeof(test_dir), "/tmp/tool_discovery_test_%d", getpid());
    mkdir(test_dir, 0755);
}

static void teardown(void)
{
    // Clean up test directory
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
    system(cmd);

    talloc_free(test_ctx);
}

// Helper: Create a test tool in a specific directory
static void create_test_tool_in_dir(const char *dir, const char *name, const char *description)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s_tool", dir, name);

    FILE *f = fopen(path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "if [ \"$1\" = \"--schema\" ]; then\n");
    fprintf(f, "  printf '{\"name\":\"%s\",\"description\":\"%s\"}'\n", name, description);
    fprintf(f, "  exit 0\n");
    fprintf(f, "fi\n");
    fprintf(f, "exit 1\n");
    fclose(f);
    chmod(path, 0755);
}

// Test: Missing/empty directories handled gracefully
START_TEST(test_missing_directories) {
    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);

    // All directories don't exist
    res_t res = ik_tool_discovery_run(
        test_ctx,
        "/nonexistent/system",
        "/nonexistent/user",
        "/nonexistent/project",
        registry
    );

    ck_assert(!is_err(&res));
    ck_assert_uint_eq(registry->count, 0);
}

END_TEST

// Test: Empty directories handled gracefully
START_TEST(test_empty_directories) {
    char system_dir[512], user_dir[512], project_dir[512];
    snprintf(system_dir, sizeof(system_dir), "%s/system", test_dir);
    snprintf(user_dir, sizeof(user_dir), "%s/user", test_dir);
    snprintf(project_dir, sizeof(project_dir), "%s/project", test_dir);

    mkdir(system_dir, 0755);
    mkdir(user_dir, 0755);
    mkdir(project_dir, 0755);

    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    res_t res = ik_tool_discovery_run(test_ctx, system_dir, user_dir, project_dir, registry);

    ck_assert(!is_err(&res));
    ck_assert_uint_eq(registry->count, 0);
}

END_TEST

// Test: Discover single tool in system directory
START_TEST(test_discover_single_tool) {
    char system_dir[512];
    snprintf(system_dir, sizeof(system_dir), "%s/system", test_dir);
    mkdir(system_dir, 0755);

    create_test_tool_in_dir(system_dir, "bash", "Shell tool");

    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    res_t res = ik_tool_discovery_run(
        test_ctx,
        system_dir,
        "/nonexistent/user",
        "/nonexistent/project",
        registry
    );

    ck_assert(!is_err(&res));
    ck_assert_uint_eq(registry->count, 1);

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "bash_tool");
    ck_assert_ptr_nonnull(entry);
    ck_assert_str_eq(entry->name, "bash_tool");
}

END_TEST

// Test: Discover multiple tools across all directories
START_TEST(test_discover_multiple_tools) {
    char system_dir[512], user_dir[512], project_dir[512];
    snprintf(system_dir, sizeof(system_dir), "%s/system", test_dir);
    snprintf(user_dir, sizeof(user_dir), "%s/user", test_dir);
    snprintf(project_dir, sizeof(project_dir), "%s/project", test_dir);

    mkdir(system_dir, 0755);
    mkdir(user_dir, 0755);
    mkdir(project_dir, 0755);

    create_test_tool_in_dir(system_dir, "bash", "System shell");
    create_test_tool_in_dir(user_dir, "python", "User python");
    create_test_tool_in_dir(project_dir, "node", "Project node");

    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    res_t res = ik_tool_discovery_run(test_ctx, system_dir, user_dir, project_dir, registry);

    ck_assert(!is_err(&res));
    ck_assert_uint_eq(registry->count, 3);

    ck_assert_ptr_nonnull(ik_tool_registry_lookup(registry, "bash_tool"));
    ck_assert_ptr_nonnull(ik_tool_registry_lookup(registry, "python_tool"));
    ck_assert_ptr_nonnull(ik_tool_registry_lookup(registry, "node_tool"));
}

END_TEST

// Test: Override precedence - project > user > system
START_TEST(test_override_precedence) {
    char system_dir[512], user_dir[512], project_dir[512];
    snprintf(system_dir, sizeof(system_dir), "%s/system", test_dir);
    snprintf(user_dir, sizeof(user_dir), "%s/user", test_dir);
    snprintf(project_dir, sizeof(project_dir), "%s/project", test_dir);

    mkdir(system_dir, 0755);
    mkdir(user_dir, 0755);
    mkdir(project_dir, 0755);

    // Same tool in all three directories with different descriptions
    create_test_tool_in_dir(system_dir, "bash", "System shell");
    create_test_tool_in_dir(user_dir, "bash", "User shell");
    create_test_tool_in_dir(project_dir, "bash", "Project shell");

    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    res_t res = ik_tool_discovery_run(test_ctx, system_dir, user_dir, project_dir, registry);

    ck_assert(!is_err(&res));
    ck_assert_uint_eq(registry->count, 1);

    // Project version should win
    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, "bash_tool");
    ck_assert_ptr_nonnull(entry);
    ck_assert(strstr(entry->path, "project") != NULL);
}

END_TEST

// Test: Skip non-executable files
START_TEST(test_skip_non_executable) {
    char system_dir[512];
    snprintf(system_dir, sizeof(system_dir), "%s/system", test_dir);
    mkdir(system_dir, 0755);

    // Create a non-executable file
    char path[1024];
    snprintf(path, sizeof(path), "%s/not_executable_tool", system_dir);
    FILE *f = fopen(path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\necho test\n");
    fclose(f);
    chmod(path, 0644);  // Read/write but not executable

    // Create an executable tool
    create_test_tool_in_dir(system_dir, "bash", "Shell tool");

    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    res_t res = ik_tool_discovery_run(
        test_ctx,
        system_dir,
        "/nonexistent/user",
        "/nonexistent/project",
        registry
    );

    ck_assert(!is_err(&res));
    ck_assert_uint_eq(registry->count, 1);
    ck_assert_ptr_nonnull(ik_tool_registry_lookup(registry, "bash_tool"));
    ck_assert_ptr_null(ik_tool_registry_lookup(registry, "not_executable_tool"));
}

END_TEST

// Test: Skip tools with invalid schema
START_TEST(test_skip_invalid_schema) {
    char system_dir[512];
    snprintf(system_dir, sizeof(system_dir), "%s/system", test_dir);
    mkdir(system_dir, 0755);

    // Create tool that returns invalid JSON
    char path[1024];
    snprintf(path, sizeof(path), "%s/bad_tool", system_dir);
    FILE *f = fopen(path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "if [ \"$1\" = \"--schema\" ]; then\n");
    fprintf(f, "  printf 'not valid json'\n");
    fprintf(f, "  exit 0\n");
    fprintf(f, "fi\n");
    fclose(f);
    chmod(path, 0755);

    // Create valid tool
    create_test_tool_in_dir(system_dir, "good", "Good tool");

    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    res_t res = ik_tool_discovery_run(
        test_ctx,
        system_dir,
        "/nonexistent/user",
        "/nonexistent/project",
        registry
    );

    ck_assert(!is_err(&res));
    ck_assert_uint_eq(registry->count, 1);
    ck_assert_ptr_nonnull(ik_tool_registry_lookup(registry, "good_tool"));
    ck_assert_ptr_null(ik_tool_registry_lookup(registry, "bad_tool"));
}

END_TEST

// Test: Skip tools that crash
START_TEST(test_skip_crashing_tool) {
    char system_dir[512];
    snprintf(system_dir, sizeof(system_dir), "%s/system", test_dir);
    mkdir(system_dir, 0755);

    // Create tool that exits with non-zero status
    char path[1024];
    snprintf(path, sizeof(path), "%s/crash_tool", system_dir);
    FILE *f = fopen(path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\nexit 1\n");
    fclose(f);
    chmod(path, 0755);

    // Create valid tool
    create_test_tool_in_dir(system_dir, "good", "Good tool");

    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    res_t res = ik_tool_discovery_run(
        test_ctx,
        system_dir,
        "/nonexistent/user",
        "/nonexistent/project",
        registry
    );

    ck_assert(!is_err(&res));
    ck_assert_uint_eq(registry->count, 1);
    ck_assert_ptr_nonnull(ik_tool_registry_lookup(registry, "good_tool"));
    ck_assert_ptr_null(ik_tool_registry_lookup(registry, "crash_tool"));
}

END_TEST

// Test: Skip tools that produce no output
START_TEST(test_skip_silent_tool) {
    char system_dir[512];
    snprintf(system_dir, sizeof(system_dir), "%s/system", test_dir);
    mkdir(system_dir, 0755);

    // Create tool that produces no output
    char path[1024];
    snprintf(path, sizeof(path), "%s/silent_tool", system_dir);
    FILE *f = fopen(path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\nexit 0\n");
    fclose(f);
    chmod(path, 0755);

    // Create valid tool
    create_test_tool_in_dir(system_dir, "good", "Good tool");

    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    res_t res = ik_tool_discovery_run(
        test_ctx,
        system_dir,
        "/nonexistent/user",
        "/nonexistent/project",
        registry
    );

    ck_assert(!is_err(&res));
    ck_assert_uint_eq(registry->count, 1);
    ck_assert_ptr_nonnull(ik_tool_registry_lookup(registry, "good_tool"));
    ck_assert_ptr_null(ik_tool_registry_lookup(registry, "silent_tool"));
}

END_TEST

// Test: Skip tools with very large schema output (buffer overflow case)
START_TEST(test_skip_large_schema) {
    char system_dir[512];
    snprintf(system_dir, sizeof(system_dir), "%s/system", test_dir);
    mkdir(system_dir, 0755);

    // Create tool that outputs > 8191 bytes (exceeds call_tool_schema buffer)
    char path[1024];
    snprintf(path, sizeof(path), "%s/large_tool", system_dir);
    FILE *f = fopen(path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "if [ \"$1\" = \"--schema\" ]; then\n");
    // Generate 9000 bytes of output using dd
    fprintf(f, "  dd if=/dev/zero bs=9000 count=1 2>/dev/null | tr '\\0' 'x'\n");
    fprintf(f, "  exit 0\n");
    fprintf(f, "fi\n");
    fclose(f);
    chmod(path, 0755);

    // Create valid tool
    create_test_tool_in_dir(system_dir, "good", "Good tool");

    ik_tool_registry_t *registry = ik_tool_registry_create(test_ctx);
    res_t res = ik_tool_discovery_run(
        test_ctx,
        system_dir,
        "/nonexistent/user",
        "/nonexistent/project",
        registry
    );

    ck_assert(!is_err(&res));
    // Should have at least the good tool (large_tool should be skipped due to invalid JSON)
    ck_assert(registry->count >= 1);
    ck_assert_ptr_nonnull(ik_tool_registry_lookup(registry, "good_tool"));
}

END_TEST

static Suite *tool_discovery_suite(void)
{
    Suite *s = suite_create("ToolDiscovery");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_missing_directories);
    tcase_add_test(tc_core, test_empty_directories);
    tcase_add_test(tc_core, test_discover_single_tool);
    tcase_add_test(tc_core, test_discover_multiple_tools);
    tcase_add_test(tc_core, test_override_precedence);
    tcase_add_test(tc_core, test_skip_non_executable);
    tcase_add_test(tc_core, test_skip_invalid_schema);
    tcase_add_test(tc_core, test_skip_crashing_tool);
    tcase_add_test(tc_core, test_skip_silent_tool);
    tcase_add_test(tc_core, test_skip_large_schema);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = tool_discovery_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
