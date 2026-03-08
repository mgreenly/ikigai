/**
 * @file agent_restore_replay_skillset_test.c
 * @brief Tests for skillset event replay during agent restore
 */
#include "tests/test_constants.h"

#include "apps/ikigai/repl/agent_restore_replay.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/replay.h"
#include "shared/error.h"
#include "shared/logger.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/scrollback.h"
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

static ik_msg_t *make_msg(TALLOC_CTX *ctx, int64_t id, const char *kind,
                           const char *content, const char *data_json)
{
    ik_msg_t *msg = talloc_zero(ctx, ik_msg_t);
    msg->id = id;
    msg->kind = kind ? talloc_strdup(msg, kind) : NULL;
    msg->content = content ? talloc_strdup(msg, content) : NULL;
    msg->data_json = data_json ? talloc_strdup(msg, data_json) : NULL;
    msg->interrupted = false;
    return msg;
}

static ik_replay_context_t *make_replay_ctx(TALLOC_CTX *ctx, ik_msg_t **msgs, size_t count)
{
    ik_replay_context_t *replay_ctx = talloc_zero(ctx, ik_replay_context_t);
    replay_ctx->capacity = count;
    replay_ctx->count = count;
    replay_ctx->messages = talloc_array(replay_ctx, ik_msg_t *, (unsigned int)count);
    for (size_t i = 0; i < count; i++) {
        replay_ctx->messages[i] = msgs[i];
    }
    return replay_ctx;
}

/* ---- skillset replay: adds catalog entries to skillset_catalog[] ---- */

START_TEST(test_replay_skillset_adds_catalog_entries) {
    ik_agent_ctx_t *agent = create_test_agent();

    ik_msg_t *msg = make_msg(test_ctx, 1, "skillset", NULL,
                              "{\"skillset\":\"developer\","
                              "\"catalog_entries\":["
                              "{\"skill\":\"database\",\"description\":\"PostgreSQL schema\"},"
                              "{\"skill\":\"style\",\"description\":\"Code style\"}]}");

    ik_agent_restore_replay_skillset(agent, msg, 2);

    ck_assert_uint_eq(agent->skillset_catalog_count, 2);
    ck_assert_str_eq(agent->skillset_catalog[0]->skill_name, "database");
    ck_assert_str_eq(agent->skillset_catalog[0]->description, "PostgreSQL schema");
    ck_assert_uint_eq(agent->skillset_catalog[0]->load_position, 2);
    ck_assert_str_eq(agent->skillset_catalog[1]->skill_name, "style");
    ck_assert_uint_eq(agent->skillset_catalog[1]->load_position, 2);
}
END_TEST

/* ---- skillset replay: NULL data_json is a no-op ---- */

START_TEST(test_replay_skillset_null_data_json) {
    ik_agent_ctx_t *agent = create_test_agent();

    ik_msg_t *msg = make_msg(test_ctx, 1, "skillset", NULL, NULL);

    ik_agent_restore_replay_skillset(agent, msg, 0);

    ck_assert_uint_eq(agent->skillset_catalog_count, 0);
}
END_TEST

/* ---- populate_scrollback: replays skillset event ---- */

START_TEST(test_populate_scrollback_skillset) {
    ik_agent_ctx_t *agent = create_test_agent();

    ik_msg_t *msgs[1];
    msgs[0] = make_msg(test_ctx, 1, "skillset", NULL,
                       "{\"skillset\":\"developer\","
                       "\"catalog_entries\":[{\"skill\":\"db\",\"description\":\"DB\"}]}");

    ik_replay_context_t *replay_ctx = make_replay_ctx(test_ctx, msgs, 1);

    ik_agent_restore_populate_scrollback(agent, replay_ctx, shared_ctx->logger);

    ck_assert_uint_eq(agent->skillset_catalog_count, 1);
    ck_assert_str_eq(agent->skillset_catalog[0]->skill_name, "db");

    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    ck_assert_uint_ge(count, 1);
}
END_TEST

/* ---- populate_scrollback: rewind trims catalog entries ---- */

START_TEST(test_populate_scrollback_rewind_trims_catalog) {
    ik_agent_ctx_t *agent = create_test_agent();

    ik_msg_t *msgs[3];
    msgs[0] = make_msg(test_ctx, 1, "mark", NULL, "{\"label\":\"start\"}");
    msgs[1] = make_msg(test_ctx, 2, "skillset", NULL,
                       "{\"skillset\":\"developer\","
                       "\"catalog_entries\":[{\"skill\":\"db\",\"description\":\"DB\"}]}");
    msgs[2] = make_msg(test_ctx, 3, "rewind", NULL,
                       "{\"target_message_id\":1,\"target_label\":\"start\"}");

    ik_replay_context_t *replay_ctx = make_replay_ctx(test_ctx, msgs, 3);

    ik_agent_restore_populate_scrollback(agent, replay_ctx, shared_ctx->logger);

    ck_assert_uint_eq(agent->skillset_catalog_count, 0);
}
END_TEST

/* ---- fork event replay: catalog_entries from fork snapshot populate agent ---- */

START_TEST(test_fork_event_with_skillset_catalog) {
    ik_agent_ctx_t *agent = create_test_agent();

    const char *fork_json =
        "{\"role\":\"child\","
        "\"pinned_paths\":[],"
        "\"toolset_filter\":[],"
        "\"loaded_skills\":[],"
        "\"skillset_catalog\":["
        "{\"skill\":\"database\",\"description\":\"PostgreSQL schema\"},"
        "{\"skill\":\"style\",\"description\":\"Code style\"}]}";

    ik_msg_t *msg = make_msg(test_ctx, 1, "fork", NULL, fork_json);

    ik_agent_restore_replay_command_effects(agent, msg, shared_ctx->logger);

    ck_assert_uint_eq(agent->skillset_catalog_count, 2);
    ck_assert_str_eq(agent->skillset_catalog[0]->skill_name, "database");
    ck_assert_str_eq(agent->skillset_catalog[0]->description, "PostgreSQL schema");
    ck_assert_uint_eq(agent->skillset_catalog[0]->load_position, 0);
    ck_assert_str_eq(agent->skillset_catalog[1]->skill_name, "style");
}
END_TEST

/* ========== Suite Configuration ========== */

static Suite *agent_restore_replay_skillset_suite(void)
{
    Suite *s = suite_create("Agent Restore Replay - Skillset");

    TCase *tc_skillset = tcase_create("SkillsetReplay");
    tcase_add_checked_fixture(tc_skillset, setup, teardown);
    tcase_add_test(tc_skillset, test_replay_skillset_adds_catalog_entries);
    tcase_add_test(tc_skillset, test_replay_skillset_null_data_json);
    tcase_add_test(tc_skillset, test_populate_scrollback_skillset);
    tcase_add_test(tc_skillset, test_populate_scrollback_rewind_trims_catalog);
    tcase_add_test(tc_skillset, test_fork_event_with_skillset_catalog);
    suite_add_tcase(s, tc_skillset);

    return s;
}

int main(void)
{
    Suite *s = agent_restore_replay_skillset_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/repl/agent_restore_replay_skillset_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
