/**
 * @file commands_basic_clear_test.c
 * @brief Unit tests for clear_generate_session_summary in commands_basic.c
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_basic.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/marks.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "shared/wrapper_internal.h"
#include "tests/helpers/test_utils_helper.h"

/* ---- mock controls for clear_generate_session_summary tests ---- */

static bool g_find_clear_fail   = false;
static bool g_query_range_fail  = false;
static size_t g_query_msg_count = 0;
static ik_msg_t g_query_msgs[4];
static ik_msg_t *g_query_ptrs[4];

res_t ik_agent_find_clear_(void *db_ctx,
                            TALLOC_CTX *ctx,
                            const char *agent_uuid,
                            int64_t max_id,
                            int64_t *clear_id_out)
{
    (void)db_ctx; (void)ctx; (void)agent_uuid; (void)max_id;
    if (g_find_clear_fail) {
        *clear_id_out = 0;
        return ERR(ctx, IO, "mock find_clear failure");
    }
    *clear_id_out = 0;
    return OK(NULL);
}

res_t ik_agent_query_range_(void *db_ctx,
                             TALLOC_CTX *ctx,
                             const void *range,
                             void ***messages_out,
                             size_t *count_out)
{
    (void)db_ctx; (void)ctx; (void)range;
    if (g_query_range_fail) {
        return ERR(ctx, IO, "mock query_range failure");
    }
    for (size_t i = 0; i < g_query_msg_count; i++) {
        g_query_ptrs[i] = &g_query_msgs[i];
    }
    *messages_out = (void **)g_query_ptrs;
    *count_out    = g_query_msg_count;
    return OK(NULL);
}

static void reset_mocks(void)
{
    g_find_clear_fail  = false;
    g_query_range_fail = false;
    g_query_msg_count  = 0;
    memset(g_query_msgs, 0, sizeof(g_query_msgs));
}

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

/* Helper: build a minimal repl context pointing at agent */
static ik_repl_ctx_t *make_repl(TALLOC_CTX *ctx, ik_agent_ctx_t *agent)
{
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = agent;
    repl->shared = agent->shared;
    return repl;
}

/* ---- /clear: clear_generate_session_summary paths ---- */

/* Path: model == NULL → early return before any DB call */
START_TEST(test_clear_no_model_skips_summary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Ensure model is NULL (default) and set a fake non-NULL db_ctx */
    ck_assert_ptr_null(agent->model);
    ik_db_ctx_t *fake_db = talloc_zero(ctx, ik_db_ctx_t);
    agent->shared->db_ctx = fake_db;

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_clear(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

/* Path: provider == NULL → early return after model check */
START_TEST(test_clear_no_provider_skips_summary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    agent->model = talloc_strdup(agent, "gpt-4o");
    /* provider stays NULL (default) */
    ik_db_ctx_t *fake_db = talloc_zero(ctx, ik_db_ctx_t);
    agent->shared->db_ctx = fake_db;

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_clear(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

/* Path: provider empty string → early return */
START_TEST(test_clear_empty_provider_skips_summary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    agent->model    = talloc_strdup(agent, "gpt-4o");
    agent->provider = talloc_strdup(agent, "");
    ik_db_ctx_t *fake_db = talloc_zero(ctx, ik_db_ctx_t);
    agent->shared->db_ctx = fake_db;

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_clear(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

/* Path: find_clear fails → continues with last_clear_id=0; query_range fails → return */
START_TEST(test_clear_find_clear_error_continues) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_find_clear_fail  = true;
    g_query_range_fail = true;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    agent->model    = talloc_strdup(agent, "gpt-4o");
    agent->provider = talloc_strdup(agent, "openai");
    ik_db_ctx_t *fake_db = talloc_zero(ctx, ik_db_ctx_t);
    agent->shared->db_ctx = fake_db;

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_clear(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

/* Path: query_range returns 0 messages → early return */
START_TEST(test_clear_empty_epoch_skips_summary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();
    g_query_msg_count = 0;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    agent->model    = talloc_strdup(agent, "gpt-4o");
    agent->provider = talloc_strdup(agent, "openai");
    ik_db_ctx_t *fake_db = talloc_zero(ctx, ik_db_ctx_t);
    agent->shared->db_ctx = fake_db;

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_clear(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

/* Path: messages present but no conversation-kind → conv_count == 0 → return */
START_TEST(test_clear_no_conv_msgs_skips_summary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    /* One message of non-conversation kind */
    static char kind_clear[] = "clear";
    static char content_empty[] = "";
    g_query_msgs[0].id      = 1;
    g_query_msgs[0].kind    = kind_clear;
    g_query_msgs[0].content = content_empty;
    g_query_msg_count = 1;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    agent->model    = talloc_strdup(agent, "gpt-4o");
    agent->provider = talloc_strdup(agent, "openai");
    ik_db_ctx_t *fake_db = talloc_zero(ctx, ik_db_ctx_t);
    agent->shared->db_ctx = fake_db;

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_clear(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

/* Path: messages with id <= 0 → start_msg_id <= 0 → return */
START_TEST(test_clear_zero_msg_id_skips_summary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    /* Message with id = 0 (no DB row) */
    static char kind_user[] = "user";
    static char content_hello[] = "hello";
    g_query_msgs[0].id      = 0;
    g_query_msgs[0].kind    = kind_user;
    g_query_msgs[0].content = content_hello;
    g_query_msg_count = 1;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    agent->model    = talloc_strdup(agent, "gpt-4o");
    agent->provider = talloc_strdup(agent, "openai");
    ik_db_ctx_t *fake_db = talloc_zero(ctx, ik_db_ctx_t);
    agent->shared->db_ctx = fake_db;

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_clear(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

/* Path: valid messages with positive IDs → reaches dispatch (no real API) */
START_TEST(test_clear_valid_epoch_dispatches_summary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    /* Two messages with valid positive IDs */
    static char kind_user2[]      = "user";
    static char content_hello2[]  = "hello";
    static char kind_asst[]       = "assistant";
    static char content_hi[]      = "hi there";
    g_query_msgs[0].id      = 10;
    g_query_msgs[0].kind    = kind_user2;
    g_query_msgs[0].content = content_hello2;
    g_query_msgs[1].id      = 11;
    g_query_msgs[1].kind    = kind_asst;
    g_query_msgs[1].content = content_hi;
    g_query_msg_count = 2;

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    agent->model    = talloc_strdup(agent, "gpt-4o");
    agent->provider = talloc_strdup(agent, "openai");
    ik_db_ctx_t *fake_db = talloc_zero(ctx, ik_db_ctx_t);
    agent->shared->db_ctx = fake_db;

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_clear(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

/* Path: recent_summary != NULL is cleared by /clear */
START_TEST(test_clear_resets_recent_summary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    reset_mocks();

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    agent->recent_summary        = talloc_strdup(agent, "old summary text");
    agent->recent_summary_tokens = 99;
    ck_assert_ptr_nonnull(agent->recent_summary);

    ik_repl_ctx_t *repl = make_repl(ctx, agent);
    res = ik_cmd_clear(ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ck_assert_ptr_null(agent->recent_summary);
    ck_assert_int_eq(agent->recent_summary_tokens, 0);

    talloc_free(ctx);
}
END_TEST

static Suite *commands_basic_clear_suite(void)
{
    Suite *s = suite_create("commands_basic_clear");

    TCase *tc_clear = tcase_create("ClearSummary");
    tcase_add_unchecked_fixture(tc_clear, suite_setup, NULL);
    tcase_add_test(tc_clear, test_clear_no_model_skips_summary);
    tcase_add_test(tc_clear, test_clear_no_provider_skips_summary);
    tcase_add_test(tc_clear, test_clear_empty_provider_skips_summary);
    tcase_add_test(tc_clear, test_clear_find_clear_error_continues);
    tcase_add_test(tc_clear, test_clear_empty_epoch_skips_summary);
    tcase_add_test(tc_clear, test_clear_no_conv_msgs_skips_summary);
    tcase_add_test(tc_clear, test_clear_zero_msg_id_skips_summary);
    tcase_add_test(tc_clear, test_clear_valid_epoch_dispatches_summary);
    tcase_add_test(tc_clear, test_clear_resets_recent_summary);
    suite_add_tcase(s, tc_clear);

    return s;
}

int32_t main(void)
{
    Suite *s = commands_basic_clear_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands_basic_clear_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
