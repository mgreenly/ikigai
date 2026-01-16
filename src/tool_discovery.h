#ifndef IK_TOOL_DISCOVERY_H
#define IK_TOOL_DISCOVERY_H

#include "error.h"
#include "tool_registry.h"
#include <talloc.h>

// Blocking wrapper: spawns all tools from ALL THREE directories, waits for completion, returns
// CRITICAL: Scans ALL THREE directories (system_dir AND user_dir AND project_dir)
// Override precedence: Project > User > System (most specific wins)
// Missing/empty directories handled gracefully (no error)
// Internally uses async primitives but blocks until done
res_t ik_tool_discovery_run(TALLOC_CTX *ctx, const char *system_dir,   // PREFIX/libexec/ikigai/ (system tools)
                            const char *user_dir,      // ~/.ikigai/tools/ (user tools)
                            const char *project_dir,   // $PWD/.ikigai/tools/ (project tools)
                            ik_tool_registry_t *registry);

#endif // IK_TOOL_DISCOVERY_H
