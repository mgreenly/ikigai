/**
 * @file clear_getcwd_test.c
 * @brief Unit tests for /clear command getcwd failure handling
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/openai/client.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

/**
 * Create a minimal REPL context for getcwd testing.
 */
static ik_repl_ctx_t *create_test_repl_minimal(void *parent)
{
    // Create scrollback buffer (80 columns is standard)
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create conversation
    res_t res = ik_openai_conversation_create(parent);
    ck_assert(is_ok(&res));
    ik_openai_conversation_t *conv = res.ok;
    ck_assert_ptr_nonnull(conv);

    // Create minimal config
    ik_cfg_t *cfg = talloc_zero(parent, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);
    
    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;
    r->current = agent;


    r->conversation = conv;
    r->shared = shared;

    return r;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    repl = create_test_repl_minimal(ctx);
    ck_assert_ptr_nonnull(repl);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Mock posix_getcwd_ to simulate failure
char *posix_getcwd_(char *buf, size_t size)
{
    (void)buf;
    (void)size;
    return NULL;
}

// Test: Clear when getcwd fails
START_TEST(test_clear_getcwd_failure)
{
    // Execute /clear - should fail when getcwd returns NULL
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_err(&res));

    // Verify error message contains expected text
    ck_assert_str_eq(res.err->msg, "Failed to get current working directory");

    // Cleanup error
    talloc_free(res.err);
}
END_TEST

static Suite *commands_clear_getcwd_suite(void)
{
    Suite *s = suite_create("Commands/Clear getcwd");
    TCase *tc = tcase_create("GetCWD Failure");

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_clear_getcwd_failure);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = commands_clear_getcwd_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
