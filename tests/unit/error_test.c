#define _GNU_SOURCE
#include <check.h>
#include <talloc.h>
#include "../../src/error.h"
#include "../test_utils.h"

// Helper function that returns success
static ik_result_t
helper_success (TALLOC_CTX *ctx)
{
  int *value = talloc (ctx, int);
  *value = 42;
  return OK (value);
}

// Helper function that returns error
static ik_result_t
helper_error (TALLOC_CTX *ctx)
{
  return ERR (ctx, INVALID_ARG, "Test error message");
}

// Helper function using CHECK macro
static ik_result_t
helper_propagate (TALLOC_CTX *ctx, bool should_fail)
{
  ik_result_t res;

  if (should_fail)
    {
      res = helper_error (ctx);
    }
  else
    {
      res = helper_success (ctx);
    }

  CHECK (res);

  // Should only reach here if res was OK
  return res;
}

// Test OK() construction
START_TEST (test_ok_construction)
{
  TALLOC_CTX *ctx = talloc_new (NULL);
  int *value = talloc (ctx, int);
  *value = 123;

  ik_result_t res = OK (value);

  ck_assert (ik_is_ok (&res));
  ck_assert (!ik_is_err (&res));
  ck_assert_ptr_eq (res.ok, value);
  ck_assert_int_eq (*(int *) res.ok, 123);

  talloc_free (ctx);
}

END_TEST
// Test ERR() construction
START_TEST (test_err_construction)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  ik_result_t res = ERR (ctx, INVALID_ARG, "Test error: %d", 42);

  ck_assert (ik_is_err (&res));
  ck_assert (!ik_is_ok (&res));
  ck_assert_ptr_nonnull (res.err);
  ck_assert_int_eq (res.err->code, IK_ERR_INVALID_ARG);
  ck_assert_str_eq (res.err->message, "Test error: 42");
  ck_assert_ptr_nonnull (res.err->file);

  talloc_free (ctx);
}

END_TEST
// Test error message extraction
START_TEST (test_error_message)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  // Test with NULL error
  const char *msg = ik_error_message (NULL);
  ck_assert_str_eq (msg, "Success");

  // Test with custom message
  ik_result_t res = ERR (ctx, INVALID_ARG, "Custom message");

  msg = ik_error_message (res.err);
  ck_assert_str_eq (msg, "Custom message");

  ik_error_code_t code = ik_error_code (res.err);
  ck_assert_int_eq (code, IK_ERR_INVALID_ARG);

  talloc_free (ctx);
}

END_TEST
// Test CHECK macro with success
START_TEST (test_try_success)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  ik_result_t res = helper_propagate (ctx, false);

  ck_assert (ik_is_ok (&res));
  ck_assert_int_eq (*(int *) res.ok, 42);

  talloc_free (ctx);
}

END_TEST
// Test CHECK macro with error propagation
START_TEST (test_try_error)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  ik_result_t res = helper_propagate (ctx, true);

  ck_assert (ik_is_err (&res));
  ck_assert_str_eq (res.err->message, "Test error message");

  talloc_free (ctx);
}

END_TEST
// Test talloc hierarchy - error freed with context
START_TEST (test_talloc_error_freed)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  ik_result_t res = helper_error (ctx);
  ck_assert (ik_is_err (&res));

  // Error should be child of ctx
  TALLOC_CTX *err_parent = talloc_parent (res.err);
  ck_assert_ptr_eq (err_parent, ctx);

  // Freeing ctx should free the error
  talloc_free (ctx);
  // No assertion here - if talloc is working, no leak
}

END_TEST
// Test talloc hierarchy - success value freed with context
START_TEST (test_talloc_ok_freed)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  ik_result_t res = helper_success (ctx);
  ck_assert (ik_is_ok (&res));

  // Value should be child of ctx
  TALLOC_CTX *val_parent = talloc_parent (res.ok);
  ck_assert_ptr_eq (val_parent, ctx);

  talloc_free (ctx);
}

END_TEST
// Test error formatting
START_TEST (test_error_fprintf)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  ik_result_t res = ERR (ctx, OUT_OF_RANGE, "Formatted error");

  // Capture output to memory buffer instead of stderr
  char buffer[256];
  FILE *memfile = fmemopen (buffer, sizeof (buffer), "w");
  ck_assert_ptr_nonnull (memfile);

  ik_error_fprintf (memfile, res.err);
  fclose (memfile);

  // Verify the output contains expected text
  // Format is: "Error: <message> [<file>:<line>]"
  ck_assert_ptr_nonnull (strstr (buffer, "Error:"));
  ck_assert_ptr_nonnull (strstr (buffer, "Formatted error"));
  ck_assert_ptr_nonnull (strstr (buffer, "error_test.c"));

  talloc_free (ctx);
}

END_TEST
// Test nested contexts
START_TEST (test_nested_contexts)
{
  TALLOC_CTX *root = talloc_new (NULL);
  TALLOC_CTX *child = talloc_new (root);

  ik_result_t res = ERR (child, INVALID_ARG, "Child error");

  ck_assert (ik_is_err (&res));
  ck_assert_ptr_eq (talloc_parent (res.err), child);

  // Freeing root should free child and error
  talloc_free (root);
}

END_TEST
// Test ik_check_null with talloc context
START_TEST (test_check_null)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  // Test NULL detection
  ik_result_t res = ik_check_null (ctx, NULL, "test_param");
  ck_assert (ik_is_err (&res));

  // Test non-NULL acceptance
  int value = 42;
  res = ik_check_null (ctx, &value, "test_param");
  ck_assert (ik_is_ok (&res));
  ck_assert_ptr_eq (res.ok, &value);

  talloc_free (ctx);
}

END_TEST
// Test ik_check_range with talloc context
START_TEST (test_check_range)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  // Test out of range (below)
  ik_result_t res = ik_check_range (ctx, 5, 10, 20, "test_val");
  ck_assert (ik_is_err (&res));

  // Test out of range (above)
  res = ik_check_range (ctx, 25, 10, 20, "test_val");
  ck_assert (ik_is_err (&res));

  // Test in range
  res = ik_check_range (ctx, 15, 10, 20, "test_val");
  ck_assert (ik_is_ok (&res));

  // Test edges
  res = ik_check_range (ctx, 10, 10, 20, "test_val");
  ck_assert (ik_is_ok (&res));
  res = ik_check_range (ctx, 20, 10, 20, "test_val");
  ck_assert (ik_is_ok (&res));

  talloc_free (ctx);
}

END_TEST
// Test error message with empty string (should fall back to error code string)
START_TEST (test_error_message_empty)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  // Create error with empty message string
  ik_result_t res = ERR (ctx, INVALID_ARG, "");

  ck_assert (ik_is_err (&res));
  // Empty message should fall back to ik_error_code_str
  const char *msg = ik_error_message (res.err);
  ck_assert_str_eq (msg, "Invalid argument");

  // Test with OOM error code too
  res = ERR (ctx, OOM, "");
  msg = ik_error_message (res.err);
  ck_assert_str_eq (msg, "Out of memory");

  // Test with OUT_OF_RANGE
  res = ERR (ctx, OUT_OF_RANGE, "");
  msg = ik_error_message (res.err);
  ck_assert_str_eq (msg, "Out of range");

  // Manually create an error with an invalid error code to test default case
  ik_error_t *bad_err = talloc_zero (ctx, ik_error_t);
  bad_err->code = (ik_error_code_t) 999;
  bad_err->message[0] = '\0';  // Empty message to trigger ik_error_code_str
  msg = ik_error_message (bad_err);
  ck_assert_str_eq (msg, "Unknown error");

  talloc_free (ctx);
}

END_TEST
// Test error fprintf with NULL error
START_TEST (test_error_fprintf_null)
{
  char buffer[256];
  FILE *memfile = fmemopen (buffer, sizeof (buffer), "w");
  ck_assert_ptr_nonnull (memfile);

  ik_error_fprintf (memfile, NULL);
  fclose (memfile);

  ck_assert_ptr_nonnull (strstr (buffer, "Success"));
}

END_TEST
// Test error fprintf with NULL file field in error
START_TEST (test_error_fprintf_null_file)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  // Manually create an error with NULL file field
  ik_error_t *err = talloc_zero (ctx, ik_error_t);
  err->code = IK_ERR_INVALID_ARG;
  err->file = NULL;  // NULL file field
  err->line = 42;
  snprintf (err->message, sizeof (err->message), "Test error");

  char buffer[256];
  FILE *memfile = fmemopen (buffer, sizeof (buffer), "w");
  ck_assert_ptr_nonnull (memfile);

  ik_error_fprintf (memfile, err);
  fclose (memfile);

  // Should print "unknown" for NULL file
  ck_assert_ptr_nonnull (strstr (buffer, "unknown"));
  ck_assert_ptr_nonnull (strstr (buffer, "Test error"));

  talloc_free (ctx);
}

END_TEST
// Test ik_error_code with NULL and non-NULL
START_TEST (test_error_code_null)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  // Test NULL case
  ck_assert_int_eq (ik_error_code (NULL), IK_OK);

  // Test non-NULL case
  ik_result_t res = ERR (ctx, INVALID_ARG, "test");
  ck_assert_int_eq (ik_error_code (res.err), IK_ERR_INVALID_ARG);

  talloc_free (ctx);
}

END_TEST
// Test error code to string conversion
START_TEST (test_error_code_str)
{
  ck_assert_str_eq (ik_error_code_str (IK_OK), "OK");
  ck_assert_str_eq (ik_error_code_str (IK_ERR_OOM), "Out of memory");
  ck_assert_str_eq (ik_error_code_str (IK_ERR_INVALID_ARG), "Invalid argument");
  ck_assert_str_eq (ik_error_code_str (IK_ERR_OUT_OF_RANGE), "Out of range");
  ck_assert_str_eq (ik_error_code_str (999), "Unknown error");
}

END_TEST
// Test static OOM error detection
START_TEST (test_oom_error_is_static)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  // Create a normal error
  ik_result_t res = ERR (ctx, INVALID_ARG, "Normal error");
  ck_assert (!ik_error_is_static (res.err));

  // The OOM error is static (we can't easily trigger real OOM in a test,
  // but we can verify the detection function works)
  extern const ik_error_t ik_oom_error;
  ck_assert (ik_error_is_static (&ik_oom_error));
  ck_assert_int_eq (ik_oom_error.code, IK_ERR_OOM);
  ck_assert_str_eq (ik_oom_error.message, "Out of memory");

  talloc_free (ctx);
}

END_TEST
// Test actual OOM behavior by injecting allocation failure
START_TEST (test_oom_on_error_allocation)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  // Tell test allocator to fail the next allocation
  oom_test_fail_next_alloc ();

  // Try to create an error - should get static OOM error
  ik_result_t res = ERR (ctx, INVALID_ARG, "This should trigger OOM");

  ck_assert (ik_is_err (&res));
  ck_assert (ik_error_is_static (res.err));
  ck_assert_int_eq (res.err->code, IK_ERR_OOM);
  ck_assert_str_eq (res.err->message, "Out of memory");

  // Reset allocator state
  oom_test_reset ();

  // Next allocation should work normally
  res = ERR (ctx, INVALID_ARG, "This should work");
  ck_assert (ik_is_err (&res));
  ck_assert (!ik_error_is_static (res.err));
  ck_assert_str_eq (res.err->message, "This should work");

  talloc_free (ctx);
}

END_TEST
// Test OOM after multiple successful allocations
START_TEST (test_oom_after_multiple_errors)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  // Configure allocator to fail after 3 successful allocations
  oom_test_fail_after_n_calls (3);

  ik_result_t res;

  // First two should succeed
  res = ERR (ctx, INVALID_ARG, "Error 1");
  ck_assert (!ik_error_is_static (res.err));

  res = ERR (ctx, INVALID_ARG, "Error 2");
  ck_assert (!ik_error_is_static (res.err));

  // Third and subsequent should fail (OOM)
  res = ERR (ctx, INVALID_ARG, "Error 3");
  ck_assert (ik_error_is_static (res.err));
  ck_assert_int_eq (res.err->code, IK_ERR_OOM);

  res = ERR (ctx, INVALID_ARG, "Error 4");
  ck_assert (ik_error_is_static (res.err));

  // Reset for other tests
  oom_test_reset ();

  talloc_free (ctx);
}

END_TEST
// Test that call count tracking works
START_TEST (test_oom_call_count)
{
  TALLOC_CTX *ctx = talloc_new (NULL);

  oom_test_reset ();
  int initial_count = oom_test_get_call_count ();

  ERR (ctx, INVALID_ARG, "Error 1");
  ERR (ctx, INVALID_ARG, "Error 2");
  ERR (ctx, INVALID_ARG, "Error 3");

  int final_count = oom_test_get_call_count ();
  ck_assert_int_eq (final_count - initial_count, 3);

  talloc_free (ctx);
}

END_TEST
// Test suite setup
  Suite * error_suite (void)
{
  Suite *s;
  TCase *tc_core;

  s = suite_create ("Error");
  tc_core = tcase_create ("Core");

  tcase_add_test (tc_core, test_ok_construction);
  tcase_add_test (tc_core, test_err_construction);
  tcase_add_test (tc_core, test_error_message);
  tcase_add_test (tc_core, test_try_success);
  tcase_add_test (tc_core, test_try_error);
  tcase_add_test (tc_core, test_talloc_error_freed);
  tcase_add_test (tc_core, test_talloc_ok_freed);
  tcase_add_test (tc_core, test_error_fprintf);
  tcase_add_test (tc_core, test_nested_contexts);
  tcase_add_test (tc_core, test_check_null);
  tcase_add_test (tc_core, test_check_range);
  tcase_add_test (tc_core, test_error_message_empty);
  tcase_add_test (tc_core, test_error_fprintf_null);
  tcase_add_test (tc_core, test_error_fprintf_null_file);
  tcase_add_test (tc_core, test_error_code_null);
  tcase_add_test (tc_core, test_error_code_str);
  tcase_add_test (tc_core, test_oom_error_is_static);
  tcase_add_test (tc_core, test_oom_on_error_allocation);
  tcase_add_test (tc_core, test_oom_after_multiple_errors);
  tcase_add_test (tc_core, test_oom_call_count);

  suite_add_tcase (s, tc_core);
  return s;
}

int
main (void)
{
  int number_failed;
  Suite *s;
  SRunner *sr;

  s = error_suite ();
  sr = srunner_create (s);

  srunner_run_all (sr, CK_NORMAL);
  number_failed = srunner_ntests_failed (sr);
  srunner_free (sr);

  return (number_failed == 0) ? 0 : 1;
}
