/**
 * @file repl_run_common.c
 * @brief Shared mock implementations for REPL event loop tests
 */

#include "repl_run_common.h"
#include <curl/curl.h>
#include <sys/select.h>
#include <errno.h>

// Mock read tracking
const char *mock_input = NULL;
size_t mock_input_pos = 0;

// Mock write tracking
bool mock_write_should_fail = false;
int32_t mock_write_fail_after = -1;  // Fail after N successful writes (-1 = never fail)
int32_t mock_write_count = 0;

// Mock select tracking
int mock_select_return_value = -999;  // -999 means use default behavior, otherwise use this value
int mock_select_call_count = 0;      // Number of times select has been called
int mock_select_return_on_call = -1;  // Return mock value only on this call number (-1 for all calls)
int mock_select_errno = -1;  // errno to set when returning error (-1 for don't set)
long captured_select_timeout_ms = -999;  // Captured timeout value in milliseconds

// Mock read error tracking
int mock_read_fail_count = -1;  // -1 = never fail, >= 0 = fail this many times
int mock_read_errno = 0;  // errno to set when read fails

// Mock curl_multi_fdset error tracking
bool mock_curl_multi_fdset_should_fail = false;

// Mock curl_multi_perform error tracking
bool mock_curl_multi_perform_should_fail = false;

// Mock curl_multi_timeout error tracking
bool mock_curl_multi_timeout_should_fail = false;
long mock_curl_timeout_value = -1;  // Default: no timeout

// Mock read wrapper for testing
ssize_t posix_read_(int fd, void *buf, size_t count)
{
    (void)fd;

    // Check if we should fail this time
    if (mock_read_fail_count >= 0) {
        mock_read_fail_count--;
        errno = mock_read_errno;
        return -1;
    }

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

// Mock select wrapper - can return timeout or indicate stdin is ready
int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    (void)nfds;
    (void)writefds;
    (void)exceptfds;

    // Capture timeout value for testing (only on first call if not yet captured)
    if (captured_select_timeout_ms == -999) {
        if (timeout != NULL) {
            captured_select_timeout_ms = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
        } else {
            captured_select_timeout_ms = -1;
        }
    }

    // Track number of calls
    int current_call = mock_select_call_count++;

    // If mock_select_return_value is set (not -999), check if we should use it
    if (mock_select_return_value != -999) {
        // If mock_select_return_on_call is -1, always use mock value
        // Otherwise, only use it on the specific call number
        if (mock_select_return_on_call == -1 || current_call == mock_select_return_on_call) {
            // Set errno if specified (for error cases)
            if (mock_select_errno != -1) {
                errno = mock_select_errno;
            }
            return mock_select_return_value;
        }
    }

    // Default behavior: indicate that stdin (fd 0) is ready for reading
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

    if (mock_curl_multi_fdset_should_fail) {
        return CURLM_BAD_HANDLE;
    }

    return CURLM_OK;
}

CURLMcode curl_multi_timeout_(CURLM *multi, long *timeout)
{
    (void)multi;

    if (mock_curl_multi_timeout_should_fail) {
        return CURLM_BAD_HANDLE;
    }

    *timeout = mock_curl_timeout_value;
    return CURLM_OK;
}

CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles)
{
    (void)multi;
    *running_handles = 0;

    if (mock_curl_multi_perform_should_fail) {
        return CURLM_BAD_HANDLE;
    }

    return CURLM_OK;
}

CURLMsg *curl_multi_info_read_(CURLM *multi, int *msgs_in_queue)
{
    (void)multi;
    *msgs_in_queue = 0;
    return NULL;
}

// Helper to initialize agents array for REPL tests
// Note: Provider multi-handles are now internal to each provider,
// not stored in agent or REPL context.
void init_repl_multi_handle(ik_repl_ctx_t *repl)
{
    // Initialize agents array with current agent
    repl->agent_count = 1;
    repl->agent_capacity = 4;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, (unsigned int)repl->agent_capacity);
    repl->agents[0] = repl->current;
}
