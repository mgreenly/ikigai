/**
 * @file cmd_pin_test.c
 * @brief Unit tests for /pin and /unpin commands
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <talloc.h>

static Suite *commands_pin_suite(void);

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
    agent->pinned_paths = NULL;
    agent->pinned_count = 0;
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

START_TEST(test_pin_no_args_empty) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/pin");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 1);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "No pinned documents.");
}
END_TEST

START_TEST(test_pin_add_path) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/pin /path/to/doc.md");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->pinned_count, 1);
    ck_assert_ptr_nonnull(repl->current->pinned_paths);
    ck_assert_str_eq(repl->current->pinned_paths[0], "/path/to/doc.md");

    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 1);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert(strstr(line, "Pinned:") != NULL);
    ck_assert(strstr(line, "/path/to/doc.md") != NULL);
}
END_TEST

START_TEST(test_pin_add_multiple_paths) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/pin /first.md");
    ck_assert(is_ok(&res));

    res = ik_cmd_dispatch(ctx, repl, "/pin /second.md");
    ck_assert(is_ok(&res));

    res = ik_cmd_dispatch(ctx, repl, "/pin /third.md");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->pinned_count, 3);
    ck_assert_str_eq(repl->current->pinned_paths[0], "/first.md");
    ck_assert_str_eq(repl->current->pinned_paths[1], "/second.md");
    ck_assert_str_eq(repl->current->pinned_paths[2], "/third.md");

    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 3);
}
END_TEST

START_TEST(test_pin_duplicate_path) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/pin /doc.md");
    ck_assert(is_ok(&res));

    res = ik_cmd_dispatch(ctx, repl, "/pin /doc.md");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->pinned_count, 1);

    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 1, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert(strstr(line, "Already pinned:") != NULL);
}
END_TEST

START_TEST(test_pin_no_args_with_paths) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/pin /first.md");
    ck_assert(is_ok(&res));
    res = ik_cmd_dispatch(ctx, repl, "/pin /second.md");
    ck_assert(is_ok(&res));

    res = ik_cmd_dispatch(ctx, repl, "/pin");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 4);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "- /first.md") != NULL);

    res = ik_scrollback_get_line_text(repl->current->scrollback, 3, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "- /second.md") != NULL);
}
END_TEST

START_TEST(test_unpin_no_args) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/unpin");
    ck_assert(is_err(&res));

    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 1);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert(strstr(line, "requires a path") != NULL);
}
END_TEST

START_TEST(test_unpin_not_pinned) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/unpin /not-pinned.md");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 1);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert(strstr(line, "Not pinned:") != NULL);
}
END_TEST

START_TEST(test_unpin_removes_path) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/pin /doc.md");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->pinned_count, 1);

    res = ik_cmd_dispatch(ctx, repl, "/unpin /doc.md");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->pinned_count, 0);
    ck_assert_ptr_null(repl->current->pinned_paths);

    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 1, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "Unpinned:") != NULL);
}
END_TEST

START_TEST(test_unpin_removes_middle_path) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/pin /first.md");
    ck_assert(is_ok(&res));
    res = ik_cmd_dispatch(ctx, repl, "/pin /second.md");
    ck_assert(is_ok(&res));
    res = ik_cmd_dispatch(ctx, repl, "/pin /third.md");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->pinned_count, 3);

    res = ik_cmd_dispatch(ctx, repl, "/unpin /second.md");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->pinned_count, 2);
    ck_assert_str_eq(repl->current->pinned_paths[0], "/first.md");
    ck_assert_str_eq(repl->current->pinned_paths[1], "/third.md");
}
END_TEST

START_TEST(test_pin_unpin_cycle) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/pin /doc.md");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->pinned_count, 1);

    res = ik_cmd_dispatch(ctx, repl, "/unpin /doc.md");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->pinned_count, 0);

    res = ik_cmd_dispatch(ctx, repl, "/pin /doc.md");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->pinned_count, 1);
    ck_assert_str_eq(repl->current->pinned_paths[0], "/doc.md");
}
END_TEST

START_TEST(test_pin_ik_uri) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/pin ik://prompts/system.md");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->pinned_count, 1);
    ck_assert_str_eq(repl->current->pinned_paths[0], "ik://prompts/system.md");
}
END_TEST

static Suite *commands_pin_suite(void)
{
    Suite *s = suite_create("commands_pin");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_pin_no_args_empty);
    tcase_add_test(tc_core, test_pin_add_path);
    tcase_add_test(tc_core, test_pin_add_multiple_paths);
    tcase_add_test(tc_core, test_pin_duplicate_path);
    tcase_add_test(tc_core, test_pin_no_args_with_paths);
    tcase_add_test(tc_core, test_unpin_no_args);
    tcase_add_test(tc_core, test_unpin_not_pinned);
    tcase_add_test(tc_core, test_unpin_removes_path);
    tcase_add_test(tc_core, test_unpin_removes_middle_path);
    tcase_add_test(tc_core, test_pin_unpin_cycle);
    tcase_add_test(tc_core, test_pin_ik_uri);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = commands_pin_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands/cmd_pin_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
