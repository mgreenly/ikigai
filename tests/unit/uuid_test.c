#include "../../src/uuid.h"
#include "../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// Helper function to check if string contains only base64url characters
static bool is_base64url(const char *str, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (!isalnum(c) && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

// Test ik_generate_uuid() returns non-NULL string
START_TEST(test_generate_uuid_returns_non_null)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char *uuid = ik_generate_uuid(ctx);
    ck_assert_ptr_nonnull(uuid);

    talloc_free(ctx);
}
END_TEST

// Test ik_generate_uuid() returns exactly 22 characters
START_TEST(test_generate_uuid_returns_22_chars)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char *uuid = ik_generate_uuid(ctx);
    ck_assert_ptr_nonnull(uuid);
    ck_assert_uint_eq(strlen(uuid), 22);

    talloc_free(ctx);
}
END_TEST

// Test ik_generate_uuid() returns valid base64url characters
START_TEST(test_generate_uuid_returns_base64url_chars)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char *uuid = ik_generate_uuid(ctx);
    ck_assert_ptr_nonnull(uuid);
    ck_assert(is_base64url(uuid, strlen(uuid)));

    talloc_free(ctx);
}
END_TEST

// Test ik_generate_uuid() returns unique values
START_TEST(test_generate_uuid_returns_unique_values)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Seed random number generator
    srand((unsigned int)time(NULL));

    char *uuid1 = ik_generate_uuid(ctx);
    char *uuid2 = ik_generate_uuid(ctx);
    char *uuid3 = ik_generate_uuid(ctx);

    ck_assert_ptr_nonnull(uuid1);
    ck_assert_ptr_nonnull(uuid2);
    ck_assert_ptr_nonnull(uuid3);

    // All should be different (with very high probability)
    ck_assert_str_ne(uuid1, uuid2);
    ck_assert_str_ne(uuid2, uuid3);
    ck_assert_str_ne(uuid1, uuid3);

    talloc_free(ctx);
}
END_TEST

// Test ik_generate_uuid() result is owned by ctx
START_TEST(test_generate_uuid_owned_by_ctx)
{
    TALLOC_CTX *parent = talloc_new(NULL);
    ck_assert_ptr_nonnull(parent);

    char *uuid = ik_generate_uuid(parent);
    ck_assert_ptr_nonnull(uuid);

    // Verify parent relationship
    TALLOC_CTX *actual_parent = talloc_parent(uuid);
    ck_assert_ptr_eq(actual_parent, parent);

    talloc_free(parent);
}
END_TEST

static Suite *uuid_suite(void)
{
    Suite *s = suite_create("UUID Generation");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_generate_uuid_returns_non_null);
    tcase_add_test(tc_core, test_generate_uuid_returns_22_chars);
    tcase_add_test(tc_core, test_generate_uuid_returns_base64url_chars);
    tcase_add_test(tc_core, test_generate_uuid_returns_unique_values);
    tcase_add_test(tc_core, test_generate_uuid_owned_by_ctx);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = uuid_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
