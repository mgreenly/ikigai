/**
 * @file message_from_db_mocked_test.c
 * @brief Unit tests for message.c with mocked yyjson functions
 *
 * Tests error paths by mocking yyjson wrapper functions to return NULL.
 */

#include "../../../src/message.h"

#include "../../../src/error.h"
#include "../../../src/msg.h"
#include "../../../src/wrapper.h"
#include "../../../src/vendor/yyjson/yyjson.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Mock yyjson_doc_get_root_ to return NULL
yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc)
{
    (void)doc;
    return NULL;
}

// Test: yyjson_doc_get_root_ returns NULL for tool_call
START_TEST(test_tool_call_null_root) {
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_call"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}
END_TEST
// Test: yyjson_doc_get_root_ returns NULL for tool_result
START_TEST(test_tool_result_null_root)
{
    ik_msg_t db_msg = {
        .kind = talloc_strdup(test_ctx, "tool_result"),
        .content = NULL,
        .data_json = talloc_strdup(test_ctx, "{}"),
    };

    ik_message_t *out = NULL;
    res_t r = ik_message_from_db_msg(test_ctx, &db_msg, &out);

    ck_assert(is_err(&r));
    ck_assert_ptr_null(out);
}

END_TEST

static Suite *message_from_db_mocked_suite(void)
{
    Suite *s = suite_create("Message from DB (Mocked)");
    TCase *tc_mocked = tcase_create("Mocked");
    tcase_set_timeout(tc_mocked, 30);

    tcase_add_checked_fixture(tc_mocked, setup, teardown);
    tcase_add_test(tc_mocked, test_tool_call_null_root);
    tcase_add_test(tc_mocked, test_tool_result_null_root);

    suite_add_tcase(s, tc_mocked);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = message_from_db_mocked_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
