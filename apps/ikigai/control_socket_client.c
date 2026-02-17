#include "control_socket_client.h"

#include "shared/panic.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "shared/poison.h"
res_t ik_ctl_connect(TALLOC_CTX *ctx, const char *socket_path, int *fd_out)
{
    assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(socket_path != NULL);   // LCOV_EXCL_BR_LINE
    assert(fd_out != NULL);        // LCOV_EXCL_BR_LINE

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return ERR(ctx, IO, "Failed to create socket: %s", strerror(errno));
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    size_t path_len = strlen(socket_path);
    if (path_len >= sizeof(addr.sun_path)) {
        close(fd);
        return ERR(ctx, IO, "Socket path too long: %s", socket_path);
    }

    memcpy(addr.sun_path, socket_path, path_len + 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return ERR(ctx, IO, "Failed to connect to %s: %s", socket_path, strerror(errno));
    }

    *fd_out = fd;
    return OK(NULL);
}

res_t ik_ctl_read_framebuffer(TALLOC_CTX *ctx, int fd, char **response_out)
{
    assert(ctx != NULL);            // LCOV_EXCL_BR_LINE
    assert(response_out != NULL);   // LCOV_EXCL_BR_LINE

    const char *request = "{\"type\":\"read_framebuffer\"}\n";
    ssize_t written = write(fd, request, strlen(request));
    if (written < 0) {
        return ERR(ctx, IO, "Failed to send request: %s", strerror(errno));
    }

    // Read response (may be large for framebuffer data)
    size_t buf_size = 65536;
    char *buf = talloc_size(ctx, buf_size);
    if (buf == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t total = 0;
    while (total < buf_size - 1) {
        ssize_t n = read(fd, buf + total, buf_size - 1 - total);
        if (n < 0) {
            talloc_free(buf);
            return ERR(ctx, IO, "Failed to read response: %s", strerror(errno));
        }
        if (n == 0) {
            break;
        }
        total += (size_t)n;

        // Check for newline (end of JSON response)
        if (memchr(buf + total - (size_t)n, '\n', (size_t)n) != NULL) {
            break;
        }
    }
    buf[total] = '\0';

    *response_out = buf;
    return OK(NULL);
}

res_t ik_ctl_send_keys(TALLOC_CTX *ctx, int fd, const char *keys)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(keys != NULL);   // LCOV_EXCL_BR_LINE

    // Build request: {"type":"send_keys","keys":"<keys>"}
    char *request = talloc_asprintf(ctx, "{\"type\":\"send_keys\",\"keys\":\"%s\"}\n", keys);
    if (request == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    ssize_t written = write(fd, request, strlen(request));
    talloc_free(request);
    if (written < 0) {
        return ERR(ctx, IO, "Failed to send keys: %s", strerror(errno));
    }

    // Read response
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        return ERR(ctx, IO, "Failed to read response: %s", strerror(errno));
    }
    buf[n] = '\0';

    // Check for error response
    if (strstr(buf, "\"error\"") != NULL) {
        return ERR(ctx, IO, "send_keys failed: %s", buf);
    }

    return OK(NULL);
}

void ik_ctl_disconnect(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}
