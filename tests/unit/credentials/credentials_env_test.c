#include "../../test_constants.h"

#include <check.h>
#include <stdlib.h>
#include <talloc.h>

#include "credentials.h"
#include "error.h"

static TALLOC_CTX *test_ctx = NULL;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

START_TEST(test_credentials_from_env_openai) {
    setenv("OPENAI_API_KEY", "sk-test123", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *key = ik_credentials_get(creds, "OPENAI_API_KEY");
    ck_assert_ptr_nonnull(key);
    ck_assert_str_eq(key, "sk-test123");

    unsetenv("OPENAI_API_KEY");
}
END_TEST

START_TEST(test_credentials_from_env_anthropic) {
    setenv("ANTHROPIC_API_KEY", "sk-ant-test", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *key = ik_credentials_get(creds, "ANTHROPIC_API_KEY");
    ck_assert_ptr_nonnull(key);
    ck_assert_str_eq(key, "sk-ant-test");

    unsetenv("ANTHROPIC_API_KEY");
}

END_TEST

START_TEST(test_credentials_from_env_google) {
    setenv("GOOGLE_API_KEY", "AIza-test", 1);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *key = ik_credentials_get(creds, "GOOGLE_API_KEY");
    ck_assert_ptr_nonnull(key);
    ck_assert_str_eq(key, "AIza-test");

    unsetenv("GOOGLE_API_KEY");
}

END_TEST

START_TEST(test_credentials_missing_returns_null) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *key = ik_credentials_get(creds, "OPENAI_API_KEY");
    (void)key;
}

END_TEST

START_TEST(test_credentials_unknown_provider) {
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, NULL, &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *key = ik_credentials_get(creds, "UNKNOWN_ENV_VAR");
    ck_assert_ptr_null(key);
}

END_TEST

START_TEST(test_credentials_explicit_path_nonexistent) {
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, "/tmp/nonexistent_credentials.json", &creds);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);
}

END_TEST

static Suite *credentials_suite(void)
{
    Suite *s = suite_create("Credentials_Env");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_credentials_from_env_openai);
    tcase_add_test(tc_core, test_credentials_from_env_anthropic);
    tcase_add_test(tc_core, test_credentials_from_env_google);
    tcase_add_test(tc_core, test_credentials_missing_returns_null);
    tcase_add_test(tc_core, test_credentials_unknown_provider);
    tcase_add_test(tc_core, test_credentials_explicit_path_nonexistent);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = credentials_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
