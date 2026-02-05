#ifndef IK_TOOL_EXECUTOR_H
#define IK_TOOL_EXECUTOR_H

#include "apps/ikigai/paths.h"
#include "apps/ikigai/tool_registry.h"

#include <sys/types.h>
#include <talloc.h>

// Execute tool from registry with ik:// URI translation
// Translates ik:// URIs to filesystem paths in arguments
// Executes external tool via registry
// Translates filesystem paths back to ik:// URIs in result
// Returns JSON result (success/failure envelope)
// child_pid_out: If not NULL, receives child process PID for interrupt handling
char *ik_tool_execute_from_registry(TALLOC_CTX *ctx,
                                    ik_tool_registry_t *registry,
                                    ik_paths_t *paths,
                                    const char *agent_id,
                                    const char *tool_name,
                                    const char *arguments,
                                    pid_t *child_pid_out);

#endif // IK_TOOL_EXECUTOR_H
