#ifndef IK_TOOL_H
#define IK_TOOL_H

#include <talloc.h>
#include "shared/error.h"
#include "vendor/yyjson/yyjson.h"

// Represents a parsed tool call from the API response
typedef struct {
    char *id;         // Tool call ID (e.g., "call_abc123"), owned by struct
    char *name;       // Function name (e.g., "glob"), owned by struct
    char *arguments;  // JSON string of arguments, owned by struct
} ik_tool_call_t;

// Create a new tool call struct.
//
// Allocates a new tool call struct on the given context.
// All string fields (id, name, arguments) are copied via talloc_strdup
// and are children of the returned struct.
//
// @param ctx Parent talloc context (can be NULL for root context)
// @param id Tool call ID string
// @param name Function name string
// @param arguments JSON arguments string
// @return Pointer to new tool call struct (owned by ctx), or NULL on OOM
ik_tool_call_t *ik_tool_call_create(TALLOC_CTX *ctx, const char *id, const char *name, const char *arguments);

// Extract string argument from tool call JSON arguments.
//
// Parses the arguments_json string as JSON, looks up the specified key,
// and returns its string value if found and of correct type.
//
// @param parent Parent talloc context for result string allocation
// @param arguments_json JSON string containing tool arguments (e.g., "{\"pattern\": \"*.c\"}")
// @param key Key to extract (e.g., "pattern")
// @return Allocated string value (owned by parent) if key found and is string type,
//         NULL if key not found, arguments_json is NULL, JSON is malformed, or value is wrong type
char *ik_tool_arg_get_string(void *parent, const char *arguments_json, const char *key);

#endif // IK_TOOL_H
