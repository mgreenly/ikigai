/**
 * @file equivalence_test.c
 * @brief OpenAI Native vs Shim Equivalence Validation
 *
 * This test suite validates that the native OpenAI provider (openai.c)
 * produces identical outputs to the shim adapter (shim.c) that wraps
 * the legacy code.
 *
 * The symbol conflict has been resolved by renaming the shim factory:
 * - ik_openai_shim_create() - Creates shim provider (wraps legacy code)
 * - ik_openai_create() - Creates native provider (new implementation)
 *
 * Both can now coexist in the same test executable.
 */

#include <check.h>
#include <stdlib.h>
#include <talloc.h>
#include "providers/openai/shim.h"
#include "providers/openai/openai.h"
#include "equivalence_fixtures.h"
#include "equivalence_compare.h"

/* ================================================================
 * Test Infrastructure (Stub)
 * ================================================================ */

/**
 * Create shim provider for testing
 *
 * Uses the shim adapter that wraps legacy OpenAI client code.
 */
static ik_provider_t *create_shim_provider(TALLOC_CTX *ctx, const char *api_key)
{
    ik_provider_t *provider = NULL;
    res_t res = ik_openai_shim_create(ctx, api_key, &provider);
    if (is_err(&res)) {
        talloc_free(res.err);
        return NULL;
    }
    return provider;
}

/**
 * Create native provider for testing
 *
 * Uses the native OpenAI implementation (openai.c).
 */
static ik_provider_t *create_native_provider(TALLOC_CTX *ctx, const char *api_key)
{
    ik_provider_t *provider = NULL;
    res_t res = ik_openai_create(ctx, api_key, &provider);
    if (is_err(&res)) {
        talloc_free(res.err);
        return NULL;
    }
    return provider;
}

/* ================================================================
 * Equivalence Tests (Stubs)
 * ================================================================ */

START_TEST(test_equivalence_simple_text)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    /* Check for skip environment variable */
    const char *skip = getenv("IK_SKIP_EQUIVALENCE_VALIDATION");
    if (skip != NULL && skip[0] == '1') {
        fprintf(stderr, "WARNING: Equivalence validation skipped via IK_SKIP_EQUIVALENCE_VALIDATION\n");
        talloc_free(ctx);
        return;
    }

    /* Create test request */
    ik_request_t *req = ik_test_fixture_simple_text(ctx);
    ck_assert_ptr_nonnull(req);

    /* Create providers */
    ik_provider_t *shim = create_shim_provider(ctx, "test-api-key");
    ik_provider_t *native = create_native_provider(ctx, "test-api-key");

    /* Both providers should now be created successfully */
    ck_assert_ptr_nonnull(shim);
    ck_assert_ptr_nonnull(native);

    /* TODO: Once mock server infrastructure is available:
     * 1. Send request through both providers with same mock server
     * 2. Collect responses
     * 3. Compare using ik_compare_responses()
     * 4. Assert equivalence
     */

    talloc_free(ctx);
}
END_TEST

START_TEST(test_equivalence_tool_call)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *skip = getenv("IK_SKIP_EQUIVALENCE_VALIDATION");
    if (skip != NULL && skip[0] == '1') {
        fprintf(stderr, "WARNING: Equivalence validation skipped\n");
        talloc_free(ctx);
        return;
    }

    /* Stub: Blocked on native provider availability */
    ik_request_t *req = ik_test_fixture_tool_call(ctx);
    ck_assert_ptr_nonnull(req);

    /* TODO: Implement once native provider is available */

    talloc_free(ctx);
}
END_TEST

START_TEST(test_equivalence_multi_turn)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *skip = getenv("IK_SKIP_EQUIVALENCE_VALIDATION");
    if (skip != NULL && skip[0] == '1') {
        fprintf(stderr, "WARNING: Equivalence validation skipped\n");
        talloc_free(ctx);
        return;
    }

    /* Stub: Blocked on native provider availability */
    ik_request_t *req = ik_test_fixture_multi_turn(ctx);
    ck_assert_ptr_nonnull(req);

    /* TODO: Implement once native provider is available */

    talloc_free(ctx);
}
END_TEST

START_TEST(test_equivalence_streaming_text)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *skip = getenv("IK_SKIP_EQUIVALENCE_VALIDATION");
    if (skip != NULL && skip[0] == '1') {
        fprintf(stderr, "WARNING: Equivalence validation skipped\n");
        talloc_free(ctx);
        return;
    }

    /* Stub: Blocked on native provider availability */
    /* Also blocked on streaming implementation in native provider */

    talloc_free(ctx);
}
END_TEST

START_TEST(test_equivalence_streaming_tool_call)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *skip = getenv("IK_SKIP_EQUIVALENCE_VALIDATION");
    if (skip != NULL && skip[0] == '1') {
        fprintf(stderr, "WARNING: Equivalence validation skipped\n");
        talloc_free(ctx);
        return;
    }

    /* Stub: Blocked on native provider availability */

    talloc_free(ctx);
}
END_TEST

START_TEST(test_equivalence_error_handling)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *skip = getenv("IK_SKIP_EQUIVALENCE_VALIDATION");
    if (skip != NULL && skip[0] == '1') {
        fprintf(stderr, "WARNING: Equivalence validation skipped\n");
        talloc_free(ctx);
        return;
    }

    /* Stub: Blocked on native provider availability */
    ik_request_t *req = ik_test_fixture_invalid_model(ctx);
    ck_assert_ptr_nonnull(req);

    /* TODO: Implement once native provider is available */

    talloc_free(ctx);
}
END_TEST

START_TEST(test_equivalence_token_usage)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *skip = getenv("IK_SKIP_EQUIVALENCE_VALIDATION");
    if (skip != NULL && skip[0] == '1') {
        fprintf(stderr, "WARNING: Equivalence validation skipped\n");
        talloc_free(ctx);
        return;
    }

    /* Stub: Blocked on native provider availability */

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite Definition
 * ================================================================ */

static Suite *equivalence_suite(void)
{
    Suite *s = suite_create("OpenAI Equivalence");

    TCase *tc_simple = tcase_create("Simple Text");
    tcase_add_test(tc_simple, test_equivalence_simple_text);
    suite_add_tcase(s, tc_simple);

    TCase *tc_tool = tcase_create("Tool Call");
    tcase_add_test(tc_tool, test_equivalence_tool_call);
    suite_add_tcase(s, tc_tool);

    TCase *tc_multi = tcase_create("Multi Turn");
    tcase_add_test(tc_multi, test_equivalence_multi_turn);
    suite_add_tcase(s, tc_multi);

    TCase *tc_stream_text = tcase_create("Streaming Text");
    tcase_add_test(tc_stream_text, test_equivalence_streaming_text);
    suite_add_tcase(s, tc_stream_text);

    TCase *tc_stream_tool = tcase_create("Streaming Tool");
    tcase_add_test(tc_stream_tool, test_equivalence_streaming_tool_call);
    suite_add_tcase(s, tc_stream_tool);

    TCase *tc_error = tcase_create("Error Handling");
    tcase_add_test(tc_error, test_equivalence_error_handling);
    suite_add_tcase(s, tc_error);

    TCase *tc_usage = tcase_create("Token Usage");
    tcase_add_test(tc_usage, test_equivalence_token_usage);
    suite_add_tcase(s, tc_usage);

    return s;
}

int main(void)
{
    Suite *s = equivalence_suite();
    SRunner *sr = srunner_create(s);

    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "OpenAI Equivalence Validation\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "STATUS: Symbol conflict resolved\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "The shim factory has been renamed to\n");
    fprintf(stderr, "ik_openai_shim_create() to avoid conflict\n");
    fprintf(stderr, "with the native ik_openai_create().\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Both providers can now coexist in the\n");
    fprintf(stderr, "same test executable.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "REMAINING WORK:\n");
    fprintf(stderr, "1. Implement mock server infrastructure\n");
    fprintf(stderr, "2. Complete full equivalence tests\n");
    fprintf(stderr, "3. Validate all scenarios before cleanup\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "\n");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
