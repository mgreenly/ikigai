#include <check.h>
#include <talloc.h>
#include "../../src/error.h"

// Test that we can directly verify the OOM error is returned
// when talloc allocation fails
START_TEST (test_oom_error_properties)
{
  // Access the static OOM error directly (visible from error.h)

  // Verify its properties
  ck_assert_int_eq (ik_oom_error.code, IK_ERR_OOM);
  ck_assert_str_eq (ik_oom_error.msg, "Out of memory");
  ck_assert_str_eq (ik_oom_error.file, "<oom>");
  ck_assert_int_eq (ik_oom_error.line, 0);

  // Verify it's detected as static
  ck_assert (ik_error_is_static (&ik_oom_error));

  // Verify we can create a result from it
  ik_result_t res = ik_err ((ik_error_t *) & ik_oom_error);
  ck_assert (ik_is_err (&res));
  ck_assert_ptr_eq (res.err, &ik_oom_error);
}

END_TEST
// Test the behavior when memory is very constrained
START_TEST (test_constrained_memory)
{
  // Create a very small context
  TALLOC_CTX *ctx = talloc_new (NULL);

  // Allocate many small chunks to fragment memory
  for (int i = 0; i < 1000; i++)
    {
      talloc_size (ctx, 1024);
    }

  // Now try to create an error - should still work in normal conditions
  ik_result_t res = ERR (ctx, INVALID_ARG, "Test in constrained memory");

  ck_assert (ik_is_err (&res));

  // In normal conditions, this won't be the static OOM error
  // But the code path exists for when talloc_zero really fails
  if (!ik_error_is_static (res.err))
    {
      ck_assert_str_eq (res.err->msg, "Test in constrained memory");
    }

  talloc_free (ctx);
}

END_TEST
// Test that static OOM error can be safely used without freeing
START_TEST (test_static_oom_no_free_needed)
{
  // In a real OOM scenario, we'd get this error
  ik_result_t res = ik_err ((ik_error_t *) & ik_oom_error);

  // We can check it, print it, propagate it
  ck_assert (ik_is_err (&res));
  ck_assert (ik_error_is_static (res.err));

  // No talloc_free needed - it's static
  // This simulates the pattern: check error, exit gracefully
}

END_TEST
// Test suite
static Suite *
oom_suite (void)
{
  Suite *s;
  TCase *tc_core;

  s = suite_create ("OOM");
  tc_core = tcase_create ("Core");

  tcase_add_test (tc_core, test_oom_error_properties);
  tcase_add_test (tc_core, test_constrained_memory);
  tcase_add_test (tc_core, test_static_oom_no_free_needed);

  suite_add_tcase (s, tc_core);
  return s;
}

int
main (void)
{
  int number_failed;
  Suite *s;
  SRunner *sr;

  s = oom_suite ();
  sr = srunner_create (s);

  srunner_run_all (sr, CK_NORMAL);
  number_failed = srunner_ntests_failed (sr);
  srunner_free (sr);

  return (number_failed == 0) ? 0 : 1;
}
