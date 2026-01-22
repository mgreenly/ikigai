#include "../../test_constants.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "credentials.h"
#include "error.h"
#include "vendor/yyjson/yyjson.h"
#include "wrapper.h"

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

static bool mock_yyjson_doc_get_root_null = false;
yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc)
{
    return mock_yyjson_doc_get_root_null ? NULL : yyjson_doc_get_root(doc);
}

static bool mock_yyjson_get_str_null = false;
const char *yyjson_get_str_(yyjson_val *val)
{
    return mock_yyjson_get_str_null ? NULL : yyjson_get_str(val);
}

START_TEST(test_yyjson_doc_get_root_null) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    const char *tmpfile = "/tmp/test_creds_mock_null_root.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"OPENAI_API_KEY\":\"test-key\"}");
    fclose(f);
    chmod(tmpfile, 0600);
    mock_yyjson_doc_get_root_null = true;
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);
    mock_yyjson_doc_get_root_null = false;
    unlink(tmpfile);
}
END_TEST

START_TEST(test_yyjson_get_str_null) {
    unsetenv("OPENAI_API_KEY");
    unsetenv("ANTHROPIC_API_KEY");
    unsetenv("GOOGLE_API_KEY");
    const char *tmpfile = "/tmp/test_creds_mock_null_str.json";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"OPENAI_API_KEY\":\"k\",\"ANTHROPIC_API_KEY\":\"k\",\"GOOGLE_API_KEY\":\"k\"}");
    fclose(f);
    chmod(tmpfile, 0600);
    mock_yyjson_get_str_null = true;
    ik_credentials_t *creds = NULL;
    res_t result = ik_credentials_load(test_ctx, tmpfile, &creds);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(creds);
    ck_assert_ptr_null(ik_credentials_get(creds, "OPENAI_API_KEY"));
    ck_assert_ptr_null(ik_credentials_get(creds, "ANTHROPIC_API_KEY"));
    ck_assert_ptr_null(ik_credentials_get(creds, "GOOGLE_API_KEY"));
    mock_yyjson_get_str_null = false;
    unlink(tmpfile);
}
END_TEST

static Suite *credentials_suite(void)
{
    Suite *s = suite_create("Credentials_Mock");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_yyjson_doc_get_root_null);
    tcase_add_test(tc_core, test_yyjson_get_str_null);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = credentials_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/credentials/credentials_mock_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
