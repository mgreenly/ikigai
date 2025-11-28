/**
 * @file debug_pipe.c
 * @brief Debug output pipe system implementation
 */

#include "debug_pipe.h"

#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

// Forward declarations
static int debug_pipe_destructor(ik_debug_pipe_t *pipe);

res_t ik_debug_pipe_create(void *parent, const char *prefix)
{
    // Create pipe
    int pipefd[2];
    if (posix_pipe_(pipefd) == -1) {
        return ERR(parent, IO, "pipe() failed: %s", strerror(errno));
    }

    int read_fd = pipefd[0];
    int write_fd = pipefd[1];

    // Set read end to non-blocking
    int flags = posix_fcntl_(read_fd, F_GETFL, 0);
    if (flags == -1) {
        posix_close_(read_fd);
        posix_close_(write_fd);
        return ERR(parent, IO, "fcntl(F_GETFL) failed: %s", strerror(errno));
    }

    if (posix_fcntl_(read_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        posix_close_(read_fd);
        posix_close_(write_fd);
        return ERR(parent, IO, "fcntl(F_SETFL) failed: %s", strerror(errno));
    }

    // Open write end as FILE*
    FILE *write_end = posix_fdopen_(write_fd, "w");
    if (write_end == NULL) {
        posix_close_(read_fd);
        posix_close_(write_fd);
        return ERR(parent, IO, "fdopen() failed: %s", strerror(errno));
    }

    // Allocate debug_pipe structure
    ik_debug_pipe_t *pipe = talloc_zero_(parent, sizeof(ik_debug_pipe_t));
    if (pipe == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    pipe->write_end = write_end;
    pipe->read_fd = read_fd;

    // Copy prefix if provided
    if (prefix != NULL) {
        pipe->prefix = talloc_strdup(pipe, prefix);
        if (pipe->prefix == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        pipe->prefix = NULL;
    }

    // Allocate initial line buffer (1KB)
    pipe->buffer_capacity = 1024;
    pipe->buffer_pos = 0;
    pipe->line_buffer = talloc_array_(pipe, sizeof(char), pipe->buffer_capacity);
    if (pipe->line_buffer == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Set up talloc destructor to clean up pipe resources
    talloc_set_destructor(pipe, debug_pipe_destructor);

    return OK(pipe);
}

// Destructor to clean up pipe resources
static int debug_pipe_destructor(ik_debug_pipe_t *pipe)
{
    if (pipe->write_end != NULL) {
        fclose(pipe->write_end);  // This also closes the underlying fd
        pipe->write_end = NULL;
    }
    // Defensive check: read_fd is always >= 0 when pipe is created successfully
    // FALSE branch is impossible without corrupting pipe state
    if (pipe->read_fd >= 0) {  // LCOV_EXCL_BR_LINE
        posix_close_(pipe->read_fd);
        pipe->read_fd = -1;
    }
    return 0;
}

res_t ik_debug_pipe_read(ik_debug_pipe_t *pipe, char ***lines_out, size_t *count_out)
{
    assert(pipe != NULL);        // LCOV_EXCL_BR_LINE
    assert(lines_out != NULL);   // LCOV_EXCL_BR_LINE
    assert(count_out != NULL);   // LCOV_EXCL_BR_LINE

    *lines_out = NULL;
    *count_out = 0;

    // Read buffer (4KB)
    char read_buf[4096];
    ssize_t nread = posix_read_(pipe->read_fd, read_buf, sizeof(read_buf));

    // Handle read errors
    if (nread == -1) {
        // Platform-specific: EAGAIN == EWOULDBLOCK on this system, so one branch of OR is impossible
        if (errno == EAGAIN || errno == EWOULDBLOCK) {  // LCOV_EXCL_BR_LINE
            // No data available (non-blocking) - not an error
            return OK(NULL);
        }
        return ERR(pipe, IO, "read() failed: %s", strerror(errno));
    }

    // No data available
    if (nread == 0) {
        return OK(NULL);
    }

    // Allocate lines array (start with capacity for 16 lines)
    size_t lines_capacity = 16;
    char **lines = talloc_array_(pipe, sizeof(char *), lines_capacity);
    if (lines == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    size_t lines_count = 0;

    // Process each byte looking for newlines
    for (ssize_t i = 0; i < nread; i++) {
        char c = read_buf[i];

        if (c == '\n') {
            // Complete line found - create output string
            char *line = NULL;
            size_t prefix_len = pipe->prefix ? strlen(pipe->prefix) : 0;
            size_t space_len = pipe->prefix ? 1 : 0;  // Space after prefix
            size_t total_len = prefix_len + space_len + pipe->buffer_pos;

            // Allocate line string (prefix + space + content + null terminator)
            line = talloc_array_(lines, sizeof(char), total_len + 1);
            if (line == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            size_t offset = 0;

            // Copy prefix if present
            if (pipe->prefix) {
                memcpy(line, pipe->prefix, prefix_len);
                offset += prefix_len;
                line[offset++] = ' ';  // Add space after prefix
            }

            // Copy line content
            if (pipe->buffer_pos > 0) {
                memcpy(line + offset, pipe->line_buffer, pipe->buffer_pos);
                offset += pipe->buffer_pos;
            }
            line[offset] = '\0';

            // Add to lines array (grow if needed)
            if (lines_count >= lines_capacity) {
                lines_capacity *= 2;
                char **new_lines = talloc_realloc_(pipe, lines, sizeof(char *) * lines_capacity);
                if (new_lines == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                lines = new_lines;
            }
            lines[lines_count++] = line;

            // Reset line buffer for next line
            pipe->buffer_pos = 0;
        } else {
            // Accumulate character in line buffer
            // Grow buffer if needed
            if (pipe->buffer_pos >= pipe->buffer_capacity) {
                pipe->buffer_capacity *= 2;
                char *new_buffer = talloc_realloc_(pipe, pipe->line_buffer,
                                                   sizeof(char) * pipe->buffer_capacity);
                if (new_buffer == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                pipe->line_buffer = new_buffer;
            }
            pipe->line_buffer[pipe->buffer_pos++] = c;
        }
    }

    *lines_out = lines;
    *count_out = lines_count;
    return OK(NULL);
}

res_t ik_debug_mgr_create(void *parent)
{
    // Allocate manager structure
    ik_debug_pipe_manager_t *mgr = talloc_zero_(parent, sizeof(ik_debug_pipe_manager_t));
    if (mgr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Initialize with capacity=4, count=0
    mgr->capacity = 4;
    mgr->count = 0;

    // Allocate initial pipes array
    mgr->pipes = talloc_array_(mgr, sizeof(ik_debug_pipe_t *), mgr->capacity);
    if (mgr->pipes == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return OK(mgr);
}

res_t ik_debug_mgr_add_pipe(ik_debug_pipe_manager_t *mgr, const char *prefix)
{
    assert(mgr != NULL);  // LCOV_EXCL_BR_LINE

    // Grow array if needed
    if (mgr->count >= mgr->capacity) {
        size_t new_capacity = mgr->capacity * 2;
        ik_debug_pipe_t **new_pipes = talloc_realloc_(mgr, mgr->pipes,
                                                      sizeof(ik_debug_pipe_t *) * new_capacity);
        if (new_pipes == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        mgr->pipes = new_pipes;
        mgr->capacity = new_capacity;
    }

    // Create debug pipe
    res_t res = ik_debug_pipe_create(mgr, prefix);
    if (is_err(&res)) {
        return res;
    }

    ik_debug_pipe_t *pipe = res.ok;

    // Add to pipes array
    mgr->pipes[mgr->count] = pipe;
    mgr->count++;

    return OK(pipe);
}

void ik_debug_mgr_add_to_fdset(ik_debug_pipe_manager_t *mgr, fd_set *read_fds, int *max_fd)
{
    assert(mgr != NULL);       // LCOV_EXCL_BR_LINE
    assert(read_fds != NULL);  // LCOV_EXCL_BR_LINE
    assert(max_fd != NULL);    // LCOV_EXCL_BR_LINE

    // Iterate over all pipes and add their read_fds to the set
    for (size_t i = 0; i < mgr->count; i++) {
        ik_debug_pipe_t *pipe = mgr->pipes[i];
        FD_SET(pipe->read_fd, read_fds);

        // Update max_fd if this fd is larger
        if (pipe->read_fd > *max_fd) {
            *max_fd = pipe->read_fd;
        }
    }
}

res_t ik_debug_mgr_handle_ready(ik_debug_pipe_manager_t *mgr, fd_set *read_fds,
                                ik_scrollback_t *scrollback, bool debug_enabled)
{
    assert(mgr != NULL);       // LCOV_EXCL_BR_LINE
    assert(read_fds != NULL);  // LCOV_EXCL_BR_LINE
    // scrollback can be NULL when debug_enabled is false

    // Iterate over all pipes
    for (size_t i = 0; i < mgr->count; i++) {
        ik_debug_pipe_t *pipe = mgr->pipes[i];

        // Check if this pipe is ready
        if (!FD_ISSET(pipe->read_fd, read_fds)) {
            continue;
        }

        // Read from pipe
        char **lines = NULL;
        size_t count = 0;
        res_t res = ik_debug_pipe_read(pipe, &lines, &count);
        if (is_err(&res)) {
            return res;
        }

        // If debug enabled, append lines to scrollback
        if (debug_enabled && count > 0) {
            assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE
            for (size_t j = 0; j < count; j++) {
                res_t append_res = ik_scrollback_append_line(scrollback, lines[j], strlen(lines[j]));
                if (is_err(&append_res)) {  // LCOV_EXCL_BR_LINE
                    talloc_free(lines);  // LCOV_EXCL_LINE
                    return append_res;  // LCOV_EXCL_LINE
                }
            }
        }

        // Free lines array (whether or not we used it)
        if (lines != NULL) {
            talloc_free(lines);
        }
    }

    return OK(NULL);
}
