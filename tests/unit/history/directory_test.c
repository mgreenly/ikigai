/**
 * @file directory_test.c
 * @brief Unit tests for history directory initialization
 */

#include "../../../src/history.h"
#include "../../../src/history_io.h"
#include "../../../src/error.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

// Forward declaration for suite function
static Suite *history_directory_suite(void);

// Test fixture
static void *ctx;
static char test_dir_buffer[256];

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a temporary test directory
    snprintf(test_dir_buffer, sizeof(test_dir_buffer), "/tmp/ikigai-history-test-XXXXXX");
    char *result = mkdtemp(test_dir_buffer);
    ck_assert_ptr_nonnull(result);

    // Change to test directory for relative path tests
    ck_assert_int_eq(chdir(test_dir_buffer), 0);
}

static void teardown(void)
{
    // Change back to original directory
    chdir("/");

    // Cleanup test directory
    if (test_dir_buffer[0] != '\0') {
        // Remove .ikigai directory/file if it exists
        char ikigai_path[512];
        snprintf(ikigai_path, sizeof(ikigai_path), "%s/.ikigai", test_dir_buffer);
        struct stat st;
        if (stat(ikigai_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                rmdir(ikigai_path);
            } else {
                unlink(ikigai_path);
            }
        }

        // Remove test directory
        rmdir(test_dir_buffer);
    }

    talloc_free(ctx);
}

// Test: Creates .ikigai directory when it doesn't exist
START_TEST(test_history_ensure_directory_creates) {
    // Verify directory doesn't exist
    struct stat st;
    ck_assert_int_eq(stat(".ikigai", &st), -1);
    ck_assert_int_eq(errno, ENOENT);

    // Call ensure_directory
    res_t res = ik_history_ensure_directory(ctx);
    ck_assert(is_ok(&res));

    // Verify directory now exists
    ck_assert_int_eq(stat(".ikigai", &st), 0);
    ck_assert(S_ISDIR(st.st_mode));

    // Verify permissions are 0755
    mode_t mode = st.st_mode & 07777;
    ck_assert_uint_eq(mode, 0755);
}
END_TEST
// Test: Succeeds when directory already exists (idempotent)
START_TEST(test_history_ensure_directory_exists) {
    // Create directory first
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);

    struct stat st;
    ck_assert_int_eq(stat(".ikigai", &st), 0);

    // Call ensure_directory - should succeed
    res_t res = ik_history_ensure_directory(ctx);
    ck_assert(is_ok(&res));

    // Verify directory still exists
    ck_assert_int_eq(stat(".ikigai", &st), 0);
    ck_assert(S_ISDIR(st.st_mode));
}

END_TEST
// Test: Handle permission denied error
START_TEST(test_history_ensure_directory_permission_denied) {
    // Mock mkdir to fail with EACCES
    // We'll do this by creating a file at .ikigai path (not a directory)
    FILE *f = fopen(".ikigai", "w");
    ck_assert_ptr_nonnull(f);
    fclose(f);

    // Now trying to create directory should fail because path exists as file
    res_t res = ik_history_ensure_directory(ctx);
    ck_assert(is_err(&res));

    // Verify we got an IO error
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(error_message(res.err), ".ikigai") != NULL);
    ck_assert(strstr(error_message(res.err), "Failed") != NULL);
}

END_TEST

static Suite *history_directory_suite(void)
{
    Suite *s = suite_create("History Directory");
    TCase *tc = tcase_create("directory");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_history_ensure_directory_creates);
    tcase_add_test(tc, test_history_ensure_directory_exists);
    tcase_add_test(tc, test_history_ensure_directory_permission_denied);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = history_directory_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/history/directory_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
