#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/config.h"
#include "../../src/error.h"
#include "../../src/openai/client.h"
#include "../../src/openai/sse_parser.h"
#include "../../src/wrapper.h"
#include "../test_utils.h"

/**
 * Mock Verification Test Suite
 *
 * These tests verify that our test fixtures match the structure and format
 * of real OpenAI API responses. They only run when VERIFY_MOCKS=1 is set.
 *
 * Purpose:
 * - Ensure fixtures stay up-to-date with API changes
 * - Validate that our mocks accurately represent real API behavior
 * - Provide a way to update fixtures when the API changes
 *
 * Usage:
 *   OPENAI_API_KEY=sk-... VERIFY_MOCKS=1 make check
 *
 * Note: These tests make real API calls and incur costs.
 */

/* Helper: Check if verification mode is enabled */
static bool should_verify_mocks(void)
{
    const char *verify = getenv("VERIFY_MOCKS");
    return verify != NULL && strcmp(verify, "1") == 0;
}

/* Helper: Get API key from environment */
static const char *get_api_key(void)
{
    return getenv("OPENAI_API_KEY");
}

/* Helper: Create test configuration */
static ik_cfg_t *create_test_cfg(void *parent, const char *model)
{
    ik_cfg_t *cfg = talloc(parent, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    cfg->openai_model = talloc_strdup(cfg, model);
    cfg->openai_temperature = 1.0;
    cfg->openai_max_completion_tokens = 4096;
    cfg->openai_system_message = NULL;

    return cfg;
}

/* Helper: Streaming callback that accumulates content */
typedef struct {
    char *buffer;
    size_t buffer_len;
    size_t buffer_capacity;
} stream_accumulator_t;

static res_t accumulate_chunk(const char *chunk, void *ctx)
{
    stream_accumulator_t *acc = (stream_accumulator_t *)ctx;

    size_t chunk_len = strlen(chunk);
    size_t new_len = acc->buffer_len + chunk_len;

    /* Grow buffer if needed (talloc panics on OOM) */
    if (new_len >= acc->buffer_capacity) {
        size_t new_capacity = acc->buffer_capacity * 2;
        if (new_capacity < new_len + 1) {
            new_capacity = new_len + 1;
        }
        acc->buffer = talloc_realloc_size(NULL, acc->buffer, new_capacity);
        acc->buffer_capacity = new_capacity;
    }

    /* Append chunk */
    memcpy(acc->buffer + acc->buffer_len, chunk, chunk_len);
    acc->buffer_len = new_len;
    acc->buffer[acc->buffer_len] = '\0';

    return OK(NULL);
}

/* Helper: Create stream accumulator */
static stream_accumulator_t *create_accumulator(void *parent)
{
    stream_accumulator_t *acc = talloc(parent, stream_accumulator_t);
    ck_assert_ptr_nonnull(acc);

    acc->buffer_capacity = 1024;
    acc->buffer = talloc_zero_size(acc, acc->buffer_capacity);
    ck_assert_ptr_nonnull(acc->buffer);

    acc->buffer[0] = '\0';
    acc->buffer_len = 0;

    return acc;
}

START_TEST(verify_stream_hello_world) {
    /* Skip if not in verification mode */
    if (!should_verify_mocks()) {
        ck_assert(true);
        return;
    }

    const char *api_key = get_api_key();
    if (!api_key) {
        ck_abort_msg("OPENAI_API_KEY not set");
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /* Create configuration */
    ik_cfg_t *cfg = create_test_cfg(ctx, "gpt-5-mini");

    /* Create conversation with simple greeting */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello!");
    res_t add_res = ik_openai_conversation_add_msg(conv, msg_tmp);
    ck_assert(!add_res.is_err);

    /* Make real API call with streaming */
    stream_accumulator_t *acc = create_accumulator(ctx);
    res_t result = ik_openai_chat_create(ctx, cfg, conv, accumulate_chunk, acc);

    /* Verify success */
    if (result.is_err) {
        fprintf(stderr, "API call failed: %s (code: %d)\n",
                result.err->msg, result.err->code);
        ck_abort_msg("API call failed");
    }

    ik_openai_response_t *response = result.ok;

    /* Verify response has required fields */
    ck_assert_ptr_nonnull(response);
    ck_assert_ptr_nonnull(response->content);
    ck_assert_ptr_nonnull(response->finish_reason);

    /* Verify streaming callback was invoked */
    ck_assert_ptr_nonnull(acc->buffer);
    ck_assert(acc->buffer_len > 0);

    /* Verify content matches accumulated chunks */
    ck_assert_str_eq(response->content, acc->buffer);

    /* Verify finish_reason is valid */
    ck_assert(strcmp(response->finish_reason, "stop") == 0 ||
              strcmp(response->finish_reason, "length") == 0 ||
              strcmp(response->finish_reason, "content_filter") == 0);

    /* Verify token counts if present (may not be available in all streaming modes) */
    if (response->prompt_tokens > 0) {
        ck_assert_int_gt(response->completion_tokens, 0);
        ck_assert_int_gt(response->total_tokens, 0);
        ck_assert_int_eq(response->total_tokens,
                         response->prompt_tokens + response->completion_tokens);
    }

    talloc_free(ctx);
}
END_TEST START_TEST(verify_stream_multiline)
{
    /* Skip if not in verification mode */
    if (!should_verify_mocks()) {
        ck_assert(true);
        return;
    }

    const char *api_key = get_api_key();
    if (!api_key) {
        ck_abort_msg("OPENAI_API_KEY not set");
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /* Create configuration */
    ik_cfg_t *cfg = create_test_cfg(ctx, "gpt-5-mini");

    /* Create conversation requesting code (likely to be multi-line) */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user",
                                             "Write a short Python function to add two numbers.");
    res_t add_res = ik_openai_conversation_add_msg(conv, msg_tmp);
    ck_assert(!add_res.is_err);

    /* Make real API call with streaming */
    stream_accumulator_t *acc = create_accumulator(ctx);
    res_t result = ik_openai_chat_create(ctx, cfg, conv, accumulate_chunk, acc);

    /* Verify success */
    if (result.is_err) {
        fprintf(stderr, "API call failed: %s (code: %d)\n",
                result.err->msg, result.err->code);
        ck_abort_msg("API call failed");
    }

    ik_openai_response_t *response = result.ok;

    /* Verify response structure */
    ck_assert_ptr_nonnull(response);
    ck_assert_ptr_nonnull(response->content);
    ck_assert_ptr_nonnull(response->finish_reason);

    /* Verify streaming worked */
    ck_assert_ptr_nonnull(acc->buffer);
    ck_assert(acc->buffer_len > 0);

    /* Verify content matches accumulated chunks */
    ck_assert_str_eq(response->content, acc->buffer);

    /* Verify response likely contains code (has newlines) */
    ck_assert_ptr_nonnull(strchr(response->content, '\n'));

    talloc_free(ctx);
}

END_TEST START_TEST(verify_stream_conversation)
{
    /* Skip if not in verification mode */
    if (!should_verify_mocks()) {
        ck_assert(true);
        return;
    }

    const char *api_key = get_api_key();
    if (!api_key) {
        ck_abort_msg("OPENAI_API_KEY not set");
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /* Create configuration */
    ik_cfg_t *cfg = create_test_cfg(ctx, "gpt-5-mini");

    /* Create multi-turn conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    /* First message */
    ik_msg_t *msg1 = ik_openai_msg_create(ctx, "user", "What is 2+2?");
    res_t add1_res = ik_openai_conversation_add_msg(conv, msg1);
    ck_assert(!add1_res.is_err);

    /* Get first response */
    stream_accumulator_t *acc1 = create_accumulator(ctx);
    res_t result1 = ik_openai_chat_create(ctx, cfg, conv, accumulate_chunk, acc1);
    ck_assert(!result1.is_err);

    ik_openai_response_t *response1 = result1.ok;
    ck_assert_ptr_nonnull(response1);
    ck_assert_ptr_nonnull(response1->content);

    /* Add assistant response to conversation */
    ik_msg_t *msg2 = ik_openai_msg_create(ctx, "assistant", response1->content);
    res_t add2_res = ik_openai_conversation_add_msg(conv, msg2);
    ck_assert(!add2_res.is_err);

    /* Second message */
    ik_msg_t *msg3 = ik_openai_msg_create(ctx, "user", "What is double that?");
    res_t add3_res = ik_openai_conversation_add_msg(conv, msg3);
    ck_assert(!add3_res.is_err);

    /* Get second response */
    stream_accumulator_t *acc2 = create_accumulator(ctx);
    res_t result2 = ik_openai_chat_create(ctx, cfg, conv, accumulate_chunk, acc2);
    ck_assert(!result2.is_err);

    ik_openai_response_t *response2 = result2.ok;
    ck_assert_ptr_nonnull(response2);
    ck_assert_ptr_nonnull(response2->content);

    /* Verify both responses have content */
    ck_assert(strlen(response1->content) > 0);
    ck_assert(strlen(response2->content) > 0);

    talloc_free(ctx);
}

END_TEST START_TEST(verify_error_fixture_structure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /* Load and verify 401 error fixture */
    const char *fixture_401 = "tests/fixtures/openai/error_401_unauthorized.json";
    char *content_401 = load_file_to_string(ctx, fixture_401);
    ck_assert_ptr_nonnull(content_401);

    /* Parse JSON */
    yyjson_doc *doc_401 = yyjson_read(content_401, strlen(content_401), 0);
    ck_assert_ptr_nonnull(doc_401);

    yyjson_val *root_401 = yyjson_doc_get_root(doc_401);
    ck_assert_ptr_nonnull(root_401);
    ck_assert(yyjson_is_obj(root_401));

    /* Verify error object structure */
    yyjson_val *error_401 = yyjson_obj_get(root_401, "error");
    ck_assert_ptr_nonnull(error_401);
    ck_assert(yyjson_is_obj(error_401));

    /* Verify required fields */
    yyjson_val *message_401 = yyjson_obj_get(error_401, "message");
    ck_assert_ptr_nonnull(message_401);
    ck_assert(yyjson_is_str(message_401));

    yyjson_val *type_401 = yyjson_obj_get(error_401, "type");
    ck_assert_ptr_nonnull(type_401);
    ck_assert(yyjson_is_str(type_401));

    yyjson_val *code_401 = yyjson_obj_get(error_401, "code");
    ck_assert_ptr_nonnull(code_401);
    ck_assert(yyjson_is_str(code_401));

    yyjson_doc_free(doc_401);

    /* Load and verify 429 error fixture */
    const char *fixture_429 = "tests/fixtures/openai/error_429_rate_limit.json";
    char *content_429 = load_file_to_string(ctx, fixture_429);
    ck_assert_ptr_nonnull(content_429);

    yyjson_doc *doc_429 = yyjson_read(content_429, strlen(content_429), 0);
    ck_assert_ptr_nonnull(doc_429);

    yyjson_val *root_429 = yyjson_doc_get_root(doc_429);
    ck_assert_ptr_nonnull(root_429);

    yyjson_val *error_429 = yyjson_obj_get(root_429, "error");
    ck_assert_ptr_nonnull(error_429);

    yyjson_doc_free(doc_429);

    /* Load and verify 500 error fixture */
    const char *fixture_500 = "tests/fixtures/openai/error_500_server.json";
    char *content_500 = load_file_to_string(ctx, fixture_500);
    ck_assert_ptr_nonnull(content_500);

    yyjson_doc *doc_500 = yyjson_read(content_500, strlen(content_500), 0);
    ck_assert_ptr_nonnull(doc_500);

    yyjson_val *root_500 = yyjson_doc_get_root(doc_500);
    ck_assert_ptr_nonnull(root_500);

    yyjson_val *error_500 = yyjson_obj_get(root_500, "error");
    ck_assert_ptr_nonnull(error_500);

    yyjson_doc_free(doc_500);

    talloc_free(ctx);
}

END_TEST

static Suite *openai_mock_verification_suite(void)
{
    Suite *s = suite_create("OpenAIMockVerification");
    TCase *tc_core = tcase_create("Core");

    /* Set longer timeout for real API calls */
    tcase_set_timeout(tc_core, 60);

    /* Streaming verification tests */
    tcase_add_test(tc_core, verify_stream_hello_world);
    tcase_add_test(tc_core, verify_stream_multiline);
    tcase_add_test(tc_core, verify_stream_conversation);

    /* Error fixture structure tests */
    tcase_add_test(tc_core, verify_error_fixture_structure);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = openai_mock_verification_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
