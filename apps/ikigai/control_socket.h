#ifndef IK_CONTROL_SOCKET_H
#define IK_CONTROL_SOCKET_H

#include "shared/error.h"
#include <talloc.h>

// Forward declaration
typedef struct ik_paths_t ik_paths_t;

// Opaque type - struct defined privately in control_socket.c
typedef struct ik_control_socket_t ik_control_socket_t;

// Initialize control socket
// Creates Unix domain socket at $IKIGAI_RUNTIME_DIR/ikigai-<pid>.sock
// Returns ERR_INVALID_ARG if paths is NULL
// Returns ERR_IO if socket creation, bind, or listen fails
res_t ik_control_socket_init(TALLOC_CTX *ctx, ik_paths_t *paths,
                              ik_control_socket_t **out);

// Destroy control socket
// Closes listen fd and unlinks socket file
// Asserts socket != NULL
void ik_control_socket_destroy(ik_control_socket_t *socket);

#endif // IK_CONTROL_SOCKET_H
