#include "tests/test_constants.h"
/* Unit tests for ik_token_cache_remove_turns_from(). */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "apps/ikigai/token_cache.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/provider_vtable.h"
#include "shared/error.h"


static int32_t g_mock_count  = 0;
static bool    g_mock_fail   = false;
static TALLOC_CTX *g_err_ctx = NULL;

static res_t mock_count_tokens(void *ctx, const ik_request_t *req, int32_t *out)
{
    (void)ctx; (void)req;
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


START_TEST(test_remove_turns_from_removes_last)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "u0"); add_user(a, "u1"); add_user(a, "u2");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c); ik_token_cache_add_turn(c); ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 10);
    ik_token_cache_record_turn(c, 1, 20);
    ik_token_cache_record_turn(c, 2, 30);
    g_mock_count = 0;
    ck_assert_int_eq(ik_token_cache_get_total(c), 60);
    ik_token_cache_remove_turns_from(c, 1);
    ck_assert_int_eq((int)ik_token_cache_get_turn_count(c), 1);
    ck_assert_int_eq(ik_token_cache_get_total(c), 10);
}
END_TEST

START_TEST(test_remove_turns_from_uncached_invalidates_total)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "u0"); add_user(a, "u1"); add_user(a, "u2");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c); ik_token_cache_add_turn(c); ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 10); ik_token_cache_record_turn(c, 2, 30);
    g_mock_fail = true;
    ik_token_cache_get_total(c); /* prime total — turn 1 stays -1 (API failed) */
    g_mock_fail = false;
    ck_assert_int_gt(ik_token_cache_peek_total(c), 0);
    ik_token_cache_remove_turns_from(c, 1);
    ck_assert_int_eq(ik_token_cache_peek_total(c), 0); /* 0 means uncached */
    ck_assert_int_eq((int)ik_token_cache_get_turn_count(c), 1);
}
END_TEST

START_TEST(test_remove_turns_from_all_clears_turns)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "u0"); add_user(a, "u1");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c); ik_token_cache_add_turn(c);
    ik_token_cache_record_turn(c, 0, 10); ik_token_cache_record_turn(c, 1, 20);
    g_mock_count = 0;
    ik_token_cache_remove_turns_from(c, 0);
    ck_assert_int_eq((int)ik_token_cache_get_turn_count(c), 0);
    ck_assert_int_eq(ik_token_cache_get_total(c), 0);
}
END_TEST

START_TEST(test_remove_turns_from_noop_at_turn_count)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "u0");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c); ik_token_cache_record_turn(c, 0, 42);
    g_mock_count = 0;
    ik_token_cache_remove_turns_from(c, 1); /* turn_index == turn_count: no-op */
    ck_assert_int_eq((int)ik_token_cache_get_turn_count(c), 1);
    ck_assert_int_eq(ik_token_cache_get_total(c), 42);
}
END_TEST

START_TEST(test_remove_turns_from_oob)
{
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, make_agent(test_ctx));
    ik_token_cache_remove_turns_from(c, 1); /* PANIC */
}
END_TEST

/* Rewind past pruning boundary resets context_start_index and pruned_turn_count */
START_TEST(test_remove_turns_from_all_resets_context_start)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    /* Add 3 user messages */
    add_user(a, "u0"); add_user(a, "u1"); add_user(a, "u2");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    /* Add 3 turns and prune the oldest to advance context_start_index */
    ik_token_cache_add_turn(c); ik_token_cache_record_turn(c, 0, 10);
    ik_token_cache_add_turn(c); ik_token_cache_record_turn(c, 1, 20);
    ik_token_cache_add_turn(c); ik_token_cache_record_turn(c, 2, 30);
    ik_token_cache_prune_oldest_turn(c); /* advance context_start_index */
    ck_assert_int_gt((int)ik_token_cache_get_context_start_turn(c), 0);

    /* Rewind past pruning boundary: remove all remaining turns */
    ik_token_cache_remove_turns_from(c, 0);
    ck_assert_int_eq((int)ik_token_cache_get_turn_count(c), 0);
    ck_assert_int_eq((int)ik_token_cache_get_context_start_index(c), 0);
    ck_assert_int_eq((int)ik_token_cache_get_context_start_turn(c), 0);
}
END_TEST

/* Rewind within pruning boundary keeps context_start_index unchanged */
START_TEST(test_remove_turns_from_partial_keeps_context_start)
{
    ik_agent_ctx_t *a = make_agent(test_ctx);
    add_user(a, "u0"); add_user(a, "u1"); add_user(a, "u2"); add_user(a, "u3");
    ik_token_cache_t *c = ik_token_cache_create(test_ctx, a);
    ik_token_cache_add_turn(c); ik_token_cache_record_turn(c, 0, 10);
    ik_token_cache_add_turn(c); ik_token_cache_record_turn(c, 1, 20);
    ik_token_cache_add_turn(c); ik_token_cache_record_turn(c, 2, 30);
    ik_token_cache_add_turn(c); ik_token_cache_record_turn(c, 3, 40);
    ik_token_cache_prune_oldest_turn(c); /* prune turn 0, context_start_index advances */
    size_t csi = ik_token_cache_get_context_start_index(c);
    ck_assert_int_gt((int)csi, 0);

    /* Rewind only removes the last turn, leaves turns 0..1 (relative) intact */
    ik_token_cache_remove_turns_from(c, 2);
    ck_assert_int_eq((int)ik_token_cache_get_turn_count(c), 2);
    ck_assert_int_eq((int)ik_token_cache_get_context_start_index(c), (int)csi);
}
END_TEST


static Suite *token_cache_remove_suite(void)
{
    Suite *s = suite_create("Token Cache Remove");

    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_remove_turns_from_removes_last);
    tcase_add_test(tc, test_remove_turns_from_uncached_invalidates_total);
    tcase_add_test(tc, test_remove_turns_from_all_clears_turns);
    tcase_add_test(tc, test_remove_turns_from_noop_at_turn_count);
    tcase_add_test(tc, test_remove_turns_from_all_resets_context_start);
    tcase_add_test(tc, test_remove_turns_from_partial_keeps_context_start);
    suite_add_tcase(s, tc);

    TCase *tc_panic = tcase_create("PANIC");
    tcase_set_timeout(tc_panic, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_panic, setup, teardown);
    tcase_add_test_raise_signal(tc_panic, test_remove_turns_from_oob, SIGABRT);
    suite_add_tcase(s, tc_panic);

    return s;
}

int main(void)
{
    Suite *s = token_cache_remove_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/token_cache_remove_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int32_t failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? 0 : 1;
}
