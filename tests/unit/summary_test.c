#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/msg.h"
#include "apps/ikigai/summary.h"

/* ---- Boundaries tests ---- */

START_TEST(test_boundaries_zero_context_start)
{
    ik_summary_range_t r = ik_summary_boundaries(10, 0);
    ck_assert_uint_eq(r.start, 0);
    ck_assert_uint_eq(r.end, 0);
}
END_TEST

START_TEST(test_boundaries_context_start_one)
{
    ik_summary_range_t r = ik_summary_boundaries(5, 1);
    ck_assert_uint_eq(r.start, 0);
    ck_assert_uint_eq(r.end, 1);
}
END_TEST

START_TEST(test_boundaries_context_start_equals_count)
{
    ik_summary_range_t r = ik_summary_boundaries(5, 5);
    ck_assert_uint_eq(r.start, 0);
    ck_assert_uint_eq(r.end, 5);
}
END_TEST

START_TEST(test_boundaries_zero_count_zero_index)
{
    ik_summary_range_t r = ik_summary_boundaries(0, 0);
    ck_assert_uint_eq(r.start, 0);
    ck_assert_uint_eq(r.end, 0);
}
END_TEST

/* ---- Transcript tests ---- */

START_TEST(test_transcript_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_msg_t *msgs[] = { NULL };
    char *t = ik_summary_transcript(ctx, msgs, 0);
    ck_assert_str_eq(t, "");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_transcript_single_user)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char kind[] = "user";
    char content[] = "Hello";
    ik_msg_t msg = { .id = 1, .kind = kind, .content = content,
                     .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &msg };
    char *t = ik_summary_transcript(ctx, msgs, 1);
    ck_assert_ptr_nonnull(strstr(t, "user: Hello\n"));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_transcript_excludes_metadata)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char ku[] = "user";      char cu[] = "hello";
    char kc[] = "clear";     char cc[] = "cleared";
    char km[] = "mark";      char cm[] = "marked";
    char kr[] = "rewind";    char cr[] = "rewound";
    char ka[] = "assistant"; char ca[] = "world";
    ik_msg_t user_msg   = { .id = 1, .kind = ku, .content = cu, .data_json = NULL, .interrupted = false };
    ik_msg_t clear_msg  = { .id = 2, .kind = kc, .content = cc, .data_json = NULL, .interrupted = false };
    ik_msg_t mark_msg   = { .id = 3, .kind = km, .content = cm, .data_json = NULL, .interrupted = false };
    ik_msg_t rewind_msg = { .id = 4, .kind = kr, .content = cr, .data_json = NULL, .interrupted = false };
    ik_msg_t asst_msg   = { .id = 5, .kind = ka, .content = ca, .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &user_msg, &clear_msg, &mark_msg, &rewind_msg, &asst_msg };

    char *t = ik_summary_transcript(ctx, msgs, 5);
    ck_assert_ptr_nonnull(strstr(t, "user: hello\n"));
    ck_assert_ptr_nonnull(strstr(t, "assistant: world\n"));
    ck_assert_ptr_null(strstr(t, "clear:"));
    ck_assert_ptr_null(strstr(t, "mark:"));
    ck_assert_ptr_null(strstr(t, "rewind:"));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_transcript_only_metadata_produces_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char kind[] = "clear";
    char content[] = "cleared";
    ik_msg_t clear_msg = { .id = 1, .kind = kind, .content = content,
                           .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &clear_msg };
    char *t = ik_summary_transcript(ctx, msgs, 1);
    ck_assert_str_eq(t, "");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_prompt_non_empty)
{
    ck_assert(strlen(IK_SUMMARY_PROMPT) > 0);
    ck_assert_ptr_nonnull(strstr(IK_SUMMARY_PROMPT, "key decisions"));
    ck_assert_ptr_nonnull(strstr(IK_SUMMARY_PROMPT, "unresolved"));
    ck_assert_ptr_nonnull(strstr(IK_SUMMARY_PROMPT, "technical details"));
    ck_assert_ptr_nonnull(strstr(IK_SUMMARY_PROMPT, "concise"));
}
END_TEST

static Suite *summary_suite(void)
{
    Suite *s = suite_create("summary");

    TCase *tc_bounds = tcase_create("Boundaries");
    tcase_add_test(tc_bounds, test_boundaries_zero_context_start);
    tcase_add_test(tc_bounds, test_boundaries_context_start_one);
    tcase_add_test(tc_bounds, test_boundaries_context_start_equals_count);
    tcase_add_test(tc_bounds, test_boundaries_zero_count_zero_index);
    suite_add_tcase(s, tc_bounds);

    TCase *tc_transcript = tcase_create("Transcript");
    tcase_add_test(tc_transcript, test_transcript_empty);
    tcase_add_test(tc_transcript, test_transcript_single_user);
    tcase_add_test(tc_transcript, test_transcript_excludes_metadata);
    tcase_add_test(tc_transcript, test_transcript_only_metadata_produces_empty);
    tcase_add_test(tc_transcript, test_prompt_non_empty);
    suite_add_tcase(s, tc_transcript);

    return s;
}

int32_t main(void)
{
    Suite *s = summary_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/summary_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
