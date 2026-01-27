#include "../../../src/config.h"
#include "../../../src/paths.h"

#include "../../../src/error.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Mock fread_ to return partial data
size_t fread_(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    // Return 0 to simulate complete read failure
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    return 0;
}

START_TEST(test_config_system_prompt_fread_partial) {

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Setup test environment
    test_paths_setup_env();

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t paths_result = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_result));

    // Create system prompt file
    const char *data_dir = ik_paths_get_data_dir(paths);
    char *prompts_dir = talloc_asprintf(ctx, "%s/prompts", data_dir);
    mkdir(prompts_dir, 0755);

    char *system_prompt_path = talloc_asprintf(ctx, "%s/system.md", prompts_dir);
    FILE *f = fopen(system_prompt_path, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Test system prompt content here");
    fclose(f);

    // Load config - fread_ will return 0 via mock
    ik_config_t *cfg = NULL;
    res_t result = ik_config_load(ctx, paths, &cfg);

    // Should fail with IO error
    ck_assert(result.is_err);
    ck_assert_int_eq(error_code(result.err), ERR_IO);
    ck_assert(strstr(result.err->msg, "Failed to read system prompt file") != NULL);

    // Clean up
    test_paths_cleanup_env();
    talloc_free(ctx);
}

END_TEST

static Suite *config_suite(void)
{
    Suite *s = suite_create("Config Fread Error");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_config_system_prompt_fread_partial);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = config_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/config/config_fread_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
