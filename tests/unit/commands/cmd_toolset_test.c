/**
 * @file cmd_toolset_test.c
 * @brief Unit tests for /toolset command
 */

#include "../../../src/agent.h"
#include "../../../src/ansi.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <talloc.h>

static Suite *commands_toolset_suite(void);

static void *ctx;
static ik_repl_ctx_t *repl;

static ik_repl_ctx_t *create_test_repl(void *parent)
{
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    shared->cfg = cfg;
    shared->db_ctx = NULL;
    shared->session_id = 0;

    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;
    agent->uuid = talloc_strdup(agent, "test-agent-uuid");
    agent->toolset_filter = NULL;
    agent->toolset_count = 0;
    agent->shared = shared;
    r->current = agent;
    r->shared = shared;

    return r;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);
    repl = create_test_repl(ctx);
    ck_assert_ptr_nonnull(repl);
}

static void teardown(void)
{
    talloc_free(ctx);
}

START_TEST(test_toolset_no_args_empty) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/toolset");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 4);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "No toolset filter active");
}
END_TEST

static Suite *commands_toolset_suite(void)
{
    Suite *s = suite_create("commands_toolset");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_toolset_no_args_empty);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = commands_toolset_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands/cmd_toolset_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
