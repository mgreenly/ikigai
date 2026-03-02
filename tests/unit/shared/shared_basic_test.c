#include "apps/ikigai/shared.h"
#include "apps/ikigai/paths.h"

#include "shared/credentials.h"
#include "shared/error.h"
#include "apps/ikigai/config.h"
#include "shared/terminal.h"
#include "apps/ikigai/render.h"
#include "apps/ikigai/history.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <errno.h>
#include <talloc.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Mock control state for terminal
static int mock_open_fail = 0;
static int mock_tcgetattr_fail = 0;
static int mock_tcsetattr_fail = 0;
static int mock_tcflush_fail = 0;
static int mock_write_fail = 0;
static int mock_ioctl_fail = 0;
static const char *mock_mkdir_fail_path = NULL;
static const char *mock_stat_fail_path = NULL;

// Mock function prototypes
int posix_open_(const char *pathname, int flags);
int posix_close_(int fd);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
int posix_ioctl_(int fd, unsigned long request, void *argp);
ssize_t posix_write_(int fd, const void *buf, size_t count);
int posix_mkdir_(const char *pathname, mode_t mode);
int posix_stat_(const char *pathname, struct stat *statbuf);

// Mock implementations for POSIX functions
int posix_open_(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;
    if (mock_open_fail) {
        return -1;
    }
    return 42; // Mock fd
}

int posix_close_(int fd)
{
    (void)fd;
    return 0;
}

int posix_tcgetattr_(int fd, struct termios *termios_p)
{
    (void)fd;
    if (mock_tcgetattr_fail) {
        return -1;
    }
    memset(termios_p, 0, sizeof(*termios_p));
    return 0;
}

int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd;
    (void)optional_actions;
    (void)termios_p;
    if (mock_tcsetattr_fail) {
        return -1;
    }
    return 0;
}

int posix_tcflush_(int fd, int queue_selector)
{
    (void)fd;
    (void)queue_selector;
    if (mock_tcflush_fail) {
        return -1;
    }
    return 0;
}

int posix_ioctl_(int fd, unsigned long request, void *argp)
{
    (void)fd;
    (void)request;
    if (mock_ioctl_fail) {
        return -1;
    }
    struct winsize *ws = (struct winsize *)argp;
    ws->ws_row = 24;
    ws->ws_col = 80;
    return 0;
}

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    if (mock_write_fail) {
        return -1;
    }
    return (ssize_t)count;
}

// Mock mkdir to fail for specific path
int posix_mkdir_(const char *pathname, mode_t mode)
{
    if (mock_mkdir_fail_path != NULL && strstr(pathname, mock_mkdir_fail_path) != NULL) {
        errno = EACCES;  // Permission denied
        return -1;
    }
    return mkdir(pathname, mode);
}

// Mock stat to fail for specific path
int posix_stat_(const char *pathname, struct stat *statbuf)
{
    if (mock_stat_fail_path != NULL && strstr(pathname, mock_stat_fail_path) != NULL) {
        errno = ENOENT;  // File not found
        return -1;
    }
    return stat(pathname, statbuf);
}

// Suite-level setup: Set log directory
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

static void reset_mocks(void)
{
    mock_open_fail = 0;
    mock_tcgetattr_fail = 0;
    mock_tcsetattr_fail = 0;
    mock_tcflush_fail = 0;
    mock_write_fail = 0;
    mock_ioctl_fail = 0;
    mock_mkdir_fail_path = NULL;
    mock_stat_fail_path = NULL;
}

// Test basic shared context initialization and memory management
START_TEST(test_shared_ctx_init_and_memory) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;

    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    ck_assert_ptr_nonnull(creds);

    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    ik_shared_ctx_t *shared = NULL;
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);

    // Test init success
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);

    // Test parent allocation
    TALLOC_CTX *actual_parent = talloc_parent(shared);
    ck_assert_ptr_eq(actual_parent, ctx);

    // Test can be freed
    int result = talloc_free(shared);
    ck_assert_int_eq(result, 0);

    talloc_free(ctx);
}
END_TEST

// Test that shared context stores and provides access to config
START_TEST(test_shared_ctx_config) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_model = talloc_strdup(cfg, "test-model");
    cfg->history_size = 100;

    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    ck_assert_ptr_nonnull(creds);

    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    ik_shared_ctx_t *shared = NULL;
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    // Test cfg pointer stored
    ck_assert_ptr_eq(shared->cfg, cfg);
    // Test cfg accessible
    ck_assert_ptr_nonnull(shared->cfg);
    ck_assert_str_eq(shared->cfg->openai_model, "test-model");

    talloc_free(ctx);
}

END_TEST

// Test terminal and render initialization
START_TEST(test_shared_ctx_terminal_and_render) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;

    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    ck_assert_ptr_nonnull(creds);

    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    ik_shared_ctx_t *shared = NULL;
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    // Test term initialized
    ck_assert_ptr_nonnull(shared->term);
    // Test render initialized
    ck_assert_ptr_nonnull(shared->render);
    // Test dimensions match
    ck_assert_int_eq(shared->render->rows, shared->term->screen_rows);
    ck_assert_int_eq(shared->render->cols, shared->term->screen_cols);

    talloc_free(ctx);
}

END_TEST

// Test history initialization
START_TEST(test_shared_ctx_history) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 250;

    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    ck_assert_ptr_nonnull(creds);

    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    ik_shared_ctx_t *shared = NULL;
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    // Test history initialized
    ck_assert_ptr_nonnull(shared->history);
    // Test capacity matches config
    ck_assert_uint_eq(shared->history->capacity, 250);

    talloc_free(ctx);
}

END_TEST

// Test debug manager and pipes initialization
START_TEST(test_shared_ctx_debug) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;

    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    ck_assert_ptr_nonnull(creds);

    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    ik_shared_ctx_t *shared = NULL;
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);

    talloc_free(ctx);
}

END_TEST

// Test that history load failure is gracefully handled
START_TEST(test_shared_ctx_history_load_failure_graceful) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;

    // Use a unique temporary directory that doesn't exist yet
    char unique_dir[256];
    snprintf(unique_dir, sizeof(unique_dir), "/tmp/ikigai_shared_test_history_%d", getpid());

    // Create logger before calling init (using /tmp to avoid mock interference)
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    // Mock stat to return ENOENT for .ikigai, forcing mkdir attempt
    // Then mock mkdir to fail for .ikigai directory creation
    // This will cause history directory creation to fail
    // Key: Create logger BEFORE setting mocks.
    // This allows logger's .ikigai creation to succeed,
    // but history's .ikigai creation to fail.
    // Tests graceful degradation when history load fails.
    mock_stat_fail_path = ".ikigai";
    mock_mkdir_fail_path = ".ikigai";

    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    ck_assert_ptr_nonnull(creds);

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);

    // Should still succeed despite history load failure (graceful degradation)
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(shared->history);

    // Reset mocks after test
    mock_stat_fail_path = NULL;
    mock_mkdir_fail_path = NULL;

    talloc_free(ctx);
}

END_TEST

// Test: ik_shared_ctx_init_with_term covers the term != NULL branch (headless mode)
START_TEST(test_shared_ctx_init_with_headless_term) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;

    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    ck_assert_ptr_nonnull(creds);

    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    // Create a headless term to exercise the term != NULL branch in shared.c
    ik_term_ctx_t *term = ik_term_init_headless(ctx);
    ck_assert_ptr_nonnull(term);

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init_with_term(ctx, cfg, creds, paths, logger, term, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_eq(shared->term, term);

    talloc_free(ctx);
}
END_TEST

static Suite *shared_basic_suite(void)
{
    Suite *s = suite_create("Shared Context Basic");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_core, suite_setup, NULL);
    tcase_add_test(tc_core, test_shared_ctx_init_and_memory);
    tcase_add_test(tc_core, test_shared_ctx_config);
    tcase_add_test(tc_core, test_shared_ctx_terminal_and_render);
    tcase_add_test(tc_core, test_shared_ctx_history);
    tcase_add_test(tc_core, test_shared_ctx_debug);
    tcase_add_test(tc_core, test_shared_ctx_history_load_failure_graceful);
    tcase_add_test(tc_core, test_shared_ctx_init_with_headless_term);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = shared_basic_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/shared_basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
