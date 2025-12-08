#include "../../../src/msg.h"
#include "../../../src/db/replay.h"
#include "../../../src/error.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

// Per-test state
static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Helper to create a test database message
static ik_message_t *create_test_db_msg(const char *kind, const char *content, const char *data_json)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->id = 1;
    msg->kind = talloc_strdup(msg, kind);
    msg->content = content ? talloc_strdup(msg, content) : NULL;
    msg->data_json = data_json ? talloc_strdup(msg, data_json) : NULL;
    return msg;
}

START_TEST(test_msg_from_db_user) {
    ik_message_t *db_msg = create_test_db_msg("user", "Hello world", NULL);

    res_t res = ik_msg_from_db(test_ctx, db_msg);
    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(res.ok);

    ik_msg_t *msg = res.ok;
    ck_assert_str_eq(msg->kind, "user");
    ck_assert_str_eq(msg->content, "Hello world");
    ck_assert_ptr_null(msg->data_json);
}
END_TEST START_TEST(test_msg_from_db_system)
{
    ik_message_t *db_msg = create_test_db_msg("system", "You are a helpful assistant", NULL);

    res_t res = ik_msg_from_db(test_ctx, db_msg);
    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(res.ok);

    ik_msg_t *msg = res.ok;
    ck_assert_str_eq(msg->kind, "system");
    ck_assert_str_eq(msg->content, "You are a helpful assistant");
    ck_assert_ptr_null(msg->data_json);
}

END_TEST START_TEST(test_msg_from_db_assistant)
{
    ik_message_t *db_msg = create_test_db_msg("assistant", "I can help you with that", NULL);

    res_t res = ik_msg_from_db(test_ctx, db_msg);
    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(res.ok);

    ik_msg_t *msg = res.ok;
    ck_assert_str_eq(msg->kind, "assistant");
    ck_assert_str_eq(msg->content, "I can help you with that");
    ck_assert_ptr_null(msg->data_json);
}

END_TEST START_TEST(test_msg_from_db_tool_call)
{
    const char *data_json =
        "{\"id\":\"call_123\",\"type\":\"function\",\"function\":{\"name\":\"glob\",\"arguments\":\"{\\\"pattern\\\":\\\"*.c\\\"}\"}}";
    ik_message_t *db_msg = create_test_db_msg("tool_call", "glob(pattern=\"*.c\")", data_json);

    res_t res = ik_msg_from_db(test_ctx, db_msg);
    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(res.ok);

    ik_msg_t *msg = res.ok;
    ck_assert_str_eq(msg->kind, "tool_call");
    ck_assert_str_eq(msg->content, "glob(pattern=\"*.c\")");
    ck_assert_str_eq(msg->data_json, data_json);
}

END_TEST START_TEST(test_msg_from_db_tool_result)
{
    const char *data_json = "{\"tool_call_id\":\"call_123\",\"content\":\"file1.c\\nfile2.c\"}";
    ik_message_t *db_msg = create_test_db_msg("tool_result", "file1.c\nfile2.c", data_json);

    res_t res = ik_msg_from_db(test_ctx, db_msg);
    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(res.ok);

    ik_msg_t *msg = res.ok;
    ck_assert_str_eq(msg->kind, "tool_result");
    ck_assert_str_eq(msg->content, "file1.c\nfile2.c");
    ck_assert_str_eq(msg->data_json, data_json);
}

END_TEST START_TEST(test_msg_from_db_skip_clear)
{
    ik_message_t *db_msg = create_test_db_msg("clear", NULL, "{}");

    res_t res = ik_msg_from_db(test_ctx, db_msg);
    ck_assert(!is_err(&res));
    ck_assert_ptr_null(res.ok);  // Should skip clear events
}

END_TEST START_TEST(test_msg_from_db_skip_mark)
{
    ik_message_t *db_msg = create_test_db_msg("mark", NULL, "{\"label\":\"checkpoint\"}");

    res_t res = ik_msg_from_db(test_ctx, db_msg);
    ck_assert(!is_err(&res));
    ck_assert_ptr_null(res.ok);  // Should skip mark events
}

END_TEST START_TEST(test_msg_from_db_skip_rewind)
{
    ik_message_t *db_msg = create_test_db_msg("rewind", NULL, "{\"target_id\":123}");

    res_t res = ik_msg_from_db(test_ctx, db_msg);
    ck_assert(!is_err(&res));
    ck_assert_ptr_null(res.ok);  // Should skip rewind events
}

END_TEST START_TEST(test_msg_from_db_null_content)
{
    ik_message_t *db_msg = create_test_db_msg("user", NULL, NULL);

    res_t res = ik_msg_from_db(test_ctx, db_msg);
    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(res.ok);

    ik_msg_t *msg = res.ok;
    ck_assert_str_eq(msg->kind, "user");
    ck_assert_ptr_null(msg->content);
    ck_assert_ptr_null(msg->data_json);
}

END_TEST START_TEST(test_msg_from_db_tool_call_null_data_json)
{
    ik_message_t *db_msg = create_test_db_msg("tool_call", "some content", NULL);

    res_t res = ik_msg_from_db(test_ctx, db_msg);
    ck_assert(!is_err(&res));
    ck_assert_ptr_nonnull(res.ok);

    ik_msg_t *msg = res.ok;
    ck_assert_str_eq(msg->kind, "tool_call");
    ck_assert_str_eq(msg->content, "some content");
    ck_assert_ptr_null(msg->data_json);
}

END_TEST

static Suite *msg_from_db_suite(void)
{
    Suite *s = suite_create("Message from DB");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_msg_from_db_user);
    tcase_add_test(tc_core, test_msg_from_db_system);
    tcase_add_test(tc_core, test_msg_from_db_assistant);
    tcase_add_test(tc_core, test_msg_from_db_tool_call);
    tcase_add_test(tc_core, test_msg_from_db_tool_result);
    tcase_add_test(tc_core, test_msg_from_db_skip_clear);
    tcase_add_test(tc_core, test_msg_from_db_skip_mark);
    tcase_add_test(tc_core, test_msg_from_db_skip_rewind);
    tcase_add_test(tc_core, test_msg_from_db_null_content);
    tcase_add_test(tc_core, test_msg_from_db_tool_call_null_data_json);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = msg_from_db_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
