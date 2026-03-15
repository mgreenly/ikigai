/**
 * @file internal_tools_bg_test.c
 * @brief Unit tests for background process internal tool handlers
 */

#include "tests/test_constants.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/bg_line_index.h"
#include "apps/ikigai/bg_process.h"
#include "apps/ikigai/bg_process_io.h"
#include "apps/ikigai/internal_tools_bg.h"
#include "shared/error.h"
#include "vendor/yyjson/yyjson.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/* Mock state */
static bool mock_pstart_fail = false;
static bool mock_read_fail   = false;
static bool mock_write_fail  = false;
static bool mock_kill_fail   = false;
static const char *mock_read_data = "line 1\nline 2\nline 3\n";

static void reset_mocks(void)
{
    mock_pstart_fail = false;
    mock_read_fail   = false;
    mock_write_fail  = false;
    mock_kill_fail   = false;
    mock_read_data   = "line 1\nline 2\nline 3\n";
}

/* Mock implementations */
res_t bg_process_start(bg_manager_t *mgr, ik_db_ctx_t *db,
                        const char *output_base_dir,
                        const char *command, const char *label,
                        const char *agent_uuid, int32_t ttl_seconds,
                        bg_process_t **out_proc)
{
    (void)db; (void)output_base_dir;
    if (mock_pstart_fail) return ERR(mgr, IO, "mock start failure");
    bg_process_t *p = talloc_zero(mgr, bg_process_t);
    p->id = 10; p->pid = 99999; p->status = BG_STATUS_RUNNING;
    p->line_index = bg_line_index_create(p);
    p->command = talloc_strdup(p, command); p->label = talloc_strdup(p, label);
    p->agent_uuid = talloc_strdup(p, agent_uuid); p->ttl_seconds = ttl_seconds;
    p->stdin_open = true; p->master_fd = -1; p->output_fd = -1; p->pidfd = -1;
    if (mgr->count < mgr->capacity) mgr->processes[mgr->count++] = p;
    *out_proc = p;
    return OK(NULL);
}

res_t bg_process_read_output(bg_process_t *proc, TALLOC_CTX *ctx,
                             bg_read_mode_t mode, int64_t tail_lines,
                             int64_t start_line, int64_t end_line,
                             uint8_t **out_buf, size_t *out_len)
{
    (void)mode; (void)tail_lines; (void)start_line; (void)end_line;
    if (mock_read_fail) { *out_buf = NULL; *out_len = 0; return ERR(proc, IO, "mock"); }
    if (mock_read_data == NULL) { *out_buf = NULL; *out_len = 0; return OK(NULL); }
    size_t len = strlen(mock_read_data);
    *out_buf = (uint8_t *)talloc_memdup(ctx, mock_read_data, len + 1);
    *out_len = len;
    return OK(NULL);
}

res_t bg_process_write_stdin(bg_process_t *proc, const char *data,
                              size_t len, bool append_newline)
{
    (void)data; (void)len; (void)append_newline;
    if (mock_write_fail) return ERR(proc, IO, "mock write failure");
    return OK(NULL);
}

res_t bg_process_close_stdin(bg_process_t *proc)
{
    proc->stdin_open = false;
    return OK(NULL);
}

res_t bg_process_kill(bg_process_t *proc)
{
    if (mock_kill_fail) return ERR(proc, INVALID_ARG, "mock kill failure");
    proc->status = BG_STATUS_KILLED;
    return OK(NULL);
}

/* Test fixture */
static TALLOC_CTX    *test_ctx;
static ik_agent_ctx_t *agent;
static bg_manager_t  *mgr;
static bg_process_t  *proc1;

static void setup(void)
{
    reset_mocks();
    test_ctx    = talloc_new(NULL);
    agent       = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->uuid = talloc_strdup(agent, "test-agent-uuid");
    mgr             = talloc_zero(test_ctx, bg_manager_t);
    mgr->processes  = talloc_array(mgr, bg_process_t *, 8);
    mgr->capacity = 8; mgr->count = 0; mgr->next_id = 2;
    proc1              = talloc_zero(mgr, bg_process_t);
    proc1->id          = 1; proc1->pid = 12345;
    proc1->status      = BG_STATUS_RUNNING;
    proc1->line_index  = bg_line_index_create(proc1);
    proc1->total_bytes = 1000;
    proc1->command     = talloc_strdup(proc1, "echo hello");
    proc1->label       = talloc_strdup(proc1, "test process");
    proc1->ttl_seconds = 300; proc1->stdin_open = true;
    proc1->master_fd = -1; proc1->output_fd = -1; proc1->pidfd = -1;
    mgr->processes[0] = proc1; mgr->count = 1;
    agent->bg_manager = mgr;
}

static void teardown(void) { talloc_free(test_ctx); reset_mocks(); }

static bool result_success(const char *json)
{
    yyjson_doc *doc  = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *s    = yyjson_obj_get(root, "tool_success");
    bool v = s && yyjson_get_bool(s);
    yyjson_doc_free(doc);
    return v;
}

/* ================================================================
 * pstart tests
 * ================================================================ */

START_TEST(test_pstart_success) {
    const char *args = "{\"command\":\"make test\",\"label\":\"tests\",\"ttl_seconds\":600}";
    char *r = ik_bg_pstart_handler(test_ctx, agent, args);
    ck_assert(result_success(r));
    yyjson_doc *doc = yyjson_read(r, strlen(r), 0);
    yyjson_val *res = yyjson_obj_get(yyjson_doc_get_root(doc), "result");
    ck_assert_int_eq(yyjson_get_int(yyjson_obj_get(res, "id")), 10);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(res, "status")), "running");
    yyjson_doc_free(doc);
} END_TEST

START_TEST(test_pstart_no_bg_manager) {
    agent->bg_manager = NULL;
    char *r = ik_bg_pstart_handler(test_ctx, agent,
        "{\"command\":\"x\",\"label\":\"y\",\"ttl_seconds\":60}");
    ck_assert(!result_success(r));
} END_TEST

START_TEST(test_pstart_missing_command) {
    char *r = ik_bg_pstart_handler(test_ctx, agent, "{\"label\":\"y\",\"ttl_seconds\":60}");
    ck_assert(!result_success(r));
} END_TEST

START_TEST(test_pstart_missing_label) {
    char *r = ik_bg_pstart_handler(test_ctx, agent, "{\"command\":\"x\",\"ttl_seconds\":60}");
    ck_assert(!result_success(r));
} END_TEST

START_TEST(test_pstart_missing_ttl) {
    char *r = ik_bg_pstart_handler(test_ctx, agent, "{\"command\":\"x\",\"label\":\"y\"}");
    ck_assert(!result_success(r));
} END_TEST

START_TEST(test_pstart_start_failed) {
    mock_pstart_fail = true;
    char *r = ik_bg_pstart_handler(test_ctx, agent,
        "{\"command\":\"x\",\"label\":\"y\",\"ttl_seconds\":60}");
    ck_assert(!result_success(r));
} END_TEST

/* ================================================================
 * pinspect tests
 * ================================================================ */

START_TEST(test_pinspect_success) {
    char *r = ik_bg_pinspect_handler(test_ctx, agent, "{\"id\":1}");
    ck_assert(result_success(r));
    yyjson_doc *doc = yyjson_read(r, strlen(r), 0);
    yyjson_val *res = yyjson_obj_get(yyjson_doc_get_root(doc), "result");
    ck_assert_int_eq(yyjson_get_int(yyjson_obj_get(res, "id")), 1);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(res, "status")), "running");
    ck_assert_ptr_nonnull(yyjson_obj_get(res, "output"));
    yyjson_doc_free(doc);
} END_TEST

START_TEST(test_pinspect_not_found) {
    char *r = ik_bg_pinspect_handler(test_ctx, agent, "{\"id\":999}");
    ck_assert(!result_success(r));
    ck_assert(strstr(r, "NOT_FOUND") != NULL);
} END_TEST

START_TEST(test_pinspect_missing_id) {
    char *r = ik_bg_pinspect_handler(test_ctx, agent, "{\"mode\":\"tail\"}");
    ck_assert(!result_success(r));
} END_TEST

START_TEST(test_pinspect_ansi_stripped) {
    /* ANSI color codes — real bg_ansi_strip removes them */
    mock_read_data = "\x1b[31mred text\x1b[0m\nplain text\n";
    char *r = ik_bg_pinspect_handler(test_ctx, agent, "{\"id\":1}");
    ck_assert(result_success(r));
    ck_assert(strstr(r, "\x1b[31m") == NULL);
    ck_assert(strstr(r, "red text") != NULL);
    ck_assert(strstr(r, "plain text") != NULL);
} END_TEST

START_TEST(test_pinspect_output_cap) {
    /* Build a string larger than 50KB */
    unsigned big = 52 * 1024;
    char *big_data = talloc_array(test_ctx, char, big + 2);
    memset(big_data, 'x', big);
    big_data[big] = '\n'; big_data[big + 1] = '\0';
    mock_read_data = big_data;
    char *r = ik_bg_pinspect_handler(test_ctx, agent, "{\"id\":1}");
    ck_assert(result_success(r));
    yyjson_doc *doc = yyjson_read(r, strlen(r), 0);
    yyjson_val *res = yyjson_obj_get(yyjson_doc_get_root(doc), "result");
    ck_assert(strlen(yyjson_get_str(yyjson_obj_get(res, "output"))) <= 50 * 1024);
    yyjson_doc_free(doc);
} END_TEST

START_TEST(test_pinspect_read_failed) {
    mock_read_fail = true;
    char *r = ik_bg_pinspect_handler(test_ctx, agent, "{\"id\":1}");
    ck_assert(!result_success(r));
    ck_assert(strstr(r, "READ_FAILED") != NULL);
} END_TEST

/* ================================================================
 * pwrite tests
 * ================================================================ */

START_TEST(test_pwrite_success) {
    char *r = ik_bg_pwrite_handler(test_ctx, agent, "{\"id\":1,\"input\":\"hello world\"}");
    ck_assert(result_success(r));
    ck_assert(strstr(r, "\"stdin_closed\":false") != NULL);
} END_TEST

START_TEST(test_pwrite_not_found) {
    char *r = ik_bg_pwrite_handler(test_ctx, agent, "{\"id\":999,\"input\":\"x\"}");
    ck_assert(!result_success(r));
    ck_assert(strstr(r, "NOT_FOUND") != NULL);
} END_TEST

START_TEST(test_pwrite_with_close_stdin) {
    char *r = ik_bg_pwrite_handler(test_ctx, agent,
        "{\"id\":1,\"input\":\"exit\",\"close_stdin\":true}");
    ck_assert(result_success(r));
    ck_assert(!proc1->stdin_open);
    ck_assert(strstr(r, "\"stdin_closed\":true") != NULL);
} END_TEST

START_TEST(test_pwrite_write_failed) {
    mock_write_fail = true;
    char *r = ik_bg_pwrite_handler(test_ctx, agent, "{\"id\":1,\"input\":\"x\"}");
    ck_assert(!result_success(r));
    ck_assert(strstr(r, "WRITE_FAILED") != NULL);
} END_TEST

/* ================================================================
 * pkill tests
 * ================================================================ */

START_TEST(test_pkill_success) {
    char *r = ik_bg_pkill_handler(test_ctx, agent, "{\"id\":1}");
    ck_assert(result_success(r));
    ck_assert_int_eq((int)proc1->status, (int)BG_STATUS_KILLED);
    yyjson_doc *doc = yyjson_read(r, strlen(r), 0);
    yyjson_val *res = yyjson_obj_get(yyjson_doc_get_root(doc), "result");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(res, "status")), "killed");
    yyjson_doc_free(doc);
} END_TEST

START_TEST(test_pkill_not_found) {
    char *r = ik_bg_pkill_handler(test_ctx, agent, "{\"id\":999}");
    ck_assert(!result_success(r));
    ck_assert(strstr(r, "NOT_FOUND") != NULL);
} END_TEST

START_TEST(test_pkill_missing_id) {
    char *r = ik_bg_pkill_handler(test_ctx, agent, "{}");
    ck_assert(!result_success(r));
} END_TEST

START_TEST(test_pkill_kill_failed) {
    mock_kill_fail = true;
    char *r = ik_bg_pkill_handler(test_ctx, agent, "{\"id\":1}");
    ck_assert(!result_success(r));
    ck_assert(strstr(r, "KILL_FAILED") != NULL);
} END_TEST

/* ================================================================
 * ps tests
 * ================================================================ */

START_TEST(test_ps_success) {
    char *r = ik_bg_ps_handler(test_ctx, agent, "{}");
    ck_assert(result_success(r));
    yyjson_doc *doc = yyjson_read(r, strlen(r), 0);
    yyjson_val *res = yyjson_obj_get(yyjson_doc_get_root(doc), "result");
    ck_assert(yyjson_is_arr(res));
    ck_assert_int_eq((int)(unsigned)yyjson_arr_size(res), 1);
    yyjson_val *entry = yyjson_arr_get_first(res);
    ck_assert_int_eq(yyjson_get_int(yyjson_obj_get(entry, "id")), 1);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(entry, "status")), "running");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(entry, "command")), "echo hello");
    yyjson_doc_free(doc);
} END_TEST

START_TEST(test_ps_empty) {
    mgr->count = 0;
    char *r = ik_bg_ps_handler(test_ctx, agent, "{}");
    ck_assert(result_success(r));
    yyjson_doc *doc = yyjson_read(r, strlen(r), 0);
    yyjson_val *res = yyjson_obj_get(yyjson_doc_get_root(doc), "result");
    ck_assert_int_eq((int)(unsigned)yyjson_arr_size(res), 0);
    yyjson_doc_free(doc);
} END_TEST

START_TEST(test_ps_no_bg_manager) {
    agent->bg_manager = NULL;
    char *r = ik_bg_ps_handler(test_ctx, agent, "{}");
    ck_assert(!result_success(r));
} END_TEST

/* Suite definition */
static Suite *internal_tools_bg_suite(void)
{
    Suite *s = suite_create("InternalToolsBg");

    TCase *tc = tcase_create("pstart");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_pstart_success);
    tcase_add_test(tc, test_pstart_no_bg_manager);
    tcase_add_test(tc, test_pstart_missing_command);
    tcase_add_test(tc, test_pstart_missing_label);
    tcase_add_test(tc, test_pstart_missing_ttl);
    tcase_add_test(tc, test_pstart_start_failed);
    suite_add_tcase(s, tc);

    tc = tcase_create("pinspect");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_pinspect_success);
    tcase_add_test(tc, test_pinspect_not_found);
    tcase_add_test(tc, test_pinspect_missing_id);
    tcase_add_test(tc, test_pinspect_ansi_stripped);
    tcase_add_test(tc, test_pinspect_output_cap);
    tcase_add_test(tc, test_pinspect_read_failed);
    suite_add_tcase(s, tc);

    tc = tcase_create("pwrite");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_pwrite_success);
    tcase_add_test(tc, test_pwrite_not_found);
    tcase_add_test(tc, test_pwrite_with_close_stdin);
    tcase_add_test(tc, test_pwrite_write_failed);
    suite_add_tcase(s, tc);

    tc = tcase_create("pkill");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_pkill_success);
    tcase_add_test(tc, test_pkill_not_found);
    tcase_add_test(tc, test_pkill_missing_id);
    tcase_add_test(tc, test_pkill_kill_failed);
    suite_add_tcase(s, tc);

    tc = tcase_create("ps");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_ps_success);
    tcase_add_test(tc, test_ps_empty);
    tcase_add_test(tc, test_ps_no_bg_manager);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = internal_tools_bg_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/tool/internal_tools_bg_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
