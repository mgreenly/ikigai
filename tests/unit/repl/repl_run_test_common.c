/**
 * @file repl_run_test_common.c
 * @brief Shared mock implementations for REPL event loop tests
 */

#include "repl_run_test_common.h"

// Mock read tracking
const char *mock_input = NULL;
size_t mock_input_pos = 0;

// Mock write tracking
bool mock_write_should_fail = false;
int32_t mock_write_fail_after = -1;  // Fail after N successful writes (-1 = never fail)
int32_t mock_write_count = 0;

// Mock read wrapper for testing
ssize_t ik_read_wrapper(int fd, void *buf, size_t count)
{
    (void)fd;

    if (!mock_input || mock_input_pos >= strlen(mock_input)) {
        return 0;  // EOF
    }

    size_t to_copy = 1;  // Read one byte at a time (simulating real terminal input)
    if (to_copy > count) {
        to_copy = count;
    }

    memcpy(buf, mock_input + mock_input_pos, to_copy);
    mock_input_pos += to_copy;

    return (ssize_t)to_copy;
}

// Mock write wrapper (suppress output during tests)
ssize_t ik_write_wrapper(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;

    if (mock_write_should_fail) {
        return -1;  // Simulate write error
    }

    if (mock_write_fail_after >= 0 && mock_write_count >= mock_write_fail_after) {
        return -1;  // Fail after N writes
    }

    mock_write_count++;
    return (ssize_t)count;
}
