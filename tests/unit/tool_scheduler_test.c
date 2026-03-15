// Unit tests for tool_scheduler.c: classify and conflict matrix
#include <check.h>
#include <stdlib.h>
#include <talloc.h>

#include "apps/ikigai/tool_scheduler.h"
#include "shared/error.h"

// ---------------------------------------------------------------------------
// Tests: access classifier
// ---------------------------------------------------------------------------

START_TEST(test_classify_file_read)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(
        ctx, "file_read", "{\"file_path\":\"/tmp/foo\"}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_PATHS);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_NONE);
    ck_assert_int_eq(a.read_path_count, 1);
    ck_assert_str_eq(a.read_paths[0], "/tmp/foo");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_file_edit)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(
        ctx, "file_edit", "{\"file_path\":\"/tmp/bar\"}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_PATHS);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_PATHS);
    ck_assert_str_eq(a.read_paths[0], "/tmp/bar");
    ck_assert_str_eq(a.write_paths[0], "/tmp/bar");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_file_write)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(
        ctx, "file_write", "{\"file_path\":\"/tmp/baz\"}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_NONE);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_PATHS);
    ck_assert_str_eq(a.write_paths[0], "/tmp/baz");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_glob)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(ctx, "glob", "{}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_WILDCARD);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_NONE);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_grep)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(ctx, "grep", "{}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_WILDCARD);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_NONE);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_list)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(ctx, "list", "{}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_WILDCARD);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_NONE);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_bash)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(ctx, "bash", "{}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_WILDCARD);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_WILDCARD);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_web_fetch)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(ctx, "web_fetch", "{}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_NONE);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_NONE);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_web_search)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(ctx, "web_search", "{}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_NONE);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_NONE);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_mem_get)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(
        ctx, "mem", "{\"action\":\"get\",\"path\":\"notes\"}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_PATHS);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_NONE);
    ck_assert_str_eq(a.read_paths[0], "mem:notes");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_mem_list)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(
        ctx, "mem", "{\"action\":\"list\",\"path\":\"docs\"}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_PATHS);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_NONE);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_mem_create)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(
        ctx, "mem", "{\"action\":\"create\",\"path\":\"notes\"}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_NONE);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_PATHS);
    ck_assert_str_eq(a.write_paths[0], "mem:notes");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_mem_update)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(
        ctx, "mem", "{\"action\":\"update\",\"path\":\"notes\"}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_NONE);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_PATHS);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_mem_delete)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(
        ctx, "mem", "{\"action\":\"delete\",\"path\":\"notes\"}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_NONE);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_PATHS);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_mem_revisions)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(
        ctx, "mem", "{\"action\":\"revisions\",\"path\":\"notes\"}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_PATHS);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_NONE);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_mem_revision_get)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(
        ctx, "mem", "{\"action\":\"revision_get\",\"path\":\"notes\"}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_PATHS);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_NONE);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_classify_unknown_is_barrier)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = ik_tool_scheduler_classify(ctx, "internal_op", "{}");
    ck_assert_int_eq(a.read_mode, IK_ACCESS_WILDCARD);
    ck_assert_int_eq(a.write_mode, IK_ACCESS_WILDCARD);
    talloc_free(ctx);
}
END_TEST

// ---------------------------------------------------------------------------
// Helpers for conflict tests
// ---------------------------------------------------------------------------

static ik_access_t make_rp(TALLOC_CTX *ctx, const char *p)
{
    ik_access_t a = {0};
    a.read_mode = IK_ACCESS_PATHS;
    a.read_paths = talloc_array(ctx, char *, 1);
    a.read_paths[0] = talloc_strdup(ctx, p);
    a.read_path_count = 1;
    return a;
}

static ik_access_t make_wp(TALLOC_CTX *ctx, const char *p)
{
    ik_access_t a = {0};
    a.write_mode = IK_ACCESS_PATHS;
    a.write_paths = talloc_array(ctx, char *, 1);
    a.write_paths[0] = talloc_strdup(ctx, p);
    a.write_path_count = 1;
    return a;
}

// ---------------------------------------------------------------------------
// Tests: conflict matrix
// ---------------------------------------------------------------------------

START_TEST(test_conflict_none_vs_none)
{
    ik_access_t a = {0}, b = {0};
    ck_assert(!ik_tool_scheduler_conflicts(&a, &b));
}
END_TEST

START_TEST(test_conflict_read_vs_read_same)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = make_rp(ctx, "/f"), b = make_rp(ctx, "/f");
    ck_assert(!ik_tool_scheduler_conflicts(&a, &b));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_conflict_read_vs_read_diff)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = make_rp(ctx, "/a"), b = make_rp(ctx, "/b");
    ck_assert(!ik_tool_scheduler_conflicts(&a, &b));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_conflict_write_vs_read_same)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = make_wp(ctx, "/f"), b = make_rp(ctx, "/f");
    ck_assert(ik_tool_scheduler_conflicts(&a, &b));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_conflict_write_vs_read_diff)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = make_wp(ctx, "/a"), b = make_rp(ctx, "/b");
    ck_assert(!ik_tool_scheduler_conflicts(&a, &b));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_conflict_write_vs_write_same)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = make_wp(ctx, "/f"), b = make_wp(ctx, "/f");
    ck_assert(ik_tool_scheduler_conflicts(&a, &b));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_conflict_write_vs_write_diff)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = make_wp(ctx, "/a"), b = make_wp(ctx, "/b");
    ck_assert(!ik_tool_scheduler_conflicts(&a, &b));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_conflict_wildcard_write_vs_read_paths)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = {0}; a.read_mode = IK_ACCESS_WILDCARD; a.write_mode = IK_ACCESS_WILDCARD;
    ik_access_t b = make_rp(ctx, "/f");
    ck_assert(ik_tool_scheduler_conflicts(&a, &b));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_conflict_wildcard_read_vs_write_paths)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_access_t a = {0}; a.read_mode = IK_ACCESS_WILDCARD;
    ik_access_t b = make_wp(ctx, "/f");
    ck_assert(ik_tool_scheduler_conflicts(&a, &b));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_conflict_wildcard_vs_wildcard)
{
    ik_access_t a = {0}, b = {0};
    a.read_mode = IK_ACCESS_WILDCARD; a.write_mode = IK_ACCESS_WILDCARD;
    b.read_mode = IK_ACCESS_WILDCARD; b.write_mode = IK_ACCESS_WILDCARD;
    ck_assert(ik_tool_scheduler_conflicts(&a, &b));
}
END_TEST

START_TEST(test_no_conflict_none_vs_wildcard_write)
{
    // NONE reads/writes vs WILDCARD write: no conflict (NONE side has no access)
    ik_access_t a = {0};
    ik_access_t b = {0}; b.write_mode = IK_ACCESS_WILDCARD;
    ck_assert(!ik_tool_scheduler_conflicts(&a, &b));
}
END_TEST

START_TEST(test_conflict_wildcard_read_vs_wildcard_write)
{
    ik_access_t a = {0}; a.read_mode = IK_ACCESS_WILDCARD;
    ik_access_t b = {0}; b.write_mode = IK_ACCESS_WILDCARD;
    ck_assert(ik_tool_scheduler_conflicts(&a, &b));
}
END_TEST

// ---------------------------------------------------------------------------
// Suite
// ---------------------------------------------------------------------------

static Suite *tool_scheduler_suite(void)
{
    Suite *s = suite_create("tool_scheduler");

    TCase *tc_c = tcase_create("Classify");
    tcase_add_test(tc_c, test_classify_file_read);
    tcase_add_test(tc_c, test_classify_file_edit);
    tcase_add_test(tc_c, test_classify_file_write);
    tcase_add_test(tc_c, test_classify_glob);
    tcase_add_test(tc_c, test_classify_grep);
    tcase_add_test(tc_c, test_classify_list);
    tcase_add_test(tc_c, test_classify_bash);
    tcase_add_test(tc_c, test_classify_web_fetch);
    tcase_add_test(tc_c, test_classify_web_search);
    tcase_add_test(tc_c, test_classify_mem_get);
    tcase_add_test(tc_c, test_classify_mem_list);
    tcase_add_test(tc_c, test_classify_mem_create);
    tcase_add_test(tc_c, test_classify_mem_update);
    tcase_add_test(tc_c, test_classify_mem_delete);
    tcase_add_test(tc_c, test_classify_mem_revisions);
    tcase_add_test(tc_c, test_classify_mem_revision_get);
    tcase_add_test(tc_c, test_classify_unknown_is_barrier);
    suite_add_tcase(s, tc_c);

    TCase *tc_f = tcase_create("Conflict");
    tcase_add_test(tc_f, test_conflict_none_vs_none);
    tcase_add_test(tc_f, test_conflict_read_vs_read_same);
    tcase_add_test(tc_f, test_conflict_read_vs_read_diff);
    tcase_add_test(tc_f, test_conflict_write_vs_read_same);
    tcase_add_test(tc_f, test_conflict_write_vs_read_diff);
    tcase_add_test(tc_f, test_conflict_write_vs_write_same);
    tcase_add_test(tc_f, test_conflict_write_vs_write_diff);
    tcase_add_test(tc_f, test_conflict_wildcard_write_vs_read_paths);
    tcase_add_test(tc_f, test_conflict_wildcard_read_vs_write_paths);
    tcase_add_test(tc_f, test_conflict_wildcard_vs_wildcard);
    tcase_add_test(tc_f, test_no_conflict_none_vs_wildcard_write);
    tcase_add_test(tc_f, test_conflict_wildcard_read_vs_wildcard_write);
    suite_add_tcase(s, tc_f);

    return s;
}

int32_t main(void)
{
    Suite   *s  = tool_scheduler_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tool_scheduler_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int n = srunner_ntests_failed(sr);
    srunner_free(sr);
    return n == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
