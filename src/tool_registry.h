#ifndef IK_TOOL_REGISTRY_H
#define IK_TOOL_REGISTRY_H

#include <inttypes.h>
#include <stdbool.h>
#include <talloc.h>

#include "error.h"
#include "vendor/yyjson/yyjson.h"

// Registry entry for a single external tool
typedef struct {
    char *name;                    // Tool name (e.g., "bash", "file_read")
    char *path;                    // Full path to executable
    yyjson_doc *schema_doc;        // Parsed schema from --schema call
    yyjson_val *schema_root;       // Root of schema (for building tools array)
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

// Add tool to registry (internal helper)
res_t ik_tool_registry_add(ik_tool_registry_t *registry, const char *name, const char *path, yyjson_doc *schema_doc);

// Clear all entries (for /refresh)
void ik_tool_registry_clear(ik_tool_registry_t *registry);

#endif // IK_TOOL_REGISTRY_H
