#include "tests/test_constants.h"
/* Unit tests for ik_token_cache_t. Mock provider — no network calls. */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/token_cache.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/provider_vtable.h"
#include "shared/error.h"


static int32_t g_mock_count  = 0;
static bool    g_mock_fail   = false;
static int     g_mock_calls  = 0;
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

static void add_user(ik_agent_ctx_t *a, const char *text)
{
    size_t idx = a->message_count++;
    a->message_capacity = a->message_count;
    a->messages = talloc_realloc(a, a->messages, ik_message_t *,
                                  (unsigned int)a->message_count);
    ik_message_t *m = talloc_zero(a, ik_message_t);
    m->role = IK_ROLE_USER;
    m->content_count = 1;
    m->content_blocks = talloc_array(m, ik_content_block_t, 1);
    m->content_blocks[0].type = IK_CONTENT_TEXT;
    m->content_blocks[0].data.text.text = talloc_strdup(m, text);
    a->messages[idx] = m;
}


START_TEST(test_create_basic)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    ck_assert_ptr_nonnull(c);
    ck_assert_int_eq(ik_token_cache_get_total(c), 0); /* mock=0, no turns */
}
END_TEST


START_TEST(test_record_and_get_turn)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    ik_token_cache_add_turn(c);
    ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 10);
    ik_token_cache_record_turn(c, 1, 20);
    ck_assert_int_eq(ik_token_cache_get_turn_tokens(c, 0), 10);
    ck_assert_int_eq(ik_token_cache_get_turn_tokens(c, 1), 20);
}
END_TEST

START_TEST(test_record_invalidates_total)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 50);
    g_mock_count = 10;
    ck_assert_int_eq(ik_token_cache_get_total(c), 70); /* sys(10)+tools(10)+50 */
    ik_token_cache_record_turn(c, 0, 99);
    ck_assert_int_eq(ik_token_cache_get_total(c), 119);
}
END_TEST


START_TEST(test_system_api_result_cached)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    g_mock_count = 75;
    ck_assert_int_eq(ik_token_cache_get_system_tokens(c), 75);
    ck_assert_int_eq(g_mock_calls, 1);
    ik_token_cache_get_system_tokens(c); /* cached — no new call */
    ck_assert_int_eq(g_mock_calls, 1);
}
END_TEST

START_TEST(test_tool_api_result_cached)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    g_mock_count = 30;
    ck_assert_int_eq(ik_token_cache_get_tool_tokens(c), 30);
    ck_assert_int_eq(g_mock_calls, 1);
    ik_token_cache_get_tool_tokens(c); /* cached */
    ck_assert_int_eq(g_mock_calls, 1);
}
END_TEST

START_TEST(test_turn_api_result_cached)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "hello");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    g_mock_count = 88;
    ck_assert_int_eq(ik_token_cache_get_turn_tokens(c, 0), 88);
    ck_assert_int_eq(g_mock_calls, 1);
    ik_token_cache_get_turn_tokens(c, 0); /* cached */
    ck_assert_int_eq(g_mock_calls, 1);
}
END_TEST


START_TEST(test_system_bytes_fallback_not_cached)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    g_mock_fail = true;
    ik_token_cache_get_system_tokens(c);
    ck_assert_int_eq(g_mock_calls, 1);
    ik_token_cache_get_system_tokens(c); /* retries API */
    ck_assert_int_eq(g_mock_calls, 2);
}
END_TEST

START_TEST(test_turn_bytes_fallback_not_cached)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "hello");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    g_mock_fail = true;
    ik_token_cache_get_turn_tokens(c, 0);
    ck_assert_int_eq(g_mock_calls, 1);
    ik_token_cache_get_turn_tokens(c, 0); /* retries */
    ck_assert_int_eq(g_mock_calls, 2);
}
END_TEST

START_TEST(test_no_provider_no_api_calls)
{
    ik_agent_ctx_t *a = talloc_zero(test_ctx, ik_agent_ctx_t);
    a->provider_instance = NULL;
    a->model = talloc_strdup(a, "test");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_get_system_tokens(c);
    ik_token_cache_get_tool_tokens(c);
    ck_assert_int_eq(g_mock_calls, 0);
}
END_TEST


START_TEST(test_total_sum)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 50);
    g_mock_count = 10;
    ck_assert_int_eq(ik_token_cache_get_total(c), 70);
}
END_TEST

START_TEST(test_total_cached)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 50);
    g_mock_count = 10;
    int32_t first = ik_token_cache_get_total(c);
    int32_t calls = g_mock_calls;
    ck_assert_int_eq(ik_token_cache_get_total(c), first);
    ck_assert_int_eq(g_mock_calls, calls);
}
END_TEST

START_TEST(test_total_recomputed_after_invalidate)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "hello");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 50);
    g_mock_count = 10;
    ck_assert_int_eq(ik_token_cache_get_total(c), 70);
    int32_t calls = g_mock_calls;
    ik_token_cache_invalidate_all(c);
    g_mock_count = 20;
    ck_assert_int_eq(ik_token_cache_get_total(c), 60); /* sys(20)+tools(20)+turn(20) */
    ck_assert_int_gt(g_mock_calls, calls);
}
END_TEST


START_TEST(test_invalidate_all)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    ik_token_cache_add_turn(c);
    g_mock_count = 50;
    ik_token_cache_get_system_tokens(c);
    ik_token_cache_get_tool_tokens(c);
    ik_token_cache_record_turn(c, 0, 30);
    ik_token_cache_get_total(c);
    ik_token_cache_invalidate_all(c);
    int32_t calls = g_mock_calls;
    ik_token_cache_get_system_tokens(c);
    ck_assert_int_gt(g_mock_calls, calls);
}
END_TEST

START_TEST(test_invalidate_system_only)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    g_mock_count = 40;
    ik_token_cache_get_system_tokens(c);
    ik_token_cache_get_tool_tokens(c);
    int32_t calls = g_mock_calls;
    ik_token_cache_invalidate_system(c);
    ik_token_cache_get_system_tokens(c);
    ck_assert_int_eq(g_mock_calls, calls + 1);
    ik_token_cache_get_tool_tokens(c); /* not invalidated */
    ck_assert_int_eq(g_mock_calls, calls + 1);
}
END_TEST

START_TEST(test_invalidate_tools_only)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    g_mock_count = 40;
    ik_token_cache_get_system_tokens(c);
    ik_token_cache_get_tool_tokens(c);
    int32_t calls = g_mock_calls;
    ik_token_cache_invalidate_tools(c);
    ik_token_cache_get_tool_tokens(c);
    ck_assert_int_eq(g_mock_calls, calls + 1);
    ik_token_cache_get_system_tokens(c); /* not invalidated */
    ck_assert_int_eq(g_mock_calls, calls + 1);
}
END_TEST


START_TEST(test_prune_oldest_turn)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "t0");
    add_user(a, "t1");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 100);
    ik_token_cache_record_turn(c, 1, 200);
    g_mock_count = 0;
    ck_assert_int_eq(ik_token_cache_get_total(c), 300);
    ik_token_cache_prune_oldest_turn(c);
    ck_assert_int_eq(ik_token_cache_get_total(c), 200);
    ck_assert_int_eq(ik_token_cache_get_turn_tokens(c, 0), 200);
}
END_TEST

START_TEST(test_prune_uncached_turn_invalidates_total)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "t0");
    add_user(a, "t1");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    ik_token_cache_add_turn(c);
    /* turn 0 uncached, turn 1 recorded */
    ik_token_cache_record_turn(c, 1, 50);
    /* Prune: turn 0 = -1, total = -1 → total stays -1 */
    ik_token_cache_prune_oldest_turn(c);
    /* After prune: turn_tokens[0] = 50 (shifted), total recomputed */
    g_mock_count = 5;
    ck_assert_int_eq(ik_token_cache_get_total(c), 60); /* sys(5)+tools(5)+50 */
}
END_TEST

START_TEST(test_prune_last_turn)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "only");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 77);
    ik_token_cache_prune_oldest_turn(c);
    ck_assert_int_eq(ik_token_cache_get_total(c), 0);
}
END_TEST

START_TEST(test_prune_empty_noop)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    ik_token_cache_prune_oldest_turn(c); /* no crash */
    ck_assert_int_eq(ik_token_cache_get_total(c), 0);
}
END_TEST

START_TEST(test_multiple_turns_sequence)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "u0"); add_user(a, "u1"); add_user(a, "u2");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    ik_token_cache_add_turn(c);
    ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 10);
    ik_token_cache_record_turn(c, 1, 20);
    ik_token_cache_record_turn(c, 2, 30);
    g_mock_count = 0;
    ck_assert_int_eq(ik_token_cache_get_total(c), 60);
    ik_token_cache_prune_oldest_turn(c);
    ck_assert_int_eq(ik_token_cache_get_total(c), 50);
    ik_token_cache_prune_oldest_turn(c);
    ck_assert_int_eq(ik_token_cache_get_total(c), 30);
    ik_token_cache_prune_oldest_turn(c);
    ck_assert_int_eq(ik_token_cache_get_total(c), 0);
}
END_TEST


START_TEST(test_clone_preserves_state)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    ik_token_cache_t *src = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(src);
    ik_token_cache_record_turn(src, 0, 55);
    g_mock_count = 10;
    ik_token_cache_get_system_tokens(src);
    ik_token_cache_get_tool_tokens(src);
    ik_token_cache_get_total(src);

    ik_token_cache_t *cl = ik_token_cache_clone(test_ctx, src, make_agent(test_ctx));
    int32_t calls = g_mock_calls;
    ck_assert_int_eq(ik_token_cache_get_system_tokens(cl), 10);
    ck_assert_int_eq(ik_token_cache_get_tool_tokens(cl), 10);
    ck_assert_int_eq(ik_token_cache_get_turn_tokens(cl, 0), 55);
    ck_assert_int_eq(g_mock_calls, calls); /* no new calls */
}
END_TEST

START_TEST(test_clone_independent)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    ik_token_cache_t *src = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(src);
    ik_token_cache_record_turn(src, 0, 10);
    ik_token_cache_t *cl = ik_token_cache_clone(test_ctx, src, make_agent(test_ctx));
    ik_token_cache_record_turn(src, 0, 999);
    ck_assert_int_eq(ik_token_cache_get_turn_tokens(cl, 0), 10);
}
END_TEST


START_TEST(test_get_turn_oob)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    ik_token_cache_get_turn_tokens(c, 0); /* PANIC */
}
END_TEST

START_TEST(test_record_turn_oob)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    ik_token_cache_record_turn(c, 0, 10); /* PANIC */
}
END_TEST


START_TEST(test_reset_clears_all)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "u0");
    add_user(a, "u1");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c);
    ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 100);
    ik_token_cache_record_turn(c, 1, 200);
    g_mock_count = 0;
    ik_token_cache_prune_oldest_turn(c); /* context_start_index > 0 */
    ck_assert_uint_gt(ik_token_cache_get_context_start_index(c), 0);

    ik_token_cache_reset(c);

    ck_assert_uint_eq(ik_token_cache_get_turn_count(c), 0);
    ck_assert_uint_eq(ik_token_cache_get_context_start_index(c), 0);
    ck_assert_int_eq(ik_token_cache_get_total(c), 0);
}
END_TEST

START_TEST(test_add_turn_invalidates_total)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    g_mock_count = 5;
    ck_assert_int_eq(ik_token_cache_get_total(c), 10); /* sys(5)+tools(5) */
    ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 50);
    ck_assert_int_eq(ik_token_cache_get_total(c), 60);
}
END_TEST


static Suite *token_cache_suite(void)
{
    Suite *s = suite_create("Token Cache");

    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_create_basic);
    tcase_add_test(tc, test_record_and_get_turn);
    tcase_add_test(tc, test_record_invalidates_total);
    tcase_add_test(tc, test_add_turn_invalidates_total);
    tcase_add_test(tc, test_system_api_result_cached);
    tcase_add_test(tc, test_tool_api_result_cached);
    tcase_add_test(tc, test_turn_api_result_cached);
    tcase_add_test(tc, test_system_bytes_fallback_not_cached);
    tcase_add_test(tc, test_turn_bytes_fallback_not_cached);
    tcase_add_test(tc, test_no_provider_no_api_calls);
    tcase_add_test(tc, test_total_sum);
    tcase_add_test(tc, test_total_cached);
    tcase_add_test(tc, test_total_recomputed_after_invalidate);
    tcase_add_test(tc, test_reset_clears_all);
    tcase_add_test(tc, test_invalidate_all);
    tcase_add_test(tc, test_invalidate_system_only);
    tcase_add_test(tc, test_invalidate_tools_only);
    tcase_add_test(tc, test_prune_oldest_turn);
    tcase_add_test(tc, test_prune_uncached_turn_invalidates_total);
    tcase_add_test(tc, test_prune_last_turn);
    tcase_add_test(tc, test_prune_empty_noop);
    tcase_add_test(tc, test_multiple_turns_sequence);
    tcase_add_test(tc, test_clone_preserves_state);
    tcase_add_test(tc, test_clone_independent);
    suite_add_tcase(s, tc);

    TCase *tc_panic = tcase_create("PANIC");
    tcase_set_timeout(tc_panic, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_panic, setup, teardown);
    tcase_add_test_raise_signal(tc_panic, test_get_turn_oob, SIGABRT);
    tcase_add_test_raise_signal(tc_panic, test_record_turn_oob, SIGABRT);
    suite_add_tcase(s, tc_panic);

    return s;
}

int main(void)
{
    Suite *s = token_cache_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/token_cache_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int32_t failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? 0 : 1;
}
