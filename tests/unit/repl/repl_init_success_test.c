/**
 * @file repl_init_success_test.c
 * @brief Unit tests for successful REPL initialization
 */

#include <check.h>
#include <talloc.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "../../../src/logger.h"

#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/paths.h"
#include "../../../src/credentials.h"
#include "../../test_utils_helper.h"
#include "../../../src/logger.h"

// Mock state (all success cases)
static bool mock_open_should_fail = false;
static bool mock_ioctl_should_fail = false;
static bool mock_sigaction_should_fail = false;
static bool mock_stat_should_fail = false;

// Forward declarations for wrapper functions
int posix_open_(const char *pathname, int flags);
int posix_ioctl_(int fd, unsigned long request, void *argp);
int posix_close_(int fd);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
ssize_t posix_write_(int fd, const void *buf, size_t count);
ssize_t posix_read_(int fd, void *buf, size_t count);
int posix_sigaction_(int signum, const struct sigaction *act, struct sigaction *oldact);
int posix_stat_(const char *pathname, struct stat *statbuf);
int posix_mkdir_(const char *pathname, mode_t mode);

// Suite-level setup: Set log directory
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

// Mock posix_open_
int posix_open_(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;

    if (mock_open_should_fail) {
        return -1;
    }

    return 99;
}

// Mock posix_ioctl_
int posix_ioctl_(int fd, unsigned long request, void *argp)
{
    (void)fd;
    (void)request;

    if (mock_ioctl_should_fail) {
        struct winsize *ws = (struct winsize *)argp;
        ws->ws_row = 0;
        ws->ws_col = 0;
        return 0;
    }

    struct winsize *ws = (struct winsize *)argp;
    ws->ws_row = 24;
    ws->ws_col = 80;
    return 0;
}

int posix_close_(int fd)
{
    (void)fd;
    return 0;
}

int posix_tcgetattr_(int fd, struct termios *termios_p)
{
    (void)fd;
    (void)termios_p;
    return 0;
}

int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd;
    (void)optional_actions;
    (void)termios_p;
    return 0;
}

int posix_tcflush_(int fd, int queue_selector)
{
    (void)fd;
    (void)queue_selector;
    return 0;
}

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    return (ssize_t)count;
}

ssize_t posix_read_(int fd, void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return 0;
}

int posix_sigaction_(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    (void)signum;
    (void)act;
    (void)oldact;

    if (mock_sigaction_should_fail) {
        return -1;
    }

    return 0;
}

int posix_stat_(const char *pathname, struct stat *statbuf)
{
    if (mock_stat_should_fail) {
        errno = EACCES;
        return -1;
    }

    if (strncmp(pathname, "/tmp", 4) == 0) {
        return stat(pathname, statbuf);
    }

    errno = ENOENT;
    return -1;
}

int posix_mkdir_(const char *pathname, mode_t mode)
{
    if (mock_stat_should_fail) {
        errno = EACCES;
        return -1;
    }

    if (strncmp(pathname, "/tmp", 4) == 0) {
        return mkdir(pathname, mode);
    }

    return 0;
}

/* Test: Successful initialization verifies debug manager creation */
START_TEST(test_repl_init_success_debug_manager) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Initialize REPL - should succeed
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);

    // Verify success
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(repl);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST

/* Test: Agent creation at REPL initialization */
START_TEST(test_repl_init_creates_agent) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Initialize REPL - should create an agent
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);

    // Verify success
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(repl);

    // Verify agent was created
    ck_assert_ptr_nonnull(repl->current);

    // Verify agent UUID is valid (non-NULL and non-empty)
    ck_assert_ptr_nonnull(repl->current->uuid);
    ck_assert(strlen(repl->current->uuid) > 0);

    // Verify agent is root agent (no parent)
    ck_assert_ptr_null(repl->current->parent_uuid);

    // Verify agent has reference to shared context
    ck_assert_ptr_eq(repl->current->shared, repl->shared);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST

/* Test: Initial agent is added to agents array */
START_TEST(test_repl_init_agent_in_array) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Initialize REPL
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);
    ck_assert(is_ok(&res));

    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Verify agent array is initialized
    ck_assert_ptr_nonnull(repl->agents);
    ck_assert_uint_eq(repl->agent_count, 1);
    ck_assert(repl->agent_capacity >= 1);

    // Verify initial agent is in array
    ck_assert_ptr_eq(repl->agents[0], repl->current);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST

/* Test: ik_repl_find_agent returns correct agent */
START_TEST(test_repl_find_agent_found) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Initialize REPL
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);
    ck_assert(is_ok(&res));

    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Find the initial agent by UUID
    const char *uuid = repl->current->uuid;
    ik_agent_ctx_t *found = ik_repl_find_agent(repl, uuid);

    // Verify found agent matches current
    ck_assert_ptr_eq(found, repl->current);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST

/* Test: ik_repl_find_agent returns NULL for unknown UUID */
START_TEST(test_repl_find_agent_not_found) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Initialize REPL
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);
    ck_assert(is_ok(&res));

    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Try to find a non-existent agent
    ik_agent_ctx_t *found = ik_repl_find_agent(repl, "nonexistent-uuid");

    // Verify NULL is returned
    ck_assert_ptr_null(found);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST

static Suite *repl_init_success_suite(void)
{
    Suite *s = suite_create("REPL Init Success");

    TCase *tc_success = tcase_create("Successful Initialization");
    tcase_set_timeout(tc_success, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_success, suite_setup, NULL);
    tcase_add_test(tc_success, test_repl_init_success_debug_manager);
    tcase_add_test(tc_success, test_repl_init_creates_agent);
    tcase_add_test(tc_success, test_repl_init_agent_in_array);
    tcase_add_test(tc_success, test_repl_find_agent_found);
    tcase_add_test(tc_success, test_repl_find_agent_not_found);
    suite_add_tcase(s, tc_success);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_init_success_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/repl_init_success_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
