/**
 * @file mark_library_errors_test.c
 * @brief Tests for marks.c gmtime and strftime error paths and error propagation through commands
 */

#include "../../../src/agent.h"
#include <check.h>
#include <talloc.h>
#include <time.h>

#include "../../../src/commands_mark.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/marks.h"
#include "../../../src/openai/client.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

// Test fixture
static TALLOC_CTX *ctx;
static ik_repl_ctx_t *repl;

// Mock state
static bool mock_gmtime_should_fail = false;
static bool mock_strftime_should_fail = false;

// Mock gmtime_ to inject failures
struct tm *gmtime_(const time_t *timep)
{
    if (mock_gmtime_should_fail) {
        return NULL;  // Simulate gmtime failure
    }

    // Call real gmtime
    return gmtime(timep);
}

// Mock strftime_ to inject failures
size_t strftime_(char *s, size_t max, const char *format, const struct tm *tm)
{
    if (mock_strftime_should_fail) {
        return 0;  // Simulate strftime failure
    }

    // Call real strftime
    // Suppress format-nonliteral warning - format string comes from caller
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    return strftime(s, max, format, tm);
#pragma GCC diagnostic pop
}

/**
 * Create a REPL context with conversation for testing
 */
static ik_repl_ctx_t *create_test_repl_with_conversation(void *parent)
{
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    ik_openai_conversation_t *conv = ik_openai_conversation_create(parent);
    ck_assert_ptr_nonnull(conv);

    // Create minimal config
    ik_cfg_t *cfg = talloc_zero(parent, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;

    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;

    agent->conversation = conv;
    r->current = agent;

    r->shared = shared;
    r->current->marks = NULL;
    r->current->mark_count = 0;

    return r;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    repl = create_test_repl_with_conversation(ctx);
    ck_assert_ptr_nonnull(repl);

    // Reset mock state
    mock_gmtime_should_fail = false;
    mock_strftime_should_fail = false;
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: gmtime failure in get_iso8601_timestamp (line 24)
START_TEST(test_gmtime_failure) {
    // Mock gmtime to fail
    mock_gmtime_should_fail = true;

    // Create mark - should fail with gmtime error
    res_t res = ik_mark_create(repl, "test_mark");

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_str_eq(error_message(res.err), "gmtime failed to convert timestamp");

    // Verify no mark was created
    ck_assert_uint_eq(repl->current->mark_count, 0);
}
END_TEST
// Test: strftime failure in get_iso8601_timestamp (line 32)
START_TEST(test_strftime_failure)
{
    // Mock strftime to fail
    mock_strftime_should_fail = true;

    // Create mark - should fail with strftime error
    res_t res = ik_mark_create(repl, "test_mark");

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_str_eq(error_message(res.err), "strftime failed to format timestamp");

    // Verify no mark was created
    ck_assert_uint_eq(repl->current->mark_count, 0);
}

END_TEST
// Test: gmtime failure with unlabeled mark
START_TEST(test_gmtime_failure_unlabeled)
{
    // Mock gmtime to fail
    mock_gmtime_should_fail = true;

    // Create unlabeled mark - should fail with gmtime error
    res_t res = ik_mark_create(repl, NULL);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_str_eq(error_message(res.err), "gmtime failed to convert timestamp");

    // Verify no mark was created
    ck_assert_uint_eq(repl->current->mark_count, 0);
}

END_TEST
// Test: strftime failure with unlabeled mark
START_TEST(test_strftime_failure_unlabeled)
{
    // Mock strftime to fail
    mock_strftime_should_fail = true;

    // Create unlabeled mark - should fail with strftime error
    res_t res = ik_mark_create(repl, NULL);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_str_eq(error_message(res.err), "strftime failed to format timestamp");

    // Verify no mark was created
    ck_assert_uint_eq(repl->current->mark_count, 0);
}

END_TEST
// Test: Successful mark creation after gmtime failure
START_TEST(test_mark_success_after_gmtime_failure)
{
    // First, trigger gmtime failure
    mock_gmtime_should_fail = true;
    res_t res = ik_mark_create(repl, "fail_mark");
    ck_assert(is_err(&res));
    ck_assert_uint_eq(repl->current->mark_count, 0);

    // Reset mock and try again
    mock_gmtime_should_fail = false;
    res = ik_mark_create(repl, "success_mark");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->mark_count, 1);

    // Verify mark was created properly
    ck_assert_ptr_nonnull(repl->current->marks[0]);
    ck_assert_str_eq(repl->current->marks[0]->label, "success_mark");
    ck_assert_ptr_nonnull(repl->current->marks[0]->timestamp);
}

END_TEST
// Test: Successful mark creation after strftime failure
START_TEST(test_mark_success_after_strftime_failure)
{
    // First, trigger strftime failure
    mock_strftime_should_fail = true;
    res_t res = ik_mark_create(repl, "fail_mark");
    ck_assert(is_err(&res));
    ck_assert_uint_eq(repl->current->mark_count, 0);

    // Reset mock and try again
    mock_strftime_should_fail = false;
    res = ik_mark_create(repl, "success_mark");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->mark_count, 1);

    // Verify mark was created properly
    ck_assert_ptr_nonnull(repl->current->marks[0]);
    ck_assert_str_eq(repl->current->marks[0]->label, "success_mark");
    ck_assert_ptr_nonnull(repl->current->marks[0]->timestamp);
}

END_TEST
// Test: ik_cmd_mark error propagation when gmtime fails (covers commands_mark.c lines 79-80)
START_TEST(test_cmd_mark_gmtime_error_propagation)
{
    // Mock gmtime to fail
    mock_gmtime_should_fail = true;

    // Call ik_cmd_mark - should propagate error from ik_mark_create
    res_t res = ik_cmd_mark(ctx, repl, "test_mark");

    // Verify error propagated correctly
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_str_eq(error_message(res.err), "gmtime failed to convert timestamp");

    // Verify no mark was created
    ck_assert_uint_eq(repl->current->mark_count, 0);
}

END_TEST
// Test: ik_cmd_mark error propagation when strftime fails (covers commands_mark.c lines 79-80)
START_TEST(test_cmd_mark_strftime_error_propagation)
{
    // Mock strftime to fail
    mock_strftime_should_fail = true;

    // Call ik_cmd_mark - should propagate error from ik_mark_create
    res_t res = ik_cmd_mark(ctx, repl, NULL);  // Test with unlabeled mark

    // Verify error propagated correctly
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert_str_eq(error_message(res.err), "strftime failed to format timestamp");

    // Verify no mark was created
    ck_assert_uint_eq(repl->current->mark_count, 0);
}

END_TEST

static Suite *mark_library_errors_suite(void)
{
    Suite *s = suite_create("Mark Library Errors");
    TCase *tc = tcase_create("gmtime/strftime failures");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_gmtime_failure);
    tcase_add_test(tc, test_strftime_failure);
    tcase_add_test(tc, test_gmtime_failure_unlabeled);
    tcase_add_test(tc, test_strftime_failure_unlabeled);
    tcase_add_test(tc, test_mark_success_after_gmtime_failure);
    tcase_add_test(tc, test_mark_success_after_strftime_failure);
    tcase_add_test(tc, test_cmd_mark_gmtime_error_propagation);
    tcase_add_test(tc, test_cmd_mark_strftime_error_propagation);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = mark_library_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
