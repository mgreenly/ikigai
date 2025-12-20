#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/uuid.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <stdlib.h>
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

// Test agent->uuid is non-NULL and exactly 22 characters
START_TEST(test_agent_uuid_non_null_and_22_chars)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->uuid);
    ck_assert_uint_eq(strlen(agent->uuid), 22);

    talloc_free(ctx);
}
END_TEST

// Test agent->uuid contains only base64url characters
START_TEST(test_agent_uuid_base64url_chars)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->uuid);
    ck_assert(is_base64url(agent->uuid, strlen(agent->uuid)));

    talloc_free(ctx);
}
END_TEST

// Test ik_generate_uuid() returns valid 22-char base64url string
START_TEST(test_generate_uuid_returns_valid_string)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    char *uuid = ik_generate_uuid(ctx);
    ck_assert_ptr_nonnull(uuid);
    ck_assert_uint_eq(strlen(uuid), 22);
    ck_assert(is_base64url(uuid, 22));

    talloc_free(ctx);
}
END_TEST

// Test that multiple UUIDs are different (with seeded random)
START_TEST(test_generate_uuid_produces_different_uuids)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Seed random number generator for reproducibility
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

static Suite *agent_uuid_suite(void)
{
    Suite *s = suite_create("Agent UUID");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_agent_uuid_non_null_and_22_chars);
    tcase_add_test(tc_core, test_agent_uuid_base64url_chars);
    tcase_add_test(tc_core, test_generate_uuid_returns_valid_string);
    tcase_add_test(tc_core, test_generate_uuid_produces_different_uuids);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = agent_uuid_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
