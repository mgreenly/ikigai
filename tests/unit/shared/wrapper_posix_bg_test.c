// Unit tests for background-process MOCKABLE wrappers in wrapper_posix.h
// Confirms each wrapper compiles and delegates to the underlying syscall.

#include <check.h>
#include <errno.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shared/wrapper_posix.h"

// setpgid_(0, 0) is a no-op: set current process to its own group
START_TEST(test_setpgid_self_noerror)
{
    int result = setpgid_(0, 0);
    ck_assert_int_eq(result, 0);
}
END_TEST

// prctl_ delegates to prctl — PR_GET_DUMPABLE returns dumpable state (0 or 1)
START_TEST(test_prctl_get_dumpable)
{
    int result = prctl_(PR_GET_DUMPABLE, 0, 0, 0, 0);
    ck_assert_int_ge(result, 0);
}
END_TEST

// pidfd_open_ delegates to pidfd_open syscall — getpid() is always valid
START_TEST(test_pidfd_open_current_pid)
{
    int fd = pidfd_open_(getpid(), 0);
    ck_assert_int_ge(fd, 0);
    close(fd);
}
END_TEST

// forkpty_ delegates to forkpty — verify child is forked and parent gets fd
START_TEST(test_forkpty_delegates)
{
    int master_fd = -1;
    pid_t pid = forkpty_(&master_fd, NULL, NULL, NULL);
    if (pid == 0) {
        // Child: exit immediately
        _exit(0);
    }
    // Parent
    ck_assert_int_ge(pid, 0);
    ck_assert_int_ge(master_fd, 0);
    close(master_fd);
    waitpid(pid, NULL, 0);
}
END_TEST

static Suite *wrapper_posix_bg_suite(void)
{
    Suite *s = suite_create("wrapper_posix bg wrappers");

    TCase *tc = tcase_create("Core");
    tcase_add_test(tc, test_setpgid_self_noerror);
    tcase_add_test(tc, test_prctl_get_dumpable);
    tcase_add_test(tc, test_pidfd_open_current_pid);
    tcase_add_test(tc, test_forkpty_delegates);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = wrapper_posix_bg_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/wrapper_posix_bg_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
