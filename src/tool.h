#ifndef IK_TOOL_H
#define IK_TOOL_H

#include <talloc.h>
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
ik_tool_call_t *ik_tool_call_create(TALLOC_CTX *ctx,
                                    const char *id,
                                    const char *name,
                                    const char *arguments);

// Helper function to add a string parameter to properties object.
//
// @param doc The yyjson mutable document
// @param properties The properties object to add to
// @param name Parameter name
// @param description Parameter description
void ik_tool_add_string_param(yyjson_mut_doc *doc, yyjson_mut_val *properties, const char *name,
                              const char *description);

// Build JSON schema for the glob tool.
//
// Creates a tool schema object following OpenAI's function calling format.
// The schema includes the tool name "glob", description, and parameter specifications
// with "pattern" as required and "path" as optional.
//
// @param doc The yyjson mutable document to build the schema in
// @return Pointer to the schema object (owned by doc), or NULL on error
yyjson_mut_val *ik_tool_build_glob_schema(yyjson_mut_doc *doc);

// Build JSON schema for the file_read tool.
//
// Creates a tool schema object following OpenAI's function calling format.
// The schema includes the tool name "file_read", description, and parameter specifications
// with "path" as required.
//
// @param doc The yyjson mutable document to build the schema in
// @return Pointer to the schema object (owned by doc), or NULL on error
yyjson_mut_val *ik_tool_build_file_read_schema(yyjson_mut_doc *doc);

// Build JSON schema for the grep tool.
//
// Creates a tool schema object following OpenAI's function calling format.
// The schema includes the tool name "grep", description, and parameter specifications
// with "pattern" as required, and "path" and "glob" as optional.
//
// @param doc The yyjson mutable document to build the schema in
// @return Pointer to the schema object (owned by doc), or NULL on error
yyjson_mut_val *ik_tool_build_grep_schema(yyjson_mut_doc *doc);

// Build JSON schema for the file_write tool.
//
// Creates a tool schema object following OpenAI's function calling format.
// The schema includes the tool name "file_write", description, and parameter specifications
// with "path" and "content" as required.
//
// @param doc The yyjson mutable document to build the schema in
// @return Pointer to the schema object (owned by doc), or NULL on error
yyjson_mut_val *ik_tool_build_file_write_schema(yyjson_mut_doc *doc);

// Build JSON schema for the bash tool.
//
// Creates a tool schema object following OpenAI's function calling format.
// The schema includes the tool name "bash", description, and parameter specifications
// with "command" as required.
//
// @param doc The yyjson mutable document to build the schema in
// @return Pointer to the schema object (owned by doc), or NULL on error
yyjson_mut_val *ik_tool_build_bash_schema(yyjson_mut_doc *doc);

// Build array containing all 5 tool schemas.
//
// Creates a JSON array containing all tool schemas in order:
// 1. glob
// 2. file_read
// 3. grep
// 4. file_write
// 5. bash
//
// @param doc The yyjson mutable document to build the array in
// @return Pointer to the array object (owned by doc), or NULL on error
yyjson_mut_val *ik_tool_build_all(yyjson_mut_doc *doc);

// Truncate output if it exceeds max_size limit.
//
// If output is NULL, returns NULL.
// If output length is <= max_size, returns talloc_strdup of output.
// If output length is > max_size, truncates to max_size and appends
// a truncation indicator: "[Output truncated: showing first X of Y bytes]"
//
// The returned string is allocated on the provided parent context.
// Caller owns the returned string.
//
// @param parent Parent talloc context for result allocation
// @param output Output string to potentially truncate (can be NULL)
// @param max_size Maximum size in bytes before truncation
// @return Truncated or copied output string (owned by parent), or NULL if output is NULL
char *ik_tool_truncate_output(void *parent, const char *output, size_t max_size);

#endif // IK_TOOL_H
