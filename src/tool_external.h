#ifndef IK_TOOL_EXTERNAL_H
#define IK_TOOL_EXTERNAL_H

#include "error.h"
#include <talloc.h>

// Execute external tool with JSON I/O
// Spawns tool process, writes arguments to stdin, reads stdout with 30s timeout
// Returns tool's JSON output as string or error
// Returns ERR_IO on timeout, crash, or process failure
res_t ik_tool_external_exec(TALLOC_CTX *ctx, const char *tool_path, const char *arguments_json, char **out_result);

#endif // IK_TOOL_EXTERNAL_H
