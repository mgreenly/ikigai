#ifndef IK_TOOL_H
#define IK_TOOL_H

#include "vendor/yyjson/yyjson.h"

// Build JSON schema for the glob tool.
//
// Creates a tool schema object following OpenAI's function calling format.
// The schema includes the tool name "glob", description, and parameter specifications
// with "pattern" as required and "path" as optional.
//
// @param doc The yyjson mutable document to build the schema in
// @return Pointer to the schema object (owned by doc), or NULL on error
yyjson_mut_val *ik_tool_build_glob_schema(yyjson_mut_doc *doc);

#endif // IK_TOOL_H
