#include "tests/helpers/test_contexts_helper.h"

#include "shared/error.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/repl.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// Mock control state for terminal
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

static void reset_mocks(void)
{
    mock_open_fail = 0;
    mock_tcgetattr_fail = 0;
    mock_tcsetattr_fail = 0;
    mock_tcflush_fail = 0;
    mock_write_fail = 0;
    mock_ioctl_fail = 0;
}

// Test that test_cfg_create() returns valid config
START_TEST(test_cfg_create_returns_valid_config) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_config_t *cfg = test_cfg_create(ctx);

    ck_assert_ptr_nonnull(cfg);
    ck_assert_int_eq(cfg->history_size, 100);
    ck_assert_ptr_nonnull(cfg->db_host);
    ck_assert_str_eq(cfg->db_host, "localhost");
    ck_assert_int_eq(cfg->db_port, 5432);
    ck_assert_ptr_nonnull(cfg->db_name);
    ck_assert_str_eq(cfg->db_name, "ikigai");
    ck_assert_ptr_nonnull(cfg->db_user);
    ck_assert_str_eq(cfg->db_user, "ikigai");

    talloc_free(ctx);
}
END_TEST
// Test that test_shared_ctx_create() succeeds
START_TEST(test_shared_ctx_create_succeeds) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = NULL;
    res_t result = test_shared_ctx_create(ctx, &shared);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(shared->cfg);
    ck_assert_ptr_nonnull(shared->term);
    ck_assert_ptr_nonnull(shared->render);
    ck_assert_ptr_nonnull(shared->history);

    talloc_free(ctx);
}

END_TEST
// Test that test_repl_create() creates both contexts
START_TEST(test_repl_create_creates_both_contexts) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = NULL;
    ik_repl_ctx_t *repl = NULL;
    res_t result = test_repl_create(ctx, &shared, &repl);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(repl);
    ck_assert_ptr_eq(repl->shared, shared);

    talloc_free(ctx);
}

END_TEST
// Test cleanup via talloc_free works
START_TEST(test_cleanup_via_talloc_free) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = NULL;
    ik_repl_ctx_t *repl = NULL;
    res_t result = test_repl_create(ctx, &shared, &repl);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_nonnull(repl);

    // Free parent context should clean up everything
    int free_result = talloc_free(ctx);
    ck_assert_int_eq(free_result, 0);
}

END_TEST
// Test that test_shared_ctx_create_with_cfg() works
START_TEST(test_shared_ctx_create_with_custom_cfg) {
    reset_mocks();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create custom config
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->history_size = 250;
    cfg->openai_model = talloc_strdup(cfg, "custom-model");

    ik_shared_ctx_t *shared = NULL;
    res_t result = test_shared_ctx_create_with_cfg(ctx, cfg, &shared);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(shared);
    ck_assert_ptr_eq(shared->cfg, cfg);
    ck_assert_int_eq(shared->cfg->history_size, 250);
    ck_assert_str_eq(shared->cfg->openai_model, "custom-model");

    talloc_free(ctx);
}

END_TEST

static Suite *test_contexts_suite(void)
{
    Suite *s = suite_create("Test Contexts Helpers");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_test(tc_core, test_cfg_create_returns_valid_config);
    tcase_add_test(tc_core, test_shared_ctx_create_succeeds);
    tcase_add_test(tc_core, test_repl_create_creates_both_contexts);
    tcase_add_test(tc_core, test_cleanup_via_talloc_free);
    tcase_add_test(tc_core, test_shared_ctx_create_with_custom_cfg);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = test_contexts_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/helpers/test_contexts_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
