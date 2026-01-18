#include "../../test_constants.h"

#include <check.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <talloc.h>
#include <unistd.h>

#define GWD char c[160];if(getcwd(c,160)==NULL)ck_abort_msg("g");
#define TST char *o=NULL;int32_t ec=0;
#define CHK ck_assert_int_eq(r,0);ck_assert_int_eq(ec,0);ck_assert_ptr_nonnull(o);

static TALLOC_CTX *tc;
static char tp[512];

static void setup(void)
{
    tc = talloc_new(NULL);
    char c[256];
    if (getcwd(c, 256) != NULL) {
        snprintf(tp, sizeof(tp), "%s/libexec/ikigai/web-fetch-tool", c);
    } else {
        snprintf(tp, sizeof(tp), "libexec/ikigai/web-fetch-tool");
    }
}

static void teardown(void)
{
    talloc_free(tc);
}

static int32_t rt(const char *i, char **o, int32_t *ec)
{
    int32_t pi[2], po[2];
    if (pipe(pi) == -1 || pipe(po) == -1) return -1;
    pid_t p = fork();
    if (p == -1) {
        close(pi[0]); close(pi[1]); close(po[0]); close(po[1]);
        return -1;
    }
    if (p == 0) {
        close(pi[1]); close(po[0]);
        dup2(pi[0], STDIN_FILENO); dup2(po[1], STDOUT_FILENO);
        close(pi[0]); close(po[1]);
        execl(tp, tp, (char *)NULL);
        exit(127);
    }
    close(pi[0]); close(po[1]);
    if (i != NULL) {
        size_t l = strlen(i);
        ssize_t w = write(pi[1], i, l);
        (void)w;
    }
    close(pi[1]);
    char buf[12288];
    ssize_t t = 0, n;
    while ((n = read(po[0], buf + t, 12287 - (size_t)t)) > 0) t += n;
    close(po[0]);
    buf[t] = '\0';
    *o = talloc_strdup(tc, buf);
    int32_t s;
    waitpid(p, &s, 0);
    *ec = WIFEXITED(s) ? WEXITSTATUS(s) : -1;
    return 0;
}

static int32_t rta(const char *arg, char **o, int32_t *ec)
{
    int32_t po[2];
    if (pipe(po) == -1) return -1;
    pid_t p = fork();
    if (p == -1) {
        close(po[0]); close(po[1]);
        return -1;
    }
    if (p == 0) {
        close(po[0]);
        dup2(po[1], STDOUT_FILENO);
        close(po[1]);
        execl(tp, tp, arg, (char *)NULL);
        exit(127);
    }
    close(po[1]);
    char buf[2048];
    ssize_t t = 0, n;
    while ((n = read(po[0], buf + t, 2047 - (size_t)t)) > 0) t += n;
    close(po[0]);
    buf[t] = '\0';
    *o = talloc_strdup(tc, buf);
    int32_t s;
    waitpid(p, &s, 0);
    *ec = WIFEXITED(s) ? WEXITSTATUS(s) : -1;
    return 0;
}

START_TEST(test_schema_flag) {
    TST

    int32_t r =rta("--schema", &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"name\"") != NULL, "n");
    ck_assert_msg(strstr(o, "web_fetch") != NULL, "w");
    ck_assert_msg(strstr(o, "\"description\"") != NULL, "d");
    ck_assert_msg(strstr(o, "\"parameters\"") != NULL, "p");
    ck_assert_msg(strstr(o, "\"url\"") != NULL, "u");
    ck_assert_msg(strstr(o, "\"required\"") != NULL, "r");
    ck_assert_msg(strstr(o, "\"offset\"") != NULL, "of");
    ck_assert_msg(strstr(o, "\"limit\"") != NULL, "li");
}
END_TEST

START_TEST(test_empty_stdin) {
    TST

    int32_t r =rt("", &o, &ec);

    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(ec, 1);
}
END_TEST

START_TEST(test_invalid_json) {
    TST

    int32_t r =rt("{invalid json", &o, &ec);

    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(ec, 1);
}
END_TEST

START_TEST(test_missing_url_field) {
    TST

    int32_t r =rt("{\"foo\":\"bar\"}", &o, &ec);

    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(ec, 1);
}
END_TEST

START_TEST(test_url_not_string) {
    TST

    int32_t r =rt("{\"url\":123}", &o, &ec);

    ck_assert_int_eq(r, 0);
    ck_assert_int_eq(ec, 1);
}
END_TEST

START_TEST(test_malformed_url) {
    TST

    int32_t r =rt("{\"url\":\"not-a-valid-url\"}", &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\"") != NULL, "s");
    ck_assert_msg(strstr(o, "false") != NULL, "f");
    ck_assert_msg(strstr(o, "\"error\"") != NULL, "e");
}
END_TEST

START_TEST(test_nonexistent_host) {
    TST

    int32_t r =rt("{\"url\":\"http://this-host-definitely-does-not-exist-12345.com\"}", &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\"") != NULL, "s");
    ck_assert_msg(strstr(o, "false") != NULL, "f");
    ck_assert_msg(strstr(o, "\"error\"") != NULL, "e");
    ck_assert_msg(strstr(o, "\"error_code\"") != NULL, "c");
    ck_assert_msg(strstr(o, "NETWORK_ERROR") != NULL, "n");
}
END_TEST

START_TEST(test_simple_html_conversion) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\": true") != NULL || strstr(o, "\"success\":true") != NULL, "t");

    ck_assert_msg(strstr(o, "Test Page") != NULL, "T");

    ck_assert_msg(strstr(o, "# Main Heading") != NULL, "1");
    ck_assert_msg(strstr(o, "## Subheading") != NULL, "2");
    ck_assert_msg(strstr(o, "This is a paragraph") != NULL, "P");
    ck_assert_msg(strstr(o, "**bold**") != NULL, "B");
    ck_assert_msg(strstr(o, "*italic*") != NULL, "I");
}
END_TEST

START_TEST(test_links_conversion) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/links.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "[this link](https://example.com)") != NULL, "x");
    ck_assert_msg(strstr(o, "[local link](/local/path)") != NULL, "y");
}
END_TEST

START_TEST(test_lists_conversion) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/lists.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "# Shopping List") != NULL, "L");
    ck_assert_msg(strstr(o, "- Apples") != NULL, "1");
    ck_assert_msg(strstr(o, "- Bananas") != NULL, "2");
    ck_assert_msg(strstr(o, "- Oranges") != NULL, "3");
}
END_TEST

START_TEST(test_scripts_stripped) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/scripts.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "alert") == NULL, "a");
    ck_assert_msg(strstr(o, "console.log") == NULL, "c");
    ck_assert_msg(strstr(o, "color: red") == NULL, "r");
    ck_assert_msg(strstr(o, "display: none") == NULL, "d");

    ck_assert_msg(strstr(o, "Visible content") != NULL, "1");
    ck_assert_msg(strstr(o, "More visible content") != NULL, "2");
}
END_TEST

START_TEST(test_formatting_conversion) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/formatting.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "`inline code`") != NULL, "C");

    ck_assert_msg(strstr(o, "**bold") != NULL && strstr(o, "text**") != NULL, "B");
    ck_assert_msg(strstr(o, "*bold italic*") != NULL, "N");

    ck_assert_msg(strstr(o, "Line break here") != NULL && strstr(o, "next line") != NULL, "L");
}
END_TEST

START_TEST(test_pagination_limit) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"limit\":2}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    char *cs = strstr(o, "\"content\"");
    ck_assert_ptr_nonnull(cs);

    int32_t nc = 0;
    for (char *p = cs; *p && nc < 10; p++) {
        if (*p == '\\' && *(p+1) == 'n') {
            nc++;
            p++;
        }
    }

    ck_assert_msg(nc <= 3, "l");
}
END_TEST

START_TEST(test_pagination_offset) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"offset\":3}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\"") != NULL, "s");
    ck_assert_msg(strstr(o, "\"content\"") != NULL, "cf");
}
END_TEST

START_TEST(test_pagination_offset_beyond) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"offset\":1000}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"content\": \"\"") != NULL || strstr(o, "\"content\":\"\"") != NULL, "e");
}
END_TEST

START_TEST(test_title_extraction) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/links.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"title\"") != NULL, "t");
    ck_assert_msg(strstr(o, "Links Test") != NULL, "L");
}
END_TEST

START_TEST(test_all_headings) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/headings.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "# Heading 1") != NULL, "1");
    ck_assert_msg(strstr(o, "## Heading 2") != NULL, "2");
    ck_assert_msg(strstr(o, "### Heading 3") != NULL, "3");
    ck_assert_msg(strstr(o, "#### Heading 4") != NULL, "4");
    ck_assert_msg(strstr(o, "##### Heading 5") != NULL, "5");
    ck_assert_msg(strstr(o, "###### Heading 6") != NULL, "6");
}
END_TEST

START_TEST(test_large_html) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/large.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\": true") != NULL || strstr(o, "\"success\":true") != NULL, "S");
}
END_TEST

START_TEST(test_html_comments) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/comments.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "Visible text") != NULL, "T");
    ck_assert_msg(strstr(o, "This is a comment") == NULL, "C");
}
END_TEST

START_TEST(test_file_not_found) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/nonexistent.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\"") != NULL && strstr(o, "false") != NULL, "E");
}
END_TEST

START_TEST(test_large_json_input) {
    TST
    GWD

    char *li = talloc_array(NULL, char, 5500);
    char *up = talloc_array(NULL, char, 5000);
    strcpy(up, "file://");
    strcat(up, c);
    strcat(up, "/tests/fixtures/html/simple.html?");
    for (int32_t i = 0; i < 200; i++) {
        char p[50];
        snprintf(p, 50, "param%d=value%d&", i, i);
        strcat(up, p);
    }
    snprintf(li, 5500, "{\"url\":\"%s\"}", up);

    int32_t r =rt(li, &o, &ec);
    talloc_free(up);
    talloc_free(li);

    CHK

    ck_assert_msg(strstr(o, "\"success\"") != NULL, "s");
}
END_TEST

START_TEST(test_http_404_error) {
    TST

    int32_t r =rt("{\"url\":\"https://httpbin.org/status/404\"}", &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\"") != NULL, "s");
    ck_assert_msg(strstr(o, "false") != NULL, "f");
    ck_assert_msg(strstr(o, "\"error\"") != NULL, "e");
    ck_assert_msg(strstr(o, "\"error_code\"") != NULL, "c");
    ck_assert_msg(strstr(o, "HTTP_ERROR") != NULL, "h");
    ck_assert_msg(strstr(o, "404") != NULL, "4");
}
END_TEST

START_TEST(test_http_500_error) {
    TST

    int32_t r =rt("{\"url\":\"https://httpbin.org/status/500\"}", &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\"") != NULL, "s");
    ck_assert_msg(strstr(o, "false") != NULL, "f");
    ck_assert_msg(strstr(o, "\"error\"") != NULL, "e");
    ck_assert_msg(strstr(o, "HTTP_ERROR") != NULL, "h");
    ck_assert_msg(strstr(o, "500") != NULL, "5");
}
END_TEST

START_TEST(test_unparseable_content) {
    TST

    int32_t r =rt("{\"url\":\"https://httpbin.org/bytes/1000\"}", &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\"") != NULL, "s");
}
END_TEST

START_TEST(test_non_integer_offset) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"offset\":\"not_a_number\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\": true") != NULL || strstr(o, "\"success\":true") != NULL, "t");
}
END_TEST

START_TEST(test_non_integer_limit) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"limit\":true}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\": true") != NULL || strstr(o, "\"success\":true") != NULL, "t");
}
END_TEST

START_TEST(test_more_elements) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/more_elements.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "**bold tag**") != NULL, "b");

    ck_assert_msg(strstr(o, "*italic tag*") != NULL, "i");

    ck_assert_msg(strstr(o, "- First ordered item") != NULL, "1");
    ck_assert_msg(strstr(o, "- Second ordered item") != NULL, "2");

    ck_assert_msg(strstr(o, "After nav element") != NULL, "n");
}
END_TEST

START_TEST(test_edge_cases) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/edge_cases.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\": true") != NULL || strstr(o, "\"success\":true") != NULL, "t");

    ck_assert_msg(strstr(o, "clickable text") != NULL, "c");
}
END_TEST

START_TEST(test_pagination_offset_and_limit) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"offset\":2,\"limit\":2}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "\"success\"") != NULL, "s");
    ck_assert_msg(strstr(o, "\"content\"") != NULL, "cf");
}
END_TEST

START_TEST(test_style_only) {
    TST
    GWD

    char i[256];
    snprintf(i, sizeof(i), "{\"url\":\"file://%s/tests/fixtures/html/style_only.html\"}", c);

    int32_t r =rt(i, &o, &ec);

    CHK

    ck_assert_msg(strstr(o, "color: blue") == NULL, "b");

    ck_assert_msg(strstr(o, "Content with style") != NULL, "c");
}
END_TEST

START_TEST(test_very_large_json_input) {
    TST
    GWD

    char *li = talloc_array(NULL, char, 12000);
    char *up = talloc_array(NULL, char, 11500);
    strcpy(up, "file://");
    strcat(up, c);
    strcat(up, "/tests/fixtures/html/simple.html?");
    for (int32_t i = 0; i < 800; i++) {
        char p[50];
        snprintf(p, 50, "x%d=%d&", i, i);
        strcat(up, p);
    }
    snprintf(li, 12000, "{\"url\":\"%s\"}", up);

    int32_t r =rt(li, &o, &ec);
    talloc_free(up);
    talloc_free(li);

    CHK
}
END_TEST

static Suite *web_fetch_suite(void)
{
    Suite *s = suite_create("W");

    TCase *t = tcase_create("C");
    tcase_set_timeout(t, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(t, setup, teardown);

    tcase_add_test(t, test_schema_flag);
    tcase_add_test(t, test_empty_stdin);
    tcase_add_test(t, test_invalid_json);
    tcase_add_test(t, test_missing_url_field);
    tcase_add_test(t, test_url_not_string);
    tcase_add_test(t, test_malformed_url);
    tcase_add_test(t, test_nonexistent_host);
    tcase_add_test(t, test_simple_html_conversion);
    tcase_add_test(t, test_links_conversion);
    tcase_add_test(t, test_lists_conversion);
    tcase_add_test(t, test_scripts_stripped);
    tcase_add_test(t, test_formatting_conversion);
    tcase_add_test(t, test_pagination_limit);
    tcase_add_test(t, test_pagination_offset);
    tcase_add_test(t, test_pagination_offset_beyond);
    tcase_add_test(t, test_title_extraction);
    tcase_add_test(t, test_all_headings);
    tcase_add_test(t, test_large_html);
    tcase_add_test(t, test_html_comments);
    tcase_add_test(t, test_file_not_found);
    tcase_add_test(t, test_large_json_input);
    tcase_add_test(t, test_http_404_error);
    tcase_add_test(t, test_http_500_error);
    tcase_add_test(t, test_unparseable_content);
    tcase_add_test(t, test_non_integer_offset);
    tcase_add_test(t, test_non_integer_limit);
    tcase_add_test(t, test_more_elements);
    tcase_add_test(t, test_edge_cases);
    tcase_add_test(t, test_pagination_offset_and_limit);
    tcase_add_test(t, test_style_only);
    tcase_add_test(t, test_very_large_json_input);

    suite_add_tcase(s, t);

    return s;
}

int main(void)
{
    int nf;
    Suite *s = web_fetch_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
