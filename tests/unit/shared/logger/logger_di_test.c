// Unit tests for DI-based logger API (ik_logger_t context)
// JSONL file writing is disabled; ik_logger_create is a no-op logger.

#include <check.h>
#include <errno.h>
#include <signal.h>
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
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_logger_di_test_%d", getpid());
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

// Test: ik_logger_create returns non-NULL logger
START_TEST(test_logger_create_returns_logger) {
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);
    ck_assert_ptr_nonnull(logger);

    teardown_test();
}
END_TEST

// Test: logger write functions do not crash (no-op)
START_TEST(test_logger_debug_writes_jsonl) {
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);
    ck_assert_ptr_nonnull(logger);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test_di");
    yyjson_mut_obj_add_int(doc, root, "value", 123);

    // Should not crash; logger is a no-op
    ik_logger_debug_json(logger, doc);

    teardown_test();
}
END_TEST

// Test: logger warn does not crash
START_TEST(test_logger_has_level_field) {
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");

    ik_logger_warn_json(logger, doc);

    teardown_test();
}
END_TEST

// Test: logger info does not crash
START_TEST(test_logger_has_timestamp_field) {
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");

    ik_logger_info_json(logger, doc);

    teardown_test();
}
END_TEST

// Test: logger error does not crash
START_TEST(test_logger_has_logline_field) {
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "di_test");
    yyjson_mut_obj_add_int(doc, root, "code", 42);

    ik_logger_error_json(logger, doc);

    teardown_test();
}
END_TEST

// Test: talloc cleanup does not crash
START_TEST(test_logger_cleanup_on_talloc_free) {
    setup_test();

    TALLOC_CTX *logger_ctx = talloc_new(NULL);
    ik_logger_t *logger = ik_logger_create(logger_ctx, test_dir);
    ck_assert_ptr_nonnull(logger);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "before_free");
    ik_logger_debug_json(logger, doc);

    // Should not crash
    talloc_free(logger_ctx);

    teardown_test();
}
END_TEST

// Test: ik_logger_reinit does not crash
START_TEST(test_logger_reinit_changes_location) {
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);
    ck_assert_ptr_nonnull(logger);

    yyjson_mut_doc *doc1 = ik_log_create();
    yyjson_mut_val *root1 = yyjson_mut_doc_get_root(doc1);
    yyjson_mut_obj_add_str(doc1, root1, "event", "before_reinit");
    ik_logger_info_json(logger, doc1);

    char new_dir[256];
    snprintf(new_dir, sizeof(new_dir), "/tmp/ikigai_logger_di_test_new_%d", getpid());
    mkdir(new_dir, 0755);

    // Should not crash
    ik_logger_reinit(logger, new_dir);

    yyjson_mut_doc *doc2 = ik_log_create();
    yyjson_mut_val *root2 = yyjson_mut_doc_get_root(doc2);
    yyjson_mut_obj_add_str(doc2, root2, "event", "after_reinit");
    ik_logger_info_json(logger, doc2);

    rmdir(new_dir);
    teardown_test();
}
END_TEST

// Test: ik_logger_get_fd returns -1 (no file open)
START_TEST(test_logger_get_fd_returns_valid_fd) {
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);
    ck_assert_ptr_nonnull(logger);

    int fd = ik_logger_get_fd(logger);
    ck_assert_int_eq(fd, -1);

    teardown_test();
}
END_TEST

// Test: ik_logger_get_fd returns -1 for NULL logger
START_TEST(test_logger_get_fd_null_logger) {
    int fd = ik_logger_get_fd(NULL);
    ck_assert_int_eq(fd, -1);
}
END_TEST

// Test: ik_logger_create does not create any file (no-op)
START_TEST(test_logger_respects_env_var_override) {
    char override_dir[256];
    snprintf(override_dir, sizeof(override_dir), "/tmp/ikigai_logger_env_test_%d", getpid());
    mkdir(override_dir, 0755);

    setenv("IKIGAI_LOG_DIR", override_dir, 1);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp/should_be_ignored");
    ck_assert_ptr_nonnull(logger);

    // No file should be created — logger is a no-op
    char expected_log[512];
    snprintf(expected_log, sizeof(expected_log), "%s/current.log", override_dir);

    struct stat st;
    ck_assert_int_ne(stat(expected_log, &st), 0);

    talloc_free(ctx);
    unsetenv("IKIGAI_LOG_DIR");
    rmdir(override_dir);
}
END_TEST

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

static Suite *logger_di_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger DI");
    tc_core = tcase_create("Core");
    tcase_add_unchecked_fixture(tc_core, suite_setup, NULL);

    tcase_add_test(tc_core, test_logger_create_returns_logger);
    tcase_add_test(tc_core, test_logger_debug_writes_jsonl);
    tcase_add_test(tc_core, test_logger_has_level_field);
    tcase_add_test(tc_core, test_logger_has_timestamp_field);
    tcase_add_test(tc_core, test_logger_has_logline_field);
    tcase_add_test(tc_core, test_logger_cleanup_on_talloc_free);
    tcase_add_test(tc_core, test_logger_reinit_changes_location);
    tcase_add_test(tc_core, test_logger_get_fd_returns_valid_fd);
    tcase_add_test(tc_core, test_logger_get_fd_null_logger);
    tcase_add_test(tc_core, test_logger_respects_env_var_override);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_di_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/logger/logger_di_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
