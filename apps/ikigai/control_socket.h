#pragma once

#include "shared/error.h"
#include <stdbool.h>
#include <sys/select.h>
#include <talloc.h>

// Forward declarations
typedef struct ik_paths_t ik_paths_t;
typedef struct ik_repl_ctx_t ik_repl_ctx_t;

// Opaque type - struct defined privately in control_socket.c
typedef struct ik_control_socket_t ik_control_socket_t;

// Initialize control socket
// Creates Unix domain socket at $IKIGAI_RUNTIME_DIR/ikigai-<pid>.sock
// Returns ERR_INVALID_ARG if paths is NULL
// Returns ERR_IO if socket creation, bind, or listen fails
res_t ik_control_socket_init(TALLOC_CTX *ctx, ik_paths_t *paths,
                              ik_control_socket_t **out);

// Destroy control socket
// Closes listen fd, client fd (if connected), and unlinks socket file
// Asserts socket != NULL
void ik_control_socket_destroy(ik_control_socket_t *socket);

// Add socket fds to fd_sets for select()
// Adds listen_fd to read_fds; if client connected, adds client_fd to read_fds
// Updates max_fd if needed
// Asserts socket != NULL
void ik_control_socket_add_to_fd_sets(ik_control_socket_t *socket,
                                       fd_set *read_fds,
                                       int *max_fd);

// Check if listen fd is ready in fd_set (after select())
// Returns true if listen_fd is set in read_fds
// Asserts socket != NULL
bool ik_control_socket_listen_ready(ik_control_socket_t *socket,
                                     fd_set *read_fds);

// Check if client fd is ready in fd_set (after select())
// Returns true if client is connected and client_fd is set in read_fds
// Asserts socket != NULL
bool ik_control_socket_client_ready(ik_control_socket_t *socket,
                                     fd_set *read_fds);

// Accept new connection on listen fd
// Closes existing client connection (if any) before accepting new one
// Returns OK on success, ERR_IO on failure
// Asserts socket != NULL
res_t ik_control_socket_accept(ik_control_socket_t *socket);

// Handle client request (read, dispatch, respond)
// Reads one JSON line from client, dispatches to handler, writes response
// Returns OK on success, ERR_IO on read/write error
// Asserts socket != NULL and repl != NULL
res_t ik_control_socket_handle_client(ik_control_socket_t *socket,
                                       ik_repl_ctx_t *repl);

// Check and fire any pending wait_idle response (call every main loop iteration).
// Sends {"type":"idle"} when agent reaches IDLE, {"type":"timeout"} on deadline.
// No-op if no pending wait_idle.
// Asserts socket != NULL and repl != NULL.
void ik_control_socket_tick(ik_control_socket_t *socket, ik_repl_ctx_t *repl);
