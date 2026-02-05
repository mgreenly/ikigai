#ifndef IK_TOOL_EXTERNAL_H
#define IK_TOOL_EXTERNAL_H

#include "shared/error.h"
#include <talloc.h>

// Execute external tool with JSON I/O
// Spawns tool process, writes arguments to stdin, reads stdout with 30s timeout
// Sets IKIGAI_AGENT_ID environment variable to agent_id before execution
// Child process calls setpgid(0, 0) to create new process group
// Parent stores child PID in *child_pid_out for interrupt handling (can be NULL)
// Returns tool's JSON output as string or error
// Returns ERR_IO on timeout, crash, or process failure
res_t ik_tool_external_exec(TALLOC_CTX *ctx, const char *tool_path, const char *agent_id, const char *arguments_json, pid_t *child_pid_out, char **out_result);

#endif // IK_TOOL_EXTERNAL_H
