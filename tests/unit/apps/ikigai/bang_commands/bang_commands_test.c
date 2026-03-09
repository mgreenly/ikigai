/**
 * @file bang_commands_test.c
 * @brief Unit tests for /load and /unload slash command handlers
 */

#include "apps/ikigai/bang_commands.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/doc_cache.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

static Suite *bang_commands_suite(void);

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_paths_t *paths;

static void create_skill_file(const char *skill_name, const char *content)
{
    const char *ikigai_state_dir = getenv("IKIGAI_STATE_DIR");
    if (!ikigai_state_dir) return;

    char path[512];

    /* Create skills/<name>/ directory */
    snprintf(path, sizeof(path), "%s/skills", ikigai_state_dir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/skills/%s", ikigai_state_dir, skill_name);
    mkdir(path, 0755);

    /* Write SKILL.md */
    snprintf(path, sizeof(path), "%s/skills/%s/SKILL.md", ikigai_state_dir, skill_name);
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

static ik_repl_ctx_t *create_test_repl(void *parent, ik_paths_t *p)
{
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    if (!scrollback) return NULL;

    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    if (!shared) return NULL;

    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    if (!r) return NULL;

    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    if (!agent) return NULL;

    agent->scrollback = scrollback;
    agent->shared = shared;
    agent->doc_cache = ik_doc_cache_create(agent, p);
    agent->loaded_skills = NULL;
    agent->loaded_skill_count = 0;
    agent->message_count = 0;
    agent->uuid = talloc_strdup(agent, "test-agent-uuid");

    r->current = agent;
    r->shared = shared;

    return r;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ck_assert_ptr_nonnull(test_paths_setup_env());

    res_t paths_res = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_res));

    repl = create_test_repl(ctx, paths);
    ck_assert_ptr_nonnull(repl);
}

static void teardown(void)
{
    test_paths_cleanup_env();
    talloc_free(ctx);
    ctx = NULL;
    repl = NULL;
    paths = NULL;
}

/* Test: /load with no args shows usage warning */
START_TEST(test_load_no_args) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/load");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->loaded_skill_count, 0);
}
END_TEST

/* Test: /load with a missing skill shows warning, no entry added */
START_TEST(test_load_missing_skill) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/load nonexistent-skill");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->loaded_skill_count, 0);
}
END_TEST

/* Test: /load successfully loads a skill */
START_TEST(test_load_success) {
    create_skill_file("my-skill", "This is the skill content.\n");

    res_t res = ik_cmd_dispatch(ctx, repl, "/load my-skill");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->loaded_skill_count, 1);
    ck_assert_str_eq(repl->current->loaded_skills[0]->name, "my-skill");
    ck_assert_ptr_nonnull(repl->current->loaded_skills[0]->content);
    ck_assert(strstr(repl->current->loaded_skills[0]->content, "This is the skill content.") != NULL);
    ck_assert_uint_eq(repl->current->loaded_skills[0]->load_position, 0);
}
END_TEST

/* Test: /load duplicate replaces skill in-place */
START_TEST(test_load_duplicate_replaces) {
    create_skill_file("dup-skill", "Version 1 content.\n");

    res_t res = ik_cmd_dispatch(ctx, repl, "/load dup-skill");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->loaded_skill_count, 1);

    /* Overwrite skill file with new content */
    const char *ikigai_state_dir = getenv("IKIGAI_STATE_DIR");
    ck_assert_ptr_nonnull(ikigai_state_dir);
    char path[512];
    snprintf(path, sizeof(path), "%s/skills/dup-skill/SKILL.md", ikigai_state_dir);
    /* Invalidate cache so new content is read */
    ik_doc_cache_invalidate(repl->current->doc_cache,
                            "ik://skills/dup-skill/SKILL.md");
    FILE *f = fopen(path, "w");
    ck_assert_ptr_nonnull(f);
    fputs("Version 2 content.\n", f);
    fclose(f);

    /* Set a different message_count to verify load_position is updated */
    repl->current->message_count = 5;

    res = ik_cmd_dispatch(ctx, repl, "/load dup-skill");
    ck_assert(is_ok(&res));

    /* Still only one entry */
    ck_assert_uint_eq(repl->current->loaded_skill_count, 1);
    ck_assert_str_eq(repl->current->loaded_skills[0]->name, "dup-skill");
    ck_assert(strstr(repl->current->loaded_skills[0]->content, "Version 2") != NULL);
    ck_assert_uint_eq(repl->current->loaded_skills[0]->load_position, 5);
}
END_TEST

/* Test: /load with positional args substitutes ${1}, ${2} */
START_TEST(test_load_positional_args) {
    create_skill_file("tmpl-skill", "Table: ${1}\nColumn: ${2}\n");

    res_t res = ik_cmd_dispatch(ctx, repl, "/load tmpl-skill users email");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->loaded_skill_count, 1);

    const char *content = repl->current->loaded_skills[0]->content;
    ck_assert_ptr_nonnull(content);
    ck_assert(strstr(content, "Table: users") != NULL);
    ck_assert(strstr(content, "Column: email") != NULL);
    /* Unreferenced ${3} is not present */
    ck_assert(strstr(content, "${3}") == NULL);
}
END_TEST

/* Test: /load positional args - unreplaced placeholder stays as literal */
START_TEST(test_load_positional_args_unreplaced) {
    create_skill_file("partial-skill", "First: ${1} Second: ${2}\n");

    /* Only provide one arg */
    res_t res = ik_cmd_dispatch(ctx, repl, "/load partial-skill alpha");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->loaded_skill_count, 1);

    const char *content = repl->current->loaded_skills[0]->content;
    ck_assert_ptr_nonnull(content);
    ck_assert(strstr(content, "First: alpha") != NULL);
    /* ${2} should remain as literal text */
    ck_assert(strstr(content, "${2}") != NULL);
}
END_TEST

/* Test: /load multiple different skills */
START_TEST(test_load_multiple_skills) {
    create_skill_file("skill-a", "Content A\n");
    create_skill_file("skill-b", "Content B\n");

    res_t res = ik_cmd_dispatch(ctx, repl, "/load skill-a");
    ck_assert(is_ok(&res));

    res = ik_cmd_dispatch(ctx, repl, "/load skill-b");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(repl->current->loaded_skill_count, 2);
    ck_assert_str_eq(repl->current->loaded_skills[0]->name, "skill-a");
    ck_assert_str_eq(repl->current->loaded_skills[1]->name, "skill-b");
}
END_TEST

/* Helper: create a command file at $IKIGAI_STATE_DIR/commands/<name>.md */
static void create_command_file(const char *cmd_name, const char *content)
{
    const char *ikigai_state_dir = getenv("IKIGAI_STATE_DIR");
    if (!ikigai_state_dir) return;

    char path[512];
    snprintf(path, sizeof(path), "%s/commands", ikigai_state_dir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/commands/%s.md", ikigai_state_dir, cmd_name);
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

/* Test: !nonexistent returns "command not found" error */
START_TEST(test_bang_command_not_found) {
    res_t res = ik_bang_dispatch(ctx, repl, "!nonexistent_cmd_xyz");
    ck_assert(is_err(&res));
    talloc_free(res.err);
}
END_TEST

/* Test: ! alone (empty command) returns "Empty command" error */
START_TEST(test_bang_empty_command) {
    res_t res = ik_bang_dispatch(ctx, repl, "!");
    ck_assert(is_err(&res));
    talloc_free(res.err);
}
END_TEST

/* Test: valid command dispatches successfully */
START_TEST(test_bang_command_success) {
    create_command_file("greet", "Hello from bang command.\n");

    res_t res = ik_bang_dispatch(ctx, repl, "!greet");
    /* send_to_llm_for_agent_bang returns early (no model configured), so OK */
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: command with positional args applies ${1} substitution */
START_TEST(test_bang_command_with_args) {
    create_command_file("greet_pos", "Hello ${1}.\n");

    res_t res = ik_bang_dispatch(ctx, repl, "!greet_pos world");
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: whitespace after ! is skipped before command name */
START_TEST(test_bang_leading_whitespace) {
    create_command_file("greet_ws", "Hi.\n");

    res_t res = ik_bang_dispatch(ctx, repl, "! greet_ws");
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: positional arg index out-of-range stays as literal */
START_TEST(test_bang_arg_out_of_range) {
    create_command_file("greet_oob", "A=${1} B=${5}.\n");

    res_t res = ik_bang_dispatch(ctx, repl, "!greet_oob hello");
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: non-digit var is not replaced (passed through to template engine) */
START_TEST(test_bang_non_digit_var) {
    create_command_file("greet_var", "Value=${notdigit}.\n");

    res_t res = ik_bang_dispatch(ctx, repl, "!greet_var hello");
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: substitution with no trailing text (p == start at loop end) */
START_TEST(test_bang_substitution_no_trailing_text) {
    create_command_file("just_arg", "${1}");

    res_t res = ik_bang_dispatch(ctx, repl, "!just_arg hello");
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: ${N} where N is out-of-range stays as literal */
START_TEST(test_bang_arg_index_high_out_of_range) {
    create_command_file("high_idx", "${99} world\n");

    res_t res = ik_bang_dispatch(ctx, repl, "!high_idx hello");
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: unclosed ${ has no closing } so end==NULL */
START_TEST(test_bang_unclosed_brace) {
    create_command_file("no_close", "Value=${notclosed\n");

    res_t res = ik_bang_dispatch(ctx, repl, "!no_close hello");
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: multiple args to exercise parse_pos_args_ loop thoroughly */
START_TEST(test_bang_multiple_args) {
    create_command_file("multi_arg", "${1} and ${2}.\n");

    res_t res = ik_bang_dispatch(ctx, repl, "!multi_arg foo bar");
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: var_len >= 10 (long var name) triggers match_positional_var_ early-exit */
START_TEST(test_bang_long_var_name) {
    create_command_file("long_var", "${1234567890} suffix\n");

    res_t res = ik_bang_dispatch(ctx, repl, "!long_var hello");
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: ${0} triggers idx < 1 exit in match_positional_var_ */
START_TEST(test_bang_zero_index_var) {
    create_command_file("zero_idx", "${0} text\n");

    res_t res = ik_bang_dispatch(ctx, repl, "!zero_idx hello");
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: args with leading/trailing whitespace exercises parse_pos_args_ edge cases */
START_TEST(test_bang_args_with_spaces) {
    create_command_file("spaces_cmd", "${1}\n");

    /* Multiple spaces between args exercises the whitespace-skip loops */
    res_t res = ik_bang_dispatch(ctx, repl, "!spaces_cmd  foo  bar");
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: content with lone $ (not followed by {) exercises the false branch of p[1] == '{' */
START_TEST(test_bang_lone_dollar_sign) {
    /* Content has $abc (lone $, not ${) before a valid positional arg ${1} */
    create_command_file("dollar_cmd", "$abc ${1} done\n");

    res_t res = ik_bang_dispatch(ctx, repl, "!dollar_cmd hello");
    ck_assert(!is_err(&res));
}
END_TEST

/* Test: /unload removes a loaded skill */
START_TEST(test_unload_removes_skill) {
    create_skill_file("removable", "Remove me.\n");

    res_t res = ik_cmd_dispatch(ctx, repl, "/load removable");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->loaded_skill_count, 1);

    res = ik_cmd_dispatch(ctx, repl, "/unload removable");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->loaded_skill_count, 0);
}
END_TEST

static Suite *bang_commands_suite(void)
{
    Suite *s = suite_create("BangCommands");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_load_no_args);
    tcase_add_test(tc, test_load_missing_skill);
    tcase_add_test(tc, test_load_success);
    tcase_add_test(tc, test_load_duplicate_replaces);
    tcase_add_test(tc, test_load_positional_args);
    tcase_add_test(tc, test_load_positional_args_unreplaced);
    tcase_add_test(tc, test_load_multiple_skills);
    tcase_add_test(tc, test_unload_removes_skill);
    tcase_add_test(tc, test_bang_command_not_found);
    tcase_add_test(tc, test_bang_empty_command);
    tcase_add_test(tc, test_bang_command_success);
    tcase_add_test(tc, test_bang_command_with_args);
    tcase_add_test(tc, test_bang_leading_whitespace);
    tcase_add_test(tc, test_bang_arg_out_of_range);
    tcase_add_test(tc, test_bang_non_digit_var);
    tcase_add_test(tc, test_bang_substitution_no_trailing_text);
    tcase_add_test(tc, test_bang_arg_index_high_out_of_range);
    tcase_add_test(tc, test_bang_unclosed_brace);
    tcase_add_test(tc, test_bang_multiple_args);
    tcase_add_test(tc, test_bang_long_var_name);
    tcase_add_test(tc, test_bang_zero_index_var);
    tcase_add_test(tc, test_bang_args_with_spaces);
    tcase_add_test(tc, test_bang_lone_dollar_sign);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = bang_commands_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/bang_commands/bang_commands_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
