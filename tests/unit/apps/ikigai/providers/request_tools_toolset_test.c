/**
 * @file request_tools_toolset_test.c
 * @brief Tests for toolset filtering in request_tools.c
 */

#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/agent.h"
#include "shared/error.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/tool_registry.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;
static ik_shared_ctx_t *shared_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/**
 * Test toolset_filter excluding tools (lines 155-164)
 * Registry has 2 tools, filter only allows 1, so 1 gets skipped
 */
START_TEST(test_toolset_filter_excludes_tool) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;
    agent->toolset_count = 1;
    agent->toolset_filter = talloc_array(agent, char *, 1);
    agent->toolset_filter[0] = talloc_strdup(agent, "allowed_tool");

    // Create registry with TWO tools, but filter only allows one
    ik_tool_registry_t *registry = talloc_zero(test_ctx, ik_tool_registry_t);
    registry->capacity = 2;
    registry->count = 2;
    registry->entries = talloc_array(registry, ik_tool_registry_entry_t, 2);

    // Tool 1: allowed
    const char *schema1_json = "{"
                               "\"name\":\"allowed_tool\","
                               "\"description\":\"This one is allowed\""
                               "}";
    yyjson_doc *schema1_doc = yyjson_read(schema1_json, strlen(schema1_json), 0);
    registry->entries[0].name = talloc_strdup(registry->entries, "allowed_tool");
    registry->entries[0].path = talloc_strdup(registry->entries, "/tmp/allowed");
    registry->entries[0].schema_doc = schema1_doc;
    registry->entries[0].schema_root = yyjson_doc_get_root(schema1_doc);

    // Tool 2: NOT in filter, should be excluded
    const char *schema2_json = "{"
                               "\"name\":\"excluded_tool\","
                               "\"description\":\"This one is excluded\""
                               "}";
    yyjson_doc *schema2_doc = yyjson_read(schema2_json, strlen(schema2_json), 0);
    registry->entries[1].name = talloc_strdup(registry->entries, "excluded_tool");
    registry->entries[1].path = talloc_strdup(registry->entries, "/tmp/excluded");
    registry->entries[1].schema_doc = schema2_doc;
    registry->entries[1].schema_root = yyjson_doc_get_root(schema2_doc);

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, registry, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);
    // Only 1 tool should be in the request (the allowed one)
    ck_assert_int_eq((int)req->tool_count, 1);
    ck_assert_str_eq(req->tools[0].name, "allowed_tool");

    yyjson_doc_free(schema1_doc);
    yyjson_doc_free(schema2_doc);
}
END_TEST

/**
 * Test toolset_filter with zero count (line 155 branch coverage)
 * toolset_filter is non-NULL but toolset_count is 0
 */
START_TEST(test_toolset_filter_zero_count) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;
    agent->toolset_count = 0;  // Zero count
    agent->toolset_filter = talloc_array(agent, char *, 1);  // Non-NULL but empty

    // Create registry with one tool
    ik_tool_registry_t *registry = talloc_zero(test_ctx, ik_tool_registry_t);
    registry->capacity = 1;
    registry->count = 1;
    registry->entries = talloc_array(registry, ik_tool_registry_entry_t, 1);

    const char *schema_json = "{"
                              "\"name\":\"some_tool\","
                              "\"description\":\"A tool\""
                              "}";
    yyjson_doc *schema_doc = yyjson_read(schema_json, strlen(schema_json), 0);
    registry->entries[0].name = talloc_strdup(registry->entries, "some_tool");
    registry->entries[0].path = talloc_strdup(registry->entries, "/tmp/some_tool");
    registry->entries[0].schema_doc = schema_doc;
    registry->entries[0].schema_root = yyjson_doc_get_root(schema_doc);

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, registry, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);
    // With toolset_count == 0, filter is inactive, so tool is included
    ck_assert_int_eq((int)req->tool_count, 1);

    yyjson_doc_free(schema_doc);
}
END_TEST

/**
 * Test NULL toolset_filter (line 159, 168 branch coverage)
 * toolset_filter is NULL, so all tools should be added
 */
START_TEST(test_toolset_filter_null) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;
    agent->toolset_count = 0;
    agent->toolset_filter = NULL;  // NULL filter

    // Create registry with one tool
    ik_tool_registry_t *registry = talloc_zero(test_ctx, ik_tool_registry_t);
    registry->capacity = 1;
    registry->count = 1;
    registry->entries = talloc_array(registry, ik_tool_registry_entry_t, 1);

    const char *schema_json = "{"
                              "\"name\":\"some_tool\","
                              "\"description\":\"A tool\""
                              "}";
    yyjson_doc *schema_doc = yyjson_read(schema_json, strlen(schema_json), 0);
    registry->entries[0].name = talloc_strdup(registry->entries, "some_tool");
    registry->entries[0].path = talloc_strdup(registry->entries, "/tmp/some_tool");
    registry->entries[0].schema_doc = schema_doc;
    registry->entries[0].schema_root = yyjson_doc_get_root(schema_doc);

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, registry, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);
    // With NULL filter, all tools are included
    ck_assert_int_eq((int)req->tool_count, 1);

    yyjson_doc_free(schema_doc);
}
END_TEST

static Suite *request_tools_toolset_suite(void)
{
    Suite *s = suite_create("Request Tools Toolset Filter");

    TCase *tc = tcase_create("Toolset Filtering");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_toolset_filter_excludes_tool);
    tcase_add_test(tc, test_toolset_filter_zero_count);
    tcase_add_test(tc, test_toolset_filter_null);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_tools_toolset_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/request_tools_toolset_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
