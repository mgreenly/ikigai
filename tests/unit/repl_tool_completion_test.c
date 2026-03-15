// Unit tests for multi-tool message assembly (repl_tool_completion.c)
#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <talloc.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/tool.h"
#include "apps/ikigai/tool_scheduler.h"
#include "apps/ikigai/repl_tool_completion.h"
#include "apps/ikigai/providers/provider.h"
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

static int32_t add_call_with_id(ik_tool_scheduler_t *sched,
                                 const char *id, const char *name,
                                 const char *args)
{
    ik_tool_call_t *tc = ik_tool_call_create(sched, id, name, args);
    ik_tool_scheduler_add(sched, tc);
    return sched->count - 1;
}

// ---------------------------------------------------------------------------
// Tests: build_multi_tool_call_msg
// ---------------------------------------------------------------------------

START_TEST(test_call_msg_single_entry_role_and_kind)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *ag = talloc_zero(ctx, ik_agent_ctx_t);
    ik_tool_scheduler_t *s = ik_tool_scheduler_create(ctx, ag);
    add_call_with_id(s, "call-1", "file_read", "{\"file_path\":\"/a\"}");

    ik_message_t *msg = ik_repl_build_multi_tool_call_msg(ag, s);
    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->role, IK_ROLE_ASSISTANT);
    ck_assert_str_eq(msg->kind, "tool_call");
    ck_assert_int_eq((int)msg->content_count, 1);
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_call_msg_single_entry_block_fields)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *ag = talloc_zero(ctx, ik_agent_ctx_t);
    ik_tool_scheduler_t *s = ik_tool_scheduler_create(ctx, ag);
    add_call_with_id(s, "call-abc", "glob", "{\"pattern\":\"*.c\"}");

    ik_message_t *msg = ik_repl_build_multi_tool_call_msg(ag, s);
    ik_content_block_t *b = &msg->content_blocks[0];
    ck_assert_int_eq(b->type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(b->data.tool_call.id, "call-abc");
    ck_assert_str_eq(b->data.tool_call.name, "glob");
    ck_assert_str_eq(b->data.tool_call.arguments, "{\"pattern\":\"*.c\"}");
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_call_msg_two_entries_count)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *ag = talloc_zero(ctx, ik_agent_ctx_t);
    ik_tool_scheduler_t *s = ik_tool_scheduler_create(ctx, ag);
    add_call_with_id(s, "id-1", "web_fetch", "{}");
    add_call_with_id(s, "id-2", "web_search", "{}");

    ik_message_t *msg = ik_repl_build_multi_tool_call_msg(ag, s);
    ck_assert_int_eq((int)msg->content_count, 2);
    ck_assert_str_eq(msg->content_blocks[0].data.tool_call.id, "id-1");
    ck_assert_str_eq(msg->content_blocks[1].data.tool_call.id, "id-2");
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

// ---------------------------------------------------------------------------
// Tests: build_multi_tool_result_msg — COMPLETED entry
// ---------------------------------------------------------------------------

START_TEST(test_result_msg_completed_not_error)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *ag = talloc_zero(ctx, ik_agent_ctx_t);
    ik_tool_scheduler_t *s = ik_tool_scheduler_create(ctx, ag);
    int32_t i = add_call_with_id(s, "call-x", "web_fetch", "{}");
    ik_tool_scheduler_on_complete(s, i, talloc_strdup(s, "{\"ok\":true}"));
    s->stream_complete = true;

    ik_message_t *msg = ik_repl_build_multi_tool_result_msg(ag, s);
    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->role, IK_ROLE_TOOL);
    ik_content_block_t *b = &msg->content_blocks[0];
    ck_assert_int_eq(b->type, IK_CONTENT_TOOL_RESULT);
    ck_assert_str_eq(b->data.tool_result.tool_call_id, "call-x");
    ck_assert_str_eq(b->data.tool_result.content, "{\"ok\":true}");
    ck_assert(!b->data.tool_result.is_error);
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_result_msg_completed_null_result_defaults_to_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *ag = talloc_zero(ctx, ik_agent_ctx_t);
    ik_tool_scheduler_t *s = ik_tool_scheduler_create(ctx, ag);
    int32_t i = add_call_with_id(s, "call-y", "web_fetch", "{}");
    ik_tool_scheduler_on_complete(s, i, NULL);
    s->stream_complete = true;

    ik_message_t *msg = ik_repl_build_multi_tool_result_msg(ag, s);
    ik_content_block_t *b = &msg->content_blocks[0];
    ck_assert_str_eq(b->data.tool_result.content, "{}");
    ck_assert(!b->data.tool_result.is_error);
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

// ---------------------------------------------------------------------------
// Tests: build_multi_tool_result_msg — ERRORED entry
// ---------------------------------------------------------------------------

START_TEST(test_result_msg_errored_is_error_true)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *ag = talloc_zero(ctx, ik_agent_ctx_t);
    ik_tool_scheduler_t *s = ik_tool_scheduler_create(ctx, ag);
    int32_t i = add_call_with_id(s, "call-e", "bash", "{}");
    ik_tool_scheduler_on_error(s, i, "execution failed: exit 1");
    s->stream_complete = true;

    ik_message_t *msg = ik_repl_build_multi_tool_result_msg(ag, s);
    ik_content_block_t *b = &msg->content_blocks[0];
    ck_assert(b->data.tool_result.is_error);
    ck_assert_str_eq(b->data.tool_result.content, "execution failed: exit 1");
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

// ---------------------------------------------------------------------------
// Tests: build_multi_tool_result_msg — SKIPPED entry
// ---------------------------------------------------------------------------

START_TEST(test_result_msg_skipped_is_error_and_mentions_prereq)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *ag = talloc_zero(ctx, ik_agent_ctx_t);
    ik_tool_scheduler_t *s = ik_tool_scheduler_create(ctx, ag);
    // file_edit followed by file_read on same path → read is blocked by edit
    int32_t i0 = add_call_with_id(s, "call-edit", "file_edit",
                                   "{\"file_path\":\"/tmp/x\"}");
    add_call_with_id(s, "call-read", "file_read",
                     "{\"file_path\":\"/tmp/x\"}");
    ik_tool_scheduler_on_error(s, i0, "write failed");
    s->stream_complete = true;

    ik_message_t *msg = ik_repl_build_multi_tool_result_msg(ag, s);
    ik_content_block_t *skipped = &msg->content_blocks[1];
    ck_assert(skipped->data.tool_result.is_error);
    ck_assert_ptr_nonnull(strstr(skipped->data.tool_result.content, "Skipped"));
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

// ---------------------------------------------------------------------------
// Tests: result ordering matches tool_use_id position
// ---------------------------------------------------------------------------

START_TEST(test_result_msg_order_matches_call_order)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_agent_ctx_t *ag = talloc_zero(ctx, ik_agent_ctx_t);
    ik_tool_scheduler_t *s = ik_tool_scheduler_create(ctx, ag);
    int32_t i0 = add_call_with_id(s, "first",  "web_fetch", "{}");
    int32_t i1 = add_call_with_id(s, "second", "web_search", "{}");
    ik_tool_scheduler_on_complete(s, i1, talloc_strdup(s, "\"results\""));
    ik_tool_scheduler_on_complete(s, i0, talloc_strdup(s, "\"page\""));
    s->stream_complete = true;

    ik_message_t *msg = ik_repl_build_multi_tool_result_msg(ag, s);
    ck_assert_str_eq(msg->content_blocks[0].data.tool_result.tool_call_id, "first");
    ck_assert_str_eq(msg->content_blocks[1].data.tool_result.tool_call_id, "second");
    ik_tool_scheduler_destroy(s);
    talloc_free(ctx);
}
END_TEST

// ---------------------------------------------------------------------------
// Suite assembly
// ---------------------------------------------------------------------------

static Suite *repl_tool_completion_suite(void)
{
    Suite *s = suite_create("repl_tool_completion");

    TCase *tc_call = tcase_create("build_multi_tool_call_msg");
    tcase_add_test(tc_call, test_call_msg_single_entry_role_and_kind);
    tcase_add_test(tc_call, test_call_msg_single_entry_block_fields);
    tcase_add_test(tc_call, test_call_msg_two_entries_count);
    suite_add_tcase(s, tc_call);

    TCase *tc_result = tcase_create("build_multi_tool_result_msg");
    tcase_add_test(tc_result, test_result_msg_completed_not_error);
    tcase_add_test(tc_result, test_result_msg_completed_null_result_defaults_to_empty);
    tcase_add_test(tc_result, test_result_msg_errored_is_error_true);
    tcase_add_test(tc_result, test_result_msg_skipped_is_error_and_mentions_prereq);
    tcase_add_test(tc_result, test_result_msg_order_matches_call_order);
    suite_add_tcase(s, tc_result);

    return s;
}

int main(void)
{
    SRunner *sr = srunner_create(repl_tool_completion_suite());
    srunner_set_xml(sr, "reports/check/unit/repl_tool_completion_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
