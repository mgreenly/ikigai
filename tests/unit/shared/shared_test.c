#include "../../../src/shared.h"

#include "../../../src/error.h"
#include "../../../src/config.h"
#include "../../../src/terminal.h"
#include "../../../src/render.h"
#include "../../../src/history.h"
#include "../../test_utils.h"

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

// Mock function prototypes
int posix_open_(const char *pathname, int flags);
int posix_close_(int fd);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
int posix_ioctl_(int fd, unsigned long request, void *argp);
ssize_t posix_write_(int fd, const void *buf, size_t count);
int posix_mkdir_(const char *pathname, mode_t mode);

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

static void reset_mocks(void)
{
    mock_open_fail = 0;
    mock_tcgetattr_fail = 0;
    mock_tcsetattr_fail = 0;
    mock_tcflush_fail = 0;
    mock_write_fail = 0;
    mock_ioctl_fail = 0;
    mock_mkdir_fail_path = NULL;
}

// Test that ik_shared_ctx_init() succeeds
START_TEST(test_shared_ctx_init_success)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;  // Required for history initialization

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);

    talloc_free(ctx);
}
END_TEST

// Test that shared_ctx is allocated under provided parent
START_TEST(test_shared_ctx_parent_allocation)
{
    reset_mocks();
    TALLOC_CTX *parent = talloc_new(NULL);
    ck_assert_ptr_nonnull(parent);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(parent, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;  // Required for history initialization

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(parent, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(parent, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);

    // Verify parent relationship
    TALLOC_CTX *actual_parent = talloc_parent(shared);
    ck_assert_ptr_eq(actual_parent, parent);

    talloc_free(parent);
}
END_TEST

// Test that shared_ctx can be freed via talloc_free
START_TEST(test_shared_ctx_can_be_freed)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;  // Required for history initialization

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);

    // Free shared context directly
    int result = talloc_free(shared);
    ck_assert_int_eq(result, 0);  // talloc_free returns 0 on success

    talloc_free(ctx);
}
END_TEST

// Test that shared_ctx stores cfg pointer
START_TEST(test_shared_ctx_stores_cfg)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;  // Required for history initialization

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_eq(shared->cfg, cfg);

    talloc_free(ctx);
}
END_TEST

// Test that shared_ctx->cfg is accessible
START_TEST(test_shared_ctx_cfg_accessible)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create cfg with specific value for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_model = talloc_strdup(cfg, "test-model");
    cfg->history_size = 100;  // Required for history initialization

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(shared->cfg);
    ck_assert_str_eq(shared->cfg->openai_model, "test-model");

    talloc_free(ctx);
}
END_TEST

// Test that shared_ctx initializes term
START_TEST(test_shared_ctx_term_initialized)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;  // Required for history initialization

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(shared->term);

    talloc_free(ctx);
}
END_TEST

// Test that shared_ctx initializes render
START_TEST(test_shared_ctx_render_initialized)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;  // Required for history initialization

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(shared->render);

    talloc_free(ctx);
}
END_TEST

// Test that render dimensions match term dimensions
START_TEST(test_shared_ctx_render_matches_term_dimensions)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;  // Required for history initialization

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(shared->term);
    ck_assert_ptr_nonnull(shared->render);

    // Verify dimensions match
    ck_assert_int_eq(shared->render->rows, shared->term->screen_rows);
    ck_assert_int_eq(shared->render->cols, shared->term->screen_cols);

    talloc_free(ctx);
}
END_TEST

// Test that shared_ctx->db_ctx is NULL when not configured
START_TEST(test_shared_ctx_db_ctx_null_when_not_configured)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test (no db_connection_string)
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;  // Required for history initialization

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_null(shared->db_ctx);

    talloc_free(ctx);
}
END_TEST

// Test that shared_ctx->session_id is 0 when not configured
START_TEST(test_shared_ctx_session_id_zero_when_not_configured)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test (no db_connection_string)
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;  // Required for history initialization

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_int_eq(shared->session_id, 0);

    talloc_free(ctx);
}
END_TEST

// Test that shared_ctx->history is initialized
START_TEST(test_shared_ctx_history_initialized)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;  // Set history size

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(shared->history);

    talloc_free(ctx);
}
END_TEST

// Test that history capacity matches config
START_TEST(test_shared_ctx_history_capacity_matches_config)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 250;  // Set custom history size

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(shared->history);
    ck_assert_uint_eq(shared->history->capacity, 250);

    talloc_free(ctx);
}
END_TEST

// Test that debug_mgr is initialized
START_TEST(test_shared_ctx_debug_mgr_initialized)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(shared->debug_mgr);
    ck_assert(!shared->debug_enabled);  // Initially false

    talloc_free(ctx);
}
END_TEST

// Test that debug pipes are created
START_TEST(test_shared_ctx_debug_pipes_created)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;

    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(shared->debug_mgr);
    ck_assert_ptr_nonnull(shared->openai_debug_pipe);
    ck_assert_ptr_nonnull(shared->db_debug_pipe);

    talloc_free(ctx);
}
END_TEST

// Test that history load failure is gracefully handled
START_TEST(test_shared_ctx_history_load_failure_graceful)
{
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal cfg for test
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 100;

    // Use a unique temporary directory that doesn't exist yet
    char unique_dir[256];
    snprintf(unique_dir, sizeof(unique_dir), "/tmp/ikigai_shared_test_history_%d", getpid());

    // Create logger before calling init (using /tmp to avoid mkdir mock interference)
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    // Mock mkdir to fail only for .ikigai directory creation under unique_dir
    // This will cause history directory creation to fail
    mock_mkdir_fail_path = unique_dir;

    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, unique_dir, ".ikigai", logger, &shared);

    // Should still succeed despite history load failure (graceful degradation)
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(shared->history);

    // Reset mock after test
    mock_mkdir_fail_path = NULL;

    talloc_free(ctx);
}
END_TEST

static Suite *shared_suite(void)
{
    Suite *s = suite_create("Shared Context");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_shared_ctx_init_success);
    tcase_add_test(tc_core, test_shared_ctx_parent_allocation);
    tcase_add_test(tc_core, test_shared_ctx_can_be_freed);
    tcase_add_test(tc_core, test_shared_ctx_stores_cfg);
    tcase_add_test(tc_core, test_shared_ctx_cfg_accessible);
    tcase_add_test(tc_core, test_shared_ctx_term_initialized);
    tcase_add_test(tc_core, test_shared_ctx_render_initialized);
    tcase_add_test(tc_core, test_shared_ctx_render_matches_term_dimensions);
    tcase_add_test(tc_core, test_shared_ctx_db_ctx_null_when_not_configured);
    tcase_add_test(tc_core, test_shared_ctx_session_id_zero_when_not_configured);
    tcase_add_test(tc_core, test_shared_ctx_history_initialized);
    tcase_add_test(tc_core, test_shared_ctx_history_capacity_matches_config);
    tcase_add_test(tc_core, test_shared_ctx_debug_mgr_initialized);
    tcase_add_test(tc_core, test_shared_ctx_debug_pipes_created);
    tcase_add_test(tc_core, test_shared_ctx_history_load_failure_graceful);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = shared_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
