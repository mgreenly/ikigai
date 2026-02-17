#pragma once

#include "shared/error.h"

#include <talloc.h>

// Connect to ikigai control socket at given path
// Returns socket fd on success, ERR_IO on failure
res_t ik_ctl_connect(TALLOC_CTX *ctx, const char *socket_path, int *fd_out);

// Send read_framebuffer request and receive JSON response
// Caller must talloc_free the returned string
res_t ik_ctl_read_framebuffer(TALLOC_CTX *ctx, int fd, char **response_out);

// Send send_keys request with given key string
// Returns OK on success, ERR on failure
res_t ik_ctl_send_keys(TALLOC_CTX *ctx, int fd, const char *keys);

// Close control socket connection
void ik_ctl_disconnect(int fd);
