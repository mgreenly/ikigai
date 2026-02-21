#include <check.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/control_socket.h"
#include "apps/ikigai/key_inject.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "shared/terminal.h"

static ik_paths_t *create_test_paths(TALLOC_CTX *ctx, const char *tmpdir)
{
    setenv("IKIGAI_BIN_DIR", tmpdir, 1);
    setenv("IKIGAI_CONFIG_DIR", tmpdir, 1);
    setenv("IKIGAI_DATA_DIR", tmpdir, 1);
    setenv("IKIGAI_LIBEXEC_DIR", tmpdir, 1);
    setenv("IKIGAI_CACHE_DIR", tmpdir, 1);
    setenv("IKIGAI_STATE_DIR", tmpdir, 1);
    setenv("IKIGAI_RUNTIME_DIR", tmpdir, 1);

    ik_paths_t *paths = NULL;
    res_t res = ik_paths_init(ctx, &paths);
    if (is_err(&res)) {
        talloc_free(res.err);
        return NULL;
    }
    return paths;
}

static int32_t setup_connected_socket(TALLOC_CTX *ctx, const char *tmpdir,
                                       ik_control_socket_t **ctl_out)
{
    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ck_assert_ptr_nonnull(paths);

    res_t res = ik_control_socket_init(ctx, paths, ctl_out);
    ck_assert(is_ok(&res));

    int32_t pid = (int32_t)getpid();
    char *socket_path = talloc_asprintf(ctx, "%s/ikigai-%d.sock",
                                         ik_paths_get_runtime_dir(paths), pid);

    int32_t client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ck_assert_int_ge(client_fd, 0);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    int32_t conn = connect(client_fd, (struct sockaddr *)&addr, sizeof(addr));
    ck_assert_int_eq(conn, 0);

    res = ik_control_socket_accept(*ctl_out);
    ck_assert(is_ok(&res));

    return client_fd;
}

static ik_repl_ctx_t *create_test_repl(TALLOC_CTX *ctx)
{
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);

    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    ik_term_ctx_t *term = talloc_zero(shared, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;
    shared->term = term;
    repl->shared = shared;

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->input_buffer_visible = true;
    repl->current = agent;

    repl->key_inject_buf = ik_key_inject_init(repl);

    repl->framebuffer = talloc_strdup(repl, "Hello\r\n");
    repl->framebuffer_len = 7;
    repl->cursor_row = 0;
    repl->cursor_col = 5;

    return repl;
}

START_TEST(test_wait_idle_already_idle)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // state is IK_AGENT_STATE_IDLE by default (zero-initialized)
    write(client_fd, "{\"type\":\"wait_idle\",\"timeout_ms\":5000}\n", 38);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
    ck_assert(strstr(buf, "\"type\":\"idle\"") != NULL);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_wait_idle_deferred_then_idle)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    atomic_store(&repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    write(client_fd, "{\"type\":\"wait_idle\",\"timeout_ms\":5000}\n", 38);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    // Response deferred — nothing readable on client_fd yet
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(client_fd, &fds);
    struct timeval tv = {0, 1000};
    int ready = select(client_fd + 1, &fds, NULL, NULL, &tv);
    ck_assert_int_eq(ready, 0);

    // Transition to idle, tick fires
    atomic_store(&repl->current->state, IK_AGENT_STATE_IDLE);
    ik_control_socket_tick(ctl, repl);

    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
    ck_assert(strstr(buf, "\"type\":\"idle\"") != NULL);

    // client_fd was closed by tick
    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_wait_idle_timeout)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    atomic_store(&repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    write(client_fd, "{\"type\":\"wait_idle\",\"timeout_ms\":1}\n", 35);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    usleep(5000);  // 5ms > 1ms timeout

    ik_control_socket_tick(ctl, repl);

    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
    ck_assert(strstr(buf, "\"type\":\"timeout\"") != NULL);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_wait_idle_already_pending)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    atomic_store(&repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    // First request - deferred
    write(client_fd, "{\"type\":\"wait_idle\",\"timeout_ms\":5000}\n", 38);
    usleep(10000);
    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    // Second client connects (accept closes first client fd, opens new)
    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ck_assert_ptr_nonnull(paths);

    int32_t pid = (int32_t)getpid();
    char *socket_path = talloc_asprintf(ctx, "%s/ikigai-%d.sock",
                                         ik_paths_get_runtime_dir(paths), pid);

    int32_t client_fd2 = socket(AF_UNIX, SOCK_STREAM, 0);
    ck_assert_int_ge(client_fd2, 0);
    struct sockaddr_un addr2;
    memset(&addr2, 0, sizeof(addr2));
    addr2.sun_family = AF_UNIX;
    strncpy(addr2.sun_path, socket_path, sizeof(addr2.sun_path) - 1);
    ck_assert_int_eq(connect(client_fd2, (struct sockaddr *)&addr2, sizeof(addr2)), 0);
    res = ik_control_socket_accept(ctl);
    ck_assert(is_ok(&res));

    write(client_fd2, "{\"type\":\"wait_idle\",\"timeout_ms\":5000}\n", 38);
    usleep(10000);
    res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    char buf[4096];
    ssize_t n = read(client_fd2, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
    ck_assert(strstr(buf, "wait_idle already pending") != NULL);

    close(client_fd);
    close(client_fd2);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_wait_idle_missing_timeout_ms)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    write(client_fd, "{\"type\":\"wait_idle\"}\n", 21);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
    ck_assert(strstr(buf, "Missing or invalid timeout_ms") != NULL);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_tick_client_closed_before_idle_fire)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    atomic_store(&repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    write(client_fd, "{\"type\":\"wait_idle\",\"timeout_ms\":5000}\n", 38);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    // Client disconnects before server fires deferred response
    close(client_fd);
    usleep(5000);

    // Agent becomes idle — tick should not crash
    atomic_store(&repl->current->state, IK_AGENT_STATE_IDLE);
    ik_control_socket_tick(ctl, repl);

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_tick_client_closed_before_timeout_fire)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    atomic_store(&repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    write(client_fd, "{\"type\":\"wait_idle\",\"timeout_ms\":1}\n", 35);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    // Client disconnects before timeout fires
    close(client_fd);
    usleep(5000);  // 5ms > 1ms timeout

    // Tick fires timeout — should not crash
    ik_control_socket_tick(ctl, repl);

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_handle_client_eof_resets_wait_idle)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    atomic_store(&repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    // First: send wait_idle to set deferred state
    write(client_fd, "{\"type\":\"wait_idle\",\"timeout_ms\":5000}\n", 38);
    usleep(10000);
    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    // Client disconnects — handle_client reads EOF
    close(client_fd);
    usleep(5000);

    // Detect EOF via handle_client (layer 1 fix)
    // Need to accept the FD into the server-side first for select to work
    // But client_fd is already closed, so handle_client should read EOF
    // Actually the server-side client_fd is still open — read returns EOF
    res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    // Verify state was reset: connect new client, send wait_idle — should succeed
    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ck_assert_ptr_nonnull(paths);
    int32_t pid = (int32_t)getpid();
    char *socket_path = talloc_asprintf(ctx, "%s/ikigai-%d.sock",
                                         ik_paths_get_runtime_dir(paths), pid);

    int32_t client_fd2 = socket(AF_UNIX, SOCK_STREAM, 0);
    ck_assert_int_ge(client_fd2, 0);
    struct sockaddr_un addr2;
    memset(&addr2, 0, sizeof(addr2));
    addr2.sun_family = AF_UNIX;
    strncpy(addr2.sun_path, socket_path, sizeof(addr2.sun_path) - 1);
    ck_assert_int_eq(connect(client_fd2, (struct sockaddr *)&addr2, sizeof(addr2)), 0);
    res = ik_control_socket_accept(ctl);
    ck_assert(is_ok(&res));

    write(client_fd2, "{\"type\":\"wait_idle\",\"timeout_ms\":5000}\n", 38);
    usleep(10000);
    res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    // Should NOT get "already pending" — deferred is accepted
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(client_fd2, &fds);
    struct timeval tv = {0, 1000};
    int ready = select(client_fd2 + 1, &fds, NULL, NULL, &tv);
    ck_assert_int_eq(ready, 0);  // No immediate response means deferred

    close(client_fd2);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_handle_client_write_fails_closes_cleanly)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Send request then immediately close client side
    write(client_fd, "{\"type\":\"read_framebuffer\"}\n", 27);
    usleep(5000);
    close(client_fd);
    usleep(5000);

    // Server reads the request and tries to write — client is gone
    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_tick_idempotent_after_fire)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    atomic_store(&repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    write(client_fd, "{\"type\":\"wait_idle\",\"timeout_ms\":5000}\n", 38);
    usleep(10000);
    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    // Fire the deferred response
    atomic_store(&repl->current->state, IK_AGENT_STATE_IDLE);
    ik_control_socket_tick(ctl, repl);

    // Read the response to confirm it fired
    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
    ck_assert(strstr(buf, "\"type\":\"idle\"") != NULL);

    // Second tick — should be a no-op, no crash
    ik_control_socket_tick(ctl, repl);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

static Suite *control_socket_tick_suite(void)
{
    Suite *s = suite_create("control_socket_tick");

    TCase *tc = tcase_create("WaitIdle");
    tcase_add_test(tc, test_wait_idle_already_idle);
    tcase_add_test(tc, test_wait_idle_deferred_then_idle);
    tcase_add_test(tc, test_wait_idle_timeout);
    tcase_add_test(tc, test_wait_idle_already_pending);
    tcase_add_test(tc, test_wait_idle_missing_timeout_ms);
    tcase_add_test(tc, test_tick_client_closed_before_idle_fire);
    tcase_add_test(tc, test_tick_client_closed_before_timeout_fire);
    tcase_add_test(tc, test_handle_client_eof_resets_wait_idle);
    tcase_add_test(tc, test_handle_client_write_fails_closes_cleanly);
    tcase_add_test(tc, test_tick_idempotent_after_fire);
    suite_add_tcase(s, tc);

    return s;
}

int32_t main(void)
{
    Suite *s = control_socket_tick_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/control_socket_tick_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
