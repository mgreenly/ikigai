#include <check.h>
#include <inttypes.h>
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
#include "apps/ikigai/token_cache.h"
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

/* Set up control socket + connected client, return client fd */
static int32_t setup_socket(TALLOC_CTX *ctx, const char *tmpdir,
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

    int conn = connect(client_fd, (struct sockaddr *)&addr, sizeof(addr));
    ck_assert_int_eq(conn, 0);

    res = ik_control_socket_accept(*ctl_out);
    ck_assert(is_ok(&res));
    return client_fd;
}

/* Create repl with no token cache (agent->token_cache == NULL) */
static ik_repl_ctx_t *create_repl_no_cache(TALLOC_CTX *ctx)
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
    agent->token_cache = NULL;
    repl->current = agent;
    repl->key_inject_buf = ik_key_inject_init(repl);
    return repl;
}

/* Create repl with a populated token cache */
static ik_repl_ctx_t *create_repl_with_cache(TALLOC_CTX *ctx,
                                               int32_t budget,
                                               int32_t *turn_tokens,
                                               size_t  turn_count,
                                               size_t  pruned)
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

    ik_token_cache_t *cache = ik_token_cache_create(agent, agent);
    ik_token_cache_set_budget(cache, budget);

    for (size_t i = 0; i < pruned; i++) {
        ik_token_cache_add_turn(cache);
        ik_token_cache_record_turn(cache, 0, 50);
        ik_token_cache_prune_oldest_turn(cache);
    }
    for (size_t i = 0; i < turn_count; i++) {
        ik_token_cache_add_turn(cache);
        ik_token_cache_record_turn(cache, i, turn_tokens[i]);
    }

    agent->token_cache = cache;
    repl->key_inject_buf = ik_key_inject_init(repl);
    return repl;
}

/* Do a round-trip: client writes request, server handles, client reads response */
static void do_round_trip(int32_t client_fd, ik_control_socket_t *ctl,
                           ik_repl_ctx_t *repl,
                           char *buf, size_t buf_size)
{
    const char *req = "{\"type\":\"read_token_cache\"}\n";
    ssize_t w = write(client_fd, req, strlen(req));
    ck_assert_int_gt(w, 0);

    res_t res = ik_control_socket_handle_client(ctl, repl);
    ck_assert(is_ok(&res));

    ssize_t n = read(client_fd, buf, buf_size - 1);
    ck_assert_int_gt(n, 0);
    buf[n] = '\0';
}

/* ---- Accessor unit tests ---- */

START_TEST(test_introspection_defaults)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ik_token_cache_t *cache = ik_token_cache_create(ctx, agent);

    ck_assert_int_eq(ik_token_cache_get_budget(cache), 100000);
    ck_assert_int_eq((int32_t)ik_token_cache_get_turn_count(cache), 0);
    ck_assert_int_eq((int32_t)ik_token_cache_get_context_start_turn(cache), 0);
    ck_assert_int_eq(ik_token_cache_peek_total(cache), 0);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_set_budget_and_peek_turn)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ik_token_cache_t *cache = ik_token_cache_create(ctx, agent);

    ik_token_cache_set_budget(cache, 8192);
    ck_assert_int_eq(ik_token_cache_get_budget(cache), 8192);

    ik_token_cache_add_turn(cache);
    ck_assert_int_eq(ik_token_cache_peek_turn_tokens(cache, 0), 0);

    ik_token_cache_record_turn(cache, 0, 999);
    ck_assert_int_eq(ik_token_cache_peek_turn_tokens(cache, 0), 999);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_context_start_turn_increments)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *agent = talloc_zero(ctx, ik_agent_ctx_t);
    ik_token_cache_t *cache = ik_token_cache_create(ctx, agent);

    ck_assert_int_eq((int32_t)ik_token_cache_get_context_start_turn(cache), 0);

    ik_token_cache_add_turn(cache);
    ik_token_cache_record_turn(cache, 0, 100);
    ik_token_cache_add_turn(cache);
    ik_token_cache_record_turn(cache, 1, 200);

    ik_token_cache_prune_oldest_turn(cache);
    ck_assert_int_eq((int32_t)ik_token_cache_get_context_start_turn(cache), 1);
    ck_assert_int_eq((int32_t)ik_token_cache_get_turn_count(cache), 1);

    ik_token_cache_prune_oldest_turn(cache);
    ck_assert_int_eq((int32_t)ik_token_cache_get_context_start_turn(cache), 2);
    ck_assert_int_eq((int32_t)ik_token_cache_get_turn_count(cache), 0);

    talloc_free(ctx);
}
END_TEST

/* ---- Dispatch round-trip tests ---- */

START_TEST(test_null_cache_returns_empty_json)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_socket(ctx, tmpdir, &ctl);
    ik_repl_ctx_t *repl = create_repl_no_cache(ctx);

    char buf[4096];
    do_round_trip(client_fd, ctl, repl, buf, sizeof(buf));

    ck_assert_ptr_nonnull(strstr(buf, "\"total_tokens\":0"));
    ck_assert_ptr_nonnull(strstr(buf, "\"budget\":100000"));
    ck_assert_ptr_nonnull(strstr(buf, "\"turn_count\":0"));
    ck_assert_ptr_nonnull(strstr(buf, "\"context_start_index\":0"));
    ck_assert_ptr_nonnull(strstr(buf, "\"turns\":[]"));

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_populated_cache_json)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char tmpdir[] = "/tmp/ik_test_XXXXXX";
    ck_assert_ptr_nonnull(mkdtemp(tmpdir));

    ik_control_socket_t *ctl = NULL;
    int32_t client_fd = setup_socket(ctx, tmpdir, &ctl);

    int32_t tokens[3] = {120, 140, 120};
    /* 2 pruned turns, 3 active turns → context_start_index=2, indices 2,3,4 */
    ik_repl_ctx_t *repl = create_repl_with_cache(ctx, 500, tokens, 3, 2);

    char buf[4096];
    do_round_trip(client_fd, ctl, repl, buf, sizeof(buf));

    ck_assert_ptr_nonnull(strstr(buf, "\"budget\":500"));
    ck_assert_ptr_nonnull(strstr(buf, "\"turn_count\":3"));
    ck_assert_ptr_nonnull(strstr(buf, "\"context_start_index\":2"));
    ck_assert_ptr_nonnull(strstr(buf, "\"index\":2"));
    ck_assert_ptr_nonnull(strstr(buf, "\"index\":3"));
    ck_assert_ptr_nonnull(strstr(buf, "\"index\":4"));
    ck_assert_ptr_nonnull(strstr(buf, "\"tokens\":120"));
    ck_assert_ptr_nonnull(strstr(buf, "\"tokens\":140"));

    close(client_fd);
    ik_control_socket_destroy(ctl);
    rmdir(tmpdir);
    talloc_free(ctx);
}
END_TEST

static Suite *token_cache_ctl_suite(void)
{
    Suite *s = suite_create("token_cache_ctl");

    TCase *tc_acc = tcase_create("Accessors");
    tcase_add_test(tc_acc, test_introspection_defaults);
    tcase_add_test(tc_acc, test_set_budget_and_peek_turn);
    tcase_add_test(tc_acc, test_context_start_turn_increments);
    suite_add_tcase(s, tc_acc);

    TCase *tc_disp = tcase_create("Dispatch");
    tcase_add_test(tc_disp, test_null_cache_returns_empty_json);
    tcase_add_test(tc_disp, test_populated_cache_json);
    suite_add_tcase(s, tc_disp);

    return s;
}

int32_t main(void)
{
    Suite *s = token_cache_ctl_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/token_cache_ctl_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
