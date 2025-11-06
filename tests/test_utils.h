#ifndef IK_TEST_UTILS_H
#define IK_TEST_UTILS_H

// Test utilities for ikigai test suite

// ========== OOM Allocator Control ==========
// These functions control the behavior of ik_talloc_zero_for_error
// in test builds, allowing simulation of out-of-memory conditions

// Make the next error allocation fail (return NULL)
void oom_test_fail_next_alloc (void);

// Fail error allocations after N successful calls
// Set to 0 to disable this behavior
void oom_test_fail_after_n_calls (int n);

// Reset OOM test state to normal operation
void oom_test_reset (void);

// Get the number of times ik_talloc_zero_for_error has been called
int oom_test_get_call_count (void);

#endif // IK_TEST_UTILS_H
