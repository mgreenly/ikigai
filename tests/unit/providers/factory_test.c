/**
 * @file factory_test.c
 * @brief Unit tests for provider factory
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include "providers/factory.h"

// Helper macro to check if a string contains a substring
#define ck_assert_str_contains(haystack, needle) \
    ck_assert_msg(strstr(haystack, needle) != NULL, \
                  "Expected '%s' to contain '%s'", haystack, needle)

/* ================================================================
 * Environment Variable Mapping Tests
 * ================================================================ */

START_TEST(test_env_var_openai)
{
    const char *env_var = ik_provider_env_var("openai");
    ck_assert_ptr_nonnull(env_var);
    ck_assert_str_eq(env_var, "OPENAI_API_KEY");
}
END_TEST

START_TEST(test_env_var_anthropic)
{
    const char *env_var = ik_provider_env_var("anthropic");
    ck_assert_ptr_nonnull(env_var);
    ck_assert_str_eq(env_var, "ANTHROPIC_API_KEY");
}
END_TEST

START_TEST(test_env_var_google)
{
    const char *env_var = ik_provider_env_var("google");
    ck_assert_ptr_nonnull(env_var);
    ck_assert_str_eq(env_var, "GOOGLE_API_KEY");
}
END_TEST

START_TEST(test_env_var_unknown)
{
    const char *env_var = ik_provider_env_var("unknown_provider");
    ck_assert_ptr_null(env_var);
}
END_TEST

START_TEST(test_env_var_null)
{
    const char *env_var = ik_provider_env_var(NULL);
    ck_assert_ptr_null(env_var);
}
END_TEST

/* ================================================================
 * Provider Validation Tests
 * ================================================================ */

START_TEST(test_is_valid_openai)
{
    ck_assert(ik_provider_is_valid("openai"));
}
END_TEST

START_TEST(test_is_valid_anthropic)
{
    ck_assert(ik_provider_is_valid("anthropic"));
}
END_TEST

START_TEST(test_is_valid_google)
{
    ck_assert(ik_provider_is_valid("google"));
}
END_TEST

START_TEST(test_is_valid_unknown)
{
    ck_assert(!ik_provider_is_valid("unknown_provider"));
}
END_TEST

START_TEST(test_is_valid_null)
{
    ck_assert(!ik_provider_is_valid(NULL));
}
END_TEST

START_TEST(test_is_valid_case_sensitive)
{
    // Provider names are case-sensitive
    ck_assert(!ik_provider_is_valid("OpenAI"));
    ck_assert(!ik_provider_is_valid("ANTHROPIC"));
}
END_TEST

/* ================================================================
 * Provider List Tests
 * ================================================================ */

START_TEST(test_provider_list)
{
    const char **list = ik_provider_list();
    ck_assert_ptr_nonnull(list);

    // Count providers
    size_t count = 0;
    for (size_t i = 0; list[i] != NULL; i++) {
        count++;
    }
    ck_assert_uint_eq(count, 3);

    // Check all three providers are present
    bool found_openai = false;
    bool found_anthropic = false;
    bool found_google = false;

    for (size_t i = 0; list[i] != NULL; i++) {
        if (strcmp(list[i], "openai") == 0) {
            found_openai = true;
        } else if (strcmp(list[i], "anthropic") == 0) {
            found_anthropic = true;
        } else if (strcmp(list[i], "google") == 0) {
            found_google = true;
        }
    }

    ck_assert(found_openai);
    ck_assert(found_anthropic);
    ck_assert(found_google);
}
END_TEST

/* ================================================================
 * Provider Creation Tests (Error Paths)
 * ================================================================ */

START_TEST(test_create_unknown_provider)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_provider_t *provider = NULL;

    res_t res = ik_provider_create(ctx, "unknown_provider", &provider);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);
    ck_assert_str_contains(error_message(res.err), "Unknown provider");

    talloc_free(ctx);
}
END_TEST

// Note: Testing missing credentials scenario requires integration testing
// because ik_provider_create calls ik_credentials_load which loads from
// ~/.config/ikigai/credentials.json. Unit testing this would require
// mocking the credentials loading or creating temporary credential files,
// which is beyond the scope of factory unit tests.

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *factory_suite(void)
{
    Suite *s = suite_create("Provider Factory");

    TCase *tc_env_var = tcase_create("Environment Variable Mapping");
    tcase_add_test(tc_env_var, test_env_var_openai);
    tcase_add_test(tc_env_var, test_env_var_anthropic);
    tcase_add_test(tc_env_var, test_env_var_google);
    tcase_add_test(tc_env_var, test_env_var_unknown);
    tcase_add_test(tc_env_var, test_env_var_null);
    suite_add_tcase(s, tc_env_var);

    TCase *tc_validation = tcase_create("Provider Validation");
    tcase_add_test(tc_validation, test_is_valid_openai);
    tcase_add_test(tc_validation, test_is_valid_anthropic);
    tcase_add_test(tc_validation, test_is_valid_google);
    tcase_add_test(tc_validation, test_is_valid_unknown);
    tcase_add_test(tc_validation, test_is_valid_null);
    tcase_add_test(tc_validation, test_is_valid_case_sensitive);
    suite_add_tcase(s, tc_validation);

    TCase *tc_list = tcase_create("Provider List");
    tcase_add_test(tc_list, test_provider_list);
    suite_add_tcase(s, tc_list);

    TCase *tc_create = tcase_create("Provider Creation");
    tcase_add_test(tc_create, test_create_unknown_provider);
    // test_create_missing_credentials is skipped - it requires integration testing
    // with actual credential file manipulation
    suite_add_tcase(s, tc_create);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = factory_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
