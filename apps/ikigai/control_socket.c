#include "control_socket.h"

#include "apps/ikigai/key_inject.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/serialize.h"
#include "apps/ikigai/shared.h"
#include "shared/panic.h"
#include "shared/wrapper.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "vendor/yyjson/yyjson.h"


#include "shared/poison.h"
struct ik_control_socket_t {
    int32_t listen_fd;
    int32_t client_fd;
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

    int32_t fd = posix_socket_(AF_UNIX, SOCK_STREAM, 0);
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

    if (posix_bind_(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return ERR(ctx, IO, "Failed to bind socket: %s", strerror(errno));
    }

    if (posix_listen_(fd, 1) < 0) {
        close(fd);
        unlink(socket_path);
        return ERR(ctx, IO, "Failed to listen on socket: %s", strerror(errno));
    }

    ik_control_socket_t *socket = talloc_zero(ctx, ik_control_socket_t);
    if (socket == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    socket->listen_fd = fd;
    socket->client_fd = -1;
    socket->socket_path = talloc_steal(socket, socket_path);

    *out = socket;
    return OK(NULL);
}

void ik_control_socket_destroy(ik_control_socket_t *socket)
{
    assert(socket != NULL);  // LCOV_EXCL_BR_LINE

    if (socket->client_fd >= 0) {
        close(socket->client_fd);
    }

    if (socket->listen_fd >= 0) {
        close(socket->listen_fd);
    }

    if (socket->socket_path != NULL) {
        unlink(socket->socket_path);
    }

    talloc_free(socket);
}

void ik_control_socket_add_to_fd_sets(ik_control_socket_t *socket,
                                       fd_set *read_fds,
                                       int *max_fd)
{
    assert(socket != NULL);     // LCOV_EXCL_BR_LINE
    assert(read_fds != NULL);   // LCOV_EXCL_BR_LINE
    assert(max_fd != NULL);     // LCOV_EXCL_BR_LINE

    FD_SET(socket->listen_fd, read_fds);
    if (socket->listen_fd > *max_fd) {
        *max_fd = socket->listen_fd;
    }

    if (socket->client_fd >= 0) {
        FD_SET(socket->client_fd, read_fds);
        if (socket->client_fd > *max_fd) {
            *max_fd = socket->client_fd;
        }
    }
}

bool ik_control_socket_listen_ready(ik_control_socket_t *socket,
                                     fd_set *read_fds)
{
    assert(socket != NULL);     // LCOV_EXCL_BR_LINE
    assert(read_fds != NULL);   // LCOV_EXCL_BR_LINE

    return FD_ISSET(socket->listen_fd, read_fds);
}

bool ik_control_socket_client_ready(ik_control_socket_t *socket,
                                     fd_set *read_fds)
{
    assert(socket != NULL);     // LCOV_EXCL_BR_LINE
    assert(read_fds != NULL);   // LCOV_EXCL_BR_LINE

    if (socket->client_fd < 0) {
        return false;
    }

    return FD_ISSET(socket->client_fd, read_fds);
}

res_t ik_control_socket_accept(ik_control_socket_t *socket)
{
    assert(socket != NULL);  // LCOV_EXCL_BR_LINE

    if (socket->client_fd >= 0) {
        close(socket->client_fd);
        socket->client_fd = -1;
    }

    int32_t client_fd = accept(socket->listen_fd, NULL, NULL);
    if (client_fd < 0) {
        return ERR(socket, IO, "Failed to accept connection: %s", strerror(errno));
    }

    socket->client_fd = client_fd;
    return OK(NULL);
}

res_t ik_control_socket_handle_client(ik_control_socket_t *socket,
                                       ik_repl_ctx_t *repl)
{
    assert(socket != NULL);  // LCOV_EXCL_BR_LINE
    assert(repl != NULL);    // LCOV_EXCL_BR_LINE

    if (socket->client_fd < 0) {
        return ERR(socket, IO, "No client connected");
    }

    char buffer[4096];
    ssize_t n = posix_read_(socket->client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        close(socket->client_fd);
        socket->client_fd = -1;
        if (n < 0) {
            return ERR(socket, IO, "Failed to read from client: %s", strerror(errno));
        }
        return OK(NULL);
    }
    buffer[n] = '\0';

    char *newline = strchr(buffer, '\n');
    if (newline != NULL) {
        *newline = '\0';
    }

    yyjson_doc *doc = yyjson_read(buffer, strlen(buffer), 0);
    if (doc == NULL) {
        const char *error_response = "{\"error\":\"Invalid JSON\"}\n";
        posix_write_(socket->client_fd, error_response, strlen(error_response));
        yyjson_doc_free(doc);
        return OK(NULL);
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *type_val = yyjson_obj_get(root, "type");
    const char *type = yyjson_get_str(type_val);

    char *response = NULL;
    if (type != NULL && strcmp(type, "read_framebuffer") == 0) {
#ifdef IKIGAI_DEV
        if (repl->dev_framebuffer != NULL) {
            res_t ser_result = ik_serialize_framebuffer(
                socket,
                (const uint8_t *)repl->dev_framebuffer,
                repl->dev_framebuffer_len,
                repl->shared->term->screen_rows,
                repl->shared->term->screen_cols,
                repl->dev_cursor_row,
                repl->dev_cursor_col,
                repl->current->input_buffer_visible
            );
            if (is_ok(&ser_result)) {
                response = (char *)ser_result.ok;
            } else {
                response = talloc_strdup(socket, "{\"error\":\"Serialization failed\"}\n");
                talloc_free(ser_result.err);
            }
        } else {
            response = talloc_strdup(socket, "{\"error\":\"No framebuffer available\"}\n");
        }
#else
        response = talloc_strdup(socket, "{\"error\":\"Framebuffer not available (not compiled with IKIGAI_DEV)\"}\n");
#endif
    } else if (type != NULL && strcmp(type, "send_keys") == 0) {
        yyjson_val *keys_val = yyjson_obj_get(root, "keys");
        const char *keys = yyjson_get_str(keys_val);

        if (keys == NULL) {
            response = talloc_strdup(socket, "{\"error\":\"Missing keys field\"}\n");
        } else {
            char *raw_bytes = NULL;
            size_t raw_len = 0;
            res_t unescape_result = ik_key_inject_unescape(socket, keys, strlen(keys), &raw_bytes, &raw_len);

            if (is_err(&unescape_result)) {
                response = talloc_strdup(socket, "{\"error\":\"Failed to unescape keys\"}\n");
                talloc_free(unescape_result.err);
            } else {
                res_t append_result = ik_key_inject_append(repl->key_inject_buf, raw_bytes, raw_len);
                talloc_free(raw_bytes);

                if (is_err(&append_result)) {
                    response = talloc_strdup(socket, "{\"error\":\"Failed to append keys\"}\n");
                    talloc_free(append_result.err);
                } else {
                    response = talloc_strdup(socket, "{\"type\":\"ok\"}\n");
                }
            }
        }
    } else {
        response = talloc_strdup(socket, "{\"error\":\"Unknown message type\"}\n");
    }

    if (response == NULL) PANIC("response must be set");  // LCOV_EXCL_BR_LINE
    size_t response_len = strlen(response);
    if (response[response_len - 1] != '\n') {
        response = talloc_strdup_append(response, "\n");
    }
    posix_write_(socket->client_fd, response, strlen(response));
    talloc_free(response);

    yyjson_doc_free(doc);
    return OK(NULL);
}
