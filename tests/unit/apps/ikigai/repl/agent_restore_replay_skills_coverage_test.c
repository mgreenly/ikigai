/**
 * @file agent_restore_replay_skills_coverage_test.c
 * @brief Coverage tests for skill replay edge cases
 */
#include "tests/test_constants.h"

#include "apps/ikigai/repl/agent_restore_replay.h"
#include "apps/ikigai/agent.h"
#include "shared/error.h"
#include "shared/logger.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/shared.h"
#include "tests/helpers/test_utils_helper.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

static TALLOC_CTX *test_ctx;
static ik_shared_ctx_t *shared_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->logger = ik_logger_create(shared_ctx, "/tmp");
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

static ik_agent_ctx_t *create_test_agent(void)
{
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared_ctx, NULL, &agent);
    ck_assert(is_ok(&res));
    return agent;
}

static ik_msg_t *make_msg(TALLOC_CTX *ctx, const char *kind, const char *data_json)
{
    ik_msg_t *msg = talloc_zero(ctx, ik_msg_t);
    msg->kind = kind ? talloc_strdup(msg, kind) : NULL;
    msg->data_json = data_json ? talloc_strdup(msg, data_json) : NULL;
    return msg;
}

/* ---- skill_load: invalid JSON → no-op ---- */

START_TEST(test_skill_load_invalid_json_noop) {
    ik_agent_ctx_t *agent = create_test_agent();
    ik_msg_t *msg = make_msg(test_ctx, "skill_load", "NOT VALID JSON!!!");

    ik_agent_restore_replay_skill_load(agent, msg, 0);

    ck_assert_uint_eq(agent->loaded_skill_count, 0);
}
END_TEST

/* ---- skill_load: null skill field → no-op ---- */

START_TEST(test_skill_load_null_skill_field_noop) {
    ik_agent_ctx_t *agent = create_test_agent();
    /* "skill" value is null → skill_name becomes NULL */
    ik_msg_t *msg = make_msg(test_ctx, "skill_load",
                             "{\"skill\":null,\"content\":\"# hi\"}");

    ik_agent_restore_replay_skill_load(agent, msg, 0);

    ck_assert_uint_eq(agent->loaded_skill_count, 0);
}
END_TEST

/* ---- skill_unload: null data_json → no-op ---- */

START_TEST(test_skill_unload_null_data_json_noop) {
    ik_agent_ctx_t *agent = create_test_agent();
    ik_msg_t *msg = make_msg(test_ctx, "skill_unload", NULL);

    ik_agent_restore_replay_skill_unload(agent, msg);

    ck_assert_uint_eq(agent->loaded_skill_count, 0);
}
END_TEST

/* ---- skill_unload: invalid JSON → no-op ---- */

START_TEST(test_skill_unload_invalid_json_noop) {
    ik_agent_ctx_t *agent = create_test_agent();
    ik_msg_t *msg = make_msg(test_ctx, "skill_unload", "GARBAGE JSON!!!");

    ik_agent_restore_replay_skill_unload(agent, msg);

    ck_assert_uint_eq(agent->loaded_skill_count, 0);
}
END_TEST

/* ---- skillset replay: invalid JSON → no-op ---- */

START_TEST(test_skillset_invalid_json_noop) {
    ik_agent_ctx_t *agent = create_test_agent();
    ik_msg_t *msg = make_msg(test_ctx, "skillset", "GARBAGE JSON!!!");

    ik_agent_restore_replay_skillset(agent, msg, 0);

    ck_assert_uint_eq(agent->skillset_catalog_count, 0);
}
END_TEST

/* ---- skillset replay: entries not array → no-op ---- */

START_TEST(test_skillset_entries_not_array_noop) {
    ik_agent_ctx_t *agent = create_test_agent();
    /* catalog_entries is an integer, not an array */
    ik_msg_t *msg = make_msg(test_ctx, "skillset",
                             "{\"catalog_entries\":42}");

    ik_agent_restore_replay_skillset(agent, msg, 0);

    ck_assert_uint_eq(agent->skillset_catalog_count, 0);
}
END_TEST

/* ---- catalog_entry_add: null skill_name → no-op ---- */

START_TEST(test_catalog_entry_add_null_skill_name_noop) {
    ik_agent_ctx_t *agent = create_test_agent();

    ik_agent_restore_replay_catalog_entry_add(agent, NULL, "desc", 0);

    ck_assert_uint_eq(agent->skillset_catalog_count, 0);
}
END_TEST

/* ---- skill_load named: null skill_name → no-op ---- */

START_TEST(test_skill_load_named_null_noop) {
    ik_agent_ctx_t *agent = create_test_agent();

    ik_agent_restore_replay_skill_load_named(agent, NULL, "content", 0);

    ck_assert_uint_eq(agent->loaded_skill_count, 0);
}
END_TEST

/* ========== Suite Configuration ========== */

static Suite *skills_coverage_suite(void)
{
    Suite *s = suite_create("Agent Restore Replay Skills Coverage");

    TCase *tc = tcase_create("edge_cases");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_skill_load_invalid_json_noop);
    tcase_add_test(tc, test_skill_load_null_skill_field_noop);
    tcase_add_test(tc, test_skill_unload_null_data_json_noop);
    tcase_add_test(tc, test_skill_unload_invalid_json_noop);
    tcase_add_test(tc, test_skillset_invalid_json_noop);
    tcase_add_test(tc, test_skillset_entries_not_array_noop);
    tcase_add_test(tc, test_catalog_entry_add_null_skill_name_noop);
    tcase_add_test(tc, test_skill_load_named_null_noop);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = skills_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/repl/agent_restore_replay_skills_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
