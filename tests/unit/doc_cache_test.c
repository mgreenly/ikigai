/**
 * @file doc_cache_test.c
 * @brief Unit tests for document cache
 */

#include "../../src/doc_cache.h"
#include "../../src/error.h"
#include "../../src/file_utils.h"
#include "../../src/paths.h"
#include "../test_utils_helper.h"

#include <check.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

static Suite *doc_cache_suite(void);

static void *ctx;
static ik_doc_cache_t *cache;
static ik_paths_t *paths;
static char *test_file_path;

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    test_paths_setup_env();
    res_t paths_res = ik_paths_init(ctx, &paths);
    ck_assert(is_ok(&paths_res));
    ck_assert_ptr_nonnull(paths);

    cache = ik_doc_cache_create(ctx, paths);
    ck_assert_ptr_nonnull(cache);

    test_file_path = talloc_strdup(ctx, "/tmp/doc_cache_test_XXXXXX");
    int32_t fd = mkstemp(test_file_path);
    ck_assert_int_ge(fd, 0);

    const char *test_content = "Test document content\n";
    ssize_t written = write(fd, test_content, strlen(test_content));
    ck_assert_int_eq(written, (ssize_t)strlen(test_content));
    close(fd);
}

static void teardown(void)
{
    if (test_file_path) {
        unlink(test_file_path);
    }
    talloc_free(ctx);
}

START_TEST(test_cache_get_loads_file) {
    char *content = NULL;
    res_t res = ik_doc_cache_get(cache, test_file_path, &content);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(content);
    ck_assert(strstr(content, "Test document content") != NULL);
}
END_TEST

START_TEST(test_cache_get_cache_hit) {
    char *content1 = NULL;
    res_t res = ik_doc_cache_get(cache, test_file_path, &content1);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(content1);

    char *content2 = NULL;
    res = ik_doc_cache_get(cache, test_file_path, &content2);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(content2);

    ck_assert_ptr_eq(content1, content2);
}
END_TEST

START_TEST(test_cache_get_missing_file) {
    char *content = NULL;
    res_t res = ik_doc_cache_get(cache, "/nonexistent/file.txt", &content);
    ck_assert(is_err(&res));
}
END_TEST

START_TEST(test_cache_invalidate_specific_path) {
    char *content1 = NULL;
    res_t res = ik_doc_cache_get(cache, test_file_path, &content1);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(content1);
    ck_assert(strstr(content1, "Test document content") != NULL);

    ik_doc_cache_invalidate(cache, test_file_path);

    int32_t fd = open(test_file_path, O_WRONLY | O_TRUNC);
    ck_assert_int_ge(fd, 0);
    write(fd, "Modified content\n", 17);
    close(fd);

    char *content2 = NULL;
    res = ik_doc_cache_get(cache, test_file_path, &content2);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(content2);
    ck_assert(strstr(content2, "Modified content") != NULL);
}
END_TEST

START_TEST(test_cache_invalidate_nonexistent_path) {
    ik_doc_cache_invalidate(cache, "/nonexistent/path.txt");
}
END_TEST

START_TEST(test_cache_clear) {
    char *content1 = NULL;
    res_t res = ik_doc_cache_get(cache, test_file_path, &content1);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(content1);
    ck_assert(strstr(content1, "Test document content") != NULL);

    ik_doc_cache_clear(cache);

    int32_t fd = open(test_file_path, O_WRONLY | O_TRUNC);
    ck_assert_int_ge(fd, 0);
    write(fd, "Cleared content\n", 16);
    close(fd);

    char *content2 = NULL;
    res = ik_doc_cache_get(cache, test_file_path, &content2);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(content2);
    ck_assert(strstr(content2, "Cleared content") != NULL);
}
END_TEST

START_TEST(test_cache_multiple_documents) {
    char *test_file_path2 = talloc_strdup(ctx, "/tmp/doc_cache_test2_XXXXXX");
    int32_t fd = mkstemp(test_file_path2);
    ck_assert_int_ge(fd, 0);
    const char *test_content2 = "Second document\n";
    ssize_t written = write(fd, test_content2, strlen(test_content2));
    ck_assert_int_eq(written, (ssize_t)strlen(test_content2));
    close(fd);

    char *content1 = NULL;
    res_t res = ik_doc_cache_get(cache, test_file_path, &content1);
    ck_assert(is_ok(&res));
    ck_assert(strstr(content1, "Test document content") != NULL);

    char *content2 = NULL;
    res = ik_doc_cache_get(cache, test_file_path2, &content2);
    ck_assert(is_ok(&res));
    ck_assert(strstr(content2, "Second document") != NULL);

    ck_assert_ptr_ne(content1, content2);

    unlink(test_file_path2);
    talloc_free(test_file_path2);
}
END_TEST

START_TEST(test_cache_expand_capacity) {
    char *file_paths[10];
    for (int32_t i = 0; i < 10; i++) {
        file_paths[i] = talloc_asprintf(ctx, "/tmp/doc_cache_test_%d_XXXXXX", i);
        int32_t fd = mkstemp(file_paths[i]);
        ck_assert_int_ge(fd, 0);
        char *content = talloc_asprintf(ctx, "Document %d\n", i);
        ssize_t written = write(fd, content, strlen(content));
        ck_assert_int_eq(written, (ssize_t)strlen(content));
        close(fd);
        talloc_free(content);

        char *cached_content = NULL;
        res_t res = ik_doc_cache_get(cache, file_paths[i], &cached_content);
        ck_assert(is_ok(&res));
        ck_assert_ptr_nonnull(cached_content);
    }

    for (int32_t i = 0; i < 10; i++) {
        unlink(file_paths[i]);
        talloc_free(file_paths[i]);
    }
}
END_TEST

START_TEST(test_cache_invalidate_middle_entry) {
    char *test_file_path2 = talloc_strdup(ctx, "/tmp/doc_cache_test2_XXXXXX");
    int32_t fd2 = mkstemp(test_file_path2);
    ck_assert_int_ge(fd2, 0);
    write(fd2, "Second\n", 7);
    close(fd2);

    char *test_file_path3 = talloc_strdup(ctx, "/tmp/doc_cache_test3_XXXXXX");
    int32_t fd3 = mkstemp(test_file_path3);
    ck_assert_int_ge(fd3, 0);
    write(fd3, "Third\n", 6);
    close(fd3);

    char *content1 = NULL;
    char *content2 = NULL;
    char *content3 = NULL;

    res_t res = ik_doc_cache_get(cache, test_file_path, &content1);
    ck_assert(is_ok(&res));
    res = ik_doc_cache_get(cache, test_file_path2, &content2);
    ck_assert(is_ok(&res));
    ck_assert(strstr(content2, "Second") != NULL);
    res = ik_doc_cache_get(cache, test_file_path3, &content3);
    ck_assert(is_ok(&res));

    ik_doc_cache_invalidate(cache, test_file_path2);

    fd2 = open(test_file_path2, O_WRONLY | O_TRUNC);
    ck_assert_int_ge(fd2, 0);
    write(fd2, "Modified\n", 9);
    close(fd2);

    char *content2_after = NULL;
    res = ik_doc_cache_get(cache, test_file_path2, &content2_after);
    ck_assert(is_ok(&res));
    ck_assert(strstr(content2_after, "Modified") != NULL);

    unlink(test_file_path2);
    unlink(test_file_path3);
    talloc_free(test_file_path2);
    talloc_free(test_file_path3);
}
END_TEST

static Suite *doc_cache_suite(void)
{
    Suite *s = suite_create("doc_cache");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_cache_get_loads_file);
    tcase_add_test(tc_core, test_cache_get_cache_hit);
    tcase_add_test(tc_core, test_cache_get_missing_file);
    tcase_add_test(tc_core, test_cache_invalidate_specific_path);
    tcase_add_test(tc_core, test_cache_invalidate_nonexistent_path);
    tcase_add_test(tc_core, test_cache_clear);
    tcase_add_test(tc_core, test_cache_multiple_documents);
    tcase_add_test(tc_core, test_cache_expand_capacity);
    tcase_add_test(tc_core, test_cache_invalidate_middle_entry);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = doc_cache_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/doc_cache_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
