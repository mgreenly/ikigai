/**
 * @file anthropic_response_helpers_content_test.c
 * @brief Unit tests for Anthropic content block parsing helper functions
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/response_helpers.h"
#include "providers/provider.h"
#include "vendor/yyjson/yyjson.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Content Block Parsing Tests
 * ================================================================ */

START_TEST(test_parse_content_blocks_empty_array)
{
    const char *json = "[]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(!is_err(&r));
    ck_assert_ptr_null(blocks);
    ck_assert_uint_eq(count, 0);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_text)
{
    const char *json = "[{\"type\": \"text\", \"text\": \"Hello world\"}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(blocks);
    ck_assert_uint_eq(count, 1);
    ck_assert_int_eq(blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(blocks[0].data.text.text, "Hello world");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_missing_type)
{
    const char *json = "[{\"text\": \"Hello\"}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(is_err(&r));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_type_not_string)
{
    const char *json = "[{\"type\": 123, \"text\": \"Hello\"}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(is_err(&r));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_text_missing_field)
{
    const char *json = "[{\"type\": \"text\"}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(is_err(&r));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_text_not_string)
{
    const char *json = "[{\"type\": \"text\", \"text\": 123}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(is_err(&r));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_thinking)
{
    const char *json = "[{\"type\": \"thinking\", \"thinking\": \"Let me think...\"}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(blocks);
    ck_assert_uint_eq(count, 1);
    ck_assert_int_eq(blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(blocks[0].data.thinking.text, "Let me think...");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_thinking_missing_field)
{
    const char *json = "[{\"type\": \"thinking\"}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(is_err(&r));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_thinking_not_string)
{
    const char *json = "[{\"type\": \"thinking\", \"thinking\": 456}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(is_err(&r));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_redacted_thinking)
{
    const char *json = "[{\"type\": \"redacted_thinking\"}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(blocks);
    ck_assert_uint_eq(count, 1);
    ck_assert_int_eq(blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(blocks[0].data.thinking.text, "[thinking redacted]");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_tool_use)
{
    const char *json = "[{\"type\": \"tool_use\", \"id\": \"call_123\", \"name\": \"get_weather\", \"input\": {\"location\": \"NYC\"}}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(blocks);
    ck_assert_uint_eq(count, 1);
    ck_assert_int_eq(blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(blocks[0].data.tool_call.id, "call_123");
    ck_assert_str_eq(blocks[0].data.tool_call.name, "get_weather");
    ck_assert_ptr_nonnull(blocks[0].data.tool_call.arguments);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_tool_use_missing_id)
{
    const char *json = "[{\"type\": \"tool_use\", \"name\": \"get_weather\", \"input\": {}}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(is_err(&r));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_tool_use_id_not_string)
{
    const char *json = "[{\"type\": \"tool_use\", \"id\": 123, \"name\": \"get_weather\", \"input\": {}}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(is_err(&r));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_tool_use_missing_name)
{
    const char *json = "[{\"type\": \"tool_use\", \"id\": \"call_123\", \"input\": {}}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(is_err(&r));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_tool_use_name_not_string)
{
    const char *json = "[{\"type\": \"tool_use\", \"id\": \"call_123\", \"name\": 456, \"input\": {}}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(is_err(&r));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_tool_use_missing_input)
{
    const char *json = "[{\"type\": \"tool_use\", \"id\": \"call_123\", \"name\": \"get_weather\"}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(is_err(&r));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_unknown_type)
{
    const char *json = "[{\"type\": \"unknown_type\"}]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(blocks);
    ck_assert_uint_eq(count, 1);
    ck_assert_int_eq(blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(blocks[0].data.text.text, "[unknown content type: unknown_type]");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_parse_content_blocks_multiple_types)
{
    const char *json = "["
        "{\"type\": \"text\", \"text\": \"Hello\"},"
        "{\"type\": \"thinking\", \"thinking\": \"Hmm...\"},"
        "{\"type\": \"tool_use\", \"id\": \"call_1\", \"name\": \"func\", \"input\": {}},"
        "{\"type\": \"redacted_thinking\"}"
    "]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ik_content_block_t *blocks = NULL;
    size_t count = 0;

    res_t r = ik_anthropic_parse_content_blocks(test_ctx, root, &blocks, &count);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(blocks);
    ck_assert_uint_eq(count, 4);

    ck_assert_int_eq(blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(blocks[0].data.text.text, "Hello");

    ck_assert_int_eq(blocks[1].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(blocks[1].data.thinking.text, "Hmm...");

    ck_assert_int_eq(blocks[2].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(blocks[2].data.tool_call.id, "call_1");

    ck_assert_int_eq(blocks[3].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(blocks[3].data.thinking.text, "[thinking redacted]");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_response_helpers_content_suite(void)
{
    Suite *s = suite_create("Anthropic Response Helpers - Content");

    TCase *tc_content = tcase_create("Content Blocks");
    tcase_add_unchecked_fixture(tc_content, setup, teardown);
    tcase_add_test(tc_content, test_parse_content_blocks_empty_array);
    tcase_add_test(tc_content, test_parse_content_blocks_text);
    tcase_add_test(tc_content, test_parse_content_blocks_missing_type);
    tcase_add_test(tc_content, test_parse_content_blocks_type_not_string);
    tcase_add_test(tc_content, test_parse_content_blocks_text_missing_field);
    tcase_add_test(tc_content, test_parse_content_blocks_text_not_string);
    tcase_add_test(tc_content, test_parse_content_blocks_thinking);
    tcase_add_test(tc_content, test_parse_content_blocks_thinking_missing_field);
    tcase_add_test(tc_content, test_parse_content_blocks_thinking_not_string);
    tcase_add_test(tc_content, test_parse_content_blocks_redacted_thinking);
    tcase_add_test(tc_content, test_parse_content_blocks_tool_use);
    tcase_add_test(tc_content, test_parse_content_blocks_tool_use_missing_id);
    tcase_add_test(tc_content, test_parse_content_blocks_tool_use_id_not_string);
    tcase_add_test(tc_content, test_parse_content_blocks_tool_use_missing_name);
    tcase_add_test(tc_content, test_parse_content_blocks_tool_use_name_not_string);
    tcase_add_test(tc_content, test_parse_content_blocks_tool_use_missing_input);
    tcase_add_test(tc_content, test_parse_content_blocks_unknown_type);
    tcase_add_test(tc_content, test_parse_content_blocks_multiple_types);
    suite_add_tcase(s, tc_content);

    return s;
}

int main(void)
{
    Suite *s = anthropic_response_helpers_content_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
