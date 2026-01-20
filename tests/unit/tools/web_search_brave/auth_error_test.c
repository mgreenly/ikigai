#include "../../../test_constants.h"

#include "tools/web_search_brave/auth_error.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

START_TEST(test_write_auth_error_json) {
    int32_t pipe_fds[2];
    ck_assert_int_eq(pipe(pipe_fds), 0);

    int32_t saved_stdout = dup(STDOUT_FILENO);
    dup2(pipe_fds[1], STDOUT_FILENO);
    close(pipe_fds[1]);

    write_auth_error_json();

    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    char buffer[8192];
    ssize_t n = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
    ck_assert(n > 0);
    buffer[n] = '\0';
    close(pipe_fds[0]);

    ck_assert(strstr(buffer, "\"success\": false") != NULL);
    ck_assert(strstr(buffer, "\"error_code\": \"AUTH_MISSING\"") != NULL);
    ck_assert(strstr(buffer, "\"_event\"") != NULL);
    ck_assert(strstr(buffer, "\"kind\": \"config_required\"") != NULL);
    ck_assert(strstr(buffer, "\"tool\": \"web_search_brave\"") != NULL);
    ck_assert(strstr(buffer, "\"credential\": \"api_key\"") != NULL);
    ck_assert(strstr(buffer, "\"signup_url\": \"https://brave.com/search/api/\"") != NULL);
    ck_assert(strstr(buffer, "Brave Search offers 2,000 free searches/month") != NULL);
}

END_TEST

static Suite *auth_error_suite(void)
{
    Suite *s = suite_create("WebSearchBraveAuthError");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_write_auth_error_json);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = auth_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
