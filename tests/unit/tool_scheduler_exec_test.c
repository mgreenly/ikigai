// Unit tests for tool_scheduler_exec.c: cascade, promote, all_terminal
#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/tool.h"
#include "apps/ikigai/tool_scheduler.h"
#include "shared/error.h"

// ---------------------------------------------------------------------------
// Mocks: prevent real thread spawning
// ---------------------------------------------------------------------------

int pthread_create_(pthread_t *t, const pthread_attr_t *a,
                    void *(*s)(void *), void *g);
int pthread_join_(pthread_t t, void **r);

int pthread_create_(pthread_t *t, const pthread_attr_t *a,
                    void *(*s)(void *), void *g)
{
    (void)a; (void)s; (void)g;
    memset(t, 0, sizeof(*t));
    return 0;
}

int pthread_join_(pthread_t t, void **r)
{
    (void)t; (void)r;
    return 0;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ik_tool_scheduler_t *make_sched(TALLOC_CTX *ctx)
{
    ik_agent_ctx_t *ag = talloc_zero(ctx, ik_agent_ctx_t);
    return ik_tool_scheduler_create(ctx, ag);
}

static int32_t add_call(ik_tool_scheduler_t *sched, const char *name,
                        const char *args)
{
    ik_tool_call_t *tc = ik_tool_call_create(sched, "id", name, args);
    ik_tool_scheduler_add(sched, tc);
    return sched->count - 1;
}

// ---------------------------------------------------------------------------
// Tests: all_terminal
// ---------------------------------------------------------------------------

START_TEST(test_all_terminal_empty_no_stream)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    s->stream_complete = false;
    ck_assert(!ik_tool_scheduler_all_terminal(s));
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_all_terminal_empty_with_stream)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    s->stream_complete = true;
    ck_assert(!ik_tool_scheduler_all_terminal(s));
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_all_terminal_completed_no_stream)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    int32_t i = add_call(s, "web_fetch", "{}");
    ik_tool_scheduler_on_complete(s, i, talloc_strdup(s, "{}"));
    s->stream_complete = false;
    ck_assert(!ik_tool_scheduler_all_terminal(s));
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_all_terminal_completed_with_stream)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    int32_t i = add_call(s, "web_fetch", "{}");
    ik_tool_scheduler_on_complete(s, i, talloc_strdup(s, "{}"));
    s->stream_complete = true;
    ck_assert(ik_tool_scheduler_all_terminal(s));
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_all_terminal_one_running)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    add_call(s, "web_fetch", "{}");
    s->stream_complete = true;
    ck_assert(!ik_tool_scheduler_all_terminal(s));
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_all_terminal_skipped_counts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    int32_t i0 = add_call(s, "file_edit", "{\"file_path\":\"/f\"}");
    add_call(s, "file_read", "{\"file_path\":\"/f\"}");
    ik_tool_scheduler_on_error(s, i0, "fail");
    s->stream_complete = true;
    ck_assert(ik_tool_scheduler_all_terminal(s));
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

// ---------------------------------------------------------------------------
// Tests: error cascading
// ---------------------------------------------------------------------------

START_TEST(test_cascade_direct)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    int32_t i0 = add_call(s, "file_edit", "{\"file_path\":\"/x\"}");
    int32_t i1 = add_call(s, "file_read", "{\"file_path\":\"/x\"}");
    ck_assert_int_eq(s->entries[i1].status, IK_SCHEDULE_QUEUED);
    ik_tool_scheduler_on_error(s, i0, "broke");
    ck_assert_int_eq(s->entries[i0].status, IK_SCHEDULE_ERRORED);
    ck_assert_int_eq(s->entries[i1].status, IK_SCHEDULE_SKIPPED);
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_cascade_transitive)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    int32_t i0 = add_call(s, "file_edit", "{\"file_path\":\"/a\"}");
    add_call(s, "file_read",  "{\"file_path\":\"/a\"}");
    add_call(s, "file_edit",  "{\"file_path\":\"/a\"}");
    ik_tool_scheduler_on_error(s, i0, "broke");
    ck_assert_int_eq(s->entries[1].status, IK_SCHEDULE_SKIPPED);
    ck_assert_int_eq(s->entries[2].status, IK_SCHEDULE_SKIPPED);
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_cascade_unrelated_not_skipped)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    int32_t i0 = add_call(s, "file_edit", "{\"file_path\":\"/x\"}");
    int32_t i1 = add_call(s, "web_fetch", "{}");
    ik_tool_scheduler_on_error(s, i0, "broke");
    ck_assert_int_eq(s->entries[i1].status, IK_SCHEDULE_RUNNING);
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

// ---------------------------------------------------------------------------
// Tests: promotion logic
// ---------------------------------------------------------------------------

START_TEST(test_promote_blocker_completes_starts_dependent)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    int32_t i0 = add_call(s, "file_edit", "{\"file_path\":\"/f\"}");
    int32_t i1 = add_call(s, "file_read", "{\"file_path\":\"/f\"}");
    ik_tool_scheduler_begin(s);
    ck_assert_int_eq(s->entries[i0].status, IK_SCHEDULE_RUNNING);
    ck_assert_int_eq(s->entries[i1].status, IK_SCHEDULE_QUEUED);
    ik_tool_scheduler_on_complete(s, i0, talloc_strdup(s, "{}"));
    ck_assert_int_eq(s->entries[i0].status, IK_SCHEDULE_COMPLETED);
    ck_assert_int_eq(s->entries[i1].status, IK_SCHEDULE_RUNNING);
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_promote_skip_when_blocker_errored)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    int32_t i0 = add_call(s, "file_edit", "{\"file_path\":\"/f\"}");
    int32_t i1 = add_call(s, "file_read", "{\"file_path\":\"/f\"}");
    ik_tool_scheduler_on_error(s, i0, "fail");
    ck_assert_int_eq(s->entries[i1].status, IK_SCHEDULE_SKIPPED);
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_promote_concurrent_reads_all_running)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    add_call(s, "file_read", "{\"file_path\":\"/a\"}");
    add_call(s, "file_read", "{\"file_path\":\"/b\"}");
    add_call(s, "file_read", "{\"file_path\":\"/c\"}");
    ik_tool_scheduler_begin(s);
    for (int32_t i = 0; i < s->count; i++)
        ck_assert_int_eq(s->entries[i].status, IK_SCHEDULE_RUNNING);
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_promote_bash_blocks_file_read)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    add_call(s, "bash", "{\"command\":\"ls\"}");
    int32_t i1 = add_call(s, "file_read", "{\"file_path\":\"/x\"}");
    ck_assert_int_eq(s->entries[i1].status, IK_SCHEDULE_QUEUED);
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_promote_web_fetch_concurrent_with_writes)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_tool_scheduler_t *s = make_sched(ctx);
    add_call(s, "file_edit", "{\"file_path\":\"/x\"}");
    int32_t i1 = add_call(s, "web_fetch", "{}");
    ik_tool_scheduler_begin(s);
    ck_assert_int_eq(s->entries[i1].status, IK_SCHEDULE_RUNNING);
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

// ---------------------------------------------------------------------------
// Suite
// ---------------------------------------------------------------------------

static Suite *tool_scheduler_exec_suite(void)
{
    Suite *s = suite_create("tool_scheduler_exec");

    TCase *tc_t = tcase_create("AllTerminal");
    tcase_add_test(tc_t, test_all_terminal_empty_no_stream);
    tcase_add_test(tc_t, test_all_terminal_empty_with_stream);
    tcase_add_test(tc_t, test_all_terminal_completed_no_stream);
    tcase_add_test(tc_t, test_all_terminal_completed_with_stream);
    tcase_add_test(tc_t, test_all_terminal_one_running);
    tcase_add_test(tc_t, test_all_terminal_skipped_counts);
    suite_add_tcase(s, tc_t);

    TCase *tc_cas = tcase_create("Cascade");
    tcase_add_test(tc_cas, test_cascade_direct);
    tcase_add_test(tc_cas, test_cascade_transitive);
    tcase_add_test(tc_cas, test_cascade_unrelated_not_skipped);
    suite_add_tcase(s, tc_cas);

    TCase *tc_p = tcase_create("Promote");
    tcase_add_test(tc_p, test_promote_blocker_completes_starts_dependent);
    tcase_add_test(tc_p, test_promote_skip_when_blocker_errored);
    tcase_add_test(tc_p, test_promote_concurrent_reads_all_running);
    tcase_add_test(tc_p, test_promote_bash_blocks_file_read);
    tcase_add_test(tc_p, test_promote_web_fetch_concurrent_with_writes);
    suite_add_tcase(s, tc_p);

    return s;
}

int32_t main(void)
{
    Suite   *s  = tool_scheduler_exec_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tool_scheduler_exec_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int n = srunner_ntests_failed(sr);
    srunner_free(sr);
    return n == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
