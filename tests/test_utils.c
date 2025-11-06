#include "test_utils.h"
#include <talloc.h>
#include <string.h>

// ========== OOM Allocator Implementation ==========

// Test control state
static struct
{
  int should_fail_next;
  int fail_after_n_calls;
  int call_count;
} oom_state = { 0 };

// Strong symbol - overrides the weak symbol in src/error.c
// This version allows controlled failure injection for testing
void *
ik_talloc_zero_for_error (TALLOC_CTX *ctx, size_t size)
{
  oom_state.call_count++;

  // Check if we should fail the next allocation
  if (oom_state.should_fail_next)
    {
      oom_state.should_fail_next = 0;
      return NULL;
    }

  // Check if we should fail after N calls
  if (oom_state.fail_after_n_calls > 0 && oom_state.call_count >= oom_state.fail_after_n_calls)
    {
      return NULL;
    }

  // Normal allocation
  return talloc_zero_size (ctx, size);
}

// ========== OOM Control API ==========

void
oom_test_fail_next_alloc (void)
{
  oom_state.should_fail_next = 1;
}

void
oom_test_fail_after_n_calls (int n)
{
  oom_state.fail_after_n_calls = n;
  oom_state.call_count = 0;
}

void
oom_test_reset (void)
{
  memset (&oom_state, 0, sizeof (oom_state));
}

int
oom_test_get_call_count (void)
{
  return oom_state.call_count;
}
