#include <check.h>
#include <stdlib.h>
#include <string.h>

#include "apps/ikigai/ansi.h"

// Test: colors enabled by default (no env vars set)
START_TEST(test_ansi_colors_enabled_default) {
    // Clear environment variables to ensure clean state
    unsetenv("NO_COLOR");
    unsetenv("TERM");

    // Initialize ANSI state
    ik_ansi_init();

    // Colors should be enabled by default
    ck_assert(ik_ansi_colors_enabled());
}
END_TEST
// Test: colors disabled when NO_COLOR is set to any value
START_TEST(test_ansi_colors_disabled_no_color_set) {
    // Set NO_COLOR to "1"
    setenv("NO_COLOR", "1", 1);
    unsetenv("TERM");

    // Initialize ANSI state
    ik_ansi_init();

    // Colors should be disabled
    ck_assert(!ik_ansi_colors_enabled());

    // Clean up
    unsetenv("NO_COLOR");
}

END_TEST
// Test: colors disabled when NO_COLOR is set to empty string
START_TEST(test_ansi_colors_disabled_no_color_empty) {
    // Set NO_COLOR to empty string
    setenv("NO_COLOR", "", 1);
    unsetenv("TERM");

    // Initialize ANSI state
    ik_ansi_init();

    // Colors should be disabled (any value, even empty, means disabled)
    ck_assert(!ik_ansi_colors_enabled());

    // Clean up
    unsetenv("NO_COLOR");
}

END_TEST
// Test: colors disabled when TERM=dumb
START_TEST(test_ansi_colors_disabled_term_dumb) {
    // Set TERM to "dumb"
    unsetenv("NO_COLOR");
    setenv("TERM", "dumb", 1);

    // Initialize ANSI state
    ik_ansi_init();

    // Colors should be disabled
    ck_assert(!ik_ansi_colors_enabled());

    // Clean up
    unsetenv("TERM");
}

END_TEST
// Test: colors enabled when TERM=xterm-256color
START_TEST(test_ansi_colors_enabled_term_xterm) {
    // Set TERM to "xterm-256color"
    unsetenv("NO_COLOR");
    setenv("TERM", "xterm-256color", 1);

    // Initialize ANSI state
    ik_ansi_init();

    // Colors should be enabled
    ck_assert(ik_ansi_colors_enabled());

    // Clean up
    unsetenv("TERM");
}

END_TEST
// Test: NO_COLOR takes precedence over TERM
START_TEST(test_ansi_no_color_precedence) {
    // Set both NO_COLOR and TERM to a color-capable terminal
    setenv("NO_COLOR", "1", 1);
    setenv("TERM", "xterm-256color", 1);

    // Initialize ANSI state
    ik_ansi_init();

    // Colors should be disabled (NO_COLOR takes precedence)
    ck_assert(!ik_ansi_colors_enabled());

    // Clean up
    unsetenv("NO_COLOR");
    unsetenv("TERM");
}

END_TEST

static Suite *ansi_config_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("ANSI Config");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_ansi_colors_enabled_default);
    tcase_add_test(tc_core, test_ansi_colors_disabled_no_color_set);
    tcase_add_test(tc_core, test_ansi_colors_disabled_no_color_empty);
    tcase_add_test(tc_core, test_ansi_colors_disabled_term_dumb);
    tcase_add_test(tc_core, test_ansi_colors_enabled_term_xterm);
    tcase_add_test(tc_core, test_ansi_no_color_precedence);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = ansi_config_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/ansi/config_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
