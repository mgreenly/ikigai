/**
 * @file request_tools_schema_test.c
 * @brief Tests for tool schema building in request_tools.c (lines 92-107)
 */

#include "../../../src/providers/request.h"
#include "../../../src/agent.h"
#include "../../../src/error.h"
#include "../../../src/shared.h"
#include "../../../src/tool_registry.h"
#include "../../test_utils_helper.h"

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
 * Test build_from_conversation with internal tools removed (rel-08)
 * After tool system removal, requests should have no tools (empty array)
 */
START_TEST(test_build_tool_parameters_json_via_conversation) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);

    // After internal tool removal, no tools should be present
    ck_assert_int_eq((int)req->tool_count, 0);
}
END_TEST

/**
 * Test build_from_conversation with tool registry populated
 */
START_TEST(test_build_from_conversation_with_registry) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;
    agent->toolset_count = 1;
    agent->toolset_filter = talloc_array(agent, char *, 1);
    agent->toolset_filter[0] = talloc_strdup(agent, "test_tool");

    // Create mock registry with test tool
    ik_tool_registry_t *registry = talloc_zero(test_ctx, ik_tool_registry_t);
    registry->capacity = 2;
    registry->count = 1;
    registry->entries = talloc_array(registry, ik_tool_registry_entry_t, 2);

    // Create schema JSON for a test tool
    const char *schema_json = "{"
                              "\"name\":\"test_tool\","
                              "\"description\":\"A test tool\","
                              "\"parameters\":{"
                              "\"type\":\"object\","
                              "\"properties\":{"
                              "\"arg1\":{\"type\":\"string\"}"
                              "}"
                              "}"
                              "}";

    yyjson_doc *schema_doc = yyjson_read(schema_json, strlen(schema_json), 0);
    ck_assert_ptr_nonnull(schema_doc);

    registry->entries[0].name = talloc_strdup(registry->entries, "test_tool");
    registry->entries[0].path = talloc_strdup(registry->entries, "/tmp/test_tool");
    registry->entries[0].schema_doc = schema_doc;
    registry->entries[0].schema_root = yyjson_doc_get_root(schema_doc);

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, registry, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);
    ck_assert_int_eq((int)req->tool_count, 1);
    ck_assert_str_eq(req->tools[0].name, "test_tool");
    ck_assert_str_eq(req->tools[0].description, "A test tool");

    yyjson_doc_free(schema_doc);
}
END_TEST

/**
 * Test build_from_conversation with registry tool missing description
 */
START_TEST(test_build_from_conversation_registry_no_description) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;
    agent->toolset_count = 1;
    agent->toolset_filter = talloc_array(agent, char *, 1);
    agent->toolset_filter[0] = talloc_strdup(agent, "tool2");

    // Create mock registry with test tool (no description field)
    ik_tool_registry_t *registry = talloc_zero(test_ctx, ik_tool_registry_t);
    registry->capacity = 2;
    registry->count = 1;
    registry->entries = talloc_array(registry, ik_tool_registry_entry_t, 2);

    // Schema with no description field
    const char *schema_json = "{"
                              "\"name\":\"tool2\","
                              "\"parameters\":{"
                              "\"type\":\"object\""
                              "}"
                              "}";

    yyjson_doc *schema_doc = yyjson_read(schema_json, strlen(schema_json), 0);
    ck_assert_ptr_nonnull(schema_doc);

    registry->entries[0].name = talloc_strdup(registry->entries, "tool2");
    registry->entries[0].path = talloc_strdup(registry->entries, "/tmp/tool2");
    registry->entries[0].schema_doc = schema_doc;
    registry->entries[0].schema_root = yyjson_doc_get_root(schema_doc);

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, registry, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);
    ck_assert_int_eq((int)req->tool_count, 1);
    ck_assert_str_eq(req->tools[0].name, "tool2");
    ck_assert_str_eq(req->tools[0].description, "");

    yyjson_doc_free(schema_doc);
}
END_TEST

/**
 * Test build_from_conversation with registry tool missing parameters
 */
START_TEST(test_build_from_conversation_registry_no_parameters) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;
    agent->toolset_count = 1;
    agent->toolset_filter = talloc_array(agent, char *, 1);
    agent->toolset_filter[0] = talloc_strdup(agent, "tool3");

    // Create mock registry with test tool (no parameters field)
    ik_tool_registry_t *registry = talloc_zero(test_ctx, ik_tool_registry_t);
    registry->capacity = 2;
    registry->count = 1;
    registry->entries = talloc_array(registry, ik_tool_registry_entry_t, 2);

    // Schema with no parameters field
    const char *schema_json = "{"
                              "\"name\":\"tool3\","
                              "\"description\":\"Another test tool\""
                              "}";

    yyjson_doc *schema_doc = yyjson_read(schema_json, strlen(schema_json), 0);
    ck_assert_ptr_nonnull(schema_doc);

    registry->entries[0].name = talloc_strdup(registry->entries, "tool3");
    registry->entries[0].path = talloc_strdup(registry->entries, "/tmp/tool3");
    registry->entries[0].schema_doc = schema_doc;
    registry->entries[0].schema_root = yyjson_doc_get_root(schema_doc);

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, registry, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);
    ck_assert_int_eq((int)req->tool_count, 1);
    ck_assert_str_eq(req->tools[0].name, "tool3");
    ck_assert_str_eq(req->tools[0].description, "Another test tool");

    yyjson_doc_free(schema_doc);
}
END_TEST

/**
 * Test build_from_conversation with registry but no tools
 */
START_TEST(test_build_from_conversation_registry_empty) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    // Create empty registry
    ik_tool_registry_t *registry = talloc_zero(test_ctx, ik_tool_registry_t);
    registry->capacity = 0;
    registry->count = 0;
    registry->entries = NULL;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, registry, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);
    ck_assert_int_eq((int)req->tool_count, 0);
}
END_TEST

/**
 * Test build_from_conversation with multiple tools in registry
 */
START_TEST(test_build_from_conversation_registry_multiple_tools) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;
    agent->toolset_count = 2;
    agent->toolset_filter = talloc_array(agent, char *, 2);
    agent->toolset_filter[0] = talloc_strdup(agent, "tool_a");
    agent->toolset_filter[1] = talloc_strdup(agent, "tool_b");

    // Create registry with two tools
    ik_tool_registry_t *registry = talloc_zero(test_ctx, ik_tool_registry_t);
    registry->capacity = 2;
    registry->count = 2;
    registry->entries = talloc_array(registry, ik_tool_registry_entry_t, 2);

    // First tool
    const char *schema1_json = "{"
                               "\"name\":\"tool_a\","
                               "\"description\":\"First tool\","
                               "\"parameters\":{\"type\":\"object\"}"
                               "}";
    yyjson_doc *schema1_doc = yyjson_read(schema1_json, strlen(schema1_json), 0);
    registry->entries[0].name = talloc_strdup(registry->entries, "tool_a");
    registry->entries[0].path = talloc_strdup(registry->entries, "/tmp/tool_a");
    registry->entries[0].schema_doc = schema1_doc;
    registry->entries[0].schema_root = yyjson_doc_get_root(schema1_doc);

    // Second tool
    const char *schema2_json = "{"
                               "\"name\":\"tool_b\","
                               "\"description\":\"Second tool\""
                               "}";
    yyjson_doc *schema2_doc = yyjson_read(schema2_json, strlen(schema2_json), 0);
    registry->entries[1].name = talloc_strdup(registry->entries, "tool_b");
    registry->entries[1].path = talloc_strdup(registry->entries, "/tmp/tool_b");
    registry->entries[1].schema_doc = schema2_doc;
    registry->entries[1].schema_root = yyjson_doc_get_root(schema2_doc);

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, registry, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);
    ck_assert_int_eq((int)req->tool_count, 2);
    ck_assert_str_eq(req->tools[0].name, "tool_a");
    ck_assert_str_eq(req->tools[0].description, "First tool");
    ck_assert_str_eq(req->tools[1].name, "tool_b");
    ck_assert_str_eq(req->tools[1].description, "Second tool");

    yyjson_doc_free(schema1_doc);
    yyjson_doc_free(schema2_doc);
}
END_TEST

/**
 * Test with null description and null parameters JSON values
 */
START_TEST(test_build_from_conversation_null_values) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;
    agent->toolset_count = 1;
    agent->toolset_filter = talloc_array(agent, char *, 1);
    agent->toolset_filter[0] = talloc_strdup(agent, "test_null");

    // Create registry with tool having null description
    ik_tool_registry_t *registry = talloc_zero(test_ctx, ik_tool_registry_t);
    registry->capacity = 1;
    registry->count = 1;
    registry->entries = talloc_array(registry, ik_tool_registry_entry_t, 1);

    // Schema with null description value
    const char *schema_json = "{"
                              "\"name\":\"test_null\","
                              "\"description\":null,"
                              "\"parameters\":null"
                              "}";

    yyjson_doc *schema_doc = yyjson_read(schema_json, strlen(schema_json), 0);
    ck_assert_ptr_nonnull(schema_doc);

    registry->entries[0].name = talloc_strdup(registry->entries, "test_null");
    registry->entries[0].path = talloc_strdup(registry->entries, "/tmp/test_null");
    registry->entries[0].schema_doc = schema_doc;
    registry->entries[0].schema_root = yyjson_doc_get_root(schema_doc);

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, registry, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);
    ck_assert_int_eq((int)req->tool_count, 1);

    yyjson_doc_free(schema_doc);
}
END_TEST

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

static Suite *request_tools_schema_suite(void)
{
    Suite *s = suite_create("Request Tools Schema");

    TCase *tc = tcase_create("Tool Schema Building");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_build_tool_parameters_json_via_conversation);
    tcase_add_test(tc, test_build_from_conversation_with_registry);
    tcase_add_test(tc, test_build_from_conversation_registry_no_description);
    tcase_add_test(tc, test_build_from_conversation_registry_no_parameters);
    tcase_add_test(tc, test_build_from_conversation_registry_empty);
    tcase_add_test(tc, test_build_from_conversation_registry_multiple_tools);
    tcase_add_test(tc, test_build_from_conversation_null_values);
    tcase_add_test(tc, test_toolset_filter_excludes_tool);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_tools_schema_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/request_tools_schema_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
