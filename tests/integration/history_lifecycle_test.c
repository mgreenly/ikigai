#include "../../src/agent.h"
#include "../../src/history.h"
#include "../../src/repl.h"
#include "../../src/shared.h"
#include "../test_utils.h"

#include <check.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>

int posix_open_(const char*,int);int posix_tcgetattr_(int,struct termios*);int posix_tcsetattr_(int,int,const struct termios*);int posix_tcflush_(int,int);ssize_t posix_write_(int,const void*,size_t);ssize_t posix_read_(int,void*,size_t);int posix_ioctl_(int,unsigned long,void*);int posix_close_(int);CURLM*curl_multi_init_(void);CURLMcode curl_multi_cleanup_(CURLM*);CURLMcode curl_multi_fdset_(CURLM*,fd_set*,fd_set*,fd_set*,int*);CURLMcode curl_multi_timeout_(CURLM*,long*);CURLMcode curl_multi_perform_(CURLM*,int*);CURLMsg*curl_multi_info_read_(CURLM*,int*);CURLMcode curl_multi_add_handle_(CURLM*,CURL*);CURLMcode curl_multi_remove_handle_(CURLM*,CURL*);const char*curl_multi_strerror_(CURLMcode);CURL*curl_easy_init_(void);void curl_easy_cleanup_(CURL*);CURLcode curl_easy_setopt_(CURL*,CURLoption,const void*);struct curl_slist*curl_slist_append_(struct curl_slist*,const char*);void curl_slist_free_all_(struct curl_slist*);int pthread_mutex_init_(pthread_mutex_t*,const pthread_mutexattr_t*);int pthread_mutex_destroy_(pthread_mutex_t*);int pthread_mutex_lock_(pthread_mutex_t*);int pthread_mutex_unlock_(pthread_mutex_t*);int pthread_create_(pthread_t*,const pthread_attr_t*,void*(*)(void*),void*);int pthread_join_(pthread_t,void**);
static int mock_tty_fd=100,mock_multi_storage,mock_easy_storage;
static char test_dir[256];
static char orig_dir[1024];
int posix_open_(const char *p,int f){(void)p;(void)f;return mock_tty_fd;}
int posix_tcgetattr_(int fd,struct termios *t){(void)fd;t->c_iflag=ICRNL|IXON;t->c_oflag=OPOST;t->c_cflag=CS8;t->c_lflag=ECHO|ICANON|IEXTEN|ISIG;t->c_cc[VMIN]=0;t->c_cc[VTIME]=0;return 0;}
int posix_tcsetattr_(int fd,int a,const struct termios *t){(void)fd;(void)a;(void)t;return 0;}
int posix_tcflush_(int fd,int q){(void)fd;(void)q;return 0;}
ssize_t posix_write_(int fd,const void *b,size_t c){(void)fd;(void)b;return(ssize_t)c;}
ssize_t posix_read_(int fd,void *b,size_t c){(void)fd;(void)b;(void)c;return 0;}
int posix_ioctl_(int fd,unsigned long r,void *a){(void)fd;(void)r;struct winsize *w=(struct winsize*)a;w->ws_row=24;w->ws_col=80;return 0;}
int posix_close_(int fd){(void)fd;return 0;}
CURLM *curl_multi_init_(void){return(CURLM*)&mock_multi_storage;}
CURLMcode curl_multi_cleanup_(CURLM *m){(void)m;return CURLM_OK;}
CURLMcode curl_multi_fdset_(CURLM *m,fd_set *r,fd_set *w,fd_set *e,int *x){(void)m;(void)r;(void)w;(void)e;*x=-1;return CURLM_OK;}
CURLMcode curl_multi_timeout_(CURLM *m,long *t){(void)m;*t=-1;return CURLM_OK;}
CURLMcode curl_multi_perform_(CURLM *m,int *r){(void)m;*r=0;return CURLM_OK;}
CURLMsg *curl_multi_info_read_(CURLM *m,int *q){(void)m;*q=0;return NULL;}
CURLMcode curl_multi_add_handle_(CURLM *m,CURL *e){(void)m;(void)e;return CURLM_OK;}
CURLMcode curl_multi_remove_handle_(CURLM *m,CURL *e){(void)m;(void)e;return CURLM_OK;}
const char *curl_multi_strerror_(CURLMcode c){return curl_multi_strerror(c);}
CURL *curl_easy_init_(void){return(CURL*)&mock_easy_storage;}
void curl_easy_cleanup_(CURL *c){(void)c;}
CURLcode curl_easy_setopt_(CURL *c,CURLoption o,const void *v){(void)c;(void)o;(void)v;return CURLE_OK;}
struct curl_slist *curl_slist_append_(struct curl_slist *l,const char *s){(void)s;return l;}
void curl_slist_free_all_(struct curl_slist *l){(void)l;}
int pthread_mutex_init_(pthread_mutex_t *m,const pthread_mutexattr_t *a){return pthread_mutex_init(m,a);}
int pthread_mutex_destroy_(pthread_mutex_t *m){return pthread_mutex_destroy(m);}
int pthread_mutex_lock_(pthread_mutex_t *m){return pthread_mutex_lock(m);}
int pthread_mutex_unlock_(pthread_mutex_t *m){return pthread_mutex_unlock(m);}
int pthread_create_(pthread_t *t,const pthread_attr_t *a,void*(*s)(void*),void *g){return pthread_create(t,a,s,g);}
int pthread_join_(pthread_t t,void **r){return pthread_join(t,r);}

static void setup_test_env(void)
{
    if (getcwd(orig_dir, sizeof(orig_dir)) == NULL) ck_abort_msg("getcwd failed");
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_test_%d", getpid());
    mkdir(test_dir, 0755);
    if (chdir(test_dir) != 0) ck_abort_msg("chdir failed");
}
static void teardown_test_env(void)
{
    if (chdir(orig_dir) != 0) ck_abort_msg("chdir failed");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", test_dir);
    system(cmd);
}
static void cleanup_test_dir(void)
{
    unlink(".ikigai/history");
    rmdir(".ikigai");
}
START_TEST(test_history_loads_on_init)
{
    setup_test_env();
    cleanup_test_dir();
    int mr = mkdir(".ikigai", 0755);
    ck_assert(mr == 0 || (mr == -1 && errno == EEXIST));
    int fd = open(".ikigai/history", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ck_assert(fd >= 0);
    const char *l1 = "{\"cmd\": \"test command 1\", \"ts\": \"2025-01-15T10:30:00Z\"}\n";
    const char *l2 = "{\"cmd\": \"test command 2\", \"ts\": \"2025-01-15T10:31:00Z\"}\n";
    ck_assert(write(fd, l1, strlen(l1)) == (ssize_t)strlen(l1));
    ck_assert(write(fd, l2, strlen(l2)) == (ssize_t)strlen(l2));
    fsync(fd); close(fd);
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;
    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    res_t result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(repl);
    ck_assert_uint_eq(repl->shared->history->count, 2);
    ck_assert_str_eq(repl->shared->history->entries[0], "test command 1");
    ck_assert_str_eq(repl->shared->history->entries[1], "test command 2");
    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
    teardown_test_env();
}
END_TEST

START_TEST(test_history_saves_on_submit)
{
    setup_test_env();
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;
    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    res_t result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));
    const char *test_cmd = "my test command";
    result = ik_input_buffer_set_text(repl->current->input_buffer, test_cmd, strlen(test_cmd));
    ck_assert(is_ok(&result));
    result = ik_repl_submit_line(repl);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(repl->shared->history->count, 1);
    ck_assert_str_eq(repl->shared->history->entries[0], "my test command");
    FILE *f = fopen(".ikigai/history", "r");
    ck_assert_ptr_nonnull(f);
    char line[256];
    ck_assert_ptr_nonnull(fgets(line, sizeof(line), f));
    ck_assert(strstr(line, "my test command") != NULL);
    fclose(f);
    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
    teardown_test_env();
}
END_TEST

START_TEST(test_history_survives_repl_restart)
{
    setup_test_env();
    cleanup_test_dir();

    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl1 = NULL;
    ik_shared_ctx_t *shared1 = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared1); ck_assert(is_ok(&r));
    res_t result = ik_repl_init(ctx, shared1, &repl1);
    ck_assert(is_ok(&result));
    const char *test_cmd = "persistent command";
    result = ik_input_buffer_set_text(repl1->current->input_buffer, test_cmd, strlen(test_cmd));
    ck_assert(is_ok(&result));
    result = ik_repl_submit_line(repl1);
    ck_assert(is_ok(&result));

    ik_repl_cleanup(repl1);
    ik_repl_ctx_t *repl2 = NULL;
    ik_shared_ctx_t *shared2 = NULL;
    // Create logger before calling init
    ik_logger_t *logger2 = ik_logger_create(ctx, "/tmp");
    r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger2, &shared2); ck_assert(is_ok(&r));
    result = ik_repl_init(ctx, shared2, &repl2);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(shared2->history->count, 1);
    ck_assert_str_eq(shared2->history->entries[0], "persistent command");

    ik_repl_cleanup(repl2);
    talloc_free(ctx);
    cleanup_test_dir();
    teardown_test_env();
}
END_TEST

START_TEST(test_history_respects_config_capacity)
{
    setup_test_env();
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 3;
    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    res_t result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(repl->shared->history->capacity, 3);
    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
    teardown_test_env();
}
END_TEST

START_TEST(test_history_empty_input_not_saved)
{
    setup_test_env();
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;
    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    res_t result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));
    result = ik_repl_submit_line(repl);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(repl->shared->history->count, 0);
    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
    teardown_test_env();
}
END_TEST

// Test: Multiline input preserved
START_TEST(test_history_multiline_preserved)
{
    setup_test_env();
    cleanup_test_dir();

    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));

    // Create REPL context
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));

    // Submit multiline command
    const char *multiline = "line 1\nline 2\nline 3";
    result = ik_input_buffer_set_text(repl->current->input_buffer, multiline, strlen(multiline));
    ck_assert(is_ok(&result));
    result = ik_repl_submit_line(repl);
    ck_assert(is_ok(&result));

    // Verify multiline was preserved
    ck_assert_uint_eq(repl->shared->history->count, 1);
    ck_assert_str_eq(repl->shared->history->entries[0], "line 1\nline 2\nline 3");

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
    teardown_test_env();
}
END_TEST

// Test: Corrupt history file doesn't crash REPL
START_TEST(test_history_file_corrupt_continues)
{
    setup_test_env();
    cleanup_test_dir();

    // Create corrupt history file
    int mkdir_result = mkdir(".ikigai", 0755);
    ck_assert(mkdir_result == 0 || (mkdir_result == -1 && errno == EEXIST));
    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "not valid json\n");
    fprintf(f, "{\"cmd\": \"valid command\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fprintf(f, "another bad line\n");
    fclose(f);

    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    // REPL should still initialize successfully
    ik_repl_ctx_t *repl = NULL;
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));

    // Create REPL context
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(repl);

    // Should have loaded the valid line
    ck_assert_uint_eq(repl->shared->history->count, 1);
    ck_assert_str_eq(repl->shared->history->entries[0], "valid command");

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
    teardown_test_env();
}
END_TEST

// Test: Submitting while browsing stops browsing
START_TEST(test_history_submit_stops_browsing)
{
    setup_test_env();
    cleanup_test_dir();

    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));

    // Create REPL context
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));

    // Add first command
    result = ik_input_buffer_set_text(repl->current->input_buffer, "command 1", 9);
    ck_assert(is_ok(&result));
    result = ik_repl_submit_line(repl);
    ck_assert(is_ok(&result));

    // Start browsing
    ik_history_start_browsing(repl->shared->history, "");
    ck_assert(ik_history_is_browsing(repl->shared->history));

    // Submit new command
    result = ik_input_buffer_set_text(repl->current->input_buffer, "command 2", 9);
    ck_assert(is_ok(&result));
    result = ik_repl_submit_line(repl);
    ck_assert(is_ok(&result));

    // Should no longer be browsing
    ck_assert(!ik_history_is_browsing(repl->shared->history));

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
    teardown_test_env();
}
END_TEST

// Test: Failed file write doesn't break REPL
START_TEST(test_history_file_write_failure)
{
    setup_test_env();
    cleanup_test_dir();

    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));

    // Create REPL context
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));

    // Make .ikigai directory read-only to cause write failure
    int mkdir_result = mkdir(".ikigai", 0755);
    ck_assert(mkdir_result == 0 || (mkdir_result == -1 && errno == EEXIST));
    chmod(".ikigai", 0555);

    // Submit command - should succeed despite file write failure
    result = ik_input_buffer_set_text(repl->current->input_buffer, "test command", 12);
    ck_assert(is_ok(&result));
    result = ik_repl_submit_line(repl);
    ck_assert(is_ok(&result));

    // History should still be updated in memory
    ck_assert_uint_eq(repl->shared->history->count, 1);
    ck_assert_str_eq(repl->shared->history->entries[0], "test command");

    // Restore permissions
    chmod(".ikigai", 0755);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
    teardown_test_env();
}
END_TEST

static Suite *history_lifecycle_suite(void)
{
    Suite *s = suite_create("History Lifecycle");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_history_loads_on_init);
    tcase_add_test(tc_core, test_history_saves_on_submit);
    tcase_add_test(tc_core, test_history_survives_repl_restart);
    tcase_add_test(tc_core, test_history_respects_config_capacity);
    tcase_add_test(tc_core, test_history_empty_input_not_saved);
    tcase_add_test(tc_core, test_history_multiline_preserved);
    tcase_add_test(tc_core, test_history_file_corrupt_continues);
    tcase_add_test(tc_core, test_history_submit_stops_browsing);
    tcase_add_test(tc_core, test_history_file_write_failure);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = history_lifecycle_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
