// Protocol UUID generation unit tests

#include <check.h>
#include <talloc.h>
#include <jansson.h>
#include <string.h>
#include <ctype.h>
#include "../../../src/protocol.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Test UUID generation produces 22-character string
START_TEST(test_protocol_generate_uuid)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    res_t res = ik_protocol_generate_uuid(ctx);
    ck_assert(is_ok(&res));

    char *uuid = (char *)res.ok;
    ck_assert_ptr_nonnull(uuid);
    ck_assert_int_eq((int)strlen(uuid), 22);

    // Verify it's base64url (no +, /, or =)
    for (int i = 0; i < 22; i++) {
        char c = uuid[i];
        ck_assert(isalnum(c) || c == '-' || c == '_');
        ck_assert(c != '+');
        ck_assert(c != '/');
        ck_assert(c != '=');
    }

    talloc_free(ctx);
}

END_TEST
// Test UUID uniqueness
START_TEST(test_protocol_uuid_uniqueness)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

#define NUM_UUIDS 100
    char *uuids[NUM_UUIDS];

    // Generate 100 UUIDs
    for (int i = 0; i < NUM_UUIDS; i++) {
        res_t res = ik_protocol_generate_uuid(ctx);
        ck_assert(is_ok(&res));
        uuids[i] = (char *)res.ok;
    }

    // Verify all are different
    for (int i = 0; i < NUM_UUIDS; i++) {
        for (int j = i + 1; j < NUM_UUIDS; j++) {
            ck_assert_str_ne(uuids[i], uuids[j]);
        }
    }

    talloc_free(ctx);
}

END_TEST
// Test UUID generation OOM when allocating result buffer
START_TEST(test_protocol_generate_uuid_oom)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Fail the array allocation for the base64url result
    oom_test_fail_next_alloc();

    res_t res = ik_protocol_generate_uuid(ctx);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
static Suite *protocol_uuid_suite(void)
{
    Suite *s = suite_create("Protocol UUID");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_protocol_generate_uuid);
    tcase_add_test(tc_core, test_protocol_uuid_uniqueness);
    tcase_add_test(tc_core, test_protocol_generate_uuid_oom);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = protocol_uuid_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
