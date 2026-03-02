// Test: posix_rename_() failure in ik_log_rotate_if_exists is silently ignored.
// When rotation fails, ik_log_init continues and opens a new log file anyway.

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "shared/logger.h"
#include "shared/wrapper.h"

// Mock posix_access_ to report the log file as existing (triggering rotation)
int posix_access_(const char *pathname, int mode)
{
    (void)mode;
    // Make any current.log appear to exist so rotation is attempted
    if (strstr(pathname, "current.log") != NULL) {
        return 0;
    }
    return access(pathname, mode);
}

// Mock posix_rename_ to always fail (covers line 79-80: rename failure -> return)
int posix_rename_(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    errno = EACCES;
    return -1;
}

START_TEST(test_rotation_rename_fail_ignored) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_rename_fail_test_%d", getpid());
    mkdir(test_dir, 0755);

    // ik_log_init will call ik_log_rotate_if_exists, which calls posix_access_
    // (returns 0 = file exists), then posix_rename_ (fails), then returns.
    // After rotation is skipped, a new log file is opened normally.
    ik_log_init(test_dir);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "after_rename_fail");
    ik_log_debug_json(doc);

    ik_log_shutdown();

    // Cleanup
    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/.ikigai/logs/current.log", test_dir);
    unlink(log_path);

    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    rmdir(logs_dir);

    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    rmdir(ikigai_dir);

    rmdir(test_dir);
}
END_TEST

static Suite *jsonl_rename_fail_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger: rename failure in rotation");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_rotation_rename_fail_ignored);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = jsonl_rename_fail_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/logger/jsonl_rename_fail_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
