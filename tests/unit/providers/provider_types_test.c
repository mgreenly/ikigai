#include <check.h>
#include <talloc.h>
#include "providers/provider.h"
#include "error.h"

/* ================================================================
 * Enum Values Verification
 * ================================================================ */

START_TEST(test_thinking_level_enum_values) {
    ck_assert_int_eq(IK_THINKING_NONE, 0);
    ck_assert_int_eq(IK_THINKING_LOW, 1);
    ck_assert_int_eq(IK_THINKING_MED, 2);
    ck_assert_int_eq(IK_THINKING_HIGH, 3);
}
END_TEST

START_TEST(test_finish_reason_enum_values) {
    ck_assert_int_eq(IK_FINISH_STOP, 0);
    ck_assert_int_eq(IK_FINISH_LENGTH, 1);
    ck_assert_int_eq(IK_FINISH_TOOL_USE, 2);
    ck_assert_int_eq(IK_FINISH_CONTENT_FILTER, 3);
    ck_assert_int_eq(IK_FINISH_ERROR, 4);
    ck_assert_int_eq(IK_FINISH_UNKNOWN, 5);
}

END_TEST

START_TEST(test_content_type_enum_values) {
    ck_assert_int_eq(IK_CONTENT_TEXT, 0);
    ck_assert_int_eq(IK_CONTENT_TOOL_CALL, 1);
    ck_assert_int_eq(IK_CONTENT_TOOL_RESULT, 2);
    ck_assert_int_eq(IK_CONTENT_THINKING, 3);
}

END_TEST

START_TEST(test_role_enum_values) {
    ck_assert_int_eq(IK_ROLE_USER, 0);
    ck_assert_int_eq(IK_ROLE_ASSISTANT, 1);
    ck_assert_int_eq(IK_ROLE_TOOL, 2);
}

END_TEST
// TEMPORARILY DISABLED during coexistence phase
// The IK_TOOL_* enum is commented out in provider.h to avoid conflict with openai/tool_choice.h
// This test will be re-enabled after old OpenAI code is removed
/*
   START_TEST(test_tool_choice_enum_values)
   {
    ck_assert_int_eq(IK_TOOL_AUTO, 0);
    ck_assert_int_eq(IK_TOOL_NONE, 1);
    ck_assert_int_eq(IK_TOOL_REQUIRED, 2);
    ck_assert_int_eq(IK_TOOL_SPECIFIC, 3);
   }
   END_TEST
 */

START_TEST(test_error_category_enum_values) {
    ck_assert_int_eq(IK_ERR_CAT_AUTH, 0);
    ck_assert_int_eq(IK_ERR_CAT_RATE_LIMIT, 1);
    ck_assert_int_eq(IK_ERR_CAT_INVALID_ARG, 2);
    ck_assert_int_eq(IK_ERR_CAT_NOT_FOUND, 3);
    ck_assert_int_eq(IK_ERR_CAT_SERVER, 4);
    ck_assert_int_eq(IK_ERR_CAT_TIMEOUT, 5);
    ck_assert_int_eq(IK_ERR_CAT_CONTENT_FILTER, 6);
    ck_assert_int_eq(IK_ERR_CAT_NETWORK, 7);
    ck_assert_int_eq(IK_ERR_CAT_UNKNOWN, 8);
}

END_TEST

START_TEST(test_stream_event_type_enum_values) {
    ck_assert_int_eq(IK_STREAM_START, 0);
    ck_assert_int_eq(IK_STREAM_TEXT_DELTA, 1);
    ck_assert_int_eq(IK_STREAM_THINKING_DELTA, 2);
    ck_assert_int_eq(IK_STREAM_TOOL_CALL_START, 3);
    ck_assert_int_eq(IK_STREAM_TOOL_CALL_DELTA, 4);
    ck_assert_int_eq(IK_STREAM_TOOL_CALL_DONE, 5);
    ck_assert_int_eq(IK_STREAM_DONE, 6);
    ck_assert_int_eq(IK_STREAM_ERROR, 7);
}

END_TEST
/* ================================================================
 * Struct Size Validation
 * ================================================================ */

START_TEST(test_struct_sizes) {
    // Verify structs have expected sizes (no unexpected padding)
    ck_assert(sizeof(ik_usage_t) > 0);
    ck_assert(sizeof(ik_thinking_config_t) > 0);
    ck_assert(sizeof(ik_content_block_t) > 0);
    ck_assert(sizeof(ik_message_t) > 0);
    ck_assert(sizeof(ik_tool_def_t) > 0);
    ck_assert(sizeof(ik_request_t) > 0);
    ck_assert(sizeof(ik_response_t) > 0);
    ck_assert(sizeof(ik_provider_error_t) > 0);
    ck_assert(sizeof(ik_stream_event_t) > 0);
    ck_assert(sizeof(ik_provider_completion_t) > 0);
    ck_assert(sizeof(ik_provider_vtable_t) > 0);
    ck_assert(sizeof(ik_provider_t) > 0);
}

END_TEST
/* ================================================================
 * Talloc Allocation
 * ================================================================ */

START_TEST(test_talloc_allocation_request) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    ck_assert_ptr_nonnull(req);

    // Verify zero initialization
    ck_assert_ptr_null(req->system_prompt);
    ck_assert_ptr_null(req->messages);
    ck_assert(req->message_count == 0);
    ck_assert_ptr_null(req->model);
    ck_assert_int_eq(req->thinking.level, IK_THINKING_NONE);
    ck_assert_int_eq(req->thinking.include_summary, 0);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_talloc_allocation_response) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_response_t *resp = talloc_zero(ctx, ik_response_t);
    ck_assert_ptr_nonnull(resp);

    // Verify zero initialization
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert(resp->content_count == 0);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
    ck_assert_int_eq(resp->usage.cached_tokens, 0);
    ck_assert_int_eq(resp->usage.total_tokens, 0);

    talloc_free(ctx);
}

END_TEST
/* ================================================================
 * Error Codes
 * ================================================================ */

START_TEST(test_error_code_provider) {
    ck_assert_int_eq(ERR_PROVIDER, 9);
    ck_assert_str_eq(error_code_str(ERR_PROVIDER), "Provider error");
}

END_TEST

START_TEST(test_error_code_missing_credentials) {
    ck_assert_int_eq(ERR_MISSING_CREDENTIALS, 10);
    ck_assert_str_eq(error_code_str(ERR_MISSING_CREDENTIALS), "Missing credentials");
}

END_TEST

START_TEST(test_error_code_not_implemented) {
    ck_assert_int_eq(ERR_NOT_IMPLEMENTED, 11);
    ck_assert_str_eq(error_code_str(ERR_NOT_IMPLEMENTED), "Not implemented");
}

END_TEST

/* ================================================================
 * Callback Type Compatibility
 * ================================================================ */

// Test callback function signatures compile correctly
static res_t test_stream_callback(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

static res_t test_completion_callback(const ik_provider_completion_t *completion, void *ctx)
{
    (void)completion;
    (void)ctx;
    return OK(NULL);
}

START_TEST(test_callback_type_assignment) {
    ik_stream_cb_t stream_cb = test_stream_callback;
    ik_provider_completion_cb_t completion_cb = test_completion_callback;

    ck_assert_ptr_nonnull(stream_cb);
    ck_assert_ptr_nonnull(completion_cb);

    // Invoke callbacks to verify signatures match
    ik_stream_event_t event = {.type = IK_STREAM_START, .index = 0};
    res_t r1 = stream_cb(&event, NULL);
    ck_assert(is_ok(&r1));

    ik_provider_completion_t completion = {.success = true, .http_status = 200};
    res_t r2 = completion_cb(&completion, NULL);
    ck_assert(is_ok(&r2));
}
END_TEST
/* ================================================================
 * Provider Inference
 * ================================================================ */

START_TEST(test_infer_provider_openai_gpt) {
    const char *provider = ik_infer_provider("gpt-5-mini");
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider, "openai");
}

END_TEST

START_TEST(test_infer_provider_openai_o1) {
    const char *provider = ik_infer_provider("o1-preview");
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider, "openai");
}

END_TEST

START_TEST(test_infer_provider_openai_o3) {
    const char *provider = ik_infer_provider("o3-mini");
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider, "openai");
}

END_TEST

START_TEST(test_infer_provider_anthropic) {
    const char *provider = ik_infer_provider("claude-sonnet-4-5");
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider, "anthropic");
}

END_TEST

START_TEST(test_infer_provider_google) {
    const char *provider = ik_infer_provider("gemini-3.0-flash");
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider, "google");
}

END_TEST

START_TEST(test_infer_provider_unknown) {
    const char *provider = ik_infer_provider("unknown-model");
    ck_assert_ptr_null(provider);
}

END_TEST

START_TEST(test_infer_provider_null) {
    const char *provider = ik_infer_provider(NULL);
    ck_assert_ptr_null(provider);
}

END_TEST

START_TEST(test_infer_provider_openai_o3_exact) {
    const char *provider = ik_infer_provider("o3");
    ck_assert_ptr_nonnull(provider);
    ck_assert_str_eq(provider, "openai");
}

END_TEST

/* ================================================================
 * Model Thinking Support
 * ================================================================ */

START_TEST(test_model_supports_thinking_null_model) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    bool supports = false;
    res_t r = ik_model_supports_thinking(NULL, &supports);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_model_supports_thinking_null_supports) {
    void *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    res_t r = ik_model_supports_thinking("gpt-5", NULL);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_model_supports_thinking_known_model) {
    bool supports = false;
    res_t r = ik_model_supports_thinking("gpt-5", &supports);
    ck_assert(is_ok(&r));
    ck_assert(supports == true);
}

END_TEST

START_TEST(test_model_supports_thinking_non_thinking_model) {
    bool supports = true;
    res_t r = ik_model_supports_thinking("gpt-4", &supports);
    ck_assert(is_ok(&r));
    ck_assert(supports == false);
}

END_TEST

START_TEST(test_model_supports_thinking_unknown_model) {
    bool supports = true;
    res_t r = ik_model_supports_thinking("unknown-model", &supports);
    ck_assert(is_ok(&r));
    ck_assert(supports == false);
}

END_TEST

/* ================================================================
 * Model Thinking Budget
 * ================================================================ */

START_TEST(test_model_get_thinking_budget_null_model) {
    int32_t budget = 0;
    res_t r = ik_model_get_thinking_budget(NULL, &budget);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    talloc_free(r.err);
}

END_TEST

START_TEST(test_model_get_thinking_budget_null_budget) {
    res_t r = ik_model_get_thinking_budget("claude-sonnet-4-5", NULL);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    talloc_free(r.err);
}

END_TEST

START_TEST(test_model_get_thinking_budget_anthropic_model) {
    int32_t budget = 0;
    res_t r = ik_model_get_thinking_budget("claude-sonnet-4-5", &budget);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(budget, 64000);
}

END_TEST

START_TEST(test_model_get_thinking_budget_openai_model) {
    int32_t budget = -1;
    res_t r = ik_model_get_thinking_budget("gpt-5", &budget);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(budget, 0);
}

END_TEST

START_TEST(test_model_get_thinking_budget_unknown_model) {
    int32_t budget = -1;
    res_t r = ik_model_get_thinking_budget("unknown-model", &budget);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(budget, 0);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *provider_types_suite(void)
{
    Suite *s = suite_create("Provider Types");

    TCase *tc_enums = tcase_create("Enum Values");
    tcase_set_timeout(tc_enums, IK_TEST_TIMEOUT);
    tcase_add_test(tc_enums, test_thinking_level_enum_values);
    tcase_add_test(tc_enums, test_finish_reason_enum_values);
    tcase_add_test(tc_enums, test_content_type_enum_values);
    tcase_add_test(tc_enums, test_role_enum_values);
    // TEMPORARILY DISABLED: test_tool_choice_enum_values (enum commented out during coexistence)
    // tcase_add_test(tc_enums, test_tool_choice_enum_values);
    tcase_add_test(tc_enums, test_error_category_enum_values);
    tcase_add_test(tc_enums, test_stream_event_type_enum_values);
    suite_add_tcase(s, tc_enums);

    TCase *tc_structs = tcase_create("Struct Validation");
    tcase_set_timeout(tc_structs, IK_TEST_TIMEOUT);
    tcase_add_test(tc_structs, test_struct_sizes);
    suite_add_tcase(s, tc_structs);

    TCase *tc_talloc = tcase_create("Talloc Allocation");
    tcase_set_timeout(tc_talloc, IK_TEST_TIMEOUT);
    tcase_add_test(tc_talloc, test_talloc_allocation_request);
    tcase_add_test(tc_talloc, test_talloc_allocation_response);
    suite_add_tcase(s, tc_talloc);

    TCase *tc_errors = tcase_create("Error Codes");
    tcase_set_timeout(tc_errors, IK_TEST_TIMEOUT);
    tcase_add_test(tc_errors, test_error_code_provider);
    tcase_add_test(tc_errors, test_error_code_missing_credentials);
    tcase_add_test(tc_errors, test_error_code_not_implemented);
    suite_add_tcase(s, tc_errors);

    TCase *tc_callbacks = tcase_create("Callback Types");
    tcase_set_timeout(tc_callbacks, IK_TEST_TIMEOUT);
    tcase_add_test(tc_callbacks, test_callback_type_assignment);
    suite_add_tcase(s, tc_callbacks);

    TCase *tc_infer = tcase_create("Provider Inference");
    tcase_set_timeout(tc_infer, IK_TEST_TIMEOUT);
    tcase_add_test(tc_infer, test_infer_provider_openai_gpt);
    tcase_add_test(tc_infer, test_infer_provider_openai_o1);
    tcase_add_test(tc_infer, test_infer_provider_openai_o3);
    tcase_add_test(tc_infer, test_infer_provider_openai_o3_exact);
    tcase_add_test(tc_infer, test_infer_provider_anthropic);
    tcase_add_test(tc_infer, test_infer_provider_google);
    tcase_add_test(tc_infer, test_infer_provider_unknown);
    tcase_add_test(tc_infer, test_infer_provider_null);
    suite_add_tcase(s, tc_infer);

    TCase *tc_thinking = tcase_create("Model Thinking Support");
    tcase_set_timeout(tc_thinking, IK_TEST_TIMEOUT);
    tcase_add_test(tc_thinking, test_model_supports_thinking_null_model);
    tcase_add_test(tc_thinking, test_model_supports_thinking_null_supports);
    tcase_add_test(tc_thinking, test_model_supports_thinking_known_model);
    tcase_add_test(tc_thinking, test_model_supports_thinking_non_thinking_model);
    tcase_add_test(tc_thinking, test_model_supports_thinking_unknown_model);
    suite_add_tcase(s, tc_thinking);

    TCase *tc_budget = tcase_create("Model Thinking Budget");
    tcase_set_timeout(tc_budget, IK_TEST_TIMEOUT);
    tcase_add_test(tc_budget, test_model_get_thinking_budget_null_model);
    tcase_add_test(tc_budget, test_model_get_thinking_budget_null_budget);
    tcase_add_test(tc_budget, test_model_get_thinking_budget_anthropic_model);
    tcase_add_test(tc_budget, test_model_get_thinking_budget_openai_model);
    tcase_add_test(tc_budget, test_model_get_thinking_budget_unknown_model);
    suite_add_tcase(s, tc_budget);

    return s;
}

int main(void)
{
    Suite *s = provider_types_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
