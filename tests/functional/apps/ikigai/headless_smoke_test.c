#include "tests/helpers/headless_instance_helper.h"
#include "tests/helpers/framebuffer_inspect_helper.h"
#include "tests/helpers/test_utils_helper.h"
#include "apps/ikigai/control_socket_client.h"
#include "shared/error.h"
#include "tests/test_constants.h"

#include <check.h>
#include <inttypes.h>
#include <talloc.h>
#include <unistd.h>

static ik_headless_instance_t *instance;
static TALLOC_CTX *test_ctx;

static const char *DB_NAME = "ikigai_test_headless_smoke";

static void suite_setup(void)
{
    instance = ik_headless_start(DB_NAME);
    ck_assert_msg(instance != NULL, "Failed to start headless ikigai");
}

static void suite_teardown(void)
{
    ik_headless_stop(instance);
    instance = NULL;
}

static void test_setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert(test_ctx != NULL);
}

static void test_teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

START_TEST(test_headless_starts_and_socket_exists)
{
    ck_assert(instance != NULL);
    ck_assert(instance->pid > 0);
    ck_assert(instance->socket_path != NULL);

    // Verify socket file exists
    ck_assert_msg(access(instance->socket_path, F_OK) == 0,
                  "Socket file does not exist: %s", instance->socket_path);
}
END_TEST

START_TEST(test_read_framebuffer_returns_valid_json)
{
    int fd = -1;
    res_t result = ik_ctl_connect(test_ctx, instance->socket_path, &fd);
    ck_assert_msg(is_ok(&result), "Failed to connect to control socket");
    ck_assert(fd >= 0);

    char *response = NULL;
    result = ik_ctl_read_framebuffer(test_ctx, fd, &response);
    ck_assert_msg(is_ok(&result), "Failed to read framebuffer");
    ck_assert(response != NULL);

    ck_assert_msg(ik_fb_is_valid(response),
                  "Framebuffer response is not valid JSON");

    ik_ctl_disconnect(fd);
}
END_TEST

START_TEST(test_send_keys_and_framebuffer_changes)
{
    int fd = -1;
    res_t result = ik_ctl_connect(test_ctx, instance->socket_path, &fd);
    ck_assert_msg(is_ok(&result), "Failed to connect to control socket");

    // Read initial framebuffer
    char *fb_before = NULL;
    result = ik_ctl_read_framebuffer(test_ctx, fd, &fb_before);
    ck_assert_msg(is_ok(&result), "Failed to read initial framebuffer");
    ck_assert(fb_before != NULL);
    ik_ctl_disconnect(fd);

    // Send some keys via a new connection
    fd = -1;
    result = ik_ctl_connect(test_ctx, instance->socket_path, &fd);
    ck_assert_msg(is_ok(&result), "Failed to reconnect");

    result = ik_ctl_send_keys(test_ctx, fd, "hello");
    ck_assert_msg(is_ok(&result), "Failed to send keys");
    ik_ctl_disconnect(fd);

    // Wait briefly for the event loop to process the keys
    usleep(200000);

    // Read framebuffer again
    fd = -1;
    result = ik_ctl_connect(test_ctx, instance->socket_path, &fd);
    ck_assert_msg(is_ok(&result), "Failed to reconnect for second read");

    char *fb_after = NULL;
    result = ik_ctl_read_framebuffer(test_ctx, fd, &fb_after);
    ck_assert_msg(is_ok(&result), "Failed to read framebuffer after keys");
    ck_assert(fb_after != NULL);

    // Verify framebuffer changed
    ck_assert_msg(ik_fb_is_valid(fb_after),
                  "Post-keys framebuffer is not valid JSON");

    // The framebuffer should contain "hello" somewhere
    ck_assert_msg(ik_fb_contains_text(fb_after, "hello"),
                  "Framebuffer does not contain 'hello' after send_keys");

    ik_ctl_disconnect(fd);
}
END_TEST

static Suite *headless_smoke_suite(void)
{
    Suite *s = suite_create("Headless Smoke");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);
    tcase_add_test(tc_core, test_headless_starts_and_socket_exists);
    tcase_add_test(tc_core, test_read_framebuffer_returns_valid_json);
    tcase_add_test(tc_core, test_send_keys_and_framebuffer_changes);
    suite_add_tcase(s, tc_core);

    return s;
}

int32_t main(void)
{
    Suite *s = headless_smoke_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/functional/apps/ikigai/headless_smoke_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
