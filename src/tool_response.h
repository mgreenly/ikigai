#ifndef IK_TOOL_RESPONSE_H
#define IK_TOOL_RESPONSE_H

#include "error.h"
#include <talloc.h>
#include "vendor/yyjson/yyjson.h"

// Tool Response Builders
//
// Centralized response building for tool results. All tools return JSON in
// one of two envelope formats:
//
// Error:   {"success": false, "error": "message"}
// Success: {"success": true, "data": {...}}
//
// Usage:
//   // For errors:
//   char *result;
//   ik_tool_response_error(ctx, "Error message", &result);
//
//   // For success with data:
//   typedef struct { ... } my_data_t;
//   static void add_my_data(yyjson_mut_doc *doc, yyjson_mut_val *data, void *ctx) {
//       my_data_t *d = ctx;
//       yyjson_mut_obj_add_str(doc, data, "output", d->output);
//   }
//
//   my_data_t data = { .output = "result" };
//   char *result;
//   ik_tool_response_success_with_data(ctx, add_my_data, &data, &result);
//
// Adding a new tool:
// 1. Define result data struct
// 2. Define callback to populate data fields
// 3. Call ik_tool_response_success_with_data() or ik_tool_response_error()

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

// Callback type for adding fields to the data object
typedef void (*ik_tool_data_adder_t)(yyjson_mut_doc *doc, yyjson_mut_val *data, void *user_ctx);

// Build success response with data object: {"success": true, "data": {...}}
// Caller provides a callback to populate the data object with tool-specific fields.
// Returns: OK(json_string) where json_string is talloc-allocated on ctx
res_t ik_tool_response_success_with_data(TALLOC_CTX *ctx, ik_tool_data_adder_t add_data, void *user_ctx, char **out);

#endif
