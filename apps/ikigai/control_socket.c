#include "control_socket.h"

#include "apps/ikigai/paths.h"
#include "shared/panic.h"
#include "shared/wrapper.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>


#include "shared/poison.h"
struct ik_control_socket_t {
    int32_t listen_fd;
    char *socket_path;
};

static res_t ensure_runtime_dir_exists(TALLOC_CTX *ctx, const char *runtime_dir)
{
    assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(runtime_dir != NULL);   // LCOV_EXCL_BR_LINE

    struct stat st;
    if (posix_stat_(runtime_dir, &st) == 0) {
        return OK(NULL);
    }

    if (posix_mkdir_(runtime_dir, 0700) != 0) {
        return ERR(ctx, IO, "Failed to create runtime directory: %s", strerror(errno));
    }

    return OK(NULL);
}

res_t ik_control_socket_init(TALLOC_CTX *ctx, ik_paths_t *paths,
                              ik_control_socket_t **out)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(out != NULL);   // LCOV_EXCL_BR_LINE

    if (paths == NULL) {
        return ERR(ctx, INVALID_ARG, "paths is NULL");
    }

    const char *runtime_dir = ik_paths_get_runtime_dir(paths);
    res_t result = ensure_runtime_dir_exists(ctx, runtime_dir);
    if (is_err(&result)) {
        return result;
    }

    int32_t pid = (int32_t)getpid();
    char *socket_path = talloc_asprintf(ctx, "%s/ikigai-%d.sock", runtime_dir, pid);
    if (socket_path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    int32_t fd = socket(AF_UNIX, SOCK_STREAM, 0);
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

    unlink(socket_path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return ERR(ctx, IO, "Failed to bind socket: %s", strerror(errno));
    }

    if (listen(fd, 1) < 0) {
        close(fd);
        unlink(socket_path);
        return ERR(ctx, IO, "Failed to listen on socket: %s", strerror(errno));
    }

    ik_control_socket_t *socket = talloc_zero(ctx, ik_control_socket_t);
    if (socket == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    socket->listen_fd = fd;
    socket->socket_path = talloc_steal(socket, socket_path);

    *out = socket;
    return OK(NULL);
}

void ik_control_socket_destroy(ik_control_socket_t *socket)
{
    assert(socket != NULL);  // LCOV_EXCL_BR_LINE

    if (socket->listen_fd >= 0) {
        close(socket->listen_fd);
    }

    if (socket->socket_path != NULL) {
        unlink(socket->socket_path);
    }

    talloc_free(socket);
}
