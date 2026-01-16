/**
 * @file cmd_fork_coverage_test_mocks.h
 * @brief Mock functions for cmd_fork_coverage_test
 */

#ifndef CMD_FORK_COVERAGE_TEST_MOCKS_H
#define CMD_FORK_COVERAGE_TEST_MOCKS_H

#include <stdbool.h>
#include "../../../src/error.h"
#include "../../../src/providers/provider.h"
#include "../../../src/providers/provider_vtable.h"
#include "../../../src/providers/request.h"

/**
 * @brief Mock implementation of start_stream for testing
 * @param ctx Provider context
 * @param req Request to stream
 * @param stream_cb Callback for stream events
 * @param stream_ctx Context for stream callback
 * @param completion_cb Callback for completion
 * @param completion_ctx Context for completion callback
 * @return OK(NULL) on success, ERR(...) on failure
 */
res_t mock_start_stream(void *ctx,
                        const ik_request_t *req,
                        ik_stream_cb_t stream_cb,
                        void *stream_ctx,
                        ik_provider_completion_cb_t completion_cb,
                        void *completion_ctx);

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
