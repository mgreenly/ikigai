#include <check.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <talloc.h>
#include <unistd.h>

#include "apps/ikigai/control_socket.h"
#include "apps/ikigai/key_inject.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "shared/terminal.h"

// Forward declarations for wrapper functions we override
int posix_stat_(const char *pathname, struct stat *statbuf);
int posix_mkdir_(const char *pathname, mode_t mode);
int posix_socket_(int domain, int type, int protocol);
int posix_bind_(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int posix_listen_(int sockfd, int backlog);
ssize_t posix_read_(int fd, void *buf, size_t count);
ssize_t posix_write_(int fd, const void *buf, size_t count);

// Mock control flags
static bool mock_stat_fail = false;
static bool mock_mkdir_fail = false;
static bool mock_socket_fail = false;
static bool mock_bind_fail = false;
static bool mock_listen_fail = false;
static bool mock_read_fail = false;

// Override posix_stat_ - can simulate missing directory
int posix_stat_(const char *pathname, struct stat *statbuf)
{
    if (mock_stat_fail) {
        errno = ENOENT;
        return -1;
    }
    (void)pathname;
    memset(statbuf, 0, sizeof(*statbuf));
    return 0;
}

// Override posix_mkdir_ to simulate failure
int posix_mkdir_(const char *pathname, mode_t mode)
{
    (void)pathname;
    (void)mode;
    if (mock_mkdir_fail) {
        errno = EACCES;
        return -1;
    }
    return mkdir(pathname, mode);
}

// Override posix_socket_ to simulate failure
int posix_socket_(int domain, int type, int protocol)
{
    if (mock_socket_fail) {
        errno = EMFILE;
        return -1;
    }
    return socket(domain, type, protocol);
}

// Override posix_bind_ to simulate failure
int posix_bind_(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (mock_bind_fail) {
        errno = EADDRINUSE;
        return -1;
    }
    return bind(sockfd, addr, addrlen);
}

// Override posix_listen_ to simulate failure
int posix_listen_(int sockfd, int backlog)
{
    if (mock_listen_fail) {
        errno = EOPNOTSUPP;
        return -1;
    }
    return listen(sockfd, backlog);
}

// Override posix_read_ to simulate failure
ssize_t posix_read_(int fd, void *buf, size_t count)
{
    if (mock_read_fail) {
        errno = ECONNRESET;
        return -1;
    }
    return read(fd, buf, count);
}

// Override posix_write_ (needed for handle_client responses)
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}

// Helper: create paths with real temp dir
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

static void reset_mocks(void)
{
    mock_stat_fail = false;
    mock_mkdir_fail = false;
    mock_socket_fail = false;
    mock_bind_fail = false;
    mock_listen_fail = false;
    mock_read_fail = false;
}

// Test: mkdir failure in ensure_runtime_dir_exists
START_TEST(test_init_mkdir_fails)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));
    reset_mocks();

    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ck_assert_ptr_nonnull(paths);

    // Make stat fail (directory "missing") so mkdir is attempted, then mkdir fails
    mock_stat_fail = true;
    mock_mkdir_fail = true;

    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, paths, &ctl);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(ctl);
    talloc_free(res.err);

    talloc_free(ctx);
}
END_TEST

// Test: socket() failure
START_TEST(test_init_socket_fails)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));
    reset_mocks();

    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ck_assert_ptr_nonnull(paths);

    mock_socket_fail = true;

    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, paths, &ctl);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(ctl);
    talloc_free(res.err);

    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// Test: bind() failure
START_TEST(test_init_bind_fails)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));
    reset_mocks();

    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ck_assert_ptr_nonnull(paths);

    mock_bind_fail = true;

    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, paths, &ctl);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(ctl);
    talloc_free(res.err);

    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// Test: listen() failure
START_TEST(test_init_listen_fails)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));
    reset_mocks();

    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ck_assert_ptr_nonnull(paths);

    mock_listen_fail = true;

    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, paths, &ctl);
    ck_assert(is_err(&res));
    ck_assert_ptr_null(ctl);
    talloc_free(res.err);

    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

// Test: read error (n < 0) in handle_client
START_TEST(test_handle_client_read_error)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));
    reset_mocks();

    ik_paths_t *paths = create_test_paths(ctx, tmpdir);
    ck_assert_ptr_nonnull(paths);

    ik_control_socket_t *ctl = NULL;
    res_t res = ik_control_socket_init(ctx, paths, &ctl);
    ck_assert(is_ok(&res));

    // Connect a client
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

    res = ik_control_socket_accept(ctl);
    ck_assert(is_ok(&res));

    // Set up minimal repl context
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

    // Make read fail
    mock_read_fail = true;

    res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_err(&res));
    talloc_free(res.err);

    mock_read_fail = false;

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

static Suite *control_socket_errors_suite(void)
{
    Suite *s = suite_create("control_socket_errors");

    TCase *tc_init = tcase_create("InitErrors");
    tcase_add_test(tc_init, test_init_mkdir_fails);
    tcase_add_test(tc_init, test_init_socket_fails);
    tcase_add_test(tc_init, test_init_bind_fails);
    tcase_add_test(tc_init, test_init_listen_fails);
    suite_add_tcase(s, tc_init);

    TCase *tc_client = tcase_create("ClientErrors");
    tcase_add_test(tc_client, test_handle_client_read_error);
    suite_add_tcase(s, tc_client);

    return s;
}

int32_t main(void)
{
    Suite *s = control_socket_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/control_socket_errors_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
