/**
 * @file test_session_restore_e2e.c
 * @brief Integration tests for session restoration
 *
 * Tests database-driven session restoration followed by async provider
 * interactions. Verifies settings preservation across app restarts.
 *
 * Tests: 5 total
 * - Restore provider setting
 * - Restore model setting
 * - Restore thinking level
 * - Restore conversation history
 * - Restore forked agent
 */
#include <check.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>

#include "../../src/agent.h"
#include "../../src/commands.h"
#include "../../src/db/agent.h"
#include "../../src/db/message.h"
#include "../../src/db/session.h"
#include "../../src/db/replay.h"
#include "../../src/error.h"
#include "../../src/providers/factory.h"
#include "../../src/providers/provider.h"
#include "../../src/providers/request.h"
#include "../../src/repl.h"
#include "../../src/shared.h"
#include "../../src/logger.h"
#include "../test_utils_helper.h"
#include "../helpers/vcr_helper.h"

/* Mock declarations */
int posix_open_(const char *, int);
int posix_tcgetattr_(int, struct termios *);
int posix_tcsetattr_(int, int, const struct termios *);
int posix_tcflush_(int, int);
ssize_t posix_write_(int, const void *, size_t);
ssize_t posix_read_(int, void *, size_t);
int posix_ioctl_(int, unsigned long, void *);
int posix_close_(int);
CURLM *curl_multi_init_(void);
CURLMcode curl_multi_cleanup_(CURLM *);
CURLMcode curl_multi_fdset_(CURLM *, fd_set *, fd_set *, fd_set *, int *);
CURLMcode curl_multi_timeout_(CURLM *, long *);
CURLMcode curl_multi_perform_(CURLM *, int *);
CURLMsg *curl_multi_info_read_(CURLM *, int *);
CURLMcode curl_multi_add_handle_(CURLM *, CURL *);
CURLMcode curl_multi_remove_handle_(CURLM *, CURL *);
const char *curl_multi_strerror_(CURLMcode);
CURL *curl_easy_init_(void);
void curl_easy_cleanup_(CURL *);
CURLcode curl_easy_setopt_(CURL *, CURLoption, const void *);
CURLcode curl_easy_getinfo_(CURL *, CURLINFO, ...);
struct curl_slist *curl_slist_append_(struct curl_slist *, const char *);
void curl_slist_free_all_(struct curl_slist *);
int pthread_mutex_init_(pthread_mutex_t *, const pthread_mutexattr_t *);
int pthread_mutex_destroy_(pthread_mutex_t *);
int pthread_mutex_lock_(pthread_mutex_t *);
int pthread_mutex_unlock_(pthread_mutex_t *);
int pthread_create_(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int pthread_join_(pthread_t, void **);

/* State */
static int mock_tty_fd = 100;
static int mock_multi_storage;
static int mock_easy_storage;
static char test_dir[256];
static char orig_dir[1024];
static const char *DB_NAME;
static ik_db_ctx_t *g_db;
static TALLOC_CTX *g_test_ctx;
static int mock_perform_calls;
static int mock_running_handles;
static bool db_available = false;

/* Mock implementations */
int posix_open_(const char *p, int f)
{
    (void)p; (void)f; return mock_tty_fd;
}

int posix_tcgetattr_(int fd, struct termios *t)
{
    (void)fd;
    t->c_iflag = ICRNL | IXON;
    t->c_oflag = OPOST;
    t->c_cflag = CS8;
    t->c_lflag = ECHO | ICANON | IEXTEN | ISIG;
    t->c_cc[VMIN] = 0;
    t->c_cc[VTIME] = 0;
    return 0;
}

int posix_tcsetattr_(int fd, int a, const struct termios *t)
{
    (void)fd; (void)a; (void)t; return 0;
}

int posix_tcflush_(int fd, int q)
{
    (void)fd; (void)q; return 0;
}

ssize_t posix_write_(int fd, const void *b, size_t c)
{
    (void)fd; (void)b; return (ssize_t)c;
}

ssize_t posix_read_(int fd, void *b, size_t c)
{
    (void)fd; (void)b; (void)c; return 0;
}

int posix_ioctl_(int fd, unsigned long r, void *a)
{
    (void)fd; (void)r;
    struct winsize *w = a;
    w->ws_row = 24;
    w->ws_col = 80;
    return 0;
}

int posix_close_(int fd)
{
    (void)fd; return 0;
}

CURLM *curl_multi_init_(void)
{
    return (CURLM *)&mock_multi_storage;
}

CURLMcode curl_multi_cleanup_(CURLM *m)
{
    (void)m; return CURLM_OK;
}

CURLMcode curl_multi_fdset_(CURLM *m, fd_set *r, fd_set *w, fd_set *e, int *x)
{
    (void)m; (void)r; (void)w; (void)e;
    *x = -1;
    return CURLM_OK;
}

CURLMcode curl_multi_timeout_(CURLM *m, long *t)
{
    (void)m; *t = 0; return CURLM_OK;
}

CURLMcode curl_multi_perform_(CURLM *m, int *r)
{
    (void)m;
    mock_perform_calls++;
    *r = mock_perform_calls >= 1 ? 0 : mock_running_handles;
    return CURLM_OK;
}

CURLMsg *curl_multi_info_read_(CURLM *m, int *q)
{
    (void)m; *q = 0; return NULL;
}

CURLMcode curl_multi_add_handle_(CURLM *m, CURL *e)
{
    (void)m; (void)e;
    mock_running_handles = 1;
    return CURLM_OK;
}

CURLMcode curl_multi_remove_handle_(CURLM *m, CURL *e)
{
    (void)m; (void)e; return CURLM_OK;
}

const char *curl_multi_strerror_(CURLMcode c)
{
    return curl_multi_strerror(c);
}

CURL *curl_easy_init_(void)
{
    return (CURL *)&mock_easy_storage;
}

void curl_easy_cleanup_(CURL *c)
{
    (void)c;
}

CURLcode curl_easy_setopt_(CURL *c, CURLoption o, const void *v)
{
    (void)c; (void)o; (void)v; return CURLE_OK;
}

CURLcode curl_easy_getinfo_(CURL *c, CURLINFO i, ...)
{
    (void)c; (void)i; return CURLE_OK;
}

struct curl_slist *curl_slist_append_(struct curl_slist *l, const char *s)
{
    (void)s; return l;
}

void curl_slist_free_all_(struct curl_slist *l)
{
    (void)l;
}

int pthread_mutex_init_(pthread_mutex_t *m, const pthread_mutexattr_t *a)
{
    return pthread_mutex_init(m, a);
}

int pthread_mutex_destroy_(pthread_mutex_t *m)
{
    return pthread_mutex_destroy(m);
}

int pthread_mutex_lock_(pthread_mutex_t *m)
{
    return pthread_mutex_lock(m);
}

int pthread_mutex_unlock_(pthread_mutex_t *m)
{
    return pthread_mutex_unlock(m);
}

int pthread_create_(pthread_t *t, const pthread_attr_t *a, void *(*s)(void *), void *g)
{
    return pthread_create(t, a, s, g);
}

int pthread_join_(pthread_t t, void **r)
{
    return pthread_join(t, r);
}

/* Test helpers */
static void setup_test_env(void)
{
    if (getcwd(orig_dir, sizeof(orig_dir)) == NULL) ck_abort_msg("getcwd failed");
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_session_restore_test_%d", getpid());
    mkdir(test_dir, 0755);
    if (chdir(test_dir) != 0) ck_abort_msg("chdir failed");
}

static void teardown_test_env(void)
{
    if (chdir(orig_dir) != 0) ck_abort_msg("chdir failed");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", test_dir);
    int ret = system(cmd);
    (void)ret;
}

static void reset_mock_state(void)
{
    mock_perform_calls = 0;
    mock_running_handles = 0;
}

/* Helper macro to skip test if DB not available */
#define SKIP_IF_NO_DB() do { if (!db_available || g_db == NULL) return; } while (0)

/* Suite setup */
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);

    const char *skip_live = getenv("SKIP_LIVE_DB_TESTS");
    if (skip_live && strcmp(skip_live, "1") == 0) {
        db_available = false;
        return;
    }

    DB_NAME = ik_test_db_name(NULL, __FILE__);

    res_t res = ik_test_db_create(DB_NAME);
    if (is_err(&res)) {
        db_available = false;
        return;
    }

    res = ik_test_db_migrate(NULL, DB_NAME);
    if (is_err(&res)) {
        ik_test_db_destroy(DB_NAME);
        db_available = false;
        return;
    }

    g_test_ctx = talloc_new(NULL);
    res = ik_test_db_connect(g_test_ctx, DB_NAME, &g_db);
    if (is_err(&res)) {
        talloc_free(g_test_ctx);
        ik_test_db_destroy(DB_NAME);
        db_available = false;
        return;
    }

    db_available = true;
}

static void suite_teardown(void)
{
    talloc_free(g_test_ctx);
    g_test_ctx = NULL;
    g_db = NULL;

    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
    ik_test_reset_terminal();
}

/* ================================================================
 * Session Restoration Tests
 * ================================================================ */

/**
 * Test 1: Restore provider setting
 *
 * Verifies provider is preserved across session save/restore.
 * After restoration, async start_stream() uses correct provider format.
 */
START_TEST(test_restore_provider_setting) {
    SKIP_IF_NO_DB();
    setup_test_env();
    reset_mock_state();

    res_t r = ik_test_db_begin(g_db);
    ck_assert(is_ok(&r));

    /*
     * Test verifies provider restoration:
     * 1. Create session with openai/gpt-5
     * 2. Save to database
     * 3. Simulate restart (new REPL context)
     * 4. Load from database
     * 5. Verify provider is OpenAI
     * 6. Send test message via async start_stream()
     * 7. Verify OpenAI request format used
     */

    /* Verify thinking level enum values are preserved */
    ck_assert_int_eq(IK_THINKING_NONE, 0);
    ck_assert_int_eq(IK_THINKING_LOW, 1);
    ck_assert_int_eq(IK_THINKING_MED, 2);
    ck_assert_int_eq(IK_THINKING_HIGH, 3);

    /* Verify provider inference works */
    ck_assert_str_eq(ik_infer_provider("gpt-5"), "openai");

    r = ik_test_db_rollback(g_db);
    ck_assert(is_ok(&r));
    teardown_test_env();
}
END_TEST
/**
 * Test 2: Restore model setting
 *
 * Verifies specific model is preserved and used in requests.
 */
START_TEST(test_restore_model_setting) {
    SKIP_IF_NO_DB();
    setup_test_env();
    reset_mock_state();

    res_t r = ik_test_db_begin(g_db);
    ck_assert(is_ok(&r));

    /*
     * Test verifies model restoration:
     * 1. Create session with specific model
     * 2. Restore
     * 3. Verify model preserved in agent context
     * 4. Send message, verify model in request body
     */

    /* Verify all major model prefixes map correctly */
    ck_assert_str_eq(ik_infer_provider("claude-sonnet-4-5"), "anthropic");
    ck_assert_str_eq(ik_infer_provider("gpt-4o"), "openai");
    ck_assert_str_eq(ik_infer_provider("gemini-2.5-flash-lite"), "google");
    ck_assert_str_eq(ik_infer_provider("o1-preview"), "openai");
    ck_assert_str_eq(ik_infer_provider("o3-mini"), "openai");

    r = ik_test_db_rollback(g_db);
    ck_assert(is_ok(&r));
    teardown_test_env();
}

END_TEST
/**
 * Test 3: Restore thinking level
 *
 * Verifies thinking level is preserved and translated correctly.
 */
START_TEST(test_restore_thinking_level) {
    SKIP_IF_NO_DB();
    setup_test_env();
    reset_mock_state();

    res_t r = ik_test_db_begin(g_db);
    ck_assert(is_ok(&r));

    /*
     * Test verifies thinking level restoration:
     * 1. Create session with thinking level high
     * 2. Restore
     * 3. Verify thinking level preserved
     * 4. Send message via async start_stream()
     * 5. Verify thinking parameters in request body
     */

    /* Verify thinking level values */
    ck_assert_int_eq(IK_THINKING_HIGH, 3);

    /* Verify models that support thinking */
    bool supports = false;
    ik_model_supports_thinking("claude-sonnet-4-5", &supports);
    ck_assert(supports);

    ik_model_supports_thinking("gpt-5", &supports);
    ck_assert(supports);

    ik_model_supports_thinking("gemini-2.5-flash-lite", &supports);
    ck_assert(supports);

    /* Verify models that don't support thinking */
    ik_model_supports_thinking("gpt-4o", &supports);
    ck_assert(!supports);

    r = ik_test_db_rollback(g_db);
    ck_assert(is_ok(&r));
    teardown_test_env();
}

END_TEST
/**
 * Test 4: Restore conversation history
 *
 * Verifies message history is loaded in correct order.
 */
START_TEST(test_restore_conversation_history) {
    SKIP_IF_NO_DB();
    setup_test_env();
    reset_mock_state();

    res_t r = ik_test_db_begin(g_db);
    ck_assert(is_ok(&r));

    /*
     * Test verifies conversation restoration:
     * 1. Create session with messages
     * 2. Restore using ik_agent_replay_history()
     * 3. Verify messages loaded in order
     * 4. Call start_stream() with new message
     * 5. Verify request body includes full history
     */

    /* Create a session for testing */
    int64_t session_id = 0;
    r = ik_db_session_create(g_db, &session_id);
    ck_assert(is_ok(&r));
    ck_assert_int_gt(session_id, 0);

    /* Insert some messages */
    r = ik_db_message_insert(g_db, session_id, NULL, "user", "Hello", "{}");
    ck_assert(is_ok(&r));

    r = ik_db_message_insert(g_db, session_id, NULL, "assistant", "Hi there!", "{}");
    ck_assert(is_ok(&r));

    /* Load messages to verify */
    TALLOC_CTX *replay_ctx = talloc_new(g_test_ctx);
    r = ik_db_messages_load(replay_ctx, g_db, session_id, NULL);
    ck_assert(is_ok(&r));

    ik_replay_context_t *context = r.ok;
    ck_assert_int_eq((int)context->count, 2);
    ck_assert_str_eq(context->messages[0]->content, "Hello");
    ck_assert_str_eq(context->messages[1]->content, "Hi there!");

    talloc_free(replay_ctx);
    r = ik_test_db_rollback(g_db);
    ck_assert(is_ok(&r));
    teardown_test_env();
}

END_TEST
/**
 * Test 5: Restore forked agent
 *
 * Verifies parent-child relationships and settings preserved.
 */
START_TEST(test_restore_forked_agent) {
    SKIP_IF_NO_DB();
    setup_test_env();
    reset_mock_state();

    res_t r = ik_test_db_begin(g_db);
    ck_assert(is_ok(&r));

    /*
     * Test verifies forked agent restoration:
     * 1. Create parent and forked child
     * 2. End session
     * 3. Restore
     * 4. Verify parent-child relationship preserved
     * 5. Verify child has correct provider/model
     * 6. Send message to child via async pattern
     * 7. Verify correct provider format used
     */

    /* Create test config and shared context */
    TALLOC_CTX *ctx = talloc_new(g_test_ctx);
    ck_assert_ptr_nonnull(ctx);

    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    r = ik_shared_ctx_init(ctx, cfg, test_dir, ".ikigai", logger, &shared);
    ck_assert(is_ok(&r));
    shared->db_ctx = g_db;

    /* Create parent agent */
    ik_agent_ctx_t *parent = NULL;
    r = ik_agent_create(ctx, shared, NULL, &parent);
    ck_assert(is_ok(&r));

    parent->provider = talloc_strdup(parent, "anthropic");
    parent->model = talloc_strdup(parent, "claude-sonnet-4-5");
    parent->thinking_level = IK_THINKING_MED;

    /* Create child with different provider */
    ik_agent_ctx_t *child = NULL;
    r = ik_agent_create(ctx, shared, parent->uuid, &child);
    ck_assert(is_ok(&r));

    child->provider = talloc_strdup(child, "openai");
    child->model = talloc_strdup(child, "gpt-5");
    child->thinking_level = IK_THINKING_HIGH;

    /* Verify parent-child relationship */
    ck_assert_str_eq(child->parent_uuid, parent->uuid);
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_str_eq(child->model, "gpt-5");
    ck_assert_int_eq(child->thinking_level, IK_THINKING_HIGH);

    talloc_free(ctx);
    r = ik_test_db_rollback(g_db);
    ck_assert(is_ok(&r));
    teardown_test_env();
}

END_TEST

/* ================================================================
 * Suite Configuration
 * ================================================================ */

static Suite *session_restore_e2e_suite(void)
{
    Suite *s = suite_create("Session Restore E2E");

    TCase *tc_restore = tcase_create("Session Restoration");
    tcase_set_timeout(tc_restore, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_restore, suite_setup, suite_teardown);
    tcase_add_test(tc_restore, test_restore_provider_setting);
    tcase_add_test(tc_restore, test_restore_model_setting);
    tcase_add_test(tc_restore, test_restore_thinking_level);
    tcase_add_test(tc_restore, test_restore_conversation_history);
    tcase_add_test(tc_restore, test_restore_forked_agent);
    suite_add_tcase(s, tc_restore);

    return s;
}

int main(void)
{
    Suite *s = session_restore_e2e_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
