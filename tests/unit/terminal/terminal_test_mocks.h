// Terminal test mocks - shared between test files
#ifndef TERMINAL_TEST_MOCKS_H
#define TERMINAL_TEST_MOCKS_H

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

// Mock control state
static int mock_open_fail = 0;
static int mock_tcgetattr_fail = 0;
static int mock_tcsetattr_fail = 0;
static int mock_tcflush_fail = 0;
static int mock_write_fail = 0;
static int mock_write_fail_on_call = 0;  // Fail on specific write call number
static int mock_ioctl_fail = 0;
static int mock_select_return = 0;        // 0 = timeout, >0 = ready
static int mock_read_fail = 0;
static int mock_close_count = 0;
static int mock_write_count = 0;
static int mock_tcsetattr_count = 0;
static int mock_tcflush_count = 0;
static const char *mock_read_response = NULL;  // Custom response for read mock

// Buffer to capture write calls
#define MOCK_WRITE_BUFFER_SIZE 1024
static char mock_write_buffer[MOCK_WRITE_BUFFER_SIZE];
static size_t mock_write_buffer_pos = 0;

// Mock function prototypes
int posix_open_(const char *pathname, int flags);
int posix_close_(int fd);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
int posix_ioctl_(int fd, unsigned long request, void *argp);
ssize_t posix_write_(int fd, const void *buf, size_t count);
int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
ssize_t posix_read_(int fd, void *buf, size_t count);

// Mock implementations
int posix_open_(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;
    if (mock_open_fail) {
        return -1;
    }
    return 42; // Mock fd
}

int posix_close_(int fd)
{
    (void)fd;
    mock_close_count++;
    return 0;
}

int posix_tcgetattr_(int fd, struct termios *termios_p)
{
    (void)fd;
    if (mock_tcgetattr_fail) {
        return -1;
    }
    // Fill with dummy data
    memset(termios_p, 0, sizeof(*termios_p));
    return 0;
}

int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd;
    (void)optional_actions;
    (void)termios_p;
    mock_tcsetattr_count++;
    if (mock_tcsetattr_fail) {
        return -1;
    }
    return 0;
}

int posix_tcflush_(int fd, int queue_selector)
{
    (void)fd;
    (void)queue_selector;
    mock_tcflush_count++;
    if (mock_tcflush_fail) {
        return -1;
    }
    return 0;
}

int posix_ioctl_(int fd, unsigned long request, void *argp)
{
    (void)fd;
    (void)request;
    if (mock_ioctl_fail) {
        return -1;
    }
    // Fill winsize with dummy data
    struct winsize *ws = (struct winsize *)argp;
    ws->ws_row = 24;
    ws->ws_col = 80;
    return 0;
}

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    mock_write_count++;
    if (mock_write_fail) {
        return -1;
    }
    if (mock_write_fail_on_call > 0 && mock_write_count == mock_write_fail_on_call) {
        return -1;
    }
    // Capture written data to buffer
    if (mock_write_buffer_pos + count < MOCK_WRITE_BUFFER_SIZE) {
        memcpy(mock_write_buffer + mock_write_buffer_pos, buf, count);
        mock_write_buffer_pos += count;
    }
    return (ssize_t)count;
}

int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    (void)nfds;
    (void)readfds;
    (void)writefds;
    (void)exceptfds;
    (void)timeout;
    return mock_select_return;
}

ssize_t posix_read_(int fd, void *buf, size_t count)
{
    (void)fd;
    (void)count;
    if (mock_read_fail) {
        return -1;
    }
    // Return a dummy CSI u response if select indicated ready
    if (mock_select_return > 0) {
        const char *response = mock_read_response ? mock_read_response : "\x1b[?0u";
        size_t len = strlen(response);
        if (len > count) len = count;
        memcpy(buf, response, len);
        return (ssize_t)len;
    }
    return 0;
}

// Reset mock state before each test
static void reset_mocks(void)
{
    mock_open_fail = 0;
    mock_tcgetattr_fail = 0;
    mock_tcsetattr_fail = 0;
    mock_tcflush_fail = 0;
    mock_write_fail = 0;
    mock_write_fail_on_call = 0;
    mock_ioctl_fail = 0;
    mock_select_return = 0;
    mock_read_fail = 0;
    mock_close_count = 0;
    mock_write_count = 0;
    mock_tcsetattr_count = 0;
    mock_tcflush_count = 0;
    memset(mock_write_buffer, 0, MOCK_WRITE_BUFFER_SIZE);
    mock_write_buffer_pos = 0;
    mock_read_response = NULL;
}

#endif // TERMINAL_TEST_MOCKS_H
