/**
 * @file repl_init_db_test.c
 * @brief Unit tests for REPL database initialization (db_init and agent_zero)
 */

#include <check.h>
#include <talloc.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/paths.h"
#include "../../../src/credentials.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/agent.h"
#include "../../test_utils_helper.h"
#include "../../../src/logger.h"

// Mock state for controlling failures
static bool mock_db_init_should_fail = false;
static bool mock_sigaction_should_fail = false;
static bool mock_ensure_agent_zero_should_fail = false;

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
res_t ik_db_init_(TALLOC_CTX *mem_ctx, const char *conn_str, const char *data_dir, void **out_ctx);
res_t ik_db_message_insert(ik_db_ctx_t *db_ctx,
                           int64_t session_id,
                           const char *agent_uuid,
                           const char *kind,
                           const char *content,
                           const char *data_json);
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
typedef struct ik_paths_t ik_paths_t;
res_t ik_db_ensure_agent_zero(ik_db_ctx_t *db, ik_paths_t *paths, char **out_uuid);
res_t ik_db_agent_insert(ik_db_ctx_t *db_ctx, const ik_agent_ctx_t *agent);
res_t ik_db_agent_get(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, const char *uuid, ik_db_agent_row_t **out);
res_t ik_db_agent_list_running(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx, ik_db_agent_row_t ***out, size_t *count);
res_t ik_db_agent_update_provider(ik_db_ctx_t *db_ctx,
                                  const char *uuid,
                                  const char *provider,
                                  const char *model,
                                  const char *thinking_level);
res_t ik_repl_restore_agents(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx);

// Forward declaration for suite function
static Suite *repl_init_db_suite(void);

// Suite-level setup: Set log directory
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

// Mock ik_db_init_ to test database connection failure
res_t ik_db_init_(TALLOC_CTX *mem_ctx, const char *conn_str, const char *data_dir, void **out_ctx)
{
    (void)conn_str;
    (void)data_dir;

    if (mock_db_init_should_fail) {
        return ERR(mem_ctx, DB_CONNECT, "Mock database connection failure");
    }

    // For session restore failure test, we need to return success
    // but provide a dummy context
    ik_db_ctx_t *dummy_ctx = talloc_zero(mem_ctx, ik_db_ctx_t);
    if (dummy_ctx == NULL) {
        return ERR(mem_ctx, IO, "Out of memory");
    }
    *out_ctx = (void *)dummy_ctx;
    return OK(dummy_ctx);
}

// Mock ik_db_ensure_agent_zero (needed because repl_init calls it)
res_t ik_db_ensure_agent_zero(ik_db_ctx_t *db, ik_paths_t *paths, char **out_uuid)
{
    (void)paths;  // Unused in mock

    if (mock_ensure_agent_zero_should_fail) {
        return ERR(db, IO, "Mock agent zero query failure");
    }

    // Return a dummy UUID allocated on the db context
    *out_uuid = talloc_strdup(db, "agent-zero-uuid");
    if (*out_uuid == NULL) {  // LCOV_EXCL_BR_LINE
        return ERR(db, OUT_OF_MEMORY, "Out of memory");  // LCOV_EXCL_LINE
    }
    return OK(NULL);
}

// Mock ik_db_agent_insert (needed because ik_cmd_fork calls it)
res_t ik_db_agent_insert(ik_db_ctx_t *db_ctx, const ik_agent_ctx_t *agent)
{
    (void)db_ctx;
    (void)agent;
    return OK(NULL);
}

// Mock ik_db_agent_get (needed because ik_cmd_fork tests call it)
res_t ik_db_agent_get(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, const char *uuid, ik_db_agent_row_t **out)
{
    (void)db_ctx;
    (void)uuid;
    ik_db_agent_row_t *row = talloc_zero(ctx, ik_db_agent_row_t);
    if (row == NULL) {  // LCOV_EXCL_BR_LINE
        return ERR(ctx, OUT_OF_MEMORY, "Out of memory");  // LCOV_EXCL_LINE
    }
    row->status = talloc_strdup(row, "running");
    *out = row;
    return OK(NULL);
}

// Mock ik_db_agent_get_last_message_id (needed because ik_cmd_fork calls it)
res_t ik_db_agent_get_last_message_id(ik_db_ctx_t *db_ctx, const char *agent_uuid,
                                      int64_t *out_message_id)
{
    (void)db_ctx;
    (void)agent_uuid;
    *out_message_id = 0;  // No messages
    return OK(NULL);
}

// Mock ik_db_agent_mark_dead (needed because ik_cmd_kill calls it)
res_t ik_db_agent_mark_dead(ik_db_ctx_t *db_ctx, const char *uuid)
{
    (void)db_ctx;
    (void)uuid;
    return OK(NULL);
}

// Mock ik_db_agent_list_running (needed because ik_cmd_agents calls it)
res_t ik_db_agent_list_running(ik_db_ctx_t *db_ctx, TALLOC_CTX *mem_ctx,
                               ik_db_agent_row_t ***out, size_t *count)
{
    (void)db_ctx;
    *out = talloc_zero_array(mem_ctx, ik_db_agent_row_t *, 0);
    *count = 0;
    return OK(NULL);
}

// Mock ik_db_agent_update_provider (needed because ik_cmd_model calls it)
res_t ik_db_agent_update_provider(ik_db_ctx_t *db_ctx, const char *uuid,
                                  const char *provider, const char *model, const char *thinking_level)
{
    (void)db_ctx;
    (void)uuid;
    (void)provider;
    (void)model;
    (void)thinking_level;
    return OK(NULL);
}

// Mock ik_repl_restore_agents (needed because repl_init calls it) - always succeeds in db_init tests
res_t ik_repl_restore_agents(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx)
{
    (void)repl;
    (void)db_ctx;
    return OK(NULL);
}

// Mock ik_db_message_insert (needed because session_restore calls it)
res_t ik_db_message_insert(ik_db_ctx_t *db_ctx,
                           int64_t session_id,
                           const char *agent_uuid,
                           const char *kind,
                           const char *content,
                           const char *data_json)
{
    (void)db_ctx;
    (void)session_id;
    (void)agent_uuid;
    (void)kind;
    (void)content;
    (void)data_json;
    return OK(NULL);
}

// Mock ik_db_session_create (needed because session_restore calls it) - always succeeds in db_init tests
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx;
    *session_id_out = 1;  // Return a dummy session ID
    return OK(NULL);
}

// Mock ik_db_session_get_active (needed because session_restore calls it) - always succeeds in db_init tests
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx;
    *session_id_out = 0;  // No active session - return 0 (not an error)
    return OK(NULL);
}

// Mock POSIX functions - pass-through to avoid link errors
int posix_open_(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;
    return 99;  // Dummy fd
}

int posix_ioctl_(int fd, unsigned long request, void *argp)
{
    (void)fd;
    (void)request;

    // Return valid dimensions
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
        return -1;  // Failure
    }
    return 0;  // Success
}

/* Test: Database init failure */
START_TEST(test_repl_init_db_init_failure) {
    void *ctx = talloc_new(NULL);

    // Enable mock failure
    mock_db_init_should_fail = true;

    // Attempt to initialize shared context with database - should fail
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->db_host = talloc_strdup(cfg, "localhost");
    cfg->db_port = 5432;
    cfg->db_name = talloc_strdup(cfg, "test");
    cfg->db_user = talloc_strdup(cfg, "test");
    // Create shared context - should fail at db_init
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

    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);

    // Verify failure at shared_ctx_init level
    ck_assert(is_err(&res));
    ck_assert_ptr_null(shared);

    // Cleanup mock state
    mock_db_init_should_fail = false;

    talloc_free(ctx);
}
END_TEST
/* Test: Agent zero ensure failure */
START_TEST(test_repl_init_ensure_agent_zero_failure) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Enable mock failure for ensure_agent_zero
    mock_ensure_agent_zero_should_fail = true;

    // Attempt to initialize REPL with database - should fail during agent zero ensure
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->db_host = talloc_strdup(cfg, "localhost");
    cfg->db_port = 5432;
    cfg->db_name = talloc_strdup(cfg, "test");
    cfg->db_user = talloc_strdup(cfg, "test");
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

    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);

    // Verify failure
    ck_assert(is_err(&res));
    ck_assert_ptr_null(repl);

    // Cleanup mock state
    mock_ensure_agent_zero_should_fail = false;

    talloc_free(ctx);
}

END_TEST
/* Test: Successful database initialization and session restore */
START_TEST(test_repl_init_db_success) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Both db_init and session_restore should succeed (mocks return success by default)
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->db_host = talloc_strdup(cfg, "localhost");
    cfg->db_port = 5432;
    cfg->db_name = talloc_strdup(cfg, "test");
    cfg->db_user = talloc_strdup(cfg, "test");
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

    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);

    // Verify success
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(repl);
    ck_assert_ptr_nonnull(repl->shared->db_ctx);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST
/* Test: Signal handler init failure with db_ctx allocated (line 80-81 cleanup) */
START_TEST(test_repl_init_signal_handler_failure_with_db) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Enable mock failure for sigaction
    mock_sigaction_should_fail = true;

    // Attempt to initialize REPL with database - db_init succeeds, signal_handler fails
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->db_host = talloc_strdup(cfg, "localhost");
    cfg->db_port = 5432;
    cfg->db_name = talloc_strdup(cfg, "test");
    cfg->db_user = talloc_strdup(cfg, "test");
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

    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);

    // Verify failure
    ck_assert(is_err(&res));
    ck_assert_ptr_null(repl);

    // Cleanup mock state
    mock_sigaction_should_fail = false;

    talloc_free(ctx);
}

END_TEST

static Suite *repl_init_db_suite(void)
{
    Suite *s = suite_create("REPL Database Initialization");

    TCase *tc_db = tcase_create("Database Failures");
    tcase_set_timeout(tc_db, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_db, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_db, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_db, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_db, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_db, suite_setup, NULL);
    tcase_add_test(tc_db, test_repl_init_db_init_failure);
    tcase_add_test(tc_db, test_repl_init_ensure_agent_zero_failure);
    tcase_add_test(tc_db, test_repl_init_signal_handler_failure_with_db);
    suite_add_tcase(s, tc_db);

    TCase *tc_db_success = tcase_create("Database Success");
    tcase_set_timeout(tc_db_success, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_db_success, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_db_success, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_db_success, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_db_success, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_db_success, suite_setup, NULL);
    tcase_add_test(tc_db_success, test_repl_init_db_success);
    suite_add_tcase(s, tc_db_success);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_init_db_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/repl_init_db_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
