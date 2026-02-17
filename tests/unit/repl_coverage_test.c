#include <check.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>

#include "apps/ikigai/control_socket.h"
#include "apps/ikigai/key_inject.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_internal.h"
#include "apps/ikigai/repl_actions.h"
#include "apps/ikigai/shared.h"
#include "shared/credentials.h"
#include "shared/error.h"
#include "shared/logger.h"
#include "shared/terminal.h"
#include "tests/helpers/test_utils_helper.h"

// Forward declarations for wrapper functions we override
int posix_open_(const char *pathname, int flags);
int posix_ioctl_(int fd, unsigned long request, void *argp);
int posix_close_(int fd);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
ssize_t posix_write_(int fd, const void *buf, size_t count);
ssize_t posix_read_(int fd, void *buf, size_t count);
int posix_stat_(const char *pathname, struct stat *statbuf);
int posix_mkdir_(const char *pathname, mode_t mode);
int posix_rename_(const char *oldpath, const char *newpath);
FILE *fopen_(const char *pathname, const char *mode);
int fclose_(FILE *stream);

// Mock function forward declarations
res_t ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action);
res_t ik_repl_render_frame(ik_repl_ctx_t *repl);
res_t ik_scrollback_append_line_(void *scrollback, const char *text, size_t length);
res_t ik_control_socket_accept(ik_control_socket_t *cs);
res_t ik_control_socket_handle_client(ik_control_socket_t *cs, ik_repl_ctx_t *repl);
bool ik_control_socket_listen_ready(ik_control_socket_t *cs, fd_set *read_fds);
bool ik_control_socket_client_ready(ik_control_socket_t *cs, fd_set *read_fds);

// Mock wrapper implementations
int posix_open_(const char *pathname, int flags)
{
    (void)pathname; (void)flags;
    return 99;
}

int posix_ioctl_(int fd, unsigned long request, void *argp)
{
    (void)fd; (void)request;
    struct winsize *ws = (struct winsize *)argp;
    ws->ws_row = 24;
    ws->ws_col = 80;
    return 0;
}

int posix_close_(int fd) { (void)fd; return 0; }

int posix_tcgetattr_(int fd, struct termios *termios_p)
{
    (void)fd; (void)termios_p;
    return 0;
}

int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd; (void)optional_actions; (void)termios_p;
    return 0;
}

int posix_tcflush_(int fd, int queue_selector)
{
    (void)fd; (void)queue_selector;
    return 0;
}

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd; (void)buf;
    return (ssize_t)count;
}

ssize_t posix_read_(int fd, void *buf, size_t count)
{
    (void)fd; (void)buf; (void)count;
    return 0;
}

int posix_stat_(const char *pathname, struct stat *statbuf)
{
    return stat(pathname, statbuf);
}

int posix_mkdir_(const char *pathname, mode_t mode)
{
    return mkdir(pathname, mode);
}

int posix_rename_(const char *oldpath, const char *newpath)
{
    return rename(oldpath, newpath);
}

FILE *fopen_(const char *pathname, const char *mode)
{
    return fopen(pathname, mode);
}

int fclose_(FILE *stream)
{
    return fclose(stream);
}

// Mock control flags
static bool mock_process_action_fail = false;
static bool mock_accept_fail = false;
static bool mock_handle_client_fail = false;
static bool mock_listen_ready = false;
static bool mock_client_ready = false;
static bool mock_render_frame_fail = false;
static TALLOC_CTX *mock_err_ctx = NULL;

res_t ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action)
{
    (void)repl;
    (void)action;
    if (mock_process_action_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, IO, "Mock process_action failure");
    }
    return OK(NULL);
}

// Mock ik_repl_render_frame
res_t ik_repl_render_frame(ik_repl_ctx_t *repl)
{
    (void)repl;
    if (mock_render_frame_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, IO, "Mock render_frame failure");
    }
    return OK(NULL);
}

// Mock ik_control_socket_accept
res_t ik_control_socket_accept(ik_control_socket_t *cs)
{
    (void)cs;
    if (mock_accept_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, IO, "Mock accept failure");
    }
    return OK(NULL);
}

// Mock ik_control_socket_handle_client
res_t ik_control_socket_handle_client(ik_control_socket_t *cs, ik_repl_ctx_t *repl)
{
    (void)cs;
    (void)repl;
    if (mock_handle_client_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, IO, "Mock handle_client failure");
    }
    return OK(NULL);
}

// Mock ik_control_socket_listen_ready
bool ik_control_socket_listen_ready(ik_control_socket_t *cs, fd_set *read_fds)
{
    (void)cs;
    (void)read_fds;
    return mock_listen_ready;
}

// Mock ik_control_socket_client_ready
bool ik_control_socket_client_ready(ik_control_socket_t *cs, fd_set *read_fds)
{
    (void)cs;
    (void)read_fds;
    return mock_client_ready;
}

// Mock ik_scrollback_append_line_ - just return OK
res_t ik_scrollback_append_line_(void *scrollback, const char *text, size_t length)
{
    (void)scrollback; (void)text; (void)length;
    return OK(NULL);
}

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

static void reset_mocks(void)
{
    mock_process_action_fail = false;
    mock_accept_fail = false;
    mock_handle_client_fail = false;
    mock_listen_ready = false;
    mock_client_ready = false;
    mock_render_frame_fail = false;
    if (mock_err_ctx) {
        talloc_free(mock_err_ctx);
        mock_err_ctx = NULL;
    }
}

// Helper: create a full repl context
static ik_repl_ctx_t *create_repl(TALLOC_CTX *ctx)
{
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    res_t res = ik_paths_init(ctx, &paths);
    if (is_err(&res)) { talloc_free(res.err); return NULL; }

    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) return NULL;

    ik_shared_ctx_t *shared = NULL;
    res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);
    if (is_err(&res)) { talloc_free(res.err); return NULL; }

    res = ik_repl_init(ctx, shared, &repl);
    if (is_err(&res)) { talloc_free(res.err); return NULL; }

    return repl;
}

// Test: handle_control_socket_events with NULL control_socket (early return)
START_TEST(test_handle_control_socket_events_null)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_repl(ctx);
    ck_assert_ptr_nonnull(repl);

    // control_socket is NULL by default (mock stat may prevent init)
    repl->control_socket = NULL;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    ik_repl_handle_control_socket_events(repl, &read_fds);
    // Should return without crashing

    talloc_free(ctx);
}
END_TEST

// Helper: create a dummy control socket pointer for testing
// Since we mock all control_socket functions, we just need a non-NULL pointer
static ik_control_socket_t *create_dummy_control_socket(TALLOC_CTX *ctx)
{
    // Allocate enough memory for the struct (opaque, but we just need non-NULL)
    return (ik_control_socket_t *)talloc_zero_size(ctx, 64);
}

// Test: handle_control_socket_events with accept error
START_TEST(test_handle_control_socket_events_accept_error)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_repl(ctx);
    ck_assert_ptr_nonnull(repl);

    repl->control_socket = create_dummy_control_socket(ctx);
    mock_listen_ready = true;
    mock_accept_fail = true;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    ik_repl_handle_control_socket_events(repl, &read_fds);

    repl->control_socket = NULL;
    talloc_free(ctx);
}
END_TEST

// Test: handle_control_socket_events with handle_client error
START_TEST(test_handle_control_socket_events_client_error)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_repl(ctx);
    ck_assert_ptr_nonnull(repl);

    repl->control_socket = create_dummy_control_socket(ctx);
    mock_client_ready = true;
    mock_handle_client_fail = true;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    ik_repl_handle_control_socket_events(repl, &read_fds);

    repl->control_socket = NULL;
    talloc_free(ctx);
}
END_TEST

// Test: handle_control_socket_events with accept success
START_TEST(test_handle_control_socket_events_accept_ok)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_repl(ctx);
    ck_assert_ptr_nonnull(repl);

    repl->control_socket = create_dummy_control_socket(ctx);
    mock_listen_ready = true;
    mock_accept_fail = false;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    ik_repl_handle_control_socket_events(repl, &read_fds);

    repl->control_socket = NULL;
    talloc_free(ctx);
}
END_TEST

// Test: handle_control_socket_events with client handling success
START_TEST(test_handle_control_socket_events_client_ok)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_repl(ctx);
    ck_assert_ptr_nonnull(repl);

    repl->control_socket = create_dummy_control_socket(ctx);
    mock_client_ready = true;
    mock_handle_client_fail = false;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    ik_repl_handle_control_socket_events(repl, &read_fds);

    repl->control_socket = NULL;
    talloc_free(ctx);
}
END_TEST

// Test: handle_key_injection with render_frame failure
START_TEST(test_handle_key_injection_render_fails)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_repl(ctx);
    ck_assert_ptr_nonnull(repl);

    // Inject a regular character (will produce IK_INPUT_CHAR, triggers render)
    res_t res = ik_key_inject_append(repl->key_inject_buf, "a", 1);
    ck_assert(is_ok(&res));

    mock_render_frame_fail = true;

    bool handled = false;
    res = ik_repl_handle_key_injection(repl, &handled);
    ck_assert(is_err(&res));
    talloc_free(res.err);

    talloc_free(ctx);
}
END_TEST

// Test: handle_key_injection with NULL buffer (early return)
START_TEST(test_handle_key_injection_null_buf)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_repl(ctx);
    ck_assert_ptr_nonnull(repl);

    repl->key_inject_buf = NULL;
    bool handled = true;
    res_t res = ik_repl_handle_key_injection(repl, &handled);
    ck_assert(is_ok(&res));
    ck_assert(!handled);

    talloc_free(ctx);
}
END_TEST

// Test: handle_key_injection with empty buffer (no pending)
START_TEST(test_handle_key_injection_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_repl(ctx);
    ck_assert_ptr_nonnull(repl);

    bool handled = true;
    res_t res = ik_repl_handle_key_injection(repl, &handled);
    ck_assert(is_ok(&res));
    ck_assert(!handled);

    talloc_free(ctx);
}
END_TEST

// Test: handle_key_injection with pending bytes (processes one)
START_TEST(test_handle_key_injection_with_bytes)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_repl(ctx);
    ck_assert_ptr_nonnull(repl);

    // Inject some bytes
    res_t res = ik_key_inject_append(repl->key_inject_buf, "ab", 2);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_key_inject_pending(repl->key_inject_buf), 2);

    bool handled = false;
    res = ik_repl_handle_key_injection(repl, &handled);
    ck_assert(is_ok(&res));
    ck_assert(handled);

    // Should have drained one byte
    ck_assert_uint_eq(ik_key_inject_pending(repl->key_inject_buf), 1);

    talloc_free(ctx);
}
END_TEST

// Test: handle_key_injection with ESC byte (IK_INPUT_UNKNOWN)
START_TEST(test_handle_key_injection_unknown_action)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_repl(ctx);
    ck_assert_ptr_nonnull(repl);

    // Inject ESC byte (0x1b) - parser enters escape state, returns IK_INPUT_UNKNOWN
    char esc = '\x1b';
    res_t res = ik_key_inject_append(repl->key_inject_buf, &esc, 1);
    ck_assert(is_ok(&res));

    bool handled = false;
    res = ik_repl_handle_key_injection(repl, &handled);
    ck_assert(is_ok(&res));
    ck_assert(handled);

    talloc_free(ctx);
}
END_TEST

// Test: handle_key_injection with process_action failure
START_TEST(test_handle_key_injection_process_action_fails)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_repl(ctx);
    ck_assert_ptr_nonnull(repl);

    // Inject a byte
    res_t res = ik_key_inject_append(repl->key_inject_buf, "x", 1);
    ck_assert(is_ok(&res));

    mock_process_action_fail = true;

    bool handled = false;
    res = ik_repl_handle_key_injection(repl, &handled);
    ck_assert(is_err(&res));
    talloc_free(res.err);

    talloc_free(ctx);
}
END_TEST

// Test: submit_line with dead agent (silent rejection)
START_TEST(test_submit_line_dead_agent)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_repl(ctx);
    ck_assert_ptr_nonnull(repl);

    // Mark agent as dead
    repl->current->dead = true;

    // Add text to input buffer
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'x'};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Submit should silently succeed without processing
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    talloc_free(ctx);
}
END_TEST

static Suite *repl_coverage_suite(void)
{
    Suite *s = suite_create("REPL Coverage");

    TCase *tc_control = tcase_create("ControlSocketEvents");
    tcase_set_timeout(tc_control, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_control, suite_setup, reset_mocks);
    tcase_add_test(tc_control, test_handle_control_socket_events_null);
    tcase_add_test(tc_control, test_handle_control_socket_events_accept_error);
    tcase_add_test(tc_control, test_handle_control_socket_events_accept_ok);
    tcase_add_test(tc_control, test_handle_control_socket_events_client_error);
    tcase_add_test(tc_control, test_handle_control_socket_events_client_ok);
    suite_add_tcase(s, tc_control);

    TCase *tc_inject = tcase_create("KeyInjection");
    tcase_set_timeout(tc_inject, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_inject, suite_setup, reset_mocks);
    tcase_add_test(tc_inject, test_handle_key_injection_null_buf);
    tcase_add_test(tc_inject, test_handle_key_injection_empty);
    tcase_add_test(tc_inject, test_handle_key_injection_with_bytes);
    tcase_add_test(tc_inject, test_handle_key_injection_unknown_action);
    tcase_add_test(tc_inject, test_handle_key_injection_process_action_fails);
    tcase_add_test(tc_inject, test_handle_key_injection_render_fails);
    suite_add_tcase(s, tc_inject);

    TCase *tc_submit = tcase_create("SubmitLine");
    tcase_set_timeout(tc_submit, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_submit, suite_setup, reset_mocks);
    tcase_add_test(tc_submit, test_submit_line_dead_agent);
    suite_add_tcase(s, tc_submit);

    return s;
}

int main(void)
{
    Suite *s = repl_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
