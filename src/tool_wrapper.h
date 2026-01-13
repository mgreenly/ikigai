#ifndef IK_TOOL_WRAPPER_H
#define IK_TOOL_WRAPPER_H

#include <inttypes.h>
#include <talloc.h>

// Wrap successful tool execution
// Returns: {"tool_success": true, "result": {...original...}}
char *ik_tool_wrap_success(TALLOC_CTX *ctx, const char *tool_result_json);

// Wrap tool failure (crash, timeout, invalid JSON)
// Returns: {"tool_success": false, "error": "...", "error_code": "..."}
char *ik_tool_wrap_failure(TALLOC_CTX *ctx, const char *error, const char *error_code);

#endif // IK_TOOL_WRAPPER_H
