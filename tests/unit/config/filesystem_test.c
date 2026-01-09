/**
 * @file filesystem_test.c
 * @brief Unit tests for config filesystem error handling
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Mock state for controlling posix_stat_
static bool mock_stat_should_fail = false;
static int32_t mock_stat_errno = ENOENT;

// Mock state for controlling posix_mkdir_
static bool mock_mkdir_should_fail = false;
static int32_t mock_mkdir_errno = EACCES;

// Forward declarations for wrapper functions
int posix_stat_(const char *pathname, struct stat *statbuf);
int posix_mkdir_(const char *pathname, mode_t mode);

// Mock posix_stat_ to test directory existence checks
int posix_stat_(const char *pathname, struct stat *statbuf)
{
    if (mock_stat_should_fail) {
        errno = mock_stat_errno;
        return -1;
    }

    // Call the real stat for normal operation
    return stat(pathname, statbuf);
}

// Mock posix_mkdir_ to test directory creation failure
int posix_mkdir_(const char *pathname, mode_t mode)
{
    if (mock_mkdir_should_fail) {
        errno = mock_mkdir_errno;
        return -1;
    }

    // For testing, we don't actually create directories
    // Just return success (tests clean up temp files)
    (void)pathname;
    (void)mode;
    return 0;
}

/* Test: mkdir failure (permission denied) */
START_TEST(test_config_mkdir_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Use a test path that requires directory creation
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_test_%d/config.json", getpid());

    // Enable mock failures
    mock_stat_should_fail = true;   // Directory doesn't exist
    mock_stat_errno = ENOENT;
    mock_mkdir_should_fail = true;  // mkdir fails with permission error
    mock_mkdir_errno = EACCES;

    // Attempt to load config - should fail when creating directory
    ik_config_t *config = NULL;
    res_t res = ik_config_load(ctx, test_config, &config);

    // Verify failure
    ck_assert(is_err(&res));

    // Cleanup mock state
    mock_stat_should_fail = false;
    mock_mkdir_should_fail = false;

    talloc_free(ctx);
}
END_TEST
/* Test: stat succeeds (directory exists) */
START_TEST(test_config_stat_directory_exists) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Use /tmp which always exists
    char test_config[512];
    snprintf(test_config, sizeof(test_config), "/tmp/ikigai_test_exists_%d.json", getpid());

    // Mock stat to succeed (directory exists)
    mock_stat_should_fail = false;

    // Create a simple config file for testing
    FILE *f = fopen(test_config, "w");
    if (f) {
        fprintf(f,
                "{\"openai_model\":\"gpt-4-turbo\",\"openai_temperature\":0.7,\"openai_max_completion_tokens\":4096,\"openai_system_message\":null,\"listen_address\":\"127.0.0.1\",\"listen_port\":1984,\"max_tool_turns\":50,\"max_output_size\":1048576}\n");
        fclose(f);
    }

    // Load config - should succeed
    ik_config_t *config = NULL;
    res_t res = ik_config_load(ctx, test_config, &config);

    // Verify success
    ck_assert(is_ok(&res));

    // Cleanup
    unlink(test_config);
    talloc_free(ctx);
}

END_TEST

static Suite *config_filesystem_suite(void)
{
    Suite *s = suite_create("Config Filesystem");

    TCase *tc_fs = tcase_create("Filesystem Operations");
    tcase_set_timeout(tc_fs, 30);
    tcase_add_test(tc_fs, test_config_mkdir_failure);
    tcase_add_test(tc_fs, test_config_stat_directory_exists);
    suite_add_tcase(s, tc_fs);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = config_filesystem_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
