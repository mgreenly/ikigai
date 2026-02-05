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

// Mock control state
static int mock_open_fail = 0;
static int mock_tcgetattr_fail = 0;
static int mock_tcsetattr_fail = 0;
static int mock_tcflush_fail = 0;
static int mock_write_fail = 0;
static int mock_ioctl_fail = 0;

// Mock function prototypes
int posix_open_(const char *pathname, int flags);
int posix_close_(int fd);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
int posix_ioctl_(int fd, unsigned long request, void *argp);
ssize_t posix_write_(int fd, const void *buf, size_t count);

// Mock implementations
int posix_open_(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;
    if (mock_open_fail) {
        return -1;
    }
    return 42;
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

// Suite-level setup
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
}

// Test database context when not configured
START_TEST(test_shared_ctx_database_unconfigured) {
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
    // Test db_ctx is NULL when not configured
    ck_assert_ptr_null(shared->db_ctx);
    // Test session_id is 0 when not configured
    ck_assert_int_eq(shared->session_id, 0);

    talloc_free(ctx);
}

END_TEST

// Test database configuration with credentials
START_TEST(test_shared_ctx_database_configured) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create config with database fields
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;
    cfg->db_host = talloc_strdup(cfg, "testhost");
    cfg->db_port = 5433;
    cfg->db_name = talloc_strdup(cfg, "testdb");
    cfg->db_user = talloc_strdup(cfg, "testuser");

    // Create credentials with db_pass
    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    ck_assert_ptr_nonnull(creds);
    creds->db_pass = talloc_strdup(creds, "testpass");

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

    // Database connection will fail since testhost doesn't exist
    // This test exercises the connection string building code path
    // The init fails because database can't connect, but that's expected
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_DB_CONNECT);

    talloc_free(ctx);
}

END_TEST

// Test database configuration with empty password
START_TEST(test_shared_ctx_database_no_password) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create config with database fields
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;
    cfg->db_host = talloc_strdup(cfg, "localhost");
    cfg->db_port = 5432;
    cfg->db_name = talloc_strdup(cfg, "nonexistent_test_db_12345");
    cfg->db_user = talloc_strdup(cfg, "ikigai");

    // Create credentials without db_pass
    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    ck_assert_ptr_nonnull(creds);
    // db_pass is NULL

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

    // Database connection will fail since nonexistent_test_db_12345 doesn't exist
    // This test exercises the empty password code path
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_DB_CONNECT);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_shared_ctx_database_partial_null_host) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;
    // Set db_name and db_user but leave db_host NULL
    cfg->db_name = talloc_strdup(cfg, "testdb");
    cfg->db_user = talloc_strdup(cfg, "testuser");
    cfg->db_port = 5432;

    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    ck_assert_ptr_nonnull(creds);

    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    ik_shared_ctx_t *shared = NULL;
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    // DB should not be initialized when db_host is NULL
    ck_assert_ptr_null(shared->db_ctx);
    ck_assert_int_eq(shared->session_id, 0);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_shared_ctx_database_partial_null_name) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;
    // Set db_host and db_user but leave db_name NULL
    cfg->db_host = talloc_strdup(cfg, "localhost");
    cfg->db_user = talloc_strdup(cfg, "testuser");
    cfg->db_port = 5432;

    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    ck_assert_ptr_nonnull(creds);

    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    ik_shared_ctx_t *shared = NULL;
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    // DB should not be initialized when db_name is NULL
    ck_assert_ptr_null(shared->db_ctx);
    ck_assert_int_eq(shared->session_id, 0);

    talloc_free(ctx);
}

END_TEST

START_TEST(test_shared_ctx_database_partial_null_user) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;
    // Set db_host and db_name but leave db_user NULL
    cfg->db_host = talloc_strdup(cfg, "localhost");
    cfg->db_name = talloc_strdup(cfg, "testdb");
    cfg->db_port = 5432;

    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    ck_assert_ptr_nonnull(creds);

    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    ik_shared_ctx_t *shared = NULL;
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    // DB should not be initialized when db_user is NULL
    ck_assert_ptr_null(shared->db_ctx);
    ck_assert_int_eq(shared->session_id, 0);

    talloc_free(ctx);
}

END_TEST

static Suite *shared_database_suite(void)
{
    Suite *s = suite_create("Shared Context Database");

    TCase *tc_db = tcase_create("Database");
    tcase_set_timeout(tc_db, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_db, suite_setup, NULL);
    tcase_add_test(tc_db, test_shared_ctx_database_unconfigured);
    tcase_add_test(tc_db, test_shared_ctx_database_configured);
    tcase_add_test(tc_db, test_shared_ctx_database_no_password);
    tcase_add_test(tc_db, test_shared_ctx_database_partial_null_host);
    tcase_add_test(tc_db, test_shared_ctx_database_partial_null_name);
    tcase_add_test(tc_db, test_shared_ctx_database_partial_null_user);
    suite_add_tcase(s, tc_db);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = shared_database_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/shared_database_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
