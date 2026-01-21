/**
 * @file tsan_verify_test.c
 * @brief Deliberate data race to verify TSan detection - DELETE AFTER VERIFICATION
 */

#include <check.h>
#include <pthread.h>
#include <stdlib.h>

static int shared_counter = 0;

static void *increment_thread(void *arg) {
    (void)arg;
    for (int i = 0; i < 1000; i++) {
        shared_counter++;  // Data race: unsynchronized access
    }
    return NULL;
}

START_TEST(test_deliberate_race) {
    pthread_t t1, t2;

    pthread_create(&t1, NULL, increment_thread, NULL);
    pthread_create(&t2, NULL, increment_thread, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    // Test passes (assertions succeed) but TSan should report the race
    ck_assert_int_gt(shared_counter, 0);
}
END_TEST

static Suite *tsan_verify_suite(void) {
    Suite *s = suite_create("tsan_verify");
    TCase *tc = tcase_create("core");
    tcase_add_test(tc, test_deliberate_race);
    suite_add_tcase(s, tc);
    return s;
}

int main(void) {
    Suite *s = tsan_verify_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
