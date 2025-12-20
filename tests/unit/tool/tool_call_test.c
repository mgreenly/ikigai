#include <check.h>
#include <talloc.h>

#include "../../../src/tool.h"

// Test fixtures
static TALLOC_CTX *ctx = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
}

// Test: ik_tool_call_create returns non-NULL struct
START_TEST(test_tool_call_create_returns_nonnull)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_abc123", "glob", "{\"pattern\": \"*.c\"}");

    ck_assert_ptr_nonnull(call);

    talloc_free(ctx);
    ctx = talloc_new(NULL);
}
END_TEST

// Test: ik_tool_call_create sets id correctly
START_TEST(test_tool_call_create_sets_id)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_abc123", "glob", "{\"pattern\": \"*.c\"}");

    ck_assert_ptr_nonnull(call);
    ck_assert_ptr_nonnull(call->id);
    ck_assert_str_eq(call->id, "call_abc123");

    talloc_free(ctx);
    ctx = talloc_new(NULL);
}
END_TEST

// Test: ik_tool_call_create sets name correctly
START_TEST(test_tool_call_create_sets_name)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_abc123", "glob", "{\"pattern\": \"*.c\"}");

    ck_assert_ptr_nonnull(call);
    ck_assert_ptr_nonnull(call->name);
    ck_assert_str_eq(call->name, "glob");

    talloc_free(ctx);
    ctx = talloc_new(NULL);
}
END_TEST

// Test: ik_tool_call_create sets arguments correctly
START_TEST(test_tool_call_create_sets_arguments)
{
    const char *args = "{\"pattern\": \"*.c\", \"path\": \"src/\"}";
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_abc123", "glob", args);

    ck_assert_ptr_nonnull(call);
    ck_assert_ptr_nonnull(call->arguments);
    ck_assert_str_eq(call->arguments, args);

    talloc_free(ctx);
    ctx = talloc_new(NULL);
}
END_TEST

// Test: ik_tool_call_create works with NULL parent context (talloc root)
START_TEST(test_tool_call_create_null_parent)
{
    ik_tool_call_t *call = ik_tool_call_create(NULL, "call_xyz", "file_read", "{\"path\": \"/tmp/test\"}");

    ck_assert_ptr_nonnull(call);
    ck_assert_str_eq(call->id, "call_xyz");
    ck_assert_str_eq(call->name, "file_read");
    ck_assert_str_eq(call->arguments, "{\"path\": \"/tmp/test\"}");

    talloc_free(call);
}
END_TEST

// Test suite
static Suite *tool_call_suite(void)
{
    Suite *s = suite_create("Tool Call");

    TCase *tc_call = tcase_create("Tool Call Creation");
    tcase_add_checked_fixture(tc_call, setup, teardown);
    tcase_add_test(tc_call, test_tool_call_create_returns_nonnull);
    tcase_add_test(tc_call, test_tool_call_create_sets_id);
    tcase_add_test(tc_call, test_tool_call_create_sets_name);
    tcase_add_test(tc_call, test_tool_call_create_sets_arguments);
    tcase_add_test(tc_call, test_tool_call_create_null_parent);
    suite_add_tcase(s, tc_call);

    return s;
}

int main(void)
{
    Suite *s = tool_call_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
