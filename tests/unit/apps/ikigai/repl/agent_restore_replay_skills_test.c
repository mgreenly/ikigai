/**
 * @file agent_restore_replay_skills_test.c
 * @brief Tests for skill replay during agent restore
 *
 * Tests replay of skill_load, skill_unload, and rewind events during
 * session restore, and fork event skill snapshot handling.
 */

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

/* Helper: Create a minimal agent */
static ik_agent_ctx_t *create_test_agent(void)
{
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared_ctx, NULL, &agent);
    ck_assert(is_ok(&res));
    return agent;
}

/* Helper: Create an ik_msg_t with given fields */
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

/* Helper: Build replay context from array of msgs */
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

/* Helper: Add an existing skill to agent for setup */
static void add_skill(ik_agent_ctx_t *agent, const char *name, size_t load_position)
{
    size_t new_cap = agent->loaded_skill_count + 1;
    agent->loaded_skills = talloc_realloc(agent, agent->loaded_skills,
                                          ik_loaded_skill_t *,
                                          (unsigned int)new_cap);
    ik_loaded_skill_t *skill = talloc_zero(agent, ik_loaded_skill_t);
    skill->name = talloc_strdup(skill, name);
    skill->content = talloc_strdup(skill, "content");
    skill->load_position = load_position;
    agent->loaded_skills[agent->loaded_skill_count] = skill;
    agent->loaded_skill_count++;
}

/* ---- skill_load replay: adds skill to loaded_skills[] ---- */

START_TEST(test_replay_skill_load_adds_skill) {
    ik_agent_ctx_t *agent = create_test_agent();

    ik_msg_t *msg = make_msg(test_ctx, 1, "skill_load", NULL,
                              "{\"skill\":\"database\",\"content\":\"# DB content\"}");

    ik_agent_restore_replay_skill_load(agent, msg, 3);

    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "database");
    ck_assert_str_eq(agent->loaded_skills[0]->content, "# DB content");
    ck_assert_uint_eq(agent->loaded_skills[0]->load_position, 3);
}
END_TEST

/* ---- skill_load replay: replaces existing skill with same name ---- */

START_TEST(test_replay_skill_load_replaces_existing) {
    ik_agent_ctx_t *agent = create_test_agent();
    add_skill(agent, "database", 0);

    ik_msg_t *msg = make_msg(test_ctx, 2, "skill_load", NULL,
                              "{\"skill\":\"database\",\"content\":\"updated\"}");

    ik_agent_restore_replay_skill_load(agent, msg, 5);

    /* Still only one skill */
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "database");
    ck_assert_str_eq(agent->loaded_skills[0]->content, "updated");
    ck_assert_uint_eq(agent->loaded_skills[0]->load_position, 5);
}
END_TEST

/* ---- skill_load replay: NULL data_json is a no-op ---- */

START_TEST(test_replay_skill_load_null_data_json) {
    ik_agent_ctx_t *agent = create_test_agent();

    ik_msg_t *msg = make_msg(test_ctx, 1, "skill_load", NULL, NULL);

    ik_agent_restore_replay_skill_load(agent, msg, 0);

    ck_assert_uint_eq(agent->loaded_skill_count, 0);
}
END_TEST

/* ---- skill_unload replay: removes skill by name ---- */

START_TEST(test_replay_skill_unload_removes_skill) {
    ik_agent_ctx_t *agent = create_test_agent();
    add_skill(agent, "database", 0);
    add_skill(agent, "style", 1);

    ik_msg_t *msg = make_msg(test_ctx, 3, "skill_unload", NULL,
                              "{\"skill\":\"database\"}");

    ik_agent_restore_replay_skill_unload(agent, msg);

    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "style");
}
END_TEST

/* ---- skill_unload replay: missing skill is a no-op ---- */

START_TEST(test_replay_skill_unload_missing_skill) {
    ik_agent_ctx_t *agent = create_test_agent();
    add_skill(agent, "database", 0);

    ik_msg_t *msg = make_msg(test_ctx, 3, "skill_unload", NULL,
                              "{\"skill\":\"nonexistent\"}");

    ik_agent_restore_replay_skill_unload(agent, msg);

    /* Skill count unchanged */
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
}
END_TEST

/* ---- populate_scrollback: replays skill_load event ---- */

START_TEST(test_populate_scrollback_skill_load) {
    ik_agent_ctx_t *agent = create_test_agent();

    /* One user message (conv_count=1 after), then a skill_load */
    ik_msg_t *msgs[2];
    msgs[0] = make_msg(test_ctx, 10, "user", "hello", NULL);
    msgs[1] = make_msg(test_ctx, 11, "skill_load", NULL,
                       "{\"skill\":\"errors\",\"content\":\"# Errors\"}");

    ik_replay_context_t *replay_ctx = make_replay_ctx(test_ctx, msgs, 2);

    ik_agent_restore_populate_scrollback(agent, replay_ctx, shared_ctx->logger);

    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "errors");
    /* load_position should reflect 1 conversation message seen before the skill_load */
    ck_assert_uint_eq(agent->loaded_skills[0]->load_position, 1);
}
END_TEST

/* ---- populate_scrollback: replays skill_unload event ---- */

START_TEST(test_populate_scrollback_skill_unload) {
    ik_agent_ctx_t *agent = create_test_agent();

    ik_msg_t *msgs[2];
    msgs[0] = make_msg(test_ctx, 10, "skill_load", NULL,
                       "{\"skill\":\"memory\",\"content\":\"# Memory\"}");
    msgs[1] = make_msg(test_ctx, 11, "skill_unload", NULL,
                       "{\"skill\":\"memory\"}");

    ik_replay_context_t *replay_ctx = make_replay_ctx(test_ctx, msgs, 2);

    ik_agent_restore_populate_scrollback(agent, replay_ctx, shared_ctx->logger);

    ck_assert_uint_eq(agent->loaded_skill_count, 0);
}
END_TEST

/* ---- populate_scrollback: rewind trims skills loaded after mark ---- */

START_TEST(test_populate_scrollback_rewind_trims_skills) {
    ik_agent_ctx_t *agent = create_test_agent();

    /* Sequence:
     * id=1: mark event (mark created at conv_count=0)
     * id=2: skill_load "database" at conv_count=0
     * id=3: rewind targeting mark at id=1 (conv_count at mark=0)
     *
     * After replay: skill_load at load_position=0, rewind target_conv=0
     * So skill with load_position >= 0 is trimmed → 0 skills remain.
     */
    ik_msg_t *msgs[3];
    msgs[0] = make_msg(test_ctx, 1, "mark", NULL, "{\"label\":\"checkpoint\"}");
    msgs[1] = make_msg(test_ctx, 2, "skill_load", NULL,
                       "{\"skill\":\"database\",\"content\":\"# DB\"}");
    msgs[2] = make_msg(test_ctx, 3, "rewind", NULL,
                       "{\"target_message_id\":1,\"target_label\":\"checkpoint\"}");

    ik_replay_context_t *replay_ctx = make_replay_ctx(test_ctx, msgs, 3);

    ik_agent_restore_populate_scrollback(agent, replay_ctx, shared_ctx->logger);

    /* Skill loaded after mark should be trimmed by rewind */
    ck_assert_uint_eq(agent->loaded_skill_count, 0);
}
END_TEST

/* ---- populate_scrollback: rewind keeps skills loaded before mark ---- */

START_TEST(test_populate_scrollback_rewind_keeps_earlier_skills) {
    ik_agent_ctx_t *agent = create_test_agent();

    /* Sequence:
     * id=1: skill_load "memory" at conv_count=0
     * id=2: mark at conv_count=0
     * id=3: skill_load "errors" at conv_count=0
     * id=4: rewind targeting mark at id=2 (conv_count=0)
     *
     * "memory" has load_position=0, mark is at conv_count=0
     * So trim skills with load_position >= 0 → both trimmed!
     *
     * But actually: mark was created at conv_count=0 (no conv messages before it).
     * skill "memory" was loaded at load_position=0.
     * skill "errors" was loaded at load_position=0.
     * rewind target_conv=0: trim all skills with load_position >= 0 → all trimmed.
     *
     * To test "keeps earlier skills", we need conv messages before the mark.
     */

    /* Sequence:
     * id=1: user message (conv_count becomes 1)
     * id=2: skill_load "memory" at conv_count=1
     * id=3: mark at conv_count=1 (mark conv_count=1)
     * id=4: skill_load "errors" at conv_count=1
     * id=5: rewind targeting mark at id=3 (target_conv=1)
     *
     * trim skills with load_position >= 1:
     * "memory" has load_position=1 → trimmed
     * "errors" has load_position=1 → trimmed
     * Hmm... both still trimmed.
     *
     * To keep a skill: load it BEFORE the mark position with conv_count < target.
     * id=1: skill_load "memory" at conv_count=0 (load_position=0)
     * id=2: user message (conv_count becomes 1)
     * id=3: mark at conv_count=1
     * id=4: skill_load "errors" at conv_count=1 (load_position=1)
     * id=5: rewind to mark (target_conv=1)
     * trim load_position >= 1: "errors" trimmed, "memory" kept.
     */
    ik_msg_t *msgs[5];
    msgs[0] = make_msg(test_ctx, 1, "skill_load", NULL,
                       "{\"skill\":\"memory\",\"content\":\"# Memory\"}");
    msgs[1] = make_msg(test_ctx, 2, "user", "hello", NULL);
    msgs[2] = make_msg(test_ctx, 3, "mark", NULL, "{\"label\":\"checkpoint\"}");
    msgs[3] = make_msg(test_ctx, 4, "skill_load", NULL,
                       "{\"skill\":\"errors\",\"content\":\"# Errors\"}");
    msgs[4] = make_msg(test_ctx, 5, "rewind", NULL,
                       "{\"target_message_id\":3,\"target_label\":\"checkpoint\"}");

    ik_replay_context_t *replay_ctx = make_replay_ctx(test_ctx, msgs, 5);

    ik_agent_restore_populate_scrollback(agent, replay_ctx, shared_ctx->logger);

    /* "memory" loaded at position 0 should survive (trim threshold=1) */
    /* "errors" loaded at position 1 should be trimmed */
    ck_assert_uint_eq(agent->loaded_skill_count, 1);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "memory");
}
END_TEST

/* ---- fork event replay: skills from fork snapshot populate agent ---- */

START_TEST(test_fork_event_with_loaded_skills) {
    ik_agent_ctx_t *agent = create_test_agent();

    const char *fork_json =
        "{\"role\":\"child\","
        "\"pinned_paths\":[],"
        "\"toolset_filter\":[],"
        "\"loaded_skills\":[{\"skill\":\"database\",\"content\":\"# DB\"},"
        "{\"skill\":\"style\",\"content\":\"# Style\"}]}";

    ik_msg_t *msg = make_msg(test_ctx, 1, "fork", NULL, fork_json);

    ik_agent_restore_replay_command_effects(agent, msg, shared_ctx->logger);

    ck_assert_uint_eq(agent->loaded_skill_count, 2);
    ck_assert_str_eq(agent->loaded_skills[0]->name, "database");
    ck_assert_str_eq(agent->loaded_skills[1]->name, "style");
    /* load_position should be 0 (inherited from parent with reset) */
    ck_assert_uint_eq(agent->loaded_skills[0]->load_position, 0);
    ck_assert_uint_eq(agent->loaded_skills[1]->load_position, 0);
}
END_TEST

/* ---- fork event replay: empty loaded_skills (prompt fork) ---- */

START_TEST(test_fork_event_with_empty_skills) {
    ik_agent_ctx_t *agent = create_test_agent();
    /* Pre-populate with a skill to ensure the fork event clears it */
    add_skill(agent, "existing", 0);

    const char *fork_json =
        "{\"role\":\"child\","
        "\"pinned_paths\":[],"
        "\"toolset_filter\":[],"
        "\"loaded_skills\":[]}";

    ik_msg_t *msg = make_msg(test_ctx, 1, "fork", NULL, fork_json);

    ik_agent_restore_replay_command_effects(agent, msg, shared_ctx->logger);

    /* Empty loaded_skills array means no skills (prompt fork) */
    ck_assert_uint_eq(agent->loaded_skill_count, 0);
}
END_TEST

/* ---- skill_load renders to scrollback without error ---- */

START_TEST(test_skill_load_renders_to_scrollback) {
    ik_agent_ctx_t *agent = create_test_agent();

    ik_msg_t *msgs[1];
    msgs[0] = make_msg(test_ctx, 1, "skill_load", NULL,
                       "{\"skill\":\"database\",\"content\":\"# DB\"}");

    ik_replay_context_t *replay_ctx = make_replay_ctx(test_ctx, msgs, 1);

    /* Should not crash or log render errors */
    ik_agent_restore_populate_scrollback(agent, replay_ctx, shared_ctx->logger);

    /* Scrollback should have at least one line (the !load <name> line) */
    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    ck_assert_uint_ge(count, 1);
}
END_TEST

/* ---- skill_unload renders to scrollback without error ---- */

START_TEST(test_skill_unload_renders_to_scrollback) {
    ik_agent_ctx_t *agent = create_test_agent();

    ik_msg_t *msgs[1];
    msgs[0] = make_msg(test_ctx, 1, "skill_unload", NULL,
                       "{\"skill\":\"database\"}");

    ik_replay_context_t *replay_ctx = make_replay_ctx(test_ctx, msgs, 1);

    ik_agent_restore_populate_scrollback(agent, replay_ctx, shared_ctx->logger);

    size_t count = ik_scrollback_get_line_count(agent->scrollback);
    ck_assert_uint_ge(count, 1);
}
END_TEST

/* ========== Suite Configuration ========== */

static Suite *agent_restore_replay_skills_suite(void)
{
    Suite *s = suite_create("Agent Restore Replay - Skills");

    TCase *tc_skill_load = tcase_create("SkillLoad");
    tcase_add_checked_fixture(tc_skill_load, setup, teardown);
    tcase_add_test(tc_skill_load, test_replay_skill_load_adds_skill);
    tcase_add_test(tc_skill_load, test_replay_skill_load_replaces_existing);
    tcase_add_test(tc_skill_load, test_replay_skill_load_null_data_json);
    suite_add_tcase(s, tc_skill_load);

    TCase *tc_skill_unload = tcase_create("SkillUnload");
    tcase_add_checked_fixture(tc_skill_unload, setup, teardown);
    tcase_add_test(tc_skill_unload, test_replay_skill_unload_removes_skill);
    tcase_add_test(tc_skill_unload, test_replay_skill_unload_missing_skill);
    suite_add_tcase(s, tc_skill_unload);

    TCase *tc_scrollback = tcase_create("ScrollbackReplay");
    tcase_add_checked_fixture(tc_scrollback, setup, teardown);
    tcase_add_test(tc_scrollback, test_populate_scrollback_skill_load);
    tcase_add_test(tc_scrollback, test_populate_scrollback_skill_unload);
    tcase_add_test(tc_scrollback, test_populate_scrollback_rewind_trims_skills);
    tcase_add_test(tc_scrollback, test_populate_scrollback_rewind_keeps_earlier_skills);
    tcase_add_test(tc_scrollback, test_skill_load_renders_to_scrollback);
    tcase_add_test(tc_scrollback, test_skill_unload_renders_to_scrollback);
    suite_add_tcase(s, tc_scrollback);

    TCase *tc_fork = tcase_create("ForkSkills");
    tcase_add_checked_fixture(tc_fork, setup, teardown);
    tcase_add_test(tc_fork, test_fork_event_with_loaded_skills);
    tcase_add_test(tc_fork, test_fork_event_with_empty_skills);
    suite_add_tcase(s, tc_fork);

    return s;
}

int main(void)
{
    Suite *s = agent_restore_replay_skills_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/agent_restore_replay_skills_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
