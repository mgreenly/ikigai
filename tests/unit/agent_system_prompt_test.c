/**
 * @file agent_system_prompt_test.c
 * @brief Unit tests for ik_agent_build_system_blocks()
 *
 * Verifies that the system block builder emits:
 *   - Block 0: base system prompt (not cacheable)
 *   - Block 1..N: pinned documents in order (cacheable)
 */

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* ================================================================
 * Helper: write content to a file, return absolute path
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

/* ================================================================
 * No pinned docs: single block (default system prompt), not cacheable
 * ================================================================ */

START_TEST(test_no_pinned_docs_single_block)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(req->system_block_count, 1);
    ck_assert_ptr_nonnull(req->system_blocks);
    ck_assert(req->system_blocks[0].cacheable == false);
    ck_assert_str_eq(req->system_blocks[0].text, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * One pinned doc: two blocks — base (not cacheable) + pinned (cacheable)
 * ================================================================ */

START_TEST(test_one_pinned_doc_two_blocks)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *prefix = test_paths_setup_env();
    ck_assert_ptr_nonnull(prefix);

    ik_paths_t *paths = NULL;
    res_t res = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Attach doc_cache using test paths */
    agent->doc_cache = ik_doc_cache_create(agent, paths);
    ck_assert_ptr_nonnull(agent->doc_cache);

    /* Write pinned doc into data dir */
    const char *data_dir = ik_paths_get_data_dir(paths);
    char *pinned_path = write_temp_file(ctx, data_dir, "pinned_doc.md", "Pinned content.");
    ck_assert_ptr_nonnull(pinned_path);

    agent->pinned_paths = talloc_array(agent, char *, 1);
    ck_assert_ptr_nonnull(agent->pinned_paths);
    agent->pinned_paths[0] = talloc_strdup(agent, pinned_path);
    agent->pinned_count = 1;

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(req->system_block_count, 2);

    /* Block 0: base system prompt, not cacheable */
    ck_assert(req->system_blocks[0].cacheable == false);
    ck_assert_str_eq(req->system_blocks[0].text, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);

    /* Block 1: pinned doc, cacheable */
    ck_assert(req->system_blocks[1].cacheable == true);
    ck_assert_str_eq(req->system_blocks[1].text, "Pinned content.");

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Two pinned docs: three blocks, order preserved, all pinned cacheable
 * ================================================================ */

START_TEST(test_two_pinned_docs_order_preserved)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *prefix = test_paths_setup_env();
    ck_assert_ptr_nonnull(prefix);

    ik_paths_t *paths = NULL;
    res_t res = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    agent->doc_cache = ik_doc_cache_create(agent, paths);
    ck_assert_ptr_nonnull(agent->doc_cache);

    const char *data_dir = ik_paths_get_data_dir(paths);
    char *path_a = write_temp_file(ctx, data_dir, "pinned_a.md", "First doc.");
    char *path_b = write_temp_file(ctx, data_dir, "pinned_b.md", "Second doc.");
    ck_assert_ptr_nonnull(path_a);
    ck_assert_ptr_nonnull(path_b);

    agent->pinned_paths = talloc_array(agent, char *, 2);
    ck_assert_ptr_nonnull(agent->pinned_paths);
    agent->pinned_paths[0] = talloc_strdup(agent, path_a);
    agent->pinned_paths[1] = talloc_strdup(agent, path_b);
    agent->pinned_count = 2;

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(req->system_block_count, 3);

    /* Block 0: base, not cacheable */
    ck_assert(req->system_blocks[0].cacheable == false);

    /* Block 1: first pinned doc, cacheable, in order */
    ck_assert(req->system_blocks[1].cacheable == true);
    ck_assert_str_eq(req->system_blocks[1].text, "First doc.");

    /* Block 2: second pinned doc, cacheable, in order */
    ck_assert(req->system_blocks[2].cacheable == true);
    ck_assert_str_eq(req->system_blocks[2].text, "Second doc.");

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Session summaries and recent summary injection
 * ================================================================ */

START_TEST(test_session_summaries_and_recent_summary)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* Set up two previous-session summaries (oldest first) */
    agent->session_summaries = talloc_array(agent, ik_session_summary_t *, 2);
    ck_assert_ptr_nonnull(agent->session_summaries);
    agent->session_summaries[0] = talloc(agent, ik_session_summary_t);
    agent->session_summaries[0]->summary = talloc_strdup(agent, "Session one summary.");
    agent->session_summaries[0]->token_count = 10;
    agent->session_summaries[1] = talloc(agent, ik_session_summary_t);
    agent->session_summaries[1]->summary = talloc_strdup(agent, "Session two summary.");
    agent->session_summaries[1]->token_count = 12;
    agent->session_summary_count = 2;

    /* Set recent summary */
    agent->recent_summary = talloc_strdup(agent, "Recent summary text.");
    agent->recent_summary_tokens = 8;

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* Expect: block 0 (base) + 2 session summaries + 1 recent = 4 blocks */
    ck_assert_uint_eq(req->system_block_count, 4);

    /* Block 0: base system prompt, not cacheable */
    ck_assert(req->system_blocks[0].cacheable == false);
    ck_assert_str_eq(req->system_blocks[0].text, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);

    /* Block 1: first session summary, cacheable */
    ck_assert(req->system_blocks[1].cacheable == true);
    ck_assert_str_eq(req->system_blocks[1].text, "Session one summary.");

    /* Block 2: second session summary, cacheable */
    ck_assert(req->system_blocks[2].cacheable == true);
    ck_assert_str_eq(req->system_blocks[2].text, "Session two summary.");

    /* Block 3: recent summary, not cacheable */
    ck_assert(req->system_blocks[3].cacheable == false);
    ck_assert_str_eq(req->system_blocks[3].text, "Recent summary text.");

    talloc_free(ctx);
}
END_TEST

START_TEST(test_only_recent_summary_no_sessions)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    /* No session summaries, only recent summary */
    agent->recent_summary = talloc_strdup(agent, "Only recent summary.");
    agent->recent_summary_tokens = 5;

    ik_request_t *req = NULL;
    res = ik_request_create(ctx, "test-model", &req);
    ck_assert(is_ok(&res));

    res = ik_agent_build_system_blocks(req, agent);
    ck_assert(is_ok(&res));

    /* Expect: block 0 (base) + 1 recent summary = 2 blocks */
    ck_assert_uint_eq(req->system_block_count, 2);

    /* Block 0: base system prompt, not cacheable */
    ck_assert(req->system_blocks[0].cacheable == false);
    ck_assert_str_eq(req->system_blocks[0].text, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);

    /* Block 1: recent summary, not cacheable */
    ck_assert(req->system_blocks[1].cacheable == false);
    ck_assert_str_eq(req->system_blocks[1].text, "Only recent summary.");

    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite
 * ================================================================ */

static Suite *agent_system_prompt_suite(void)
{
    Suite *s = suite_create("agent_system_prompt");

    TCase *tc_no_pins = tcase_create("NoPinnedDocs");
    tcase_add_checked_fixture(tc_no_pins, suite_setup, NULL);
    tcase_add_test(tc_no_pins, test_no_pinned_docs_single_block);
    suite_add_tcase(s, tc_no_pins);

    TCase *tc_one_pin = tcase_create("OnePinnedDoc");
    tcase_add_checked_fixture(tc_one_pin, suite_setup, NULL);
    tcase_add_test(tc_one_pin, test_one_pinned_doc_two_blocks);
    suite_add_tcase(s, tc_one_pin);

    TCase *tc_two_pins = tcase_create("TwoPinnedDocs");
    tcase_add_checked_fixture(tc_two_pins, suite_setup, NULL);
    tcase_add_test(tc_two_pins, test_two_pinned_docs_order_preserved);
    suite_add_tcase(s, tc_two_pins);

    TCase *tc_summaries = tcase_create("SessionSummaries");
    tcase_add_checked_fixture(tc_summaries, suite_setup, NULL);
    tcase_add_test(tc_summaries, test_session_summaries_and_recent_summary);
    tcase_add_test(tc_summaries, test_only_recent_summary_no_sessions);
    suite_add_tcase(s, tc_summaries);

    return s;
}

int32_t main(void)
{
    Suite *s = agent_system_prompt_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/agent_system_prompt_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
