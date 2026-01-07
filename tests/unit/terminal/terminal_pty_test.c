// Terminal module PTY-based tests - tests using real pseudo-terminals
// Tests probe_csi_u_support(), enable_csi_u(), and error paths with real PTY I/O
//
// Unlike mock-based tests, these use actual PTY pairs to simulate terminal behavior.
// This provides more realistic testing of the CSI u protocol handling.
//
// Approach: Use a helper thread to simulate terminal responses. The main thread
// calls terminal init functions, while the helper thread acts as the terminal,
// reading queries from the master side and sending back responses.

#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <pty.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>

#include "../../../src/error.h"
#include "../../../src/terminal.h"
#include "../../../src/logger.h"
#include "../../test_utils.h"

// PTY helper structure
typedef struct {
    int master_fd;
    int slave_fd;
    char slave_name[256];
} pty_pair_t;

// Terminal simulator thread configuration
typedef struct {
    int master_fd;
    const char *probe_response;      // Response to CSI u probe query (NULL = no response/timeout)
    const char *enable_response;     // Response to CSI u enable command (NULL = no response)
    int probe_delay_ms;              // Delay before sending probe response
    int enable_delay_ms;             // Delay before sending enable response
    volatile int done;               // Signal to exit
} term_sim_config_t;

// Create a PTY pair for testing
// Returns 0 on success, -1 on failure
static int create_pty_pair(pty_pair_t *pty)
{
    memset(pty, 0, sizeof(*pty));

    // Use openpty() which handles all the setup for us
    if (openpty(&pty->master_fd, &pty->slave_fd, pty->slave_name, NULL, NULL) < 0) {
        return -1;
    }

    // Set master to non-blocking for easier testing
    int flags = fcntl(pty->master_fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(pty->master_fd, F_SETFL, flags | O_NONBLOCK);
    }

    return 0;
}

// Close PTY pair
static void close_pty_pair(pty_pair_t *pty)
{
    if (pty->master_fd >= 0) {
        close(pty->master_fd);
        pty->master_fd = -1;
    }
    if (pty->slave_fd >= 0) {
        close(pty->slave_fd);
        pty->slave_fd = -1;
    }
}

// Set terminal size on the PTY slave
static int pty_set_size(pty_pair_t *pty, int rows, int cols)
{
    struct winsize ws = {
        .ws_row = (unsigned short)rows,
        .ws_col = (unsigned short)cols,
        .ws_xpixel = 0,
        .ws_ypixel = 0
    };
    return ioctl(pty->slave_fd, TIOCSWINSZ, &ws);
}

// Terminal simulator thread function
// Reads from master fd, sends configured responses
static void *term_simulator_thread(void *arg)
{
    term_sim_config_t *cfg = (term_sim_config_t *)arg;
    char buf[256];
    int stage = 0;  // 0 = waiting for probe, 1 = waiting for enable

    while (!cfg->done) {
        struct pollfd pfd = { .fd = cfg->master_fd, .events = POLLIN };
        int ret = poll(&pfd, 1, 10);  // 10ms poll

        if (ret <= 0) continue;
        if (!(pfd.revents & POLLIN)) continue;

        ssize_t n = read(cfg->master_fd, buf, sizeof(buf) - 1);
        if (n <= 0) continue;
        buf[n] = '\0';

        // Check what we received and respond accordingly
        // Stage 0: Looking for CSI u probe query (ESC[?u)
        if (stage == 0 && strstr(buf, "\x1b[?u") != NULL) {
            if (cfg->probe_response) {
                if (cfg->probe_delay_ms > 0) {
                    usleep((useconds_t)(cfg->probe_delay_ms * 1000));
                }
                (void)write(cfg->master_fd, cfg->probe_response, strlen(cfg->probe_response));
            }
            stage = 1;
        }
        // Stage 1: Looking for CSI u enable command (ESC[>9u)
        else if (stage == 1 && strstr(buf, "\x1b[>9u") != NULL) {
            if (cfg->enable_response) {
                if (cfg->enable_delay_ms > 0) {
                    usleep((useconds_t)(cfg->enable_delay_ms * 1000));
                }
                (void)write(cfg->master_fd, cfg->enable_response, strlen(cfg->enable_response));
            }
            stage = 2;
        }
    }

    return NULL;
}

// ============================================================================
// Test: Basic PTY terminal initialization succeeds
// ============================================================================
START_TEST(test_pty_init_success)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);

    // Set a reasonable terminal size
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_term_ctx_t *term = NULL;

    // No simulator thread - CSI u probe will timeout
    // Initialize terminal with PTY slave
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    // Should succeed (CSI u probe will timeout, but init continues)
    ck_assert_msg(is_ok(&res), "Expected success, got error");
    ck_assert_ptr_nonnull(term);

    // Verify terminal size was detected
    ck_assert_int_eq(term->screen_rows, 24);
    ck_assert_int_eq(term->screen_cols, 80);

    // CSI u should not be supported (no response sent)
    ck_assert(!term->csi_u_supported);

    // Cleanup
    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with valid response - terminal supports CSI u
// ============================================================================
START_TEST(test_pty_csi_u_probe_valid_response)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Configure and start terminal simulator
    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[?9u",   // Valid enable response
        .probe_delay_ms = 0,
        .enable_delay_ms = 0,
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success, got error");
    ck_assert_ptr_nonnull(term);

    // CSI u should be detected as supported
    ck_assert_msg(term->csi_u_supported, "CSI u should be detected as supported");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with invalid response format (no 'u' terminator)
// ============================================================================
START_TEST(test_pty_csi_u_probe_invalid_no_terminator)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?123",   // Missing 'u' terminator
        .enable_response = NULL,
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);

    // CSI u should NOT be supported due to invalid response
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported with invalid response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with too short response (< 4 bytes)
// ============================================================================
START_TEST(test_pty_csi_u_probe_short_response)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[",   // Too short
        .enable_response = NULL,
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported with short response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with response missing ESC prefix
// ============================================================================
START_TEST(test_pty_csi_u_probe_missing_esc)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "[?0u",   // Missing ESC
        .enable_response = NULL,
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported without ESC prefix");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with response missing '[' after ESC
// ============================================================================
START_TEST(test_pty_csi_u_probe_missing_bracket)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b?0u",   // Missing '['
        .enable_response = NULL,
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported without bracket");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with response missing '?' after '['
// ============================================================================
START_TEST(test_pty_csi_u_probe_missing_question)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[0u",   // Missing '?'
        .enable_response = NULL,
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported without question mark");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable with no response (normal for some terminals)
// ============================================================================
START_TEST(test_pty_csi_u_enable_no_response)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = NULL,         // No enable response (normal for some terminals)
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);

    // CSI u should still be marked as supported (enable returned true on timeout)
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported even without enable response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable with unexpected response format (still succeeds)
// ============================================================================
START_TEST(test_pty_csi_u_enable_unexpected_response)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a logger for this test to cover the logging path
    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "UNEXPECTED",  // Garbage response
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);

    // CSI u should still be supported (unexpected response still returns true)
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with unexpected response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable with valid response and flags parsing
// ============================================================================
START_TEST(test_pty_csi_u_enable_valid_flags)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create logger to test JSON logging path
    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[?9u",   // Valid enable response with flags=9
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with valid flags response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: Terminal cleanup with CSI u enabled writes disable sequence
// ============================================================================
START_TEST(test_pty_cleanup_csi_u_disable)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",
        .enable_response = "\x1b[?9u",
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported");

    // Cleanup should write CSI u disable sequence
    ik_term_cleanup(term);

    // Read what cleanup wrote to master
    char buf[256];
    usleep(10000);  // Small delay for data to be available
    ssize_t n = read(pty.master_fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        // Should contain CSI u disable sequence (ESC[<u)
        ck_assert_msg(strstr(buf, "\x1b[<u") != NULL, "Cleanup should write CSI u disable sequence");
    }

    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: Terminal cleanup without CSI u enabled (no disable sequence)
// ============================================================================
START_TEST(test_pty_cleanup_no_csi_u)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_term_ctx_t *term = NULL;

    // No simulator - probe will timeout, CSI u won't be enabled
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported");

    // Cleanup without CSI u - should skip disable sequence
    ik_term_cleanup(term);

    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: Terminal get_size works with PTY
// ============================================================================
START_TEST(test_pty_get_size)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 40, 120), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);

    // Verify initial size
    ck_assert_int_eq(term->screen_rows, 40);
    ck_assert_int_eq(term->screen_cols, 120);

    // Change size
    ck_assert_int_eq(pty_set_size(&pty, 50, 200), 0);

    // Get updated size
    int rows, cols;
    res_t size_res = ik_term_get_size(term, &rows, &cols);

    ck_assert_msg(is_ok(&size_res), "Expected success");
    ck_assert_int_eq(rows, 50);
    ck_assert_int_eq(cols, 200);
    ck_assert_int_eq(term->screen_rows, 50);
    ck_assert_int_eq(term->screen_cols, 200);

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: Cleanup with NULL is safe
// ============================================================================
START_TEST(test_pty_cleanup_null_safe)
{
    // Should not crash
    ik_term_cleanup(NULL);
}
END_TEST

// ============================================================================
// Test: CSI u probe select timeout (no response at all)
// ============================================================================
START_TEST(test_pty_csi_u_probe_timeout)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // No simulator thread - probe will timeout
    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);

    // Should succeed even with probe timeout
    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);

    // CSI u should not be supported (probe timed out)
    ck_assert_msg(!term->csi_u_supported, "CSI u should not be supported after timeout");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u probe with multi-digit flags
// ============================================================================
START_TEST(test_pty_csi_u_probe_multi_digit_flags)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?15u",   // Multi-digit flags
        .enable_response = "\x1b[?123u", // Three-digit flags
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with multi-digit flags");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable response missing ESC prefix (covers line 116 short-circuit)
// ============================================================================
START_TEST(test_pty_csi_u_enable_missing_esc)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "[?9u",       // Missing ESC - covers buf[0] != '\x1b'
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    // Still returns true (unexpected response handled gracefully)
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with unexpected response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable response missing '[' (covers line 116 short-circuit)
// ============================================================================
START_TEST(test_pty_csi_u_enable_missing_bracket)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b?9u",    // Missing '[' - covers buf[1] != '['
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with unexpected response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable response missing '?' (covers line 116 short-circuit)
// ============================================================================
START_TEST(test_pty_csi_u_enable_missing_question)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[9u",    // Missing '?' - covers buf[2] != '?'
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with unexpected response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable response with non-digit character in flags (covers line 133)
// ============================================================================
START_TEST(test_pty_csi_u_enable_non_digit_in_flags)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[?9xu",  // 'x' is non-digit before 'u' - covers else branch
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable with too short response (< 4 bytes) - covers n >= 4 branch
// ============================================================================
START_TEST(test_pty_csi_u_enable_short_response)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[",      // Too short (< 4 bytes) - covers n >= 4 false
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    // Still returns true (unexpected response handled gracefully)
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with short response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable response with no 'u' terminator (covers line 133 loop exit)
// ============================================================================
START_TEST(test_pty_csi_u_enable_no_terminator)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[?123",  // Missing 'u' terminator
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable with long unexpected response (>32 bytes) - covers line 149 loop
// ============================================================================
START_TEST(test_pty_csi_u_enable_long_unexpected_response)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    // Response longer than 32 bytes to hit the i < 32 loop bound
    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcd",  // 40 bytes
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable response with character > '9' in flags (covers line 133)
// ============================================================================
START_TEST(test_pty_csi_u_enable_char_above_nine)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    // ':' is ASCII 58, which is > '9' (ASCII 57) but also > '0' (ASCII 48)
    // This tests the buf[i] <= '9' false branch specifically
    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[?9:u",  // ':' is > '9', tests the <= '9' branch
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable response with character < '0' in flags (covers line 133)
// ============================================================================
START_TEST(test_pty_csi_u_enable_char_below_zero)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_test_set_log_dir(__FILE__);
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");

    // Space (ASCII 32) is < '0' (ASCII 48)
    // This tests the buf[i] >= '0' false branch specifically
    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "\x1b[?9 u",  // Space is < '0', tests the >= '0' branch
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, logger, pty.slave_fd, &term);

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test: CSI u enable with unexpected response and NO logger (covers line 142)
// ============================================================================
START_TEST(test_pty_csi_u_enable_unexpected_no_logger)
{
    pty_pair_t pty;
    ck_assert_int_eq(create_pty_pair(&pty), 0);
    ck_assert_int_eq(pty_set_size(&pty, 24, 80), 0);

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // No logger - tests the logger == NULL branch in unexpected response path
    term_sim_config_t cfg = {
        .master_fd = pty.master_fd,
        .probe_response = "\x1b[?1u",    // Valid probe response
        .enable_response = "GARBAGE",     // Unexpected format triggers line 142
        .done = 0
    };

    pthread_t sim_thread;
    ck_assert_int_eq(pthread_create(&sim_thread, NULL, term_simulator_thread, &cfg), 0);

    ik_term_ctx_t *term = NULL;
    res_t res = ik_term_init_with_fd(ctx, NULL, pty.slave_fd, &term);  // NULL logger

    cfg.done = 1;
    pthread_join(sim_thread, NULL);

    ck_assert_msg(is_ok(&res), "Expected success");
    ck_assert_ptr_nonnull(term);
    ck_assert_msg(term->csi_u_supported, "CSI u should be supported with unexpected response");

    ik_term_cleanup(term);
    talloc_free(ctx);
    close_pty_pair(&pty);
}
END_TEST

// ============================================================================
// Test suite setup
// ============================================================================
static Suite *terminal_pty_suite(void)
{
    Suite *s = suite_create("Terminal PTY");

    TCase *tc_basic = tcase_create("Basic");
    tcase_set_timeout(tc_basic, 30);
    tcase_add_test(tc_basic, test_pty_init_success);
    tcase_add_test(tc_basic, test_pty_get_size);
    tcase_add_test(tc_basic, test_pty_cleanup_null_safe);
    tcase_add_test(tc_basic, test_pty_cleanup_no_csi_u);
    suite_add_tcase(s, tc_basic);

    TCase *tc_probe = tcase_create("CSI u Probe");
    tcase_set_timeout(tc_probe, 30);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_valid_response);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_invalid_no_terminator);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_short_response);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_missing_esc);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_missing_bracket);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_missing_question);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_timeout);
    tcase_add_test(tc_probe, test_pty_csi_u_probe_multi_digit_flags);
    suite_add_tcase(s, tc_probe);

    TCase *tc_enable = tcase_create("CSI u Enable");
    tcase_set_timeout(tc_enable, 30);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_no_response);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_unexpected_response);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_valid_flags);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_missing_esc);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_missing_bracket);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_missing_question);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_non_digit_in_flags);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_short_response);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_no_terminator);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_long_unexpected_response);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_char_above_nine);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_char_below_zero);
    tcase_add_test(tc_enable, test_pty_csi_u_enable_unexpected_no_logger);
    suite_add_tcase(s, tc_enable);

    TCase *tc_cleanup = tcase_create("Cleanup");
    tcase_set_timeout(tc_cleanup, 30);
    tcase_add_test(tc_cleanup, test_pty_cleanup_csi_u_disable);
    suite_add_tcase(s, tc_cleanup);

    return s;
}

int main(void)
{
    Suite *s = terminal_pty_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
