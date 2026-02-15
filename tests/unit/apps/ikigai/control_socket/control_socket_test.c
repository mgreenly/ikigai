#include "apps/ikigai/control_socket.h"

#include "apps/ikigai/paths.h"
#include "shared/error.h"

#include <check.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

static TALLOC_CTX *test_ctx;
static ik_paths_t *test_paths;

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    setenv("IKIGAI_BIN_DIR", "/test/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/test/config", 1);
    setenv("IKIGAI_DATA_DIR", "/test/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/test/libexec", 1);
    setenv("IKIGAI_CACHE_DIR", "/test/cache", 1);
    setenv("IKIGAI_STATE_DIR", "/test/state", 1);
    setenv("IKIGAI_RUNTIME_DIR", "/tmp/ikigai-test-runtime", 1);
    setenv("HOME", "/home/testuser", 1);

    res_t result = ik_paths_init(test_ctx, &test_paths);
    ck_assert(is_ok(&result));
}

static void teardown(void)
{
    talloc_free(test_ctx);

    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
    unsetenv("IKIGAI_CACHE_DIR");
    unsetenv("IKIGAI_STATE_DIR");
    unsetenv("IKIGAI_RUNTIME_DIR");
    unsetenv("HOME");

    rmdir("/tmp/ikigai-test-runtime");
}

START_TEST(test_control_socket_init_success) {
    ik_control_socket_t *socket = NULL;
    res_t result = ik_control_socket_init(test_ctx, test_paths, &socket);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(socket);

    int32_t pid = (int32_t)getpid();
    char expected_path[256];
    snprintf(expected_path, sizeof(expected_path), "/tmp/ikigai-test-runtime/ikigai-%d.sock", pid);

    struct stat st;
    int32_t stat_result = stat(expected_path, &st);
    ck_assert_int_eq(stat_result, 0);

    ik_control_socket_destroy(socket);

    stat_result = stat(expected_path, &st);
    ck_assert_int_eq(stat_result, -1);
}
END_TEST

START_TEST(test_control_socket_init_null_paths) {
    ik_control_socket_t *socket = NULL;
    res_t result = ik_control_socket_init(test_ctx, NULL, &socket);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
    ck_assert_ptr_null(socket);
}
END_TEST

START_TEST(test_control_socket_destroy_removes_file) {
    ik_control_socket_t *socket = NULL;
    res_t result = ik_control_socket_init(test_ctx, test_paths, &socket);
    ck_assert(is_ok(&result));

    int32_t pid = (int32_t)getpid();
    char expected_path[256];
    snprintf(expected_path, sizeof(expected_path), "/tmp/ikigai-test-runtime/ikigai-%d.sock", pid);

    struct stat st;
    int32_t stat_result = stat(expected_path, &st);
    ck_assert_int_eq(stat_result, 0);

    ik_control_socket_destroy(socket);

    stat_result = stat(expected_path, &st);
    ck_assert_int_eq(stat_result, -1);
}
END_TEST

static Suite *control_socket_suite(void)
{
    Suite *s = suite_create("control_socket");

    TCase *tc = tcase_create("core");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_control_socket_init_success);
    tcase_add_test(tc, test_control_socket_init_null_paths);
    tcase_add_test(tc, test_control_socket_destroy_removes_file);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = control_socket_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/control_socket/control_socket_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
