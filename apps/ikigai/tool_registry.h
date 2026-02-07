#ifndef IK_TOOL_REGISTRY_H
#define IK_TOOL_REGISTRY_H

#include <inttypes.h>
#include <stdbool.h>
#include <talloc.h>

#include "shared/error.h"
#include "vendor/yyjson/yyjson.h"

// Forward declarations
typedef struct ik_agent_ctx ik_agent_ctx_t;
typedef struct ik_repl_ctx_t ik_repl_ctx_t;

// Tool type enum
typedef enum {
    IK_TOOL_EXTERNAL,   // External executable tool (fork/exec)
    IK_TOOL_INTERNAL    // Internal C function tool (in-process)
} ik_tool_type_t;

// Internal tool handler - runs on worker thread
// Returns JSON result string allocated on ctx
typedef char *(*ik_tool_internal_fn)(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *arguments_json);

// Tool completion hook - runs on main thread after tool finishes
typedef void (*ik_tool_complete_fn)(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent);

// Registry entry for a tool (external or internal)
typedef struct {
    char *name;                    // Tool name (e.g., "bash", "file_read")
    char *path;                    // Full path to executable (NULL for internal tools)
    yyjson_doc *schema_doc;        // Parsed schema from --schema call
    yyjson_val *schema_root;       // Root of schema (for building tools array)
    ik_tool_type_t type;           // Tool type (external or internal)
    ik_tool_internal_fn handler;   // Handler function (NULL for external tools)
    ik_tool_complete_fn on_complete; // Completion hook (NULL if not needed)
} ik_tool_registry_entry_t;

// Dynamic runtime registry
typedef struct {
    ik_tool_registry_entry_t *entries;
    size_t count;
    size_t capacity;
} ik_tool_registry_t;

// Create registry
ik_tool_registry_t *ik_tool_registry_create(TALLOC_CTX *ctx);

// Look up tool by name
ik_tool_registry_entry_t *ik_tool_registry_lookup(ik_tool_registry_t *registry, const char *name);

// Build tools array for LLM
yyjson_mut_val *ik_tool_registry_build_all(ik_tool_registry_t *registry, yyjson_mut_doc *doc);

// Add external tool to registry
res_t ik_tool_registry_add(ik_tool_registry_t *registry, const char *name, const char *path, yyjson_doc *schema_doc);

// Add internal tool to registry
res_t ik_tool_registry_add_internal(ik_tool_registry_t *registry, const char *name, yyjson_doc *schema_doc,
                                    ik_tool_internal_fn handler, ik_tool_complete_fn on_complete);

// Clear all entries (for /refresh)
void ik_tool_registry_clear(ik_tool_registry_t *registry);

// Sort entries alphabetically by name
void ik_tool_registry_sort(ik_tool_registry_t *registry);

#endif // IK_TOOL_REGISTRY_H
