/**
 * @file commands_tool_test.c
 * @brief Unit tests for /tool and /refresh commands
 */

#include "../../../src/agent.h"
#include "../../../src/commands_tool.h"
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../../src/json_allocator.h"
#include "../../../src/paths.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/tool_registry.h"
#include "../../test_utils.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;
static ik_tool_registry_t *registry;

// Helper to create a simple schema document
static yyjson_doc *create_test_schema(const char *tool_name)
{
    char *json = talloc_asprintf(ctx, "{\"name\":\"%s\",\"description\":\"Test tool\"}", tool_name);
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(json, strlen(json), 0, &allocator, NULL);
    if (doc == NULL) {
        ck_abort_msg("Failed to parse test schema");
    }
    talloc_free(json);
    return doc;
}

// Helper to get all scrollback text
static const char *get_scrollback_text(ik_scrollback_t *sb)
{
    return sb->text_buffer;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment for paths
    test_paths_setup_env();

    // Create scrollback buffer
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create minimal config
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Create paths
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));
    ck_assert_ptr_nonnull(paths);

    // Create tool registry
    registry = ik_tool_registry_create(ctx);
    ck_assert_ptr_nonnull(registry);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->paths = paths;
    shared->tool_registry = registry;

    // Create REPL context
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;

    repl->current = agent;
    repl->shared = shared;
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: /tool with no tools shows empty message
START_TEST(test_tool_no_tools) {
    res_t res = ik_cmd_tool(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    const char *text = get_scrollback_text(repl->current->scrollback);
    ck_assert_str_eq(text, "No tools available\n");
}

END_TEST

// Test: /tool lists all available tools
START_TEST(test_tool_list_all) {
    yyjson_doc *schema1 = create_test_schema("bash");
    yyjson_doc *schema2 = create_test_schema("grep");

    ik_tool_registry_add(registry, "bash", "/usr/bin/bash", schema1);
    ik_tool_registry_add(registry, "grep", "/usr/bin/grep", schema2);

    res_t res = ik_cmd_tool(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    const char *text = get_scrollback_text(repl->current->scrollback);
    ck_assert_ptr_nonnull(strstr(text, "Available tools:"));
    ck_assert_ptr_nonnull(strstr(text, "bash (/usr/bin/bash)"));
    ck_assert_ptr_nonnull(strstr(text, "grep (/usr/bin/grep)"));
}

END_TEST

// Test: /tool with whitespace-only args lists all tools
START_TEST(test_tool_whitespace_args) {
    yyjson_doc *schema = create_test_schema("bash");
    ik_tool_registry_add(registry, "bash", "/usr/bin/bash", schema);

    res_t res = ik_cmd_tool(ctx, repl, "   \t  ");
    ck_assert(is_ok(&res));

    const char *text = get_scrollback_text(repl->current->scrollback);
    ck_assert_ptr_nonnull(strstr(text, "Available tools:"));
}

END_TEST

// Test: /tool with specific tool shows schema
START_TEST(test_tool_show_schema) {
    yyjson_doc *schema = create_test_schema("bash");
    ik_tool_registry_add(registry, "bash", "/usr/bin/bash", schema);

    res_t res = ik_cmd_tool(ctx, repl, "bash");
    ck_assert(is_ok(&res));

    const char *text = get_scrollback_text(repl->current->scrollback);
    ck_assert_ptr_nonnull(strstr(text, "Tool: bash"));
    ck_assert_ptr_nonnull(strstr(text, "Path: /usr/bin/bash"));
    ck_assert_ptr_nonnull(strstr(text, "Schema:"));
}

END_TEST

// Test: /tool with leading whitespace in tool name
START_TEST(test_tool_show_schema_whitespace) {
    yyjson_doc *schema = create_test_schema("bash");
    ik_tool_registry_add(registry, "bash", "/usr/bin/bash", schema);

    res_t res = ik_cmd_tool(ctx, repl, "  \t bash");
    ck_assert(is_ok(&res));

    const char *text = get_scrollback_text(repl->current->scrollback);
    ck_assert_ptr_nonnull(strstr(text, "Tool: bash"));
}

END_TEST

// Test: /tool with non-existent tool
START_TEST(test_tool_not_found) {
    res_t res = ik_cmd_tool(ctx, repl, "nonexistent");
    ck_assert(is_ok(&res));

    const char *text = get_scrollback_text(repl->current->scrollback);
    ck_assert_ptr_nonnull(strstr(text, "Tool not found: nonexistent"));
}

END_TEST

// Test: /refresh clears and reloads registry
START_TEST(test_refresh_clears_registry) {
    yyjson_doc *schema = create_test_schema("bash");
    ik_tool_registry_add(registry, "bash", "/usr/bin/bash", schema);
    ck_assert_uint_eq(registry->count, 1);

    res_t res = ik_cmd_refresh(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    const char *text = get_scrollback_text(repl->current->scrollback);
    ck_assert_ptr_nonnull(strstr(text, "Tool registry refreshed"));
}

END_TEST

// Test: /refresh with args (args are ignored)
START_TEST(test_refresh_with_args) {
    res_t res = ik_cmd_refresh(ctx, repl, "ignored args");
    ck_assert(is_ok(&res));

    const char *text = get_scrollback_text(repl->current->scrollback);
    ck_assert_ptr_nonnull(strstr(text, "Tool registry refreshed"));
}

END_TEST

static Suite *commands_tool_suite(void)
{
    Suite *s = suite_create("Commands/Tool");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_tool_no_tools);
    tcase_add_test(tc, test_tool_list_all);
    tcase_add_test(tc, test_tool_whitespace_args);
    tcase_add_test(tc, test_tool_show_schema);
    tcase_add_test(tc, test_tool_show_schema_whitespace);
    tcase_add_test(tc, test_tool_not_found);
    tcase_add_test(tc, test_refresh_clears_registry);
    tcase_add_test(tc, test_refresh_with_args);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = commands_tool_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
