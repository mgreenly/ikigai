#include "../../test_constants.h"

#include "tools/list/list.h"

#include <check.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    // Set test environment variables
    setenv("IKIGAI_AGENT_ID", "test-agent", 1);
    setenv("IKIGAI_STATE_DIR", "/tmp/ikigai-list-test", 1);

    // Clean up any existing test data
    system("rm -rf /tmp/ikigai-list-test");
}

static void teardown(void)
{
    // Clean up test data
    system("rm -rf /tmp/ikigai-list-test");

    talloc_free(test_ctx);

    // Clear environment variables
    unsetenv("IKIGAI_AGENT_ID");
    unsetenv("IKIGAI_STATE_DIR");
}

START_TEST(test_list_rpush)
{
    // Capture stdout
    int32_t stdout_fd = dup(STDOUT_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDOUT_FILENO);

    int32_t result = list_execute(test_ctx, "rpush", "First item");

    // Restore stdout
    fflush(stdout);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdout_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"ok\":true"));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":1"));
}

END_TEST

START_TEST(test_list_lpush)
{
    // First add an item with rpush
    list_execute(test_ctx, "rpush", "First item");

    // Capture stdout
    int32_t stdout_fd = dup(STDOUT_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDOUT_FILENO);

    int32_t result = list_execute(test_ctx, "lpush", "Urgent item");

    // Restore stdout
    fflush(stdout);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdout_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"ok\":true"));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":2"));
}

END_TEST

START_TEST(test_list_count)
{
    // Add some items
    list_execute(test_ctx, "rpush", "Item 1");
    list_execute(test_ctx, "rpush", "Item 2");
    list_execute(test_ctx, "rpush", "Item 3");

    // Capture stdout
    int32_t stdout_fd = dup(STDOUT_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDOUT_FILENO);

    int32_t result = list_execute(test_ctx, "count", NULL);

    // Restore stdout
    fflush(stdout);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdout_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"ok\":true"));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":3"));
}

END_TEST

START_TEST(test_list_count_empty)
{
    // Capture stdout
    int32_t stdout_fd = dup(STDOUT_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDOUT_FILENO);

    int32_t result = list_execute(test_ctx, "count", NULL);

    // Restore stdout
    fflush(stdout);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdout_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"ok\":true"));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":0"));
}

END_TEST

START_TEST(test_list_list)
{
    // Add some items
    list_execute(test_ctx, "rpush", "First");
    list_execute(test_ctx, "rpush", "Second");

    // Capture stdout
    int32_t stdout_fd = dup(STDOUT_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDOUT_FILENO);

    int32_t result = list_execute(test_ctx, "list", NULL);

    // Restore stdout
    fflush(stdout);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdout_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"ok\":true"));
    ck_assert_ptr_nonnull(strstr(output, "\"items\""));
    ck_assert_ptr_nonnull(strstr(output, "First"));
    ck_assert_ptr_nonnull(strstr(output, "Second"));
}

END_TEST

START_TEST(test_list_lpop)
{
    // Add some items
    list_execute(test_ctx, "rpush", "First");
    list_execute(test_ctx, "rpush", "Second");

    // Capture stdout
    int32_t stdout_fd = dup(STDOUT_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDOUT_FILENO);

    int32_t result = list_execute(test_ctx, "lpop", NULL);

    // Restore stdout
    fflush(stdout);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdout_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"ok\":true"));
    ck_assert_ptr_nonnull(strstr(output, "\"item\":\"First\""));
}

END_TEST

START_TEST(test_list_rpop)
{
    // Add some items
    list_execute(test_ctx, "rpush", "First");
    list_execute(test_ctx, "rpush", "Second");

    // Capture stdout
    int32_t stdout_fd = dup(STDOUT_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDOUT_FILENO);

    int32_t result = list_execute(test_ctx, "rpop", NULL);

    // Restore stdout
    fflush(stdout);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdout_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"ok\":true"));
    ck_assert_ptr_nonnull(strstr(output, "\"item\":\"Second\""));
}

END_TEST

START_TEST(test_list_lpop_empty)
{
    // Capture stdout
    int32_t stdout_fd = dup(STDOUT_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDOUT_FILENO);

    int32_t result = list_execute(test_ctx, "lpop", NULL);

    // Restore stdout
    fflush(stdout);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdout_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"ok\":false"));
}

END_TEST

START_TEST(test_list_lpeek)
{
    // Add some items
    list_execute(test_ctx, "rpush", "First");
    list_execute(test_ctx, "rpush", "Second");

    // Capture stdout
    int32_t stdout_fd = dup(STDOUT_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDOUT_FILENO);

    int32_t result = list_execute(test_ctx, "lpeek", NULL);

    // Restore stdout
    fflush(stdout);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdout_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"ok\":true"));
    ck_assert_ptr_nonnull(strstr(output, "\"item\":\"First\""));

    // Verify list still has both items
    stdout_fd = dup(STDOUT_FILENO);
    capture = tmpfile();
    dup2(fileno(capture), STDOUT_FILENO);

    list_execute(test_ctx, "count", NULL);

    fflush(stdout);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdout_fd);

    rewind(capture);
    fgets(output, sizeof(output), capture);
    fclose(capture);

    ck_assert_ptr_nonnull(strstr(output, "\"count\":2"));
}

END_TEST

START_TEST(test_list_rpeek)
{
    // Add some items
    list_execute(test_ctx, "rpush", "First");
    list_execute(test_ctx, "rpush", "Second");

    // Capture stdout
    int32_t stdout_fd = dup(STDOUT_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDOUT_FILENO);

    int32_t result = list_execute(test_ctx, "rpeek", NULL);

    // Restore stdout
    fflush(stdout);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdout_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"ok\":true"));
    ck_assert_ptr_nonnull(strstr(output, "\"item\":\"Second\""));
}

END_TEST

START_TEST(test_list_rpeek_empty)
{
    // Capture stdout
    int32_t stdout_fd = dup(STDOUT_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDOUT_FILENO);

    int32_t result = list_execute(test_ctx, "rpeek", NULL);

    // Restore stdout
    fflush(stdout);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdout_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"ok\":false"));
}

END_TEST

START_TEST(test_list_missing_agent_id)
{
    // Clear IKIGAI_AGENT_ID
    unsetenv("IKIGAI_AGENT_ID");

    // Capture stderr
    int32_t stderr_fd = dup(STDERR_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDERR_FILENO);

    int32_t result = list_execute(test_ctx, "count", NULL);

    // Restore stderr
    fflush(stderr);
    dup2(stderr_fd, STDERR_FILENO);
    close(stderr_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 1);
    ck_assert_ptr_nonnull(strstr(output, "IKIGAI_AGENT_ID"));

    // Restore for teardown
    setenv("IKIGAI_AGENT_ID", "test-agent", 1);
}

END_TEST

START_TEST(test_list_missing_state_dir)
{
    // Clear IKIGAI_STATE_DIR
    unsetenv("IKIGAI_STATE_DIR");

    // Capture stderr
    int32_t stderr_fd = dup(STDERR_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDERR_FILENO);

    int32_t result = list_execute(test_ctx, "count", NULL);

    // Restore stderr
    fflush(stderr);
    dup2(stderr_fd, STDERR_FILENO);
    close(stderr_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 1);
    ck_assert_ptr_nonnull(strstr(output, "IKIGAI_STATE_DIR"));

    // Restore for teardown
    setenv("IKIGAI_STATE_DIR", "/tmp/ikigai-list-test", 1);
}

END_TEST

START_TEST(test_list_unknown_operation)
{
    // Capture stderr
    int32_t stderr_fd = dup(STDERR_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDERR_FILENO);

    int32_t result = list_execute(test_ctx, "invalid", NULL);

    // Restore stderr
    fflush(stderr);
    dup2(stderr_fd, STDERR_FILENO);
    close(stderr_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 1);
    ck_assert_ptr_nonnull(strstr(output, "unknown operation"));
}

END_TEST

START_TEST(test_list_push_missing_item)
{
    // Capture stderr
    int32_t stderr_fd = dup(STDERR_FILENO);
    FILE *capture = tmpfile();
    dup2(fileno(capture), STDERR_FILENO);

    int32_t result = list_execute(test_ctx, "lpush", NULL);

    // Restore stderr
    fflush(stderr);
    dup2(stderr_fd, STDERR_FILENO);
    close(stderr_fd);

    // Read captured output
    rewind(capture);
    char output[1024];
    memset(output, 0, sizeof(output));
    size_t bytes_read = fread(output, 1, sizeof(output) - 1, capture);
    output[bytes_read] = '\0';
    fclose(capture);

    ck_assert_int_eq(result, 1);
    ck_assert_ptr_nonnull(strstr(output, "item required"));
}

END_TEST

static Suite *list_suite(void)
{
    Suite *s = suite_create("List");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_list_rpush);
    tcase_add_test(tc, test_list_lpush);
    tcase_add_test(tc, test_list_count);
    tcase_add_test(tc, test_list_count_empty);
    tcase_add_test(tc, test_list_list);
    tcase_add_test(tc, test_list_lpop);
    tcase_add_test(tc, test_list_rpop);
    tcase_add_test(tc, test_list_lpop_empty);
    tcase_add_test(tc, test_list_lpeek);
    tcase_add_test(tc, test_list_rpeek);
    tcase_add_test(tc, test_list_rpeek_empty);
    tcase_add_test(tc, test_list_missing_agent_id);
    tcase_add_test(tc, test_list_missing_state_dir);
    tcase_add_test(tc, test_list_unknown_operation);
    tcase_add_test(tc, test_list_push_missing_item);

    suite_add_tcase(s, tc);
    return s;
}

int32_t main(void)
{
    Suite *s = list_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/list/list_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
