#ifndef IK_TOOL_RESPONSE_H
#define IK_TOOL_RESPONSE_H

#include "error.h"
#include <talloc.h>
#include "vendor/yyjson/yyjson.h"

// Build error response: {"success": false, "error": "message"}
// Returns: OK(json_string) where json_string is talloc-allocated on ctx
res_t ik_tool_response_error(TALLOC_CTX *ctx, const char *error_msg, char **out);

// Build success response: {"success": true, "output": "content"}
// Returns: OK(json_string) where json_string is talloc-allocated on ctx
res_t ik_tool_response_success(TALLOC_CTX *ctx, const char *output, char **out);

// Build success response with additional fields
// Caller provides a callback to add custom fields to the root object
// Returns: OK(json_string) where json_string is talloc-allocated on ctx
typedef void (*ik_tool_field_adder_t)(yyjson_mut_doc *doc, yyjson_mut_val *root, void *user_ctx);
res_t ik_tool_response_success_ex(TALLOC_CTX *ctx,
                                   const char *output,
                                   ik_tool_field_adder_t add_fields,
                                   void *user_ctx,
                                   char **out);

#endif
