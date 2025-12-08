// Unit tests for JSONL logger timestamp formatting

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <regex.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../../../src/logger.h"

// Helper: setup temp directory and init logger
static char test_dir[256];
static char log_file_path[512];

static void setup_logger(void)
{
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_timestamp_test_%d", getpid());
    mkdir(test_dir, 0755);
    ik_log_init(test_dir);
    snprintf(log_file_path, sizeof(log_file_path), "%s/.ikigai/logs/current.log", test_dir);
}

static void teardown_logger(void)
{
    ik_log_shutdown();
    unlink(log_file_path);
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    rmdir(logs_dir);
    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    rmdir(ikigai_dir);
    rmdir(test_dir);
}

static char *read_log_file(void)
{
    FILE *f = fopen(log_file_path, "r");
    if (!f) return NULL;

    static char buffer[4096];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[len] = '\0';
    fclose(f);
    return buffer;
}

// Test: timestamp format matches ISO 8601 pattern with milliseconds and timezone
START_TEST(test_jsonl_timestamp_iso8601_format)
{
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "msg", "test");

    ik_log_debug_json(doc);

    char *buffer = read_log_file();
    ck_assert_ptr_nonnull(buffer);

    // Parse JSON to extract timestamp
    yyjson_doc *parsed = yyjson_read(buffer, strlen(buffer), 0);
    ck_assert_ptr_ne(parsed, NULL);

    yyjson_val *timestamp_val = yyjson_obj_get(yyjson_doc_get_root(parsed), "timestamp");
    ck_assert_ptr_ne(timestamp_val, NULL);

    const char *timestamp = yyjson_get_str(timestamp_val);
    ck_assert_ptr_ne(timestamp, NULL);

    // Verify format: YYYY-MM-DDTHH:MM:SS.mmm±HH:MM
    regex_t regex;
    int reti = regcomp(&regex, "^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}[+-][0-9]{2}:[0-9]{2}$", REG_EXTENDED);
    ck_assert_int_eq(reti, 0);

    reti = regexec(&regex, timestamp, 0, NULL, 0);
    ck_assert_int_eq(reti, 0);

    regfree(&regex);
    yyjson_doc_free(parsed);
    teardown_logger();
}
END_TEST

// Test: timestamp includes exactly 3 millisecond digits
START_TEST(test_jsonl_timestamp_milliseconds)
{
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "msg", "test");

    ik_log_debug_json(doc);

    char *buffer = read_log_file();
    ck_assert_ptr_nonnull(buffer);

    // Parse JSON to extract timestamp
    yyjson_doc *parsed = yyjson_read(buffer, strlen(buffer), 0);
    ck_assert_ptr_ne(parsed, NULL);

    yyjson_val *timestamp_val = yyjson_obj_get(yyjson_doc_get_root(parsed), "timestamp");
    const char *timestamp = yyjson_get_str(timestamp_val);

    // Find the dot and verify 3 digits after it before ± sign
    const char *dot = strchr(timestamp, '.');
    ck_assert_ptr_ne(dot, NULL);

    const char *plus_or_minus = strchr(dot, '+');
    if (plus_or_minus == NULL)
        plus_or_minus = strchr(dot, '-');
    ck_assert_ptr_ne(plus_or_minus, NULL);

    // Should be exactly 4 characters: dot + 3 digits
    ck_assert_int_eq(plus_or_minus - dot, 4);

    yyjson_doc_free(parsed);
    teardown_logger();
}
END_TEST

// Test: timestamp includes timezone offset ±HH:MM
START_TEST(test_jsonl_timestamp_timezone_offset)
{
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "msg", "test");

    ik_log_debug_json(doc);

    char *buffer = read_log_file();
    ck_assert_ptr_nonnull(buffer);

    // Parse JSON to extract timestamp
    yyjson_doc *parsed = yyjson_read(buffer, strlen(buffer), 0);
    yyjson_val *timestamp_val = yyjson_obj_get(yyjson_doc_get_root(parsed), "timestamp");
    const char *timestamp = yyjson_get_str(timestamp_val);

    // Should end with ±HH:MM
    size_t len = strlen(timestamp);
    ck_assert(len >= 6);

    // Last 6 characters should be ±HH:MM
    const char *offset = timestamp + len - 6;
    ck_assert(offset[0] == '+' || offset[0] == '-');
    ck_assert(offset[1] >= '0' && offset[1] <= '2');
    ck_assert(offset[2] >= '0' && offset[2] <= '9');
    ck_assert(offset[3] == ':');
    ck_assert(offset[4] >= '0' && offset[4] <= '5');
    ck_assert(offset[5] >= '0' && offset[5] <= '9');

    yyjson_doc_free(parsed);
    teardown_logger();
}
END_TEST

// Test: timestamp is approximately current time (within 1 second tolerance)
START_TEST(test_jsonl_timestamp_current_time)
{
    setup_logger();

    time_t before = time(NULL);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "msg", "test");

    ik_log_debug_json(doc);

    time_t after = time(NULL);

    char *buffer = read_log_file();
    ck_assert_ptr_nonnull(buffer);

    // Parse JSON to extract timestamp
    yyjson_doc *parsed = yyjson_read(buffer, strlen(buffer), 0);
    yyjson_val *timestamp_val = yyjson_obj_get(yyjson_doc_get_root(parsed), "timestamp");
    const char *timestamp = yyjson_get_str(timestamp_val);

    // Parse timestamp: YYYY-MM-DDTHH:MM:SS.mmm±HH:MM
    int year, month, day, hour, min, sec;
    sscanf(timestamp, "%d-%d-%d", &year, &month, &day);

    struct tm tm_check = {0};
    tm_check.tm_year = year - 1900;
    tm_check.tm_mon = month - 1;
    tm_check.tm_mday = day;
    sscanf(timestamp + 11, "%d:%d:%d", &hour, &min, &sec);
    tm_check.tm_hour = hour;
    tm_check.tm_min = min;
    tm_check.tm_sec = sec;

    time_t ts_logged = mktime(&tm_check);

    // Should be within 1 second of the capture time
    ck_assert(ts_logged >= before - 1);
    ck_assert(ts_logged <= after + 1);

    yyjson_doc_free(parsed);
    teardown_logger();
}
END_TEST

static Suite *jsonl_timestamp_suite(void)
{
    Suite *s = suite_create("JSONL Timestamp");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_jsonl_timestamp_iso8601_format);
    tcase_add_test(tc_core, test_jsonl_timestamp_milliseconds);
    tcase_add_test(tc_core, test_jsonl_timestamp_timezone_offset);
    tcase_add_test(tc_core, test_jsonl_timestamp_current_time);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = jsonl_timestamp_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
