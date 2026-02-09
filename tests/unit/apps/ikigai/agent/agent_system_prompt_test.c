/**
 * @file agent_system_prompt_test.c
 * @brief Tests for ik_agent_get_effective_system_prompt function
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/config_defaults.h"
#include "apps/ikigai/doc_cache.h"
#include "apps/ikigai/scrollback.h"
#include "shared/error.h"
#include "apps/ikigai/file_utils.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/shared.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

static TALLOC_CTX *test_ctx;
static ik_shared_ctx_t *shared_ctx;
static char temp_dir[256];

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);

    // Create temporary directory for test files
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/ikigai_test_XXXXXX");
    ck_assert_ptr_nonnull(mkdtemp(temp_dir));

    // Setup paths with test environment
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    res_t paths_res = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&paths_res));
    shared_ctx->paths = paths;
}

static void teardown(void)
{
    // Clean up temporary directory
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_dir);
    (void)system(cmd);

    talloc_free(test_ctx);
}

// Test pinned files path - when pinned_count > 0 and doc_cache != NULL
START_TEST(test_effective_prompt_with_pinned_files) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;

    // Create doc_cache
    agent->doc_cache = ik_doc_cache_create(agent, agent->shared->paths);
    ck_assert_ptr_nonnull(agent->doc_cache);

    // Create a test file to pin
    char *test_file = talloc_asprintf(test_ctx, "%s/test.md", temp_dir);
    ck_assert_ptr_nonnull(test_file);
    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Test content from pinned file\n");
    fclose(f);

    // Pin the file
    agent->pinned_count = 1;
    agent->pinned_paths = talloc_array(agent, char *, 1);
    agent->pinned_paths[0] = talloc_strdup(agent, test_file);
    talloc_free(test_file);

    // Get effective prompt
    char *prompt = NULL;
    res_t res = ik_agent_get_effective_system_prompt(agent, &prompt);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(prompt);
    ck_assert(strstr(prompt, "Test content from pinned file") != NULL);

    talloc_free(prompt);
}
END_TEST

// Test pinned files with empty assembled string
START_TEST(test_effective_prompt_pinned_empty_assembled) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;

    // Create doc_cache
    agent->doc_cache = ik_doc_cache_create(agent, agent->shared->paths);
    ck_assert_ptr_nonnull(agent->doc_cache);

    // Pin a non-existent file (doc_cache_get will fail)
    agent->pinned_count = 1;
    agent->pinned_paths = talloc_array(agent, char *, 1);
    agent->pinned_paths[0] = talloc_strdup(agent, "/nonexistent/file.md");

    // Get effective prompt - should fall back to config
    char *prompt = NULL;
    res_t res = ik_agent_get_effective_system_prompt(agent, &prompt);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(prompt);
    // Should fall back to hardcoded default
    ck_assert_str_eq(prompt, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);

    talloc_free(prompt);
}
END_TEST

// Test prompt.md file path - when shared != NULL and paths != NULL
START_TEST(test_effective_prompt_from_file) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->pinned_count = 0;
    agent->pinned_paths = NULL;
    agent->doc_cache = NULL;

    // Create system directory and prompt.md
    const char *data_dir = ik_paths_get_data_dir(agent->shared->paths);
    char *system_dir = talloc_asprintf(test_ctx, "%s/system", data_dir);
    ck_assert_ptr_nonnull(system_dir);
    mkdir(system_dir, 0755);

    char *prompt_file = talloc_asprintf(test_ctx, "%s/prompt.md", system_dir);
    ck_assert_ptr_nonnull(prompt_file);
    FILE *f = fopen(prompt_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Custom system prompt from file\n");
    fclose(f);
    talloc_free(prompt_file);
    talloc_free(system_dir);

    // Get effective prompt
    char *prompt = NULL;
    res_t res = ik_agent_get_effective_system_prompt(agent, &prompt);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(prompt);
    ck_assert(strstr(prompt, "Custom system prompt from file") != NULL);

    talloc_free(prompt);
}
END_TEST

// Test prompt.md with empty content
START_TEST(test_effective_prompt_file_empty) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->pinned_count = 0;
    agent->pinned_paths = NULL;
    agent->doc_cache = NULL;

    // Create system directory and empty prompt.md
    const char *data_dir = ik_paths_get_data_dir(agent->shared->paths);
    char *system_dir = talloc_asprintf(test_ctx, "%s/system", data_dir);
    ck_assert_ptr_nonnull(system_dir);
    mkdir(system_dir, 0755);

    char *prompt_file = talloc_asprintf(test_ctx, "%s/prompt.md", system_dir);
    ck_assert_ptr_nonnull(prompt_file);
    FILE *f = fopen(prompt_file, "w");
    ck_assert_ptr_nonnull(f);
    fclose(f);
    talloc_free(prompt_file);
    talloc_free(system_dir);

    // Get effective prompt - should fall back to config
    char *prompt = NULL;
    res_t res = ik_agent_get_effective_system_prompt(agent, &prompt);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(prompt);
    // Should fall back to hardcoded default
    ck_assert_str_eq(prompt, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);

    talloc_free(prompt);
}
END_TEST

// Test prompt.md missing - falls back to config
START_TEST(test_effective_prompt_file_missing) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->pinned_count = 0;
    agent->pinned_paths = NULL;
    agent->doc_cache = NULL;

    // Get effective prompt - should fall back to config
    char *prompt = NULL;
    res_t res = ik_agent_get_effective_system_prompt(agent, &prompt);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(prompt);
    // Should fall back to hardcoded default
    ck_assert_str_eq(prompt, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);

    talloc_free(prompt);
}
END_TEST

// Test template variables with unresolved fields trigger warnings
START_TEST(test_effective_prompt_with_unresolved_template_variables) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->uuid = talloc_strdup(agent, "test-uuid-123");
    agent->name = talloc_strdup(agent, "TestAgent");

    // Create scrollback to capture warnings
    agent->scrollback = ik_scrollback_create(agent, 80);
    ck_assert_ptr_nonnull(agent->scrollback);

    // Create doc_cache
    agent->doc_cache = ik_doc_cache_create(agent, agent->shared->paths);
    ck_assert_ptr_nonnull(agent->doc_cache);

    // Create a test file with unresolved template variables
    char *test_file = talloc_asprintf(test_ctx, "%s/template_test.md", temp_dir);
    ck_assert_ptr_nonnull(test_file);
    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Agent UUID: ${agent.uuid}\n");
    fprintf(f, "Bad field: ${agent.uuuid}\n");
    fprintf(f, "Another bad: ${config.nonexistent}\n");
    fclose(f);

    // Pin the file
    agent->pinned_count = 1;
    agent->pinned_paths = talloc_array(agent, char *, 1);
    agent->pinned_paths[0] = talloc_strdup(agent, test_file);
    talloc_free(test_file);

    // Get effective prompt
    char *prompt = NULL;
    res_t res = ik_agent_get_effective_system_prompt(agent, &prompt);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(prompt);

    // Verify the resolved variable appears in the prompt
    ck_assert(strstr(prompt, "test-uuid-123") != NULL);

    // Verify the unresolved variables remain as-is
    ck_assert(strstr(prompt, "${agent.uuuid}") != NULL);
    ck_assert(strstr(prompt, "${config.nonexistent}") != NULL);

    // Verify warnings were added to scrollback
    ik_scrollback_t *sb = agent->scrollback;
    ck_assert(sb->count >= 2);

    // Check that warnings were added by looking at the text buffer
    bool found_uuuid_warning = false;
    bool found_nonexistent_warning = false;
    for (size_t i = 0; i < sb->count; i++) {
        const char *line = &sb->text_buffer[sb->text_offsets[i]];
        if (strstr(line, "${agent.uuuid}") != NULL) {
            found_uuuid_warning = true;
        }
        if (strstr(line, "${config.nonexistent}") != NULL) {
            found_nonexistent_warning = true;
        }
    }
    ck_assert(found_uuuid_warning);
    ck_assert(found_nonexistent_warning);

    talloc_free(prompt);
}
END_TEST

// Test unresolved variables without scrollback (no warnings displayed)
START_TEST(test_effective_prompt_unresolved_no_scrollback) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->uuid = talloc_strdup(agent, "test-uuid-123");
    agent->name = talloc_strdup(agent, "TestAgent");
    agent->scrollback = NULL;  // No scrollback

    // Create doc_cache
    agent->doc_cache = ik_doc_cache_create(agent, agent->shared->paths);
    ck_assert_ptr_nonnull(agent->doc_cache);

    // Create a test file with unresolved template variables
    char *test_file = talloc_asprintf(test_ctx, "%s/template_test2.md", temp_dir);
    ck_assert_ptr_nonnull(test_file);
    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Bad: ${agent.uuuid}\n");
    fclose(f);

    // Pin the file
    agent->pinned_count = 1;
    agent->pinned_paths = talloc_array(agent, char *, 1);
    agent->pinned_paths[0] = talloc_strdup(agent, test_file);
    talloc_free(test_file);

    // Get effective prompt - should succeed without warnings
    char *prompt = NULL;
    res_t res = ik_agent_get_effective_system_prompt(agent, &prompt);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(prompt);
    ck_assert(strstr(prompt, "${agent.uuuid}") != NULL);

    talloc_free(prompt);
}
END_TEST

// Test template processing with NULL shared context
START_TEST(test_effective_prompt_template_null_shared) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = NULL;  // NULL shared context
    agent->uuid = talloc_strdup(agent, "test-uuid-456");

    // Create doc_cache with NULL shared - need paths from test setup
    agent->doc_cache = ik_doc_cache_create(agent, shared_ctx->paths);
    ck_assert_ptr_nonnull(agent->doc_cache);

    // Create a test file with template variables
    char *test_file = talloc_asprintf(test_ctx, "%s/template_test3.md", temp_dir);
    ck_assert_ptr_nonnull(test_file);
    FILE *f = fopen(test_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "UUID: ${agent.uuid}\n");
    fclose(f);

    // Pin the file
    agent->pinned_count = 1;
    agent->pinned_paths = talloc_array(agent, char *, 1);
    agent->pinned_paths[0] = talloc_strdup(agent, test_file);
    talloc_free(test_file);

    // Get effective prompt - template processing with NULL config
    char *prompt = NULL;
    res_t res = ik_agent_get_effective_system_prompt(agent, &prompt);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(prompt);
    ck_assert(strstr(prompt, "test-uuid-456") != NULL);

    talloc_free(prompt);
}
END_TEST

static Suite *agent_system_prompt_suite(void)
{
    Suite *s = suite_create("Agent System Prompt");

    TCase *tc = tcase_create("Effective Prompt");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_effective_prompt_with_pinned_files);
    tcase_add_test(tc, test_effective_prompt_pinned_empty_assembled);
    tcase_add_test(tc, test_effective_prompt_from_file);
    tcase_add_test(tc, test_effective_prompt_file_empty);
    tcase_add_test(tc, test_effective_prompt_file_missing);
    tcase_add_test(tc, test_effective_prompt_with_unresolved_template_variables);
    tcase_add_test(tc, test_effective_prompt_unresolved_no_scrollback);
    tcase_add_test(tc, test_effective_prompt_template_null_shared);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = agent_system_prompt_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/agent/agent_system_prompt_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
