#include "tests/test_constants.h"
/* Coverage tests for ik_token_cache_t: estimate_turn_bytes paths and clamping. */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/token_cache.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/provider_vtable.h"
#include "shared/error.h"

static int32_t g_mock_count = 0;
static bool g_mock_fail = false;
static int g_mock_calls = 0;
static TALLOC_CTX *g_err_ctx = NULL;

static res_t mock_count_tokens(void *ctx, const ik_request_t *req, int32_t *out)
{
    (void)ctx; (void)req;
    g_mock_calls++;
    if (g_mock_fail) return ERR(g_err_ctx, PROVIDER, "mock failure");
    *out = g_mock_count;
    return OK(NULL);
}

static const ik_provider_vtable_t g_mock_vtable = { .count_tokens = mock_count_tokens };
static struct ik_provider g_mock_provider = { .name = "mock", .vt = &g_mock_vtable };

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    g_err_ctx = talloc_new(NULL);
    g_mock_count = 0;
    g_mock_fail = false;
    g_mock_calls = 0;
}

static void teardown(void)
{
    talloc_free(test_ctx);
    talloc_free(g_err_ctx);
}

static ik_agent_ctx_t *make_agent(TALLOC_CTX *ctx)
{
    ik_agent_ctx_t *a = talloc_zero(ctx, ik_agent_ctx_t);
    a->provider_instance = &g_mock_provider;
    a->model = talloc_strdup(a, "mock-model");
    return a;
}

/* ----------------------------------------------------------------
 * Helper: add a message with a specific content block type
 * ---------------------------------------------------------------- */
static void add_msg_with_block(ik_agent_ctx_t *a, ik_role_t role,
                                ik_content_type_t btype, const char *text)
{
    size_t idx = a->message_count++;
    a->message_capacity = a->message_count;
    a->messages = talloc_realloc(a, a->messages, ik_message_t *,
                                 (unsigned int)a->message_count);
    ik_message_t *m = talloc_zero(a, ik_message_t);
    m->role = role;
    m->content_count = 1;
    m->content_blocks = talloc_array(m, ik_content_block_t, 1);
    m->content_blocks[0].type = btype;
    switch (btype) {
        case IK_CONTENT_TOOL_CALL:
            m->content_blocks[0].data.tool_call.arguments = talloc_strdup(m, text);
            break;
        case IK_CONTENT_TOOL_RESULT:
            m->content_blocks[0].data.tool_result.content = talloc_strdup(m, text);
            break;
        case IK_CONTENT_THINKING:
            m->content_blocks[0].data.thinking.text = talloc_strdup(m, text);
            break;
        default:
            m->content_blocks[0].data.text.text = talloc_strdup(m, text);
            break;
    }
    a->messages[idx] = m;
}

/* ----------------------------------------------------------------
 * Test: estimate_turn_bytes covers TOOL_CALL, TOOL_RESULT, THINKING
 * (lines 72-80) via the fallback path when provider fails.
 * ---------------------------------------------------------------- */
START_TEST(test_estimate_bytes_tool_call) {
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_msg_with_block(a, IK_ROLE_USER,      IK_CONTENT_TOOL_CALL,   "args");
    add_msg_with_block(a, IK_ROLE_ASSISTANT, IK_CONTENT_TOOL_RESULT, "result");

    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);

    g_mock_fail = true; /* force provider to return TOKEN_UNCACHED → fallback */
    int32_t t = ik_token_cache_get_turn_tokens(c, 0);
    ck_assert_int_gt(t, 0); /* bytes > 0 → estimated tokens > 0 */
}
END_TEST

START_TEST(test_estimate_bytes_thinking) {
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_msg_with_block(a, IK_ROLE_USER,      IK_CONTENT_THINKING, "thinking text");
    add_msg_with_block(a, IK_ROLE_ASSISTANT, IK_CONTENT_TEXT,     "response");

    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);

    g_mock_fail = true;
    int32_t t = ik_token_cache_get_turn_tokens(c, 0);
    ck_assert_int_gt(t, 0);
}
END_TEST

/* Test: estimate_turn_bytes with NULL tool_call.arguments (line 73 false branch) */
START_TEST(test_estimate_bytes_null_tool_call_args) {
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_msg_with_block(a, IK_ROLE_USER, IK_CONTENT_TOOL_CALL, NULL);
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    g_mock_fail = true;
    int32_t t = ik_token_cache_get_turn_tokens(c, 0);
    ck_assert_int_eq(t, 0);
}
END_TEST

/* Test: estimate_turn_bytes with NULL tool_result.content (line 76 false branch) */
START_TEST(test_estimate_bytes_null_tool_result_content) {
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_msg_with_block(a, IK_ROLE_USER, IK_CONTENT_TOOL_RESULT, NULL);
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    g_mock_fail = true;
    int32_t t = ik_token_cache_get_turn_tokens(c, 0);
    ck_assert_int_eq(t, 0);
}
END_TEST

/* Test: estimate_turn_bytes with REDACTED_THINKING type (line 78 false branch) */
START_TEST(test_estimate_bytes_redacted_thinking) {
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_msg_with_block(a, IK_ROLE_USER, IK_CONTENT_REDACTED_THINKING, "encrypted");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    g_mock_fail = true;
    int32_t t = ik_token_cache_get_turn_tokens(c, 0);
    ck_assert_int_eq(t, 0); /* no bytes counted for redacted_thinking */
}
END_TEST

/* Test: estimate_turn_bytes with NULL thinking.text (line 79 false branch) */
START_TEST(test_estimate_bytes_null_thinking_text) {
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_msg_with_block(a, IK_ROLE_USER, IK_CONTENT_THINKING, NULL);
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    g_mock_fail = true;
    int32_t t = ik_token_cache_get_turn_tokens(c, 0);
    ck_assert_int_eq(t, 0);
}
END_TEST

/* ----------------------------------------------------------------
 * Test: clamp_context_start when context_start_index > message_count
 * (lines 460-462)
 * ---------------------------------------------------------------- */
START_TEST(test_clamp_context_start) {
    ik_agent_ctx_t *a = make_agent(test_ctx);

    size_t idx = a->message_count++;
    a->message_capacity = a->message_count;
    a->messages = talloc_realloc(a, a->messages, ik_message_t *,
                                 (unsigned int)a->message_count);
    ik_message_t *m0 = talloc_zero(a, ik_message_t);
    m0->role = IK_ROLE_USER;
    m0->content_count = 1;
    m0->content_blocks = talloc_array(m0, ik_content_block_t, 1);
    m0->content_blocks[0].type = IK_CONTENT_TEXT;
    m0->content_blocks[0].data.text.text = talloc_strdup(m0, "u0");
    a->messages[idx] = m0;

    idx = a->message_count++;
    a->message_capacity = a->message_count;
    a->messages = talloc_realloc(a, a->messages, ik_message_t *,
                                 (unsigned int)a->message_count);
    ik_message_t *m1 = talloc_zero(a, ik_message_t);
    m1->role = IK_ROLE_USER;
    m1->content_count = 1;
    m1->content_blocks = talloc_array(m1, ik_content_block_t, 1);
    m1->content_blocks[0].type = IK_CONTENT_TEXT;
    m1->content_blocks[0].data.text.text = talloc_strdup(m1, "u1");
    a->messages[idx] = m1;

    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 10);
    ik_token_cache_record_turn(c, 1, 20);
    g_mock_count = 0;
    ik_token_cache_get_total(c); /* cache the total */

    /* Prune once: context_start_index advances */
    ik_token_cache_prune_oldest_turn(c);
    ck_assert_uint_gt(ik_token_cache_get_context_start_index(c), 0);

    /* Clamp with message_count smaller than context_start_index */
    ik_token_cache_clamp_context_start(c, 0);

    /* context_start_index should be reset to 0 */
    ck_assert_uint_eq(ik_token_cache_get_context_start_index(c), 0);
}
END_TEST

static Suite *token_cache_coverage_suite(void)
{
    Suite *s = suite_create("Token Cache Coverage");

    TCase *tc = tcase_create("Estimate");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_estimate_bytes_tool_call);
    tcase_add_test(tc, test_estimate_bytes_thinking);
    tcase_add_test(tc, test_estimate_bytes_null_tool_call_args);
    tcase_add_test(tc, test_estimate_bytes_null_tool_result_content);
    tcase_add_test(tc, test_estimate_bytes_redacted_thinking);
    tcase_add_test(tc, test_estimate_bytes_null_thinking_text);
    tcase_add_test(tc, test_clamp_context_start);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = token_cache_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/token_cache_coverage_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int32_t failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? 0 : 1;
}
