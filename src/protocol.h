// Protocol module - WebSocket message parsing, serialization, and UUID generation
// Handles post-handshake messages only (handshake parsed inline in handler)

#ifndef IK_PROTOCOL_H
#define IK_PROTOCOL_H

#include <talloc.h>
#include <jansson.h>
#include "error.h"

// Message envelope structure
typedef struct {
    char *sess_id;              // Session ID (base64url UUID, 22 chars)
    char *corr_id;              // Correlation ID (base64url UUID, 22 chars) - optional
    char *type;                 // Message type string
    json_t *payload;            // Generic JSON payload (caller interprets based on type)
} ik_protocol_msg_t;

// Parse envelope message from JSON string
ik_result_t ik_protocol_msg_parse(TALLOC_CTX *ctx, const char *json_str);

// Serialize envelope message to JSON string
ik_result_t ik_protocol_msg_serialize(TALLOC_CTX *ctx, ik_protocol_msg_t *msg);

// Generate base64url-encoded UUID (22 characters)
ik_result_t ik_protocol_generate_uuid(TALLOC_CTX *ctx);

// Create error message
ik_result_t ik_protocol_msg_create_err(TALLOC_CTX *ctx,
                                       const char *sess_id,
                                       const char *corr_id,
                                       const char *source,
                                       const char *err_msg);

// Create assistant response message
ik_result_t ik_protocol_msg_create_assistant_resp(TALLOC_CTX *ctx,
                                                  const char *sess_id,
                                                  const char *corr_id,
                                                  json_t *payload);

#endif // IK_PROTOCOL_H
