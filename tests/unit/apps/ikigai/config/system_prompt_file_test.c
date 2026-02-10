#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "apps/ikigai/config.h"
#include "shared/error.h"
#include "apps/ikigai/paths.h"
#include "tests/helpers/test_utils_helper.h"

// Helper to create prompts directory
static void create_prompts_dir(TALLOC_CTX *ctx, ik_paths_t *paths)
{
    const char *data_dir = ik_paths_get_data_dir(paths);
    char *prompts_dir = talloc_asprintf(ctx, "%s/prompts", data_dir);
    mkdir(prompts_dir, 0755);
}

// Helper to write system.md file
static void write_system_md(TALLOC_CTX *ctx, ik_paths_t *paths, const char *content)
{
    const char *data_dir = ik_paths_get_data_dir(paths);
    char *system_md_path = talloc_asprintf(ctx, "%s/prompts/system.md", data_dir);
    FILE *f = fopen(system_md_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "%s", content);
    fclose(f);
}

// Test 1: File exists with valid content → uses file content
START_TEST(test_system_prompt_from_file) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    test_paths_setup_env();

    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create prompts directory and write system.md
    create_prompts_dir(ctx, paths);
    write_system_md(ctx, paths, "Custom system prompt from file.");

    // Load config
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(cfg);

    // Verify file content is used
    ck_assert_str_eq(cfg->openai_system_message, "Custom system prompt from file.");

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

// Test 2: Neither file nor config → uses default constant
START_TEST(test_system_prompt_default) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    test_paths_setup_env();

    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Load config (no config.json, no system.md)
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(cfg);

    // Verify default constant is used
    ck_assert_str_eq(cfg->openai_system_message, "You are a personal agent and are operating inside the Ikigai orchestration platform.");

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

// Test 3: File exists but is empty → fails loudly
START_TEST(test_system_prompt_file_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    test_paths_setup_env();

    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create empty system.md file
    create_prompts_dir(ctx, paths);
    write_system_md(ctx, paths, "");

    // Load config - should fail
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_IO);
    ck_assert(strstr(result.err->msg, "empty") != NULL);

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

// Test 4: File exists but exceeds 1KB → fails loudly
START_TEST(test_system_prompt_file_too_large) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    test_paths_setup_env();

    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create system.md file with >1KB content
    create_prompts_dir(ctx, paths);
    const char *data_dir = ik_paths_get_data_dir(paths);
    char *system_md_path = talloc_asprintf(ctx, "%s/prompts/system.md", data_dir);
    FILE *f = fopen(system_md_path, "w");
    ck_assert_ptr_nonnull(f);
    // Write 1025 bytes (exceeds 1KB limit)
    for (int32_t i = 0; i < 1025; i++) {
        fputc('A', f);
    }
    fclose(f);

    // Load config - should fail
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_IO);
    ck_assert(strstr(result.err->msg, "exceeds") != NULL || strstr(result.err->msg, "1KB") != NULL);

    test_paths_cleanup_env();
    talloc_free(ctx);
}
END_TEST

static Suite *system_prompt_file_suite(void)
{
    Suite *s = suite_create("Config System Prompt File");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_system_prompt_from_file);
    tcase_add_test(tc_core, test_system_prompt_default);
    tcase_add_test(tc_core, test_system_prompt_file_empty);
    tcase_add_test(tc_core, test_system_prompt_file_too_large);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = system_prompt_file_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/config/system_prompt_file_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
