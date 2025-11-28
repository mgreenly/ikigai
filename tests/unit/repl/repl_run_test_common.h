/**
 * @file repl_run_test_common.h
 * @brief Shared mock functions for REPL event loop tests
 */

#ifndef IK_REPL_RUN_TEST_COMMON_H
#define IK_REPL_RUN_TEST_COMMON_H

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include "../../../src/repl.h"
#include "../../../src/input_buffer.h"
#include "../../../src/input.h"
#include "../../../src/terminal.h"
#include "../../../src/render.h"
#include "../../test_utils.h"

// Mock read tracking
extern const char *mock_input;
extern size_t mock_input_pos;

// Mock write tracking
extern bool mock_write_should_fail;
extern int32_t mock_write_fail_after;
extern int32_t mock_write_count;

// Mock wrapper functions (implemented once, shared across test files)
ssize_t ik_read_wrapper(int fd, void *buf, size_t count);
ssize_t ik_write_wrapper(int fd, const void *buf, size_t count);

#endif // IK_REPL_RUN_TEST_COMMON_H
