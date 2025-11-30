#ifndef IK_TOOL_H
#define IK_TOOL_H

#include "vendor/yyjson/yyjson.h"

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

#endif // IK_TOOL_H
