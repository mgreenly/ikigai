// Unit tests for logger rotation failure path
// JSONL file writing is disabled; rotation is a no-op.

#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <talloc.h>
#include "shared/logger.h"
#include "shared/wrapper.h"
#include "tests/helpers/test_utils_helper.h"

static char test_dir[256];
static TALLOC_CTX *test_ctx = NULL;

static void setup_test(void)
{
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_logger_rotation_test_%d", getpid());
    mkdir(test_dir, 0755);
    test_ctx = talloc_new(NULL);
}

static void teardown_test(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
    rmdir(test_dir);
}

// Mock posix_rename_ - kept to satisfy linker, not called since file writing is disabled
int posix_rename_(const char *old, const char *new)
{
    (void)old;
    (void)new;
    errno = EACCES;
    return -1;
}

// Test: creating multiple loggers does not crash (rotation is a no-op)
START_TEST(test_logger_rotation_failure_ignored) {
    setup_test();

    ik_logger_t *logger1 = ik_logger_create(test_ctx, test_dir);
    ck_assert_ptr_nonnull(logger1);

    yyjson_mut_doc *doc1 = ik_log_create();
    yyjson_mut_val *root1 = yyjson_mut_doc_get_root(doc1);
    yyjson_mut_obj_add_str(doc1, root1, "event", "before_failed_rotation");
    ik_logger_info_json(logger1, doc1);

    talloc_free(logger1);

    TALLOC_CTX *ctx2 = talloc_new(NULL);
    ik_logger_t *logger2 = ik_logger_create(ctx2, test_dir);
    ck_assert_ptr_nonnull(logger2);

    yyjson_mut_doc *doc2 = ik_log_create();
    yyjson_mut_val *root2 = yyjson_mut_doc_get_root(doc2);
    yyjson_mut_obj_add_str(doc2, root2, "event", "after_failed_rotation");
    ik_logger_info_json(logger2, doc2);

    talloc_free(ctx2);
    teardown_test();
}
END_TEST

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

static Suite *logger_rotation_failure_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger Rotation Failure");
    tc_core = tcase_create("Core");
    tcase_add_unchecked_fixture(tc_core, suite_setup, NULL);

    tcase_add_test(tc_core, test_logger_rotation_failure_ignored);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_rotation_failure_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/logger/logger_rotation_failure_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
