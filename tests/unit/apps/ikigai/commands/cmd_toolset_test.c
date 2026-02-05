/**
 * @file cmd_toolset_test.c
 * @brief Unit tests for /toolset command
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/ansi.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/config.h"
#include "shared/error.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "tests/helpers/test_utils_helper.h"

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

START_TEST(test_toolset_set_single) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/toolset bash");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->toolset_count, 1);
    ck_assert_ptr_nonnull(repl->current->toolset_filter);
    ck_assert_str_eq(repl->current->toolset_filter[0], "bash");

    res = ik_cmd_dispatch(ctx, repl, "/toolset");
    ck_assert(is_ok(&res));

    size_t count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_ge(count, 5);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, count - 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert(strstr(line, "bash") != NULL);
}
END_TEST

START_TEST(test_toolset_set_multiple_comma) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/toolset bash, file_read, file_write");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->toolset_count, 3);
    ck_assert_ptr_nonnull(repl->current->toolset_filter);
    ck_assert_str_eq(repl->current->toolset_filter[0], "bash");
    ck_assert_str_eq(repl->current->toolset_filter[1], "file_read");
    ck_assert_str_eq(repl->current->toolset_filter[2], "file_write");
}
END_TEST

START_TEST(test_toolset_set_multiple_space) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/toolset bash file_read file_write");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->toolset_count, 3);
    ck_assert_ptr_nonnull(repl->current->toolset_filter);
    ck_assert_str_eq(repl->current->toolset_filter[0], "bash");
    ck_assert_str_eq(repl->current->toolset_filter[1], "file_read");
    ck_assert_str_eq(repl->current->toolset_filter[2], "file_write");
}
END_TEST

START_TEST(test_toolset_set_replace) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/toolset bash, file_read");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->toolset_count, 2);

    res = ik_cmd_dispatch(ctx, repl, "/toolset glob");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->toolset_count, 1);
    ck_assert_ptr_nonnull(repl->current->toolset_filter);
    ck_assert_str_eq(repl->current->toolset_filter[0], "glob");
}
END_TEST

START_TEST(test_toolset_messy_whitespace) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/toolset  ,, bash ,  , file_read  ,");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->toolset_count, 2);
    ck_assert_ptr_nonnull(repl->current->toolset_filter);
    ck_assert_str_eq(repl->current->toolset_filter[0], "bash");
    ck_assert_str_eq(repl->current->toolset_filter[1], "file_read");
}
END_TEST

START_TEST(test_toolset_capacity_overflow) {
    char cmd[512];
    int32_t n = snprintf(cmd, sizeof(cmd),
        "/toolset t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14,t15,t16,t17,t18");
    ck_assert_int_gt(n, 0);
    ck_assert_int_lt(n, (int32_t)sizeof(cmd));

    res_t res = ik_cmd_dispatch(ctx, repl, cmd);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->toolset_count, 18);
}
END_TEST

START_TEST(test_toolset_list_with_colors_disabled) {
    setenv("NO_COLOR", "1", 1);
    ik_ansi_init();

    res_t res = ik_cmd_dispatch(ctx, repl, "/toolset bash,grep");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->toolset_count, 2);

    res = ik_cmd_dispatch(ctx, repl, "/toolset");
    ck_assert(is_ok(&res));

    size_t count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_ge(count, 5);

    unsetenv("NO_COLOR");
    ik_ansi_init();
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

    TCase *tc_set = tcase_create("Set");
    tcase_set_timeout(tc_set, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_set, setup, teardown);

    tcase_add_test(tc_set, test_toolset_set_single);
    tcase_add_test(tc_set, test_toolset_set_multiple_comma);
    tcase_add_test(tc_set, test_toolset_set_multiple_space);
    tcase_add_test(tc_set, test_toolset_set_replace);
    tcase_add_test(tc_set, test_toolset_messy_whitespace);
    tcase_add_test(tc_set, test_toolset_capacity_overflow);
    tcase_add_test(tc_set, test_toolset_list_with_colors_disabled);

    suite_add_tcase(s, tc_set);

    return s;
}

int main(void)
{
    Suite *s = commands_toolset_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/cmd_toolset_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
