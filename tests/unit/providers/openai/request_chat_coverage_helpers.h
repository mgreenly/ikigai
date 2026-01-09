#pragma once

#include "providers/request.h"
#include "message.h"
#include "tool.h"

#include <talloc.h>

/* Helper: Create a minimal request */
ik_request_t *ik_test_create_minimal_request(void *ctx);

/* Helper: Add a tool to a request */
void ik_test_add_tool(void *ctx, ik_request_t *req, const char *name, const char *desc, const char *params);

/* Helper: Add a text message to a request */
void ik_test_add_message(void *ctx, ik_request_t *req, ik_role_t role, const char *text);