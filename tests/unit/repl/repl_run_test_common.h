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
#include "../../../src/input_buffer/core.h"
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

#endif // IK_REPL_RUN_TEST_COMMON_H
