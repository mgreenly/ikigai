/**
 * @file repl_run_helper.h
 * @brief Shared mock functions for REPL event loop tests
 */

#ifndef IK_REPL_RUN_COMMON_H
#define IK_REPL_RUN_COMMON_H

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include "../../../src/repl.h"
#include "../../../src/repl_event_handlers.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/input.h"
#include "../../../src/terminal.h"
#include "../../../src/render.h"
#include "../../../src/providers/provider.h"
#include "../../test_utils_helper.h"

// Mock read tracking
extern const char *mock_input;
extern size_t mock_input_pos;

// Mock write tracking
extern bool mock_write_should_fail;
extern int32_t mock_write_fail_after;
extern int32_t mock_write_count;

// Mock select tracking
extern int mock_select_return_value;  // -999 to use default behavior, otherwise use this value
extern int mock_select_call_count;   // Number of times select has been called
extern int mock_select_return_on_call;  // Return mock value only on this call number (0-based, -1 for all calls)
extern int mock_select_errno;  // errno to set when returning error (-1 for don't set)
extern long captured_select_timeout_ms;  // Captured timeout value in milliseconds (-999 = not set)

// Mock read error tracking
extern int mock_read_fail_count;  // Number of times read should fail (-1 = never fail, 0 = fail once, 1 = fail twice, etc.)
extern int mock_read_errno;  // errno to set when read fails

// Mock curl_multi_fdset error tracking
extern bool mock_curl_multi_fdset_should_fail;  // Set to true to make curl_multi_fdset_ return error

// Mock curl_multi_perform error tracking
extern bool mock_curl_multi_perform_should_fail;  // Set to true to make curl_multi_perform_ return error

// Mock curl_multi_timeout error tracking
extern bool mock_curl_multi_timeout_should_fail;  // Set to true to make curl_multi_timeout_ return error
extern long mock_curl_timeout_value;  // Timeout value to return (-1 = no timeout, >= 0 = timeout in ms)

// Mock wrapper functions (implemented once, shared across test files)
ssize_t posix_read_(int fd, void *buf, size_t count);
ssize_t posix_write_(int fd, const void *buf, size_t count);
int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

// Curl mock prototypes
#include <curl/curl.h>
#include <sys/select.h>
CURLM *curl_multi_init_(void);
CURLMcode curl_multi_cleanup_(CURLM *multi);
CURLMcode curl_multi_fdset_(CURLM *multi, fd_set *read_fd_set, fd_set *write_fd_set, fd_set *exc_fd_set, int *max_fd);
CURLMcode curl_multi_timeout_(CURLM *multi, long *timeout);
CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles);
CURLMsg *curl_multi_info_read_(CURLM *multi, int *msgs_in_queue);

// Helper to initialize multi handle for REPL tests
void init_repl_multi_handle(ik_repl_ctx_t *repl);

#endif // IK_REPL_RUN_COMMON_H
