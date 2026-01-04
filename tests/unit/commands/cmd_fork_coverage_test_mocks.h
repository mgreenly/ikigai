/**
 * @file cmd_fork_coverage_test_mocks.h
 * @brief Mock functions for cmd_fork_coverage_test
 */

#ifndef CMD_FORK_COVERAGE_TEST_MOCKS_H
#define CMD_FORK_COVERAGE_TEST_MOCKS_H

#include <stdbool.h>

/**
 * @brief Control whether mock provider should fail
 * @param should_fail true to make mock fail, false for success
 */
void ik_test_mock_set_provider_failure(bool should_fail);

/**
 * @brief Control whether mock request building should fail
 * @param should_fail true to make mock fail, false for success
 */
void ik_test_mock_set_request_failure(bool should_fail);

/**
 * @brief Control whether mock stream should fail
 * @param should_fail true to make mock fail, false for success
 */
void ik_test_mock_set_stream_failure(bool should_fail);

/**
 * @brief Reset all mock failure flags to false
 */
void ik_test_mock_reset_flags(void);

#endif // CMD_FORK_COVERAGE_TEST_MOCKS_H