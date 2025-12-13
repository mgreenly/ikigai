/**
 * @file repl_autoscroll_test.c
 * @brief Unit tests for auto-scroll to bottom on input buffer actions (Bug #6 fix)
 *
 * When the user scrolls up to view scrollback and then performs any input buffer
 * editing action (typing, deleting, navigation), the viewport should auto-scroll
 * to the bottom to show the cursor and input buffer.
 */

#include <check.h>
#include "../../../src/agent.h"
#include <talloc.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/input.h"
#include "../../test_utils.h"

// Forward declarations
int posix_open_(const char *pathname, int flags);
int posix_ioctl_(int fd, unsigned long request, void *argp);
int posix_close_(int fd);
int posix_tcgetattr_(int fd, struct termios *p);
int posix_tcsetattr_(int fd, int o, const struct termios *p);
int posix_tcflush_(int fd, int q);
ssize_t posix_write_(int fd, const void *b, size_t c);
ssize_t posix_read_(int fd, void *b, size_t c);

// Mock wrapper function implementations
int posix_open_(const char *pathname, int flags)
{
    (void)pathname; (void)flags;
    return 99;
}

int posix_ioctl_(int fd, unsigned long request, void *argp)
{
    (void)fd; (void)request;
    struct winsize *ws = (struct winsize *)argp;
    ws->ws_row = 24;
    ws->ws_col = 80;
    return 0;
}

int posix_close_(int fd)
{
    (void)fd; return 0;
}

int posix_tcgetattr_(int fd, struct termios *p)
{
    (void)fd; (void)p; return 0;
}

int posix_tcsetattr_(int fd, int o, const struct termios *p)
{
    (void)fd; (void)o; (void)p; return 0;
}

int posix_tcflush_(int fd, int q)
{
    (void)fd; (void)q; return 0;
}

ssize_t posix_write_(int fd, const void *b, size_t c)
{
    (void)fd; (void)b; return (ssize_t)c;
}

ssize_t posix_read_(int fd, void *b, size_t c)
{
    (void)fd; (void)b; (void)c; return 0;
}

// Helper: Create REPL with scrollback and set viewport offset
static void setup_repl_scrolled(void *ctx, ik_repl_ctx_t **repl_out, size_t offset)
{
    ik_cfg_t *cfg = ik_test_create_config(ctx);

    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    res_t res = ik_shared_ctx_init(ctx, cfg, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, repl_out);
    ck_assert(is_ok(&res));
    for (int32_t i = 0; i < 50; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "scrollback line %d", i);
        res = ik_scrollback_append_line((*repl_out)->current->scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    (*repl_out)->current->viewport_offset = offset;
}

// Helper: Test action auto-scrolls to bottom
static void test_action_autoscrolls(ik_input_action_t *action, bool should_autoscroll)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;
    setup_repl_scrolled(ctx, &repl, 10);

    res_t res = ik_repl_process_action(repl, action);
    ck_assert(is_ok(&res));

    if (should_autoscroll) {
        ck_assert_uint_eq(repl->current->viewport_offset, 0);
    } else {
        ck_assert_uint_ne(repl->current->viewport_offset, 0);
    }
    talloc_free(ctx);
}

START_TEST(test_autoscroll_on_char_insert) {
    ik_input_action_t action = { .type = IK_INPUT_CHAR, .codepoint = 'x' };
    test_action_autoscrolls(&action, true);
}
END_TEST START_TEST(test_autoscroll_on_insert_newline)
{
    ik_input_action_t action = { .type = IK_INPUT_INSERT_NEWLINE };
    test_action_autoscrolls(&action, true);
}

END_TEST START_TEST(test_autoscroll_on_backspace)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;
    setup_repl_scrolled(ctx, &repl, 10);

    // Add text first so backspace has something to delete
    ik_input_action_t insert = { .type = IK_INPUT_CHAR, .codepoint = 'x' };
    res_t res = ik_repl_process_action(repl, &insert);
    ck_assert(is_ok(&res));
    repl->current->viewport_offset = 20;  // Reset to scrolled position

    ik_input_action_t action = { .type = IK_INPUT_BACKSPACE };
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->viewport_offset, 0);
    talloc_free(ctx);
}

END_TEST START_TEST(test_autoscroll_on_delete)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;
    setup_repl_scrolled(ctx, &repl, 10);

    // Add text and move cursor left
    ik_input_action_t insert = { .type = IK_INPUT_CHAR, .codepoint = 'x' };
    res_t res = ik_repl_process_action(repl, &insert);
    ck_assert(is_ok(&res));
    ik_input_action_t left = { .type = IK_INPUT_ARROW_LEFT };
    res = ik_repl_process_action(repl, &left);
    ck_assert(is_ok(&res));
    repl->current->viewport_offset = 25;  // Reset to scrolled position

    ik_input_action_t action = { .type = IK_INPUT_DELETE };
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->viewport_offset, 0);
    talloc_free(ctx);
}

END_TEST START_TEST(test_autoscroll_on_cursor_navigation)
{
    // Arrow left/right should autoscroll (not affected by scroll detector)
    ik_input_action_t left_right_actions[] = {
        { .type = IK_INPUT_ARROW_LEFT },
        { .type = IK_INPUT_ARROW_RIGHT }
    };
    for (size_t i = 0; i < sizeof(left_right_actions) / sizeof(left_right_actions[0]); i++) {
        test_action_autoscrolls(&left_right_actions[i], true);
    }

    // Arrow up/down should NOT autoscroll - they scroll the viewport instead.
    // When viewport is already scrolled, up/down continue scrolling rather
    // than jumping to bottom. This allows the user to navigate scrollback.
    // Note: These go through the scroll detector which buffers them, so we
    // won't see the scroll action immediately - offset stays unchanged.
    ik_input_action_t up_down_actions[] = {
        { .type = IK_INPUT_ARROW_UP },
        { .type = IK_INPUT_ARROW_DOWN }
    };
    for (size_t i = 0; i < sizeof(up_down_actions) / sizeof(up_down_actions[0]); i++) {
        test_action_autoscrolls(&up_down_actions[i], false);
    }
}

END_TEST START_TEST(test_autoscroll_on_ctrl_shortcuts)
{
    ik_input_action_t actions[] = {
        { .type = IK_INPUT_CTRL_A },  // Jump to line start
        { .type = IK_INPUT_CTRL_E },  // Jump to line end
        { .type = IK_INPUT_CTRL_K },  // Kill to line end
        { .type = IK_INPUT_CTRL_U },  // Kill line
        { .type = IK_INPUT_CTRL_W }   // Delete word backward
    };
    for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); i++) {
        test_action_autoscrolls(&actions[i], true);
    }
}

END_TEST START_TEST(test_no_autoscroll_on_page_up)
{
    ik_input_action_t action = { .type = IK_INPUT_PAGE_UP };
    test_action_autoscrolls(&action, false);
}

END_TEST START_TEST(test_no_autoscroll_on_page_down)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;
    setup_repl_scrolled(ctx, &repl, 20);

    ik_input_action_t action = { .type = IK_INPUT_PAGE_DOWN };
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));
    ck_assert_uint_lt(repl->current->viewport_offset, 20);  // Scrolled down but not to 0
    talloc_free(ctx);
}

END_TEST

static Suite *repl_autoscroll_suite(void)
{
    Suite *s = suite_create("repl_autoscroll");
    TCase *tc = tcase_create("autoscroll");
    tcase_set_timeout(tc, 30);
    tcase_add_test(tc, test_autoscroll_on_char_insert);
    tcase_add_test(tc, test_autoscroll_on_insert_newline);
    tcase_add_test(tc, test_autoscroll_on_backspace);
    tcase_add_test(tc, test_autoscroll_on_delete);
    tcase_add_test(tc, test_autoscroll_on_cursor_navigation);
    tcase_add_test(tc, test_autoscroll_on_ctrl_shortcuts);
    tcase_add_test(tc, test_no_autoscroll_on_page_up);
    tcase_add_test(tc, test_no_autoscroll_on_page_down);
    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = repl_autoscroll_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
