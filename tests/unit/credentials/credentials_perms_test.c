#include "../../test_constants.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

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

START_TEST(test_load_with_insecure_permissions) {
    const char *tmpfile = "/tmp/test_creds_warning.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{}");
    fclose(f);
    chmod(tmpfile, 0644);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    unlink(tmpfile);
}

END_TEST

START_TEST(test_file_based_credentials) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");

    const char *tmpfile = "/tmp/test_creds_file.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"OPENAI_API_KEY\":\"file-openai-key\","
            "\"ANTHROPIC_API_KEY\":\"file-anthropic-key\","
            "\"GOOGLE_API_KEY\":\"file-google-key\"}");
    fclose(f);
    chmod(tmpfile, 0600);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    const char *openai_key = ik_credentials_get(creds, "OPENAI_API_KEY");
    ck_assert_ptr_nonnull(openai_key);
    ck_assert_str_eq(openai_key, "file-openai-key");

    const char *anthropic_key = ik_credentials_get(creds, "ANTHROPIC_API_KEY");
    ck_assert_ptr_nonnull(anthropic_key);
    ck_assert_str_eq(anthropic_key, "file-anthropic-key");

    const char *google_key = ik_credentials_get(creds, "GOOGLE_API_KEY");
    ck_assert_ptr_nonnull(google_key);
    ck_assert_str_eq(google_key, "file-google-key");

    unlink(tmpfile);
}

END_TEST

START_TEST(test_invalid_json_file) {
    const char *tmpfile = "/tmp/test_creds_invalid.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{this is not valid json}");
    fclose(f);
    chmod(tmpfile, 0600);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    unlink(tmpfile);
}

END_TEST

START_TEST(test_json_root_not_object) {
    const char *tmpfile = "/tmp/test_creds_array.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "[\"not\", \"an\", \"object\"]");
    fclose(f);
    chmod(tmpfile, 0600);

    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);

    unlink(tmpfile);
}

END_TEST

static Suite *credentials_suite(void)
{
    Suite *s = suite_create("Credentials_Perms");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_load_with_insecure_permissions);
    tcase_add_test(tc_core, test_file_based_credentials);
    tcase_add_test(tc_core, test_invalid_json_file);
    tcase_add_test(tc_core, test_json_root_not_object);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = credentials_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/credentials/credentials_perms_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
