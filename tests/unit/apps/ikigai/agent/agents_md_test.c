/**
 * @file agents_md_test.c
 * @brief Unit tests for AGENTS.md system block loading
 *
 * Verifies that ik_agent_build_system_blocks():
 *   - Adds IK_SYSTEM_BLOCK_AGENTS_MD when AGENTS.md exists in CWD
 *   - Omits block when AGENTS.md is absent
 *   - Processes template variables in AGENTS.md content
 *   - Positions the block after skills and before summaries
 *   - Re-reads AGENTS.md when cache is cleared (simulates /clear)
 *
 * Note: ik_test_create_agent() sets agents_md_loaded=true to isolate other tests.
 * Each test here resets it to false to opt-in to AGENTS.md loading.
 */

#include <check.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/config_defaults.h"
#include "apps/ikigai/db/session_summary.h"
#include "apps/ikigai/providers/provider_types.h"
#include "apps/ikigai/providers/request.h"
#include "shared/error.h"
#include "shared/wrapper_posix.h"
#include "tests/helpers/test_utils_helper.h"

/* ================================================================
 * Mock: posix_getcwd_
 * Tests set g_agents_md_test_cwd before exercising the system.
 * Empty string makes the mock return NULL (simulates failure).
 * ================================================================ */

static char g_agents_md_test_cwd[PATH_MAX] = "";

char *posix_getcwd_(char *buf, size_t size)
{
    if (g_agents_md_test_cwd[0] == '\0') return NULL;
    strncpy(buf, g_agents_md_test_cwd, size - 1);
    buf[size - 1] = '\0';
    return buf;
}

/* ================================================================
 * Helper: write a file, return path
 * ================================================================ */

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

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
    g_agents_md_test_cwd[0] = '\0';
}

/* ================================================================
 * Test: AGENTS.md present → block added
 * ================================================================ */

START_TEST(test_agents_md_present_adds_block) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *prefix = test_paths_setup_env();
    ck_assert_ptr_nonnull(prefix);

    /* Create a temp dir and write AGENTS.md */
    char tmpdir[PATH_MAX];
    snprintf(tmpdir, sizeof(tmpdir), "%s/agents_md_present", prefix);
    mkdir(tmpdir, 0755);
    write_temp_file(ctx, tmpdir, "AGENTS.md", "Project context.");
    strncpy(g_agents_md_test_cwd, tmpdir, sizeof(g_agents_md_test_cwd) - 1);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->agents_md_loaded = false;  /* opt-in to AGENTS.md loading */

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* Find the AGENTS.md block */
    bool found = false;
    for (size_t i = 0; i < req->system_block_count; i++) {
        if (req->system_blocks[i].type == IK_SYSTEM_BLOCK_AGENTS_MD) {
            found = true;
            ck_assert_str_eq(req->system_blocks[i].text, "Project context.");
            ck_assert(req->system_blocks[i].cacheable == true);
        }
    }
    ck_assert_msg(found, "IK_SYSTEM_BLOCK_AGENTS_MD not found in system blocks");

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Test: AGENTS.md absent → no block added
 * ================================================================ */

START_TEST(test_agents_md_absent_no_block) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *prefix = test_paths_setup_env();
    ck_assert_ptr_nonnull(prefix);

    /* Create a temp dir but do NOT write AGENTS.md */
    char tmpdir[PATH_MAX];
    snprintf(tmpdir, sizeof(tmpdir), "%s/agents_md_absent", prefix);
    mkdir(tmpdir, 0755);
    strncpy(g_agents_md_test_cwd, tmpdir, sizeof(g_agents_md_test_cwd) - 1);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->agents_md_loaded = false;  /* opt-in to AGENTS.md loading */

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* Verify no AGENTS.md block */
    for (size_t i = 0; i < req->system_block_count; i++) {
        ck_assert_int_ne(req->system_blocks[i].type, IK_SYSTEM_BLOCK_AGENTS_MD);
    }

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Test: Template variables resolved ($$ → $)
 * ================================================================ */

START_TEST(test_agents_md_template_expanded) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *prefix = test_paths_setup_env();
    ck_assert_ptr_nonnull(prefix);

    char tmpdir[PATH_MAX];
    snprintf(tmpdir, sizeof(tmpdir), "%s/agents_md_template", prefix);
    mkdir(tmpdir, 0755);
    /* $$ is the escape for a literal $ sign in the template engine */
    write_temp_file(ctx, tmpdir, "AGENTS.md", "Cost: $$100");
    strncpy(g_agents_md_test_cwd, tmpdir, sizeof(g_agents_md_test_cwd) - 1);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->agents_md_loaded = false;  /* opt-in to AGENTS.md loading */

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    bool found = false;
    for (size_t i = 0; i < req->system_block_count; i++) {
        if (req->system_blocks[i].type == IK_SYSTEM_BLOCK_AGENTS_MD) {
            found = true;
            /* $$ should have been expanded to $ by template engine */
            ck_assert_str_eq(req->system_blocks[i].text, "Cost: $100");
        }
    }
    ck_assert_msg(found, "IK_SYSTEM_BLOCK_AGENTS_MD not found after template expansion");

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Test: Block ordering — AGENTS.md after skills, before summaries
 * ================================================================ */

START_TEST(test_agents_md_ordering_after_skills_before_summaries) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *prefix = test_paths_setup_env();
    ck_assert_ptr_nonnull(prefix);

    char tmpdir[PATH_MAX];
    snprintf(tmpdir, sizeof(tmpdir), "%s/agents_md_order", prefix);
    mkdir(tmpdir, 0755);
    write_temp_file(ctx, tmpdir, "AGENTS.md", "Agents context.");
    strncpy(g_agents_md_test_cwd, tmpdir, sizeof(g_agents_md_test_cwd) - 1);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->agents_md_loaded = false;  /* opt-in to AGENTS.md loading */

    /* Add a loaded skill */
    agent->loaded_skills = talloc_array(agent, ik_loaded_skill_t *, 1);
    ck_assert_ptr_nonnull(agent->loaded_skills);
    agent->loaded_skills[0] = talloc_zero(agent, ik_loaded_skill_t);
    agent->loaded_skills[0]->name    = talloc_strdup(agent, "test-skill");
    agent->loaded_skills[0]->content = talloc_strdup(agent, "Skill content.");
    agent->loaded_skill_count = 1;

    /* Add a session summary */
    agent->session_summaries = talloc_array(agent, ik_session_summary_t *, 1);
    ck_assert_ptr_nonnull(agent->session_summaries);
    agent->session_summaries[0] = talloc_zero(agent, ik_session_summary_t);
    agent->session_summaries[0]->summary = talloc_strdup(agent, "Session summary.");
    agent->session_summary_count = 1;

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* Find positions of SKILL, AGENTS_MD, and SESSION_SUMMARY blocks */
    int skill_idx = -1, agents_md_idx = -1, summary_idx = -1;
    for (size_t i = 0; i < req->system_block_count; i++) {
        switch (req->system_blocks[i].type) {
            case IK_SYSTEM_BLOCK_SKILL:           skill_idx     = (int)i; break;
            case IK_SYSTEM_BLOCK_AGENTS_MD:       agents_md_idx = (int)i; break;
            case IK_SYSTEM_BLOCK_SESSION_SUMMARY: summary_idx   = (int)i; break;
            default: break;
        }
    }

    ck_assert_msg(skill_idx >= 0, "SKILL block not found");
    ck_assert_msg(agents_md_idx >= 0, "AGENTS_MD block not found");
    ck_assert_msg(summary_idx >= 0, "SESSION_SUMMARY block not found");

    /* AGENTS.md must come after skills and before summaries */
    ck_assert_int_gt(agents_md_idx, skill_idx);
    ck_assert_int_lt(agents_md_idx, summary_idx);

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Test: Content re-read after cache cleared (/clear simulation)
 * ================================================================ */

START_TEST(test_agents_md_reread_after_cache_clear) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *prefix = test_paths_setup_env();
    ck_assert_ptr_nonnull(prefix);

    char tmpdir[PATH_MAX];
    snprintf(tmpdir, sizeof(tmpdir), "%s/agents_md_reread", prefix);
    mkdir(tmpdir, 0755);
    write_temp_file(ctx, tmpdir, "AGENTS.md", "First content.");
    strncpy(g_agents_md_test_cwd, tmpdir, sizeof(g_agents_md_test_cwd) - 1);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->agents_md_loaded = false;  /* opt-in to AGENTS.md loading */

    /* First build: loads and caches AGENTS.md */
    ik_request_t *req1 = NULL;
    res = ik_request_create(ctx, "test-model", &req1);
    ck_assert(is_ok(&res));
    res = ik_agent_build_system_blocks(req1, agent);
    ck_assert(is_ok(&res));
    ck_assert(agent->agents_md_loaded == true);
    ck_assert_ptr_nonnull(agent->agents_md_content);

    /* Simulate /clear: reset cache */
    talloc_free(agent->agents_md_content);
    agent->agents_md_content = NULL;
    agent->agents_md_loaded = false;

    /* Update AGENTS.md on disk */
    write_temp_file(ctx, tmpdir, "AGENTS.md", "Second content.");

    /* Second build: should re-read the new content */
    ik_request_t *req2 = NULL;
    res = ik_request_create(ctx, "test-model", &req2);
    ck_assert(is_ok(&res));
    res = ik_agent_build_system_blocks(req2, agent);
    ck_assert(is_ok(&res));

    bool found = false;
    for (size_t i = 0; i < req2->system_block_count; i++) {
        if (req2->system_blocks[i].type == IK_SYSTEM_BLOCK_AGENTS_MD) {
            found = true;
            ck_assert_str_eq(req2->system_blocks[i].text, "Second content.");
        }
    }
    ck_assert_msg(found, "AGENTS.md block not found after cache clear");

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Test: getcwd failure → no block, no crash
 * ================================================================ */

START_TEST(test_agents_md_getcwd_failure_no_block) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    /* Empty string makes mock return NULL */
    g_agents_md_test_cwd[0] = '\0';

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));
    agent->agents_md_loaded = false;  /* opt-in to AGENTS.md loading */

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    for (size_t i = 0; i < req->system_block_count; i++) {
        ck_assert_int_ne(req->system_blocks[i].type, IK_SYSTEM_BLOCK_AGENTS_MD);
    }

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite
 * ================================================================ */

static Suite *agents_md_suite(void)
{
    Suite *s = suite_create("agents_md");

    TCase *tc_present = tcase_create("Present");
    tcase_add_checked_fixture(tc_present, suite_setup, NULL);
    tcase_add_test(tc_present, test_agents_md_present_adds_block);
    suite_add_tcase(s, tc_present);

    TCase *tc_absent = tcase_create("Absent");
    tcase_add_checked_fixture(tc_absent, suite_setup, NULL);
    tcase_add_test(tc_absent, test_agents_md_absent_no_block);
    suite_add_tcase(s, tc_absent);

    TCase *tc_template = tcase_create("Template");
    tcase_add_checked_fixture(tc_template, suite_setup, NULL);
    tcase_add_test(tc_template, test_agents_md_template_expanded);
    suite_add_tcase(s, tc_template);

    TCase *tc_order = tcase_create("Ordering");
    tcase_add_checked_fixture(tc_order, suite_setup, NULL);
    tcase_add_test(tc_order, test_agents_md_ordering_after_skills_before_summaries);
    suite_add_tcase(s, tc_order);

    TCase *tc_reread = tcase_create("CacheClear");
    tcase_add_checked_fixture(tc_reread, suite_setup, NULL);
    tcase_add_test(tc_reread, test_agents_md_reread_after_cache_clear);
    suite_add_tcase(s, tc_reread);

    TCase *tc_getcwd_fail = tcase_create("GetCWDFailure");
    tcase_add_checked_fixture(tc_getcwd_fail, suite_setup, NULL);
    tcase_add_test(tc_getcwd_fail, test_agents_md_getcwd_failure_no_block);
    suite_add_tcase(s, tc_getcwd_fail);

    return s;
}

int32_t main(void)
{
    Suite *s = agents_md_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/agent/agents_md_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
