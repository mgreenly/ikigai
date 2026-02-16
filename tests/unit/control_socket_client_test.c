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

#ifdef IKIGAI_DEV
    repl->dev_framebuffer = talloc_strdup(repl, "Hello\r\n");
    repl->dev_framebuffer_len = 7;
    repl->dev_cursor_row = 0;
    repl->dev_cursor_col = 5;
#endif

    return repl;
}

START_TEST(test_client_ready_with_connection)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);

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

    ck_assert(ik_key_inject_pending(repl->key_inject_buf) == 5);

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

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

START_TEST(test_handle_client_disconnect)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    close(client_fd);
    usleep(10000);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    fd_set read_fds;
    FD_ZERO(&read_fds);
    ck_assert(!ik_control_socket_client_ready(ctl, &read_fds));

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

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

    res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_err(&res));
    talloc_free(res.err);

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_handle_client_after_disconnect)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_connected_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    close(client_fd);
    usleep(10000);
    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_err(&res));
    talloc_free(res.err);

    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

static Suite *control_socket_client_suite(void)
{
    Suite *s = suite_create("control_socket_client");

    TCase *tc = tcase_create("HandleClient");
    tcase_add_test(tc, test_client_ready_with_connection);
    tcase_add_test(tc, test_handle_client_unknown_type);
    tcase_add_test(tc, test_handle_client_invalid_json);
    tcase_add_test(tc, test_handle_client_send_keys);
    tcase_add_test(tc, test_handle_client_send_keys_missing);
    tcase_add_test(tc, test_handle_client_read_framebuffer);
    tcase_add_test(tc, test_handle_client_read_framebuffer_null);
    tcase_add_test(tc, test_handle_client_disconnect);
    tcase_add_test(tc, test_handle_client_null_type);
    tcase_add_test(tc, test_handle_client_no_client);
    tcase_add_test(tc, test_handle_client_after_disconnect);
    suite_add_tcase(s, tc);

    return s;
}

int32_t main(void)
{
    Suite *s = control_socket_client_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/control_socket_client_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
