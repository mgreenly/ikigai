/**
 * @file repl_run_test_common.c
 * @brief Shared mock implementations for REPL event loop tests
 */

#include "repl_run_test_common.h"
#include "../../../src/openai/client_multi.h"
#include <curl/curl.h>
#include <sys/select.h>

// Mock read tracking
const char *mock_input = NULL;
size_t mock_input_pos = 0;

// Mock write tracking
bool mock_write_should_fail = false;
int32_t mock_write_fail_after = -1;  // Fail after N successful writes (-1 = never fail)
int32_t mock_write_count = 0;

// Mock read wrapper for testing
ssize_t posix_read_(int fd, void *buf, size_t count)
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
ssize_t posix_write_(int fd, const void *buf, size_t count)
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

// Mock select wrapper - always indicates stdin is ready
int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    (void)nfds;
    (void)writefds;
    (void)exceptfds;
    (void)timeout;

    // Always indicate that stdin (fd 0) is ready for reading
    // This allows the test to proceed without blocking
    return FD_ISSET(0, readfds) ? 1 : 0;
}

// Mock curl functions for REPL run tests
static int mock_curl_storage;

CURLM *curl_multi_init_(void)
{
    return (CURLM *)&mock_curl_storage;
}

CURLMcode curl_multi_cleanup_(CURLM *multi)
{
    (void)multi;
    return CURLM_OK;
}

CURLMcode curl_multi_fdset_(CURLM *multi, fd_set *read_fd_set,
                            fd_set *write_fd_set, fd_set *exc_fd_set,
                            int *max_fd)
{
    (void)multi;
    (void)read_fd_set;
    (void)write_fd_set;
    (void)exc_fd_set;
    *max_fd = -1;
    return CURLM_OK;
}

CURLMcode curl_multi_timeout_(CURLM *multi, long *timeout)
{
    (void)multi;
    *timeout = -1;
    return CURLM_OK;
}

CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles)
{
    (void)multi;
    *running_handles = 0;
    return CURLM_OK;
}

CURLMsg *curl_multi_info_read_(CURLM *multi, int *msgs_in_queue)
{
    (void)multi;
    *msgs_in_queue = 0;
    return NULL;
}

// Helper to initialize multi handle for REPL tests
void init_repl_multi_handle(ik_repl_ctx_t *repl)
{
    res_t res = ik_openai_multi_create(repl);
    if (is_err(&res)) {
        // If we can't create the multi handle, set it to NULL
        // This shouldn't happen in tests, but handle it gracefully
        repl->multi = NULL;
    } else {
        repl->multi = (struct ik_openai_multi *)res.ok;
    }
}
