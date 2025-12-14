/**
 * @file repl_init_db_test.c
 * @brief Unit tests for REPL database initialization error handling
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
#include "../../../src/db/connection.h"
#include "../../../src/repl/session_restore.h"
#include "../../test_utils.h"
#include "../../../src/logger.h"

// Mock state for controlling failures
static bool mock_db_init_should_fail = false;
static bool mock_session_restore_should_fail = false;
static bool mock_sigaction_should_fail = false;

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
res_t ik_db_init_(TALLOC_CTX *mem_ctx, const char *conn_str, void **out_ctx);
res_t ik_repl_restore_session_(void *repl, void *db_ctx, void *cfg);
res_t ik_db_ensure_agent_zero(ik_db_ctx_t *db, char **out_uuid);
res_t ik_db_message_insert(ik_db_ctx_t *db_ctx, int64_t session_id,
                           const char *kind, const char *content,
                           const char *data_json);
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
res_t ik_db_messages_load(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id);

// Forward declaration for suite function
static Suite *repl_init_db_suite(void);

// Mock ik_db_init_ to test database connection failure
res_t ik_db_init_(TALLOC_CTX *mem_ctx, const char *conn_str, void **out_ctx)
{
    (void)conn_str;

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

// Mock ik_repl_restore_session_ to test session restoration failure
res_t ik_repl_restore_session_(void *repl, void *db_ctx, void *cfg)
{
    (void)db_ctx;
    (void)cfg;

    if (mock_session_restore_should_fail) {
        return ERR(repl, IO, "Mock session restore failure");
    }

    return OK(NULL);
}

// Mock ik_db_ensure_agent_zero to provide a test UUID
res_t ik_db_ensure_agent_zero(ik_db_ctx_t *db, char **out_uuid)
{
    (void)db;

    // Return a test UUID
    *out_uuid = talloc_strdup(db, "test-agent-zero-uuid");
    if (*out_uuid == NULL) {
        return ERR(db, OUT_OF_MEMORY, "Out of memory");
    }
    return OK(*out_uuid);
}

// Mock ik_db_message_insert (needed because session_restore calls it)
res_t ik_db_message_insert(ik_db_ctx_t *db_ctx,
                           int64_t session_id,
                           const char *kind,
                           const char *content,
                           const char *data_json)
{
    (void)db_ctx;
    (void)session_id;
    (void)kind;
    (void)content;
    (void)data_json;
    return OK(NULL);
}

// Mock ik_db_session_create (needed because session_restore calls it)
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx;
    *session_id_out = 1;  // Return a dummy session ID
    return OK(NULL);
}

// Mock ik_db_session_get_active (needed because session_restore calls it)
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx;
    (void)session_id_out;
    return ERR(db_ctx, IO, "No active session");  // Return not found to trigger session creation
}

// Mock ik_db_messages_load (needed because session_restore calls it)
res_t ik_db_messages_load(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id)
{
    (void)ctx;
    (void)db_ctx;
    (void)session_id;
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
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->db_connection_string = talloc_strdup(cfg, "postgresql://localhost/test");
    // Create shared context - should fail at db_init
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    // Verify failure at shared_ctx_init level
    ck_assert(is_err(&res));
    ck_assert_ptr_null(shared);

    // Cleanup mock state
    mock_db_init_should_fail = false;

    talloc_free(ctx);
}
END_TEST
/* Test: Session restore failure */
START_TEST(test_repl_init_session_restore_failure)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Enable mock failure for session restore
    mock_session_restore_should_fail = true;

    // Attempt to initialize REPL with database - should fail during session restore
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->db_connection_string = talloc_strdup(cfg, "postgresql://localhost/test");
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);

    // Verify failure
    ck_assert(is_err(&res));
    ck_assert_ptr_null(repl);

    // Cleanup mock state
    mock_session_restore_should_fail = false;

    talloc_free(ctx);
}

END_TEST
/* Test: Successful database initialization and session restore */
START_TEST(test_repl_init_db_success)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Both db_init and session_restore should succeed (mocks return success by default)
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->db_connection_string = talloc_strdup(cfg, "postgresql://localhost/test");
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
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
START_TEST(test_repl_init_signal_handler_failure_with_db)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    // Enable mock failure for sigaction
    mock_sigaction_should_fail = true;

    // Attempt to initialize REPL with database - db_init succeeds, signal_handler fails
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->db_connection_string = talloc_strdup(cfg, "postgresql://localhost/test");
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
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
    tcase_set_timeout(tc_db, 30);
    tcase_add_test(tc_db, test_repl_init_db_init_failure);
    tcase_add_test(tc_db, test_repl_init_session_restore_failure);
    tcase_add_test(tc_db, test_repl_init_signal_handler_failure_with_db);
    suite_add_tcase(s, tc_db);

    TCase *tc_db_success = tcase_create("Database Success");
    tcase_set_timeout(tc_db_success, 30);
    tcase_add_test(tc_db_success, test_repl_init_db_success);
    suite_add_tcase(s, tc_db_success);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_init_db_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
