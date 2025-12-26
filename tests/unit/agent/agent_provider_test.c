#include "../../../src/agent.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/error.h"
#include "../../../src/providers/provider.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <stdlib.h>

// Forward declaration from agent_provider.c
typedef struct ik_provider ik_provider_t;
extern res_t ik_provider_create(TALLOC_CTX *ctx, const char *name, ik_provider_t **out);

// Helper to create a test row with thinking level
static ik_db_agent_row_t *create_test_row(TALLOC_CTX *ctx, const char *thinking_level_str)
{
    ik_db_agent_row_t *row = talloc_zero(ctx, ik_db_agent_row_t);
    row->uuid = talloc_strdup(row, "test-uuid");
    row->provider = talloc_strdup(row, "openai");
    row->model = talloc_strdup(row, "gpt-4");
    row->thinking_level = thinking_level_str ? talloc_strdup(row, thinking_level_str) : NULL;
    return row;
}

// Test ik_agent_apply_defaults() with NULL config
START_TEST(test_apply_defaults_null_config)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    res_t res = ik_agent_apply_defaults(agent, NULL);
    ck_assert(is_err(&res));
    ck_assert_uint_eq(res.err->code, ERR_INVALID_ARG);

    talloc_free(ctx);
}
END_TEST

// Test ik_agent_apply_defaults() success
START_TEST(test_apply_defaults_success)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    ik_config_t *config = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(config);
    config->default_provider = talloc_strdup(config, "openai");
    config->openai_model = talloc_strdup(config, "gpt-4");

    res_t res = ik_agent_apply_defaults(agent, config);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent->provider);
    ck_assert_str_eq(agent->provider, "openai");
    ck_assert_ptr_nonnull(agent->model);
    ck_assert_str_eq(agent->model, "gpt-4");
    ck_assert_int_eq(agent->thinking_level, IK_THINKING_MED);
    ck_assert_ptr_null(agent->provider_instance);

    talloc_free(ctx);
}
END_TEST

// Test ik_agent_restore_from_row() with NULL row
START_TEST(test_restore_from_row_null)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    res_t res = ik_agent_restore_from_row(agent, NULL);
    ck_assert(is_err(&res));
    ck_assert_uint_eq(res.err->code, ERR_INVALID_ARG);

    talloc_free(ctx);
}
END_TEST

// Test ik_agent_restore_from_row() with all NULL provider fields
START_TEST(test_restore_from_row_null_fields)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    ik_db_agent_row_t *row = talloc_zero(ctx, ik_db_agent_row_t);
    row->uuid = talloc_strdup(row, "test-uuid");
    row->name = NULL;
    row->parent_uuid = NULL;
    row->fork_message_id = talloc_strdup(row, "0");
    row->status = talloc_strdup(row, "running");
    row->created_at = 12345;
    row->ended_at = 0;
    row->provider = NULL;
    row->model = NULL;
    row->thinking_level = NULL;

    res_t res = ik_agent_restore_from_row(agent, row);
    ck_assert(is_ok(&res));
    ck_assert_ptr_null(agent->provider);
    ck_assert_ptr_null(agent->model);
    ck_assert_int_eq(agent->thinking_level, 0);
    ck_assert_ptr_null(agent->provider_instance);

    talloc_free(ctx);
}
END_TEST

// Test ik_agent_restore_from_row() with all provider fields set
START_TEST(test_restore_from_row_with_fields)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    ik_db_agent_row_t *row = talloc_zero(ctx, ik_db_agent_row_t);
    row->uuid = talloc_strdup(row, "test-uuid");
    row->name = NULL;
    row->parent_uuid = NULL;
    row->fork_message_id = talloc_strdup(row, "0");
    row->status = talloc_strdup(row, "running");
    row->created_at = 12345;
    row->ended_at = 0;
    row->provider = talloc_strdup(row, "anthropic");
    row->model = talloc_strdup(row, "claude-3-5-sonnet-20241022");
    row->thinking_level = talloc_strdup(row, "high");

    res_t res = ik_agent_restore_from_row(agent, row);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent->provider);
    ck_assert_str_eq(agent->provider, "anthropic");
    ck_assert_ptr_nonnull(agent->model);
    ck_assert_str_eq(agent->model, "claude-3-5-sonnet-20241022");
    ck_assert_int_eq(agent->thinking_level, IK_THINKING_HIGH);
    ck_assert_ptr_null(agent->provider_instance);

    talloc_free(ctx);
}
END_TEST

// Test parse_thinking_level with NULL thinking level
START_TEST(test_thinking_level_null)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    ik_db_agent_row_t *row = create_test_row(ctx, NULL);

    res_t res = ik_agent_restore_from_row(agent, row);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(agent->thinking_level, IK_THINKING_NONE);

    talloc_free(ctx);
}
END_TEST

// Test parse_thinking_level through ik_agent_restore_from_row
START_TEST(test_thinking_level_none)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    ik_db_agent_row_t *row = create_test_row(ctx, "none");

    res_t res = ik_agent_restore_from_row(agent, row);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(agent->thinking_level, IK_THINKING_NONE);

    talloc_free(ctx);
}
END_TEST

// Test parse_thinking_level with "low"
START_TEST(test_thinking_level_low)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    ik_db_agent_row_t *row = create_test_row(ctx, "low");

    res_t res = ik_agent_restore_from_row(agent, row);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(agent->thinking_level, IK_THINKING_LOW);

    talloc_free(ctx);
}
END_TEST

// Test parse_thinking_level with "med"
START_TEST(test_thinking_level_med)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    ik_db_agent_row_t *row = create_test_row(ctx, "med");

    res_t res = ik_agent_restore_from_row(agent, row);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(agent->thinking_level, IK_THINKING_MED);

    talloc_free(ctx);
}
END_TEST

// Test parse_thinking_level with "medium"
START_TEST(test_thinking_level_medium)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    ik_db_agent_row_t *row = create_test_row(ctx, "medium");

    res_t res = ik_agent_restore_from_row(agent, row);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(agent->thinking_level, IK_THINKING_MED);

    talloc_free(ctx);
}
END_TEST

// Test parse_thinking_level with unknown value defaults to none
START_TEST(test_thinking_level_unknown)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    ik_db_agent_row_t *row = create_test_row(ctx, "invalid-value");

    res_t res = ik_agent_restore_from_row(agent, row);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(agent->thinking_level, IK_THINKING_NONE);

    talloc_free(ctx);
}
END_TEST

// Test ik_agent_get_provider() with cached provider
START_TEST(test_get_provider_cached)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->provider = talloc_strdup(agent, "openai");

    // Create a mock provider instance
    ik_provider_t *mock_provider = talloc_zero(agent, ik_provider_t);
    agent->provider_instance = mock_provider;

    ik_provider_t *out = NULL;
    res_t res = ik_agent_get_provider(agent, &out);
    ck_assert(is_ok(&res));
    ck_assert_ptr_eq(out, mock_provider);
    ck_assert_ptr_eq(res.ok, mock_provider);

    talloc_free(ctx);
}
END_TEST

// Test ik_agent_get_provider() with NULL provider
START_TEST(test_get_provider_null)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->provider = NULL;

    ik_provider_t *out = NULL;
    res_t res = ik_agent_get_provider(agent, &out);
    ck_assert(is_err(&res));
    ck_assert_uint_eq(res.err->code, ERR_INVALID_ARG);

    talloc_free(ctx);
}
END_TEST

// Test ik_agent_get_provider() with empty provider
START_TEST(test_get_provider_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->provider = talloc_strdup(agent, "");

    ik_provider_t *out = NULL;
    res_t res = ik_agent_get_provider(agent, &out);
    ck_assert(is_err(&res));
    ck_assert_uint_eq(res.err->code, ERR_INVALID_ARG);

    talloc_free(ctx);
}
END_TEST

// Test ik_agent_get_provider() when provider creation fails
// Note: This test requires actual provider creation to fail, which happens
// when credentials are missing. We test the error handling path here.
START_TEST(test_get_provider_creation_fails)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->provider = talloc_strdup(agent, "invalid-provider");

    ik_provider_t *out = NULL;
    res_t res = ik_agent_get_provider(agent, &out);
    // Should fail because invalid-provider is not a recognized provider
    ck_assert(is_err(&res));
    ck_assert_uint_eq(res.err->code, ERR_MISSING_CREDENTIALS);

    talloc_free(ctx);
}
END_TEST

// Test ik_agent_get_provider() success path when provider creation succeeds
START_TEST(test_get_provider_success)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->provider = talloc_strdup(agent, "anthropic");

    // Set dummy API key so provider creation succeeds
    setenv("ANTHROPIC_API_KEY", "test-key-123", 1);

    ik_provider_t *out = NULL;
    res_t res = ik_agent_get_provider(agent, &out);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(out);
    ck_assert_ptr_eq(agent->provider_instance, out);

    // Clean up environment variable
    unsetenv("ANTHROPIC_API_KEY");

    talloc_free(ctx);
}
END_TEST

// Test ik_agent_invalidate_provider() with NULL cached provider
START_TEST(test_invalidate_provider_null)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->provider_instance = NULL;

    // Should be safe to call with NULL
    ik_agent_invalidate_provider(agent);
    ck_assert_ptr_null(agent->provider_instance);

    talloc_free(ctx);
}
END_TEST

// Test ik_agent_invalidate_provider() with cached provider
START_TEST(test_invalidate_provider_cached)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    // Create a mock provider instance
    ik_provider_t *mock_provider = talloc_zero(agent, ik_provider_t);
    agent->provider_instance = mock_provider;

    // Invalidate should free and set to NULL
    ik_agent_invalidate_provider(agent);
    ck_assert_ptr_null(agent->provider_instance);

    // Should be safe to call again
    ik_agent_invalidate_provider(agent);
    ck_assert_ptr_null(agent->provider_instance);

    talloc_free(ctx);
}
END_TEST

static Suite *agent_provider_suite(void)
{
    Suite *s = suite_create("Agent Provider");

    TCase *tc_apply_defaults = tcase_create("Apply Defaults");
    tcase_add_test(tc_apply_defaults, test_apply_defaults_null_config);
    tcase_add_test(tc_apply_defaults, test_apply_defaults_success);
    suite_add_tcase(s, tc_apply_defaults);

    TCase *tc_restore = tcase_create("Restore From Row");
    tcase_add_test(tc_restore, test_restore_from_row_null);
    tcase_add_test(tc_restore, test_restore_from_row_null_fields);
    tcase_add_test(tc_restore, test_restore_from_row_with_fields);
    suite_add_tcase(s, tc_restore);

    TCase *tc_thinking = tcase_create("Thinking Level Parsing");
    tcase_add_test(tc_thinking, test_thinking_level_null);
    tcase_add_test(tc_thinking, test_thinking_level_none);
    tcase_add_test(tc_thinking, test_thinking_level_low);
    tcase_add_test(tc_thinking, test_thinking_level_med);
    tcase_add_test(tc_thinking, test_thinking_level_medium);
    tcase_add_test(tc_thinking, test_thinking_level_unknown);
    suite_add_tcase(s, tc_thinking);

    TCase *tc_get_provider = tcase_create("Get Provider");
    tcase_add_test(tc_get_provider, test_get_provider_cached);
    tcase_add_test(tc_get_provider, test_get_provider_null);
    tcase_add_test(tc_get_provider, test_get_provider_empty);
    tcase_add_test(tc_get_provider, test_get_provider_creation_fails);
    tcase_add_test(tc_get_provider, test_get_provider_success);
    suite_add_tcase(s, tc_get_provider);

    TCase *tc_invalidate = tcase_create("Invalidate Provider");
    tcase_add_test(tc_invalidate, test_invalidate_provider_null);
    tcase_add_test(tc_invalidate, test_invalidate_provider_cached);
    suite_add_tcase(s, tc_invalidate);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = agent_provider_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
