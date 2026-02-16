#include <check.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "apps/ikigai/control_socket.h"
#include "apps/ikigai/key_inject.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "shared/terminal.h"

// NULL paths returns ERR_INVALID_ARG
START_TEST(test_init_null_paths)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, NULL, &ctl);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(ctl);
    talloc_free(ctx);
}
END_TEST

// Helper to create paths for testing
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

// Helper: create control socket, connect a client, accept connection
// Returns client_fd (caller must close). Sets *ctl_out to the control socket.
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

// Helper: create minimal repl context for handle_client
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

#ifdef IKIGAI_DEV
    repl->dev_framebuffer = talloc_strdup(repl, "Hello\r\n");
    repl->dev_framebuffer_len = 7;
    repl->dev_cursor_row = 0;
    repl->dev_cursor_col = 5;
#endif

    return repl;
}

// Init and destroy lifecycle with real socket
START_TEST(test_init_destroy)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ck_assert_ptr_nonnull(paths);

    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, paths, &ctl);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(ctl);

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// Add to fd_sets populates correctly
START_TEST(test_add_to_fd_sets)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, paths, &ctl);
    ck_assert(is_ok(&res));

    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fd = 0;
    ik_control_socket_add_to_fd_sets(ctl, &read_fds, &max_fd);
    ck_assert_int_gt(max_fd, 0);

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// listen_ready returns false when not in fd_set
START_TEST(test_listen_ready_false)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, paths, &ctl);
    ck_assert(is_ok(&res));

    fd_set read_fds;
    FD_ZERO(&read_fds);
    ck_assert(!ik_control_socket_listen_ready(ctl, &read_fds));

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// client_ready returns false with no client connected
START_TEST(test_client_ready_no_client)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, paths, &ctl);
    ck_assert(is_ok(&res));

    fd_set read_fds;
    FD_ZERO(&read_fds);
    ck_assert(!ik_control_socket_client_ready(ctl, &read_fds));

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// Accept a real connection
START_TEST(test_accept_connection)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);

    // After accept, client_ready should work with proper fd_set
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fd = 0;
    ik_control_socket_add_to_fd_sets(ctl, &read_fds, &max_fd);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// Destroy socket that had a client connected
START_TEST(test_destroy_with_client)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// Accept replaces existing client
START_TEST(test_accept_replaces_client)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, paths, &ctl);
    ck_assert(is_ok(&res));

    int32_t pid = (int32_t)getpid();
    char *socket_path = talloc_asprintf(ctx, "%s/ikigai-%d.sock",
                                         ik_paths_get_runtime_dir(paths), pid);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    int32_t client1 = socket(AF_UNIX, SOCK_STREAM, 0);
    connect(client1, (struct sockaddr *)&addr, sizeof(addr));
    res = ik_control_socket_accept(ctl);
    ck_assert(is_ok(&res));

    int32_t client2 = socket(AF_UNIX, SOCK_STREAM, 0);
    connect(client2, (struct sockaddr *)&addr, sizeof(addr));
    res = ik_control_socket_accept(ctl);
    ck_assert(is_ok(&res));

    close(client1);
    close(client2);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// client_ready returns true when client fd is in fd_set
START_TEST(test_client_ready_with_connection)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);

    // Write something from client so it appears ready on server side
    write(client_fd, "x", 1);
    usleep(10000);

    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_fd = 0;
    ik_control_socket_add_to_fd_sets(ctl, &read_fds, &max_fd);

    struct timeval tv = {0, 10000};
    select(max_fd + 1, &read_fds, NULL, NULL, &tv);

    bool ready = ik_control_socket_client_ready(ctl, &read_fds);
    ck_assert(ready);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// handle_client: unknown message type
START_TEST(test_handle_client_unknown_type)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    write(client_fd, "{\"type\":\"foo\"}\n", 15);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
    ck_assert(strstr(buf, "Unknown message type") != NULL);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// handle_client: invalid JSON
START_TEST(test_handle_client_invalid_json)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    write(client_fd, "not json\n", 9);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
    ck_assert(strstr(buf, "Invalid JSON") != NULL);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// handle_client: send_keys with valid keys
START_TEST(test_handle_client_send_keys)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    write(client_fd, "{\"type\":\"send_keys\",\"keys\":\"hello\"}\n", 35);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
    ck_assert(strstr(buf, "\"type\":\"ok\"") != NULL);

    // Verify keys were injected
    ck_assert(ik_key_inject_pending(repl->key_inject_buf) == 5);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// handle_client: send_keys with missing keys field
START_TEST(test_handle_client_send_keys_missing)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    write(client_fd, "{\"type\":\"send_keys\"}\n", 20);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
    ck_assert(strstr(buf, "Missing keys field") != NULL);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// handle_client: read_framebuffer
START_TEST(test_handle_client_read_framebuffer)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    write(client_fd, "{\"type\":\"read_framebuffer\"}\n", 27);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
#ifdef IKIGAI_DEV
    // With dev_framebuffer set, should get framebuffer JSON
    ck_assert(strstr(buf, "framebuffer") != NULL);
#else
    ck_assert(strstr(buf, "not compiled with IKIGAI_DEV") != NULL);
#endif

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// handle_client: read_framebuffer with NULL dev_framebuffer
START_TEST(test_handle_client_read_framebuffer_null)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

#ifdef IKIGAI_DEV
    talloc_free(repl->dev_framebuffer);
    repl->dev_framebuffer = NULL;
    repl->dev_framebuffer_len = 0;
#endif

    write(client_fd, "{\"type\":\"read_framebuffer\"}\n", 27);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
#ifdef IKIGAI_DEV
    ck_assert(strstr(buf, "No framebuffer available") != NULL);
#else
    ck_assert(strstr(buf, "not compiled with IKIGAI_DEV") != NULL);
#endif

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// handle_client: client disconnect (read returns 0)
START_TEST(test_handle_client_disconnect)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Close client before handle_client reads
    close(client_fd);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    // After disconnect, client_ready should return false
    fd_set read_fds;
    FD_ZERO(&read_fds);
    ck_assert(!ik_control_socket_client_ready(ctl, &read_fds));

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// handle_client: no type field in JSON
START_TEST(test_handle_client_null_type)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    write(client_fd, "{\"data\":\"test\"}\n", 16);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
    ck_assert(strstr(buf, "Unknown message type") != NULL);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// handle_client: no client connected (client_fd < 0)
START_TEST(test_handle_client_no_client)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, paths, &ctl);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Call handle_client without accepting any connection
    res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_err(&res));
    talloc_free(res.err);

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// handle_client: call again after disconnect (client_fd reset to -1)
START_TEST(test_handle_client_after_disconnect)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Close client -> handle_client reads 0 -> resets client_fd to -1
    close(client_fd);
    usleep(10000);
    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    // Second call with no client should return error
    res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_err(&res));
    talloc_free(res.err);

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// Init with socket path too long
START_TEST(test_init_path_too_long)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create a temp dir, then inside it create a deeply nested dir
    // sun_path is 108 bytes, so total path with /ikigai-<pid>.sock must exceed that
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    // Create a long subdir name (90 chars of 'a')
    char longdir[256];
    snprintf(longdir, sizeof(longdir), "%s/%.*s", tmpdir, 90,
             "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
             "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    mkdir(longdir, 0700);

    ik_paths_t *paths = create_test_paths(ctx, longdir);
    ck_assert_ptr_nonnull(paths);

    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, paths, &ctl);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(ctl);
    talloc_free(res.err);

    rmdir(longdir);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

static Suite *control_socket_suite(void)
{
    Suite *s = suite_create("control_socket");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_init_null_paths);
    tcase_add_test(tc_core, test_init_destroy);
    tcase_add_test(tc_core, test_add_to_fd_sets);
    tcase_add_test(tc_core, test_listen_ready_false);
    tcase_add_test(tc_core, test_client_ready_no_client);
    tcase_add_test(tc_core, test_accept_connection);
    tcase_add_test(tc_core, test_destroy_with_client);
    tcase_add_test(tc_core, test_accept_replaces_client);
    tcase_add_test(tc_core, test_init_path_too_long);
    suite_add_tcase(s, tc_core);

    TCase *tc_client = tcase_create("HandleClient");
    tcase_add_test(tc_client, test_client_ready_with_connection);
    tcase_add_test(tc_client, test_handle_client_unknown_type);
    tcase_add_test(tc_client, test_handle_client_invalid_json);
    tcase_add_test(tc_client, test_handle_client_send_keys);
    tcase_add_test(tc_client, test_handle_client_send_keys_missing);
    tcase_add_test(tc_client, test_handle_client_read_framebuffer);
    tcase_add_test(tc_client, test_handle_client_read_framebuffer_null);
    tcase_add_test(tc_client, test_handle_client_disconnect);
    tcase_add_test(tc_client, test_handle_client_null_type);
    tcase_add_test(tc_client, test_handle_client_no_client);
    tcase_add_test(tc_client, test_handle_client_after_disconnect);
    suite_add_tcase(s, tc_client);

    return s;
}

int32_t main(void)
{
    Suite *s = control_socket_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/control_socket_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
