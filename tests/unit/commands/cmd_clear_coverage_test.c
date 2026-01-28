/**
 * @file cmd_clear_coverage_test.c
 * @brief Coverage tests for ik_cmd_clear error paths
 */

#include "../../../src/commands_basic.h"
#include "../../../src/agent.h"
#include "../../../src/config.h"
#include "../../../src/db/message.h"
#include "../../../src/error.h"
#include "../../../src/event_render.h"
#include "../../../src/logger.h"
#include "../../../src/paths.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;
static char temp_dir[256];

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    // Create temporary directory
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/ikigai_test_XXXXXX");
    ck_assert_ptr_nonnull(mkdtemp(temp_dir));

    // Create minimal repl context
    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared->cfg = ik_test_create_config(repl->shared);

    // Setup paths with test environment
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    res_t paths_res = ik_paths_init(repl->shared, &paths);
    ck_assert(is_ok(&paths_res));
    repl->shared->paths = paths;

    repl->shared->logger = ik_logger_create(repl->shared, temp_dir);
    repl->shared->db_ctx = NULL;
    repl->shared->session_id = 0;

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_agent_create(repl, repl->shared, NULL, &agent);
    ck_assert(is_ok(&agent_res));
    repl->current = agent;
}

static void teardown(void)
{
    // Clean up temporary directory
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_dir);
    (void)system(cmd);

    talloc_free(test_ctx);
}

// Test when effective_prompt is NULL (config fallback path)
START_TEST(test_clear_null_effective_prompt) {
    // Set up agent with NULL shared->cfg to test NULL prompt path
    repl->current->shared->cfg->openai_system_message = NULL;
    repl->current->shared->paths = NULL;
    repl->current->pinned_count = 0;

    res_t res = ik_cmd_clear(test_ctx, repl, NULL);

    // Should succeed - hardcoded default will be used
    ck_assert(is_ok(&res));
}
END_TEST

static Suite *cmd_clear_coverage_suite(void)
{
    Suite *s = suite_create("Command Clear Coverage");

    TCase *tc = tcase_create("Prompt Paths");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_clear_null_effective_prompt);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = cmd_clear_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands/cmd_clear_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
