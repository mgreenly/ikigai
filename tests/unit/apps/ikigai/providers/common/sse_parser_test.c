#include "tests/test_constants.h"
#include "apps/ikigai/providers/common/sse_parser.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

/* Test: Parser creation */
START_TEST(test_parser_create) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);
    ck_assert_ptr_nonnull(parser);
    ck_assert_ptr_nonnull(parser->buffer);
    ck_assert_uint_eq(parser->len, 0);
    ck_assert_uint_gt(parser->cap, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: Empty buffer returns NULL */
START_TEST(test_empty_buffer) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_null(event);

    talloc_free(ctx);
}

END_TEST
/* Test: Single event */
START_TEST(test_single_event) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    const char *input = "data: hello\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_null(event->event);
    ck_assert_ptr_nonnull(event->data);
    ck_assert_str_eq(event->data, "hello");

    /* No more events */
    ik_sse_event_t *event2 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_null(event2);

    talloc_free(ctx);
}

END_TEST
/* Test: Event with type */
START_TEST(test_event_with_type) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    const char *input = "event: message\ndata: content\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_nonnull(event->event);
    ck_assert_str_eq(event->event, "message");
    ck_assert_ptr_nonnull(event->data);
    ck_assert_str_eq(event->data, "content");

    talloc_free(ctx);
}

END_TEST
/* Test: Multiple events */
START_TEST(test_multiple_events) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    const char *input = "data: first\n\ndata: second\n\ndata: third\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    /* Extract first event */
    ik_sse_event_t *event1 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event1);
    ck_assert_str_eq(event1->data, "first");

    /* Extract second event */
    ik_sse_event_t *event2 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event2);
    ck_assert_str_eq(event2->data, "second");

    /* Extract third event */
    ik_sse_event_t *event3 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event3);
    ck_assert_str_eq(event3->data, "third");

    /* No more events */
    ik_sse_event_t *event4 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_null(event4);

    talloc_free(ctx);
}

END_TEST
/* Test: Partial feed */
START_TEST(test_partial_feed) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    /* Feed event in chunks */
    ik_sse_parser_feed(parser, "data: ", 6);
    ik_sse_event_t *event1 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_null(event1); /* No complete event yet */

    ik_sse_parser_feed(parser, "partial", 7);
    ik_sse_event_t *event2 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_null(event2); /* Still no complete event */

    ik_sse_parser_feed(parser, "\n\n", 2);
    ik_sse_event_t *event3 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event3); /* Now we have a complete event */
    ck_assert_str_eq(event3->data, "partial");

    talloc_free(ctx);
}

END_TEST
/* Test: Done marker detection */
START_TEST(test_done_marker) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    const char *input = "data: [DONE]\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert(ik_sse_event_is_done(event));

    talloc_free(ctx);
}

END_TEST
/* Test: Not done marker */
START_TEST(test_not_done) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    const char *input = "data: regular content\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert(!ik_sse_event_is_done(event));

    talloc_free(ctx);
}

END_TEST

/* Test: Multi-line data */
START_TEST(test_multiline_data) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    const char *input = "data: line1\ndata: line2\ndata: line3\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_nonnull(event->data);
    ck_assert_str_eq(event->data, "line1\nline2\nline3");

    talloc_free(ctx);
}

END_TEST
/* Test: Empty data field (data: with no content) */
START_TEST(test_empty_data_field) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    const char *input = "data:\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_nonnull(event->data);
    ck_assert_str_eq(event->data, "");

    talloc_free(ctx);
}

END_TEST
/* Test: Event type without space after colon */
START_TEST(test_event_type_no_space) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    const char *input = "event:message\ndata: test\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_nonnull(event->event);
    ck_assert_str_eq(event->event, "message");

    talloc_free(ctx);
}

END_TEST
/* Test: Buffer growth */
START_TEST(test_buffer_growth) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    /* Create a large data payload that exceeds initial buffer size */
    char large_data[8192];
    memset(large_data, 'x', sizeof(large_data) - 1);
    large_data[sizeof(large_data) - 1] = '\0';

    /* Feed the large data */
    ik_sse_parser_feed(parser, "data: ", 6);
    ik_sse_parser_feed(parser, large_data, strlen(large_data));
    ik_sse_parser_feed(parser, "\n\n", 2);

    /* Extract event */
    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_nonnull(event->data);
    ck_assert_str_eq(event->data, large_data);

    talloc_free(ctx);
}

END_TEST
/* Test: Partial data remaining in buffer */
START_TEST(test_partial_remaining) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    /* Feed complete event + partial next event */
    const char *input = "data: complete\n\ndata: partial";
    ik_sse_parser_feed(parser, input, strlen(input));

    /* Extract first event */
    ik_sse_event_t *event1 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event1);
    ck_assert_str_eq(event1->data, "complete");

    /* No second event yet */
    ik_sse_event_t *event2 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_null(event2);

    /* Complete the second event */
    ik_sse_parser_feed(parser, "\n\n", 2);
    ik_sse_event_t *event3 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event3);
    ck_assert_str_eq(event3->data, "partial");

    talloc_free(ctx);
}

END_TEST

/* Test: Feed with zero length (line 46) */
START_TEST(test_feed_zero_length) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    /* Feed with len=0 should return early without modifying parser */
    size_t initial_len = parser->len;
    ik_sse_parser_feed(parser, "data", 0);
    ck_assert_uint_eq(parser->len, initial_len);

    /* Also test with NULL data and len=0 (allowed by assertion) */
    ik_sse_parser_feed(parser, NULL, 0);
    ck_assert_uint_eq(parser->len, initial_len);

    talloc_free(ctx);
}
END_TEST

/* Test: CRLF delimiter only (line 83 - crlf_delimiter != NULL && delimiter == NULL) */
START_TEST(test_crlf_delimiter_only) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    /* Use CRLF delimiter without any LF-only delimiter */
    const char *input = "data: crlf_only\r\n\r\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_nonnull(event->data);
    ck_assert_str_eq(event->data, "crlf_only");

    talloc_free(ctx);
}
END_TEST

/* Test: CRLF delimiter before LF delimiter (line 83 - crlf_delimiter < delimiter) */
START_TEST(test_crlf_before_lf) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    /* CRLF delimiter comes before LF delimiter in buffer */
    const char *input = "data: first\r\n\r\ndata: second\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    /* First event should use CRLF delimiter */
    ik_sse_event_t *event1 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event1);
    ck_assert_str_eq(event1->data, "first");

    /* Second event should use LF delimiter */
    ik_sse_event_t *event2 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event2);
    ck_assert_str_eq(event2->data, "second");

    talloc_free(ctx);
}
END_TEST

/* Test: Empty event (no data, no event type - just delimiter) */
START_TEST(test_empty_event) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    /* Just the delimiter with no content - should create event with NULL data */
    const char *input = "\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_null(event->data);
    ck_assert_ptr_null(event->event);

    talloc_free(ctx);
}
END_TEST

/* Test: Event with comment line (ignored line type) */
START_TEST(test_event_with_comment) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    /* Comment lines (starting with ':') should be ignored */
    const char *input = ": this is a comment\ndata: content\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_nonnull(event->data);
    ck_assert_str_eq(event->data, "content");

    talloc_free(ctx);
}
END_TEST

/* Test: is_done with NULL data (line 210) */
START_TEST(test_is_done_null_data) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    /* Create an event with no data field (just delimiter) */
    const char *input = "\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_null(event->data);

    /* is_done should return false for NULL data */
    ck_assert(!ik_sse_event_is_done(event));

    talloc_free(ctx);
}
END_TEST

/* Test: LF delimiter before CRLF delimiter (line 83 branch 5) */
START_TEST(test_lf_before_crlf) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    /* LF delimiter comes before CRLF delimiter in buffer */
    const char *input = "data: first\n\ndata: second\r\n\r\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    /* First event should use LF delimiter */
    ik_sse_event_t *event1 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event1);
    ck_assert_str_eq(event1->data, "first");

    /* Second event should use CRLF delimiter */
    ik_sse_event_t *event2 = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event2);
    ck_assert_str_eq(event2->data, "second");

    talloc_free(ctx);
}
END_TEST

/* Test: Event with short line (< 5 chars) - line 152 branch 1 */
START_TEST(test_event_with_short_line) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    /* Include a short line (less than 5 chars) that should be ignored */
    const char *input = "id\ndata: content\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_nonnull(event->data);
    ck_assert_str_eq(event->data, "content");

    talloc_free(ctx);
}
END_TEST

/* Test: Data field without space after colon */
START_TEST(test_data_no_space) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_sse_parser_t *parser = ik_sse_parser_create(ctx);

    /* Use data: without space */
    const char *input = "data:content\n\n";
    ik_sse_parser_feed(parser, input, strlen(input));

    ik_sse_event_t *event = ik_sse_parser_next(parser, ctx);
    ck_assert_ptr_nonnull(event);
    ck_assert_ptr_nonnull(event->data);
    ck_assert_str_eq(event->data, "content");

    talloc_free(ctx);
}
END_TEST

/* Test suite */
static Suite *sse_parser_suite(void)
{
    Suite *s = suite_create("SSE Parser");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_test(tc_core, test_parser_create);
    tcase_add_test(tc_core, test_empty_buffer);
    tcase_add_test(tc_core, test_single_event);
    tcase_add_test(tc_core, test_event_with_type);
    tcase_add_test(tc_core, test_multiple_events);
    tcase_add_test(tc_core, test_partial_feed);
    tcase_add_test(tc_core, test_done_marker);
    tcase_add_test(tc_core, test_not_done);
    tcase_add_test(tc_core, test_multiline_data);
    tcase_add_test(tc_core, test_empty_data_field);
    tcase_add_test(tc_core, test_event_type_no_space);
    tcase_add_test(tc_core, test_buffer_growth);
    tcase_add_test(tc_core, test_partial_remaining);
    tcase_add_test(tc_core, test_feed_zero_length);
    tcase_add_test(tc_core, test_crlf_delimiter_only);
    tcase_add_test(tc_core, test_crlf_before_lf);
    tcase_add_test(tc_core, test_empty_event);
    tcase_add_test(tc_core, test_event_with_comment);
    tcase_add_test(tc_core, test_is_done_null_data);
    tcase_add_test(tc_core, test_lf_before_crlf);
    tcase_add_test(tc_core, test_event_with_short_line);
    tcase_add_test(tc_core, test_data_no_space);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = sse_parser_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/common/sse_parser_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
