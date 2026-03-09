/**
 * @file agent_system_prompt_coverage_test.c
 * @brief Coverage tests for agent_system_prompt.c (loaded skills, prompt file, skill append)
 */

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/config_defaults.h"
#include "apps/ikigai/doc_cache.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/providers/request.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

static char *write_temp_file(TALLOC_CTX *ctx, const char *dir, const char *name,
                             const char *content)
{
    char *path = talloc_asprintf(ctx, "%s/%s", dir, name);
    if (path == NULL) return NULL;
    FILE *f = fopen(path, "w");
    if (f == NULL) return NULL;
    fputs(content, f);
    fclose(f);
    return path;
}

/* ================================================================
 * Effective system prompt with loaded skills (covers append_loaded_skills_)
 * ================================================================ */

START_TEST(test_effective_system_prompt_appends_loaded_skills) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Add one loaded skill with content */
    agent->loaded_skills = talloc_array(agent, ik_loaded_skill_t *, 1);
    ck_assert_ptr_nonnull(agent->loaded_skills);
    agent->loaded_skills[0] = talloc_zero(agent, ik_loaded_skill_t);
    agent->loaded_skills[0]->name    = talloc_strdup(agent, "test-skill");
    agent->loaded_skills[0]->content = talloc_strdup(agent, " SKILL_APPEND");
    agent->loaded_skill_count = 1;

    char *prompt = NULL;
    res = ik_agent_get_effective_system_prompt(agent, &prompt);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(prompt);
    ck_assert(strstr(prompt, "SKILL_APPEND") != NULL);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Effective system prompt with NULL-content skill (covers branch 83)
 * ================================================================ */

START_TEST(test_effective_system_prompt_null_content_skill) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Add one loaded skill with NULL content (skipped by append_loaded_skills_) */
    agent->loaded_skills = talloc_array(agent, ik_loaded_skill_t *, 1);
    ck_assert_ptr_nonnull(agent->loaded_skills);
    agent->loaded_skills[0] = talloc_zero(agent, ik_loaded_skill_t);
    agent->loaded_skills[0]->name    = talloc_strdup(agent, "test-skill");
    agent->loaded_skills[0]->content = NULL;
    agent->loaded_skill_count = 1;

    char *prompt = NULL;
    res = ik_agent_get_effective_system_prompt(agent, &prompt);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(prompt);
    /* Default prompt since skills with NULL content are skipped */
    ck_assert_str_eq(prompt, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Loaded skill: skill content appears as cacheable SKILL block
 * ================================================================ */

START_TEST(test_loaded_skill_content_in_system_blocks) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Add one loaded skill with content */
    agent->loaded_skills = talloc_array(agent, ik_loaded_skill_t *, 1);
    ck_assert_ptr_nonnull(agent->loaded_skills);
    agent->loaded_skills[0] = talloc_zero(agent, ik_loaded_skill_t);
    agent->loaded_skills[0]->name    = talloc_strdup(agent, "test-skill");
    agent->loaded_skills[0]->content = talloc_strdup(agent, "Skill content here.");
    agent->loaded_skill_count = 1;

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* Expect: block 0 (base, not cacheable) + block 1 (skill, cacheable) */
    ck_assert_uint_eq(req->system_block_count, 2);
    ck_assert(req->system_blocks[0].cacheable == false);
    ck_assert_int_eq(req->system_blocks[0].type, IK_SYSTEM_BLOCK_BASE_PROMPT);
    ck_assert(req->system_blocks[1].cacheable == true);
    ck_assert_str_eq(req->system_blocks[1].text, "Skill content here.");
    ck_assert_int_eq(req->system_blocks[1].type, IK_SYSTEM_BLOCK_SKILL);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * resolve_base_prompt_: reads system/prompt.md when paths is set
 * ================================================================ */

START_TEST(test_base_prompt_from_prompt_file) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *prefix = test_paths_setup_env();
    ck_assert_ptr_nonnull(prefix);

    ik_paths_t *paths = NULL;
    res_t res = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Attach paths to shared context so resolve_base_prompt_ finds prompt.md */
    agent->shared->paths = paths;

    /* Create system/prompt.md in data dir */
    const char *data_dir = ik_paths_get_data_dir(paths);
    char *system_dir = talloc_asprintf(ctx, "%s/system", data_dir);
    ck_assert_ptr_nonnull(system_dir);
    mkdir(system_dir, 0755);
    char *prompt_path = write_temp_file(ctx, system_dir, "prompt.md", "Custom base prompt.");
    ck_assert_ptr_nonnull(prompt_path);

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* Block 0 should use the custom base prompt from the file */
    ck_assert_uint_ge(req->system_block_count, 1);
    ck_assert_str_eq(req->system_blocks[0].text, "Custom base prompt.");
    ck_assert_int_eq(req->system_blocks[0].type, IK_SYSTEM_BLOCK_BASE_PROMPT);
    ck_assert(req->system_blocks[0].cacheable == false);

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * resolve_base_prompt_: paths set but prompt.md missing → falls through to default
 * ================================================================ */

START_TEST(test_base_prompt_falls_through_when_file_missing) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *prefix = test_paths_setup_env();
    ck_assert_ptr_nonnull(prefix);

    ik_paths_t *paths = NULL;
    res_t res = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Attach paths but do NOT create system/prompt.md */
    agent->shared->paths = paths;

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* Falls through to hardcoded default since file is missing */
    ck_assert_uint_ge(req->system_block_count, 1);
    ck_assert_str_eq(req->system_blocks[0].text, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);
    ck_assert_int_eq(req->system_blocks[0].type, IK_SYSTEM_BLOCK_BASE_PROMPT);

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Loaded skill with NULL content: skill block not emitted (branch 256)
 * ================================================================ */

START_TEST(test_loaded_skill_null_content_skipped) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Add a loaded skill with NULL content */
    agent->loaded_skills = talloc_array(agent, ik_loaded_skill_t *, 1);
    ck_assert_ptr_nonnull(agent->loaded_skills);
    agent->loaded_skills[0] = talloc_zero(agent, ik_loaded_skill_t);
    agent->loaded_skills[0]->name    = talloc_strdup(agent, "test-skill");
    agent->loaded_skills[0]->content = NULL;
    agent->loaded_skill_count = 1;

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* NULL-content skill is skipped: only the base block */
    ck_assert_uint_eq(req->system_block_count, 1);
    ck_assert_int_eq(req->system_blocks[0].type, IK_SYSTEM_BLOCK_BASE_PROMPT);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Session summary with NULL summary field: block skipped (branch 269)
 * ================================================================ */

START_TEST(test_session_summary_null_skipped) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* One session summary with NULL summary text */
    agent->session_summaries = talloc_array(agent, ik_session_summary_t *, 1);
    ck_assert_ptr_nonnull(agent->session_summaries);
    agent->session_summaries[0] = talloc_zero(agent, ik_session_summary_t);
    agent->session_summaries[0]->summary = NULL;
    agent->session_summary_count = 1;

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* NULL summary is skipped: only the base block */
    ck_assert_uint_eq(req->system_block_count, 1);
    ck_assert_int_eq(req->system_blocks[0].type, IK_SYSTEM_BLOCK_BASE_PROMPT);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite
 * ================================================================ */

static Suite *agent_system_prompt_coverage_suite(void)
{
    Suite *s = suite_create("agent_system_prompt_coverage");

    TCase *tc_skills = tcase_create("LoadedSkills");
    tcase_add_checked_fixture(tc_skills, suite_setup, NULL);
    tcase_add_test(tc_skills, test_loaded_skill_content_in_system_blocks);
    tcase_add_test(tc_skills, test_loaded_skill_null_content_skipped);
    tcase_add_test(tc_skills, test_session_summary_null_skipped);
    suite_add_tcase(s, tc_skills);

    TCase *tc_prompt_file = tcase_create("PromptFile");
    tcase_add_checked_fixture(tc_prompt_file, suite_setup, NULL);
    tcase_add_test(tc_prompt_file, test_base_prompt_from_prompt_file);
    tcase_add_test(tc_prompt_file, test_base_prompt_falls_through_when_file_missing);
    suite_add_tcase(s, tc_prompt_file);

    TCase *tc_skill_append = tcase_create("SkillAppend");
    tcase_add_checked_fixture(tc_skill_append, suite_setup, NULL);
    tcase_add_test(tc_skill_append, test_effective_system_prompt_appends_loaded_skills);
    tcase_add_test(tc_skill_append, test_effective_system_prompt_null_content_skill);
    suite_add_tcase(s, tc_skill_append);

    return s;
}

int32_t main(void)
{
    Suite *s = agent_system_prompt_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/agent_system_prompt_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
