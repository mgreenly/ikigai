#include "../../../src/mail/msg.h"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <time.h>

// Test create allocates message
START_TEST(test_msg_create_allocates_message) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_mail_msg_t *msg = ik_mail_msg_create(ctx, "from-uuid", "to-uuid", "test body");

    ck_assert_ptr_nonnull(msg);

    talloc_free(ctx);
}
END_TEST
// Test fields copied correctly
START_TEST(test_msg_create_copies_fields)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *from = "sender-uuid-123";
    const char *to = "recipient-uuid-456";
    const char *body = "Hello, this is a test message";

    ik_mail_msg_t *msg = ik_mail_msg_create(ctx, from, to, body);

    ck_assert_ptr_nonnull(msg);
    ck_assert_ptr_nonnull(msg->from_uuid);
    ck_assert_ptr_nonnull(msg->to_uuid);
    ck_assert_ptr_nonnull(msg->body);
    ck_assert_str_eq(msg->from_uuid, from);
    ck_assert_str_eq(msg->to_uuid, to);
    ck_assert_str_eq(msg->body, body);

    talloc_free(ctx);
}

END_TEST
// Test timestamp set to current time
START_TEST(test_msg_create_sets_timestamp)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    int64_t before = time(NULL);
    ik_mail_msg_t *msg = ik_mail_msg_create(ctx, "from", "to", "body");
    int64_t after = time(NULL);

    ck_assert_ptr_nonnull(msg);
    ck_assert_int_ge(msg->timestamp, before);
    ck_assert_int_le(msg->timestamp, after);

    talloc_free(ctx);
}

END_TEST
// Test read defaults to false
START_TEST(test_msg_create_read_defaults_false)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_mail_msg_t *msg = ik_mail_msg_create(ctx, "from", "to", "body");

    ck_assert_ptr_nonnull(msg);
    ck_assert(msg->read == false);

    talloc_free(ctx);
}

END_TEST
// Test id defaults to 0 (set on insert)
START_TEST(test_msg_create_id_defaults_zero)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_mail_msg_t *msg = ik_mail_msg_create(ctx, "from", "to", "body");

    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->id, 0);

    talloc_free(ctx);
}

END_TEST
// Test freed with talloc_free
START_TEST(test_msg_freed_with_talloc_free)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_mail_msg_t *msg = ik_mail_msg_create(ctx, "from", "to", "body");
    ck_assert_ptr_nonnull(msg);

    // talloc_free should succeed and return 0
    int result = talloc_free(ctx);
    ck_assert_int_eq(result, 0);
}

END_TEST

static Suite *msg_suite(void)
{
    Suite *s = suite_create("Mail Message");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_msg_create_allocates_message);
    tcase_add_test(tc_core, test_msg_create_copies_fields);
    tcase_add_test(tc_core, test_msg_create_sets_timestamp);
    tcase_add_test(tc_core, test_msg_create_read_defaults_false);
    tcase_add_test(tc_core, test_msg_create_id_defaults_zero);
    tcase_add_test(tc_core, test_msg_freed_with_talloc_free);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = msg_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
