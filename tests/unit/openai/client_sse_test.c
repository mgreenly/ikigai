#include <check.h>
#include <talloc.h>
#include <string.h>
#include "openai/client.h"
#include "config.h"
#include "vendor/yyjson/yyjson.h"

/* Test fixtures */
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

/*
 * SSE parser tests
 */

START_TEST(test_sse_parser_create) {
    res_t res = ik_openai_sse_parser_create(ctx);
    ck_assert(!res.is_err);

    ik_openai_sse_parser_t *parser = res.ok;
    ck_assert_ptr_nonnull(parser);
    ck_assert_ptr_nonnull(parser->buffer);
    ck_assert_uint_eq(parser->buffer_len, 0);
    ck_assert(parser->buffer_cap > 0);
}

END_TEST START_TEST(test_sse_parser_feed_partial_data)
{
    res_t parser_res = ik_openai_sse_parser_create(ctx);
    ck_assert(!parser_res.is_err);
    ik_openai_sse_parser_t *parser = parser_res.ok;

    /* Feed partial data (no \n\n delimiter) */
    const char *data = "data: {\"test\": \"value\"}";
    res_t feed_res = ik_openai_sse_parser_feed(parser, data, strlen(data));
    ck_assert(!feed_res.is_err);

    /* No complete event yet */
    res_t event_res = ik_openai_sse_parser_get_event(parser);
    ck_assert(!event_res.is_err);
    ck_assert_ptr_null(event_res.ok);

    /* Buffer should contain the partial data */
    ck_assert_uint_eq(parser->buffer_len, strlen(data));
    ck_assert_str_eq(parser->buffer, data);
}

END_TEST START_TEST(test_sse_parser_feed_complete_event)
{
    res_t parser_res = ik_openai_sse_parser_create(ctx);
    ck_assert(!parser_res.is_err);
    ik_openai_sse_parser_t *parser = parser_res.ok;

    /* Feed complete event with \n\n delimiter */
    const char *data = "data: {\"test\": \"value\"}\n\n";
    res_t feed_res = ik_openai_sse_parser_feed(parser, data, strlen(data));
    ck_assert(!feed_res.is_err);

    /* Should have one complete event */
    res_t event_res = ik_openai_sse_parser_get_event(parser);
    ck_assert(!event_res.is_err);
    ck_assert_ptr_nonnull(event_res.ok);

    char *event = event_res.ok;
    ck_assert_str_eq(event, "data: {\"test\": \"value\"}");

    /* Buffer should now be empty */
    ck_assert_uint_eq(parser->buffer_len, 0);

    /* No more events */
    res_t event_res2 = ik_openai_sse_parser_get_event(parser);
    ck_assert(!event_res2.is_err);
    ck_assert_ptr_null(event_res2.ok);
}

END_TEST START_TEST(test_sse_parser_feed_multiple_events)
{
    res_t parser_res = ik_openai_sse_parser_create(ctx);
    ck_assert(!parser_res.is_err);
    ik_openai_sse_parser_t *parser = parser_res.ok;

    /* Feed multiple events at once */
    const char *data = "data: event1\n\ndata: event2\n\ndata: event3\n\n";
    res_t feed_res = ik_openai_sse_parser_feed(parser, data, strlen(data));
    ck_assert(!feed_res.is_err);

    /* Extract first event */
    res_t event1_res = ik_openai_sse_parser_get_event(parser);
    ck_assert(!event1_res.is_err);
    ck_assert_str_eq(event1_res.ok, "data: event1");

    /* Extract second event */
    res_t event2_res = ik_openai_sse_parser_get_event(parser);
    ck_assert(!event2_res.is_err);
    ck_assert_str_eq(event2_res.ok, "data: event2");

    /* Extract third event */
    res_t event3_res = ik_openai_sse_parser_get_event(parser);
    ck_assert(!event3_res.is_err);
    ck_assert_str_eq(event3_res.ok, "data: event3");

    /* No more events */
    res_t event4_res = ik_openai_sse_parser_get_event(parser);
    ck_assert(!event4_res.is_err);
    ck_assert_ptr_null(event4_res.ok);
}

END_TEST START_TEST(test_sse_parser_feed_chunked_event)
{
    res_t parser_res = ik_openai_sse_parser_create(ctx);
    ck_assert(!parser_res.is_err);
    ik_openai_sse_parser_t *parser = parser_res.ok;

    /* Simulate streaming: feed event in multiple chunks */
    ik_openai_sse_parser_feed(parser, "data: {\"", 8);
    ck_assert_ptr_null(ik_openai_sse_parser_get_event(parser).ok);

    ik_openai_sse_parser_feed(parser, "test\": \"", 8);
    ck_assert_ptr_null(ik_openai_sse_parser_get_event(parser).ok);

    ik_openai_sse_parser_feed(parser, "value\"}", 7);
    ck_assert_ptr_null(ik_openai_sse_parser_get_event(parser).ok);

    ik_openai_sse_parser_feed(parser, "\n", 1);
    ck_assert_ptr_null(ik_openai_sse_parser_get_event(parser).ok);

    /* Final chunk completes the event */
    ik_openai_sse_parser_feed(parser, "\n", 1);

    res_t event_res = ik_openai_sse_parser_get_event(parser);
    ck_assert(!event_res.is_err);
    ck_assert_str_eq(event_res.ok, "data: {\"test\": \"value\"}");
}

END_TEST START_TEST(test_sse_parser_buffer_growth)
{
    res_t parser_res = ik_openai_sse_parser_create(ctx);
    ck_assert(!parser_res.is_err);
    ik_openai_sse_parser_t *parser = parser_res.ok;

    /* Create large data that exceeds initial buffer capacity */
    const size_t large_size = 8192;  /* Larger than initial 4096 */
    const size_t alloc_size = large_size + 3;
    char *large_data = talloc_array(ctx, char, (unsigned int)alloc_size);
    memset(large_data, 'x', large_size);
    large_data[large_size] = '\n';
    large_data[large_size + 1] = '\n';
    large_data[large_size + 2] = '\0';

    /* Feed large data - should trigger buffer growth */
    res_t feed_res = ik_openai_sse_parser_feed(parser, large_data, large_size + 2);
    ck_assert(!feed_res.is_err);

    /* Should be able to extract the event */
    res_t event_res = ik_openai_sse_parser_get_event(parser);
    ck_assert(!event_res.is_err);
    ck_assert_ptr_nonnull(event_res.ok);

    char *event = event_res.ok;
    ck_assert_uint_eq(strlen(event), large_size);
}

END_TEST START_TEST(test_sse_parser_empty_feed)
{
    res_t parser_res = ik_openai_sse_parser_create(ctx);
    ck_assert(!parser_res.is_err);
    ik_openai_sse_parser_t *parser = parser_res.ok;

    /* Feed empty data */
    res_t feed_res = ik_openai_sse_parser_feed(parser, "", 0);
    ck_assert(!feed_res.is_err);

    /* Buffer should still be empty */
    ck_assert_uint_eq(parser->buffer_len, 0);
}

END_TEST START_TEST(test_sse_parser_done_marker)
{
    res_t parser_res = ik_openai_sse_parser_create(ctx);
    ck_assert(!parser_res.is_err);
    ik_openai_sse_parser_t *parser = parser_res.ok;

    /* Feed DONE marker (as seen in OpenAI streams) */
    const char *data = "data: [DONE]\n\n";
    res_t feed_res = ik_openai_sse_parser_feed(parser, data, strlen(data));
    ck_assert(!feed_res.is_err);

    /* Should extract the DONE marker */
    res_t event_res = ik_openai_sse_parser_get_event(parser);
    ck_assert(!event_res.is_err);
    ck_assert_str_eq(event_res.ok, "data: [DONE]");
}

END_TEST START_TEST(test_sse_parser_partial_then_complete)
{
    res_t parser_res = ik_openai_sse_parser_create(ctx);
    ck_assert(!parser_res.is_err);
    ik_openai_sse_parser_t *parser = parser_res.ok;

    /* Feed partial event */
    ik_openai_sse_parser_feed(parser, "data: partial", 13);

    /* No event yet */
    ck_assert_ptr_null(ik_openai_sse_parser_get_event(parser).ok);

    /* Complete the event */
    ik_openai_sse_parser_feed(parser, "\n\ndata: next\n\n", 14);

    /* Extract first event */
    res_t event1_res = ik_openai_sse_parser_get_event(parser);
    ck_assert_str_eq(event1_res.ok, "data: partial");

    /* Extract second event */
    res_t event2_res = ik_openai_sse_parser_get_event(parser);
    ck_assert_str_eq(event2_res.ok, "data: next");
}

END_TEST
/*
 * SSE event parsing tests
 */

START_TEST(test_parse_sse_event_with_content)
{
    const char *event = "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_nonnull(res.ok);
    ck_assert_str_eq(res.ok, "Hello");
}

END_TEST START_TEST(test_parse_sse_event_done_marker)
{
    const char *event = "data: [DONE]";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_sse_event_no_content)
{
    const char *event = "data: {\"choices\":[{\"delta\":{}}]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_sse_event_role_only)
{
    /* First event often contains role but no content */
    const char *event = "data: {\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_sse_event_malformed_json)
{
    const char *event = "data: {\"malformed\"";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(res.is_err);
}

END_TEST START_TEST(test_parse_sse_event_missing_prefix)
{
    const char *event = "{\"choices\":[{\"delta\":{\"content\":\"test\"}}]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(res.is_err);
}

END_TEST START_TEST(test_parse_sse_event_missing_choices)
{
    const char *event = "data: {\"other\":\"field\"}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_sse_event_empty_choices)
{
    const char *event = "data: {\"choices\":[]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_sse_event_choices_not_array)
{
    /* choices exists but is not an array */
    const char *event = "data: {\"choices\":\"invalid\"}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_sse_event_finish_reason)
{
    /* Event with finish_reason but no content */
    const char *event = "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_sse_event_multiline_content)
{
    const char *event = "data: {\"choices\":[{\"delta\":{\"content\":\"Line 1\\nLine 2\"}}]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_nonnull(res.ok);
    ck_assert_str_eq(res.ok, "Line 1\nLine 2");
}

END_TEST START_TEST(test_parse_sse_event_special_chars)
{
    const char *event = "data: {\"choices\":[{\"delta\":{\"content\":\"Test: \\\"quoted\\\"\"}}]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_nonnull(res.ok);
    ck_assert_str_eq(res.ok, "Test: \"quoted\"");
}

END_TEST START_TEST(test_parse_sse_event_json_root_not_object)
{
    /* JSON root is an array instead of object */
    const char *event = "data: [\"not\", \"an\", \"object\"]";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(res.is_err);
    ck_assert(res.err->code == ERR_PARSE);
}

END_TEST START_TEST(test_parse_sse_event_choice0_not_object)
{
    /* choices[0] is a string instead of object */
    const char *event = "data: {\"choices\":[\"not_an_object\"]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_sse_event_choice0_null)
{
    /* choices[0] is JSON null (not a NULL pointer, but a yyjson val representing null) */
    const char *event = "data: {\"choices\":[null]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_yyjson_arr_get_wrapper_out_of_bounds)
{
    /* Test out of bounds access returns NULL */
    const char *json = "[1, 2]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *elem = yyjson_arr_get_wrapper(root, 10);  /* Out of bounds */
    ck_assert_ptr_null(elem);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_parse_sse_event_delta_missing)
{
    /* choice[0] exists but is missing delta key */
    const char *event = "data: {\"choices\":[{\"index\":0}]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_sse_event_delta_not_object)
{
    /* delta is a string instead of object */
    const char *event = "data: {\"choices\":[{\"delta\":\"not_an_object\"}]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST START_TEST(test_parse_sse_event_content_not_string)
{
    /* content exists but is not a string */
    const char *event = "data: {\"choices\":[{\"delta\":{\"content\":123}}]}";
    res_t res = ik_openai_parse_sse_event(ctx, event);
    ck_assert(!res.is_err);
    ck_assert_ptr_null(res.ok);
}

END_TEST

/*
 * Test suite
 */

static Suite *openai_sse_suite(void)
{
    Suite *s = suite_create("OpenAI SSE");

    TCase *tc_sse = tcase_create("SSE Parser");
    tcase_add_checked_fixture(tc_sse, setup, teardown);
    tcase_add_test(tc_sse, test_sse_parser_create);
    tcase_add_test(tc_sse, test_sse_parser_feed_partial_data);
    tcase_add_test(tc_sse, test_sse_parser_feed_complete_event);
    tcase_add_test(tc_sse, test_sse_parser_feed_multiple_events);
    tcase_add_test(tc_sse, test_sse_parser_feed_chunked_event);
    tcase_add_test(tc_sse, test_sse_parser_buffer_growth);
    tcase_add_test(tc_sse, test_sse_parser_empty_feed);
    tcase_add_test(tc_sse, test_sse_parser_done_marker);
    tcase_add_test(tc_sse, test_sse_parser_partial_then_complete);
    suite_add_tcase(s, tc_sse);

    TCase *tc_sse_parse = tcase_create("SSE Event Parsing");
    tcase_add_checked_fixture(tc_sse_parse, setup, teardown);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_with_content);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_done_marker);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_no_content);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_role_only);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_malformed_json);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_missing_prefix);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_missing_choices);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_empty_choices);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_choices_not_array);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_finish_reason);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_multiline_content);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_special_chars);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_json_root_not_object);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_choice0_not_object);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_choice0_null);
    tcase_add_test(tc_sse_parse, test_yyjson_arr_get_wrapper_out_of_bounds);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_delta_missing);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_delta_not_object);
    tcase_add_test(tc_sse_parse, test_parse_sse_event_content_not_string);
    suite_add_tcase(s, tc_sse_parse);

    return s;
}

int main(void)
{
    Suite *s = openai_sse_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
