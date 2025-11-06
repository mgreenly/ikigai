#include <check.h>
#include <stdlib.h>
#include "../../src/foo.h"

START_TEST (test_add_positive_numbers)
{
  ck_assert_int_eq (add (2, 3), 5);
  ck_assert_int_eq (add (10, 20), 30);
}

END_TEST
START_TEST (test_add_negative_numbers)
{
  ck_assert_int_eq (add (-5, -3), -8);
  ck_assert_int_eq (add (-10, 10), 0);
}

END_TEST
START_TEST (test_add_zero)
{
  ck_assert_int_eq (add (0, 0), 0);
  ck_assert_int_eq (add (5, 0), 5);
  ck_assert_int_eq (add (0, 5), 5);
}

END_TEST Suite *
foo_suite (void)
{
  Suite *s;
  TCase *tc_core;

  s = suite_create ("Foo");
  tc_core = tcase_create ("Core");

  tcase_add_test (tc_core, test_add_positive_numbers);
  tcase_add_test (tc_core, test_add_negative_numbers);
  tcase_add_test (tc_core, test_add_zero);
  suite_add_tcase (s, tc_core);

  return s;
}

int
main (void)
{
  int number_failed;
  Suite *s;
  SRunner *sr;

  s = foo_suite ();
  sr = srunner_create (s);

  srunner_run_all (sr, CK_NORMAL);
  number_failed = srunner_ntests_failed (sr);
  srunner_free (sr);

  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
