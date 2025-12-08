#ifndef IK_TOOL_H
#define IK_TOOL_H

#include <talloc.h>
#include "error.h"
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

// Extract integer argument from tool call JSON arguments.
//
// Parses the arguments_json string as JSON, looks up the specified key,
// and returns its integer value if found and of correct type.
//
// @param arguments_json JSON string containing tool arguments (e.g., "{\"timeout\": 30}")
// @param key Key to extract (e.g., "timeout")
// @param out_value Pointer to int where value will be stored on success
// @return true if key found and value extracted successfully,
//         false if key not found, arguments_json is NULL, out_value is NULL,
//         JSON is malformed, or value is wrong type
bool ik_tool_arg_get_int(const char *arguments_json, const char *key, int *out_value);

// Execute glob tool to find files matching a pattern.
//
// Uses POSIX glob() to find files matching the given pattern in the specified path.
// Returns a JSON string in envelope format:
// - Success: {"success": true, "data": {"output": "file1\nfile2", "count": 2}}
// - Error: {"success": false, "error": "error message"}
//
// @param parent Parent talloc context for result allocation
// @param pattern Glob pattern (e.g., "*.c", "src/**/*.h")
// @param path Base directory to search in (can be NULL for current directory)
// @return res_t containing JSON string (owned by parent) or error
res_t ik_tool_exec_glob(void *parent, const char *pattern, const char *path);

// Execute file_read tool to read contents of a file.
//
// Reads the entire contents of a file and returns it as a JSON string.
// Returns a JSON string in envelope format:
// - Success: {"success": true, "data": {"output": "file contents"}}
// - Error: {"success": false, "error": "File not found: path"} or other errors
//
// @param parent Parent talloc context for result allocation
// @param path Path to file to read
// @return res_t containing JSON string (owned by parent) or error
res_t ik_tool_exec_file_read(void *parent, const char *path);

// Execute grep tool to search for a pattern in files.
//
// Searches for pattern matches in files and returns results with line numbers.
// Returns a JSON string in envelope format:
// - Success: {"success": true, "data": {"output": "file.c:42: matching line", "count": 1}}
// - Error: {"success": false, "error": "error message"}
//
// @param parent Parent talloc context for result allocation
// @param pattern Search pattern (required)
// @param glob Optional file pattern filter (e.g., "*.c"), may be NULL
// @param path Optional base directory to search in, may be NULL for current directory
// @return res_t containing JSON string (owned by parent) or error
res_t ik_tool_exec_grep(void *parent, const char *pattern, const char *glob, const char *path);

// Execute file_write tool to write content to a file.
//
// Writes the given content to a file at the specified path. Creates the file
// if it doesn't exist, or overwrites it if it does.
// Returns a JSON string in envelope format:
// - Success: {"success": true, "data": {"output": "Wrote 20 bytes to filename", "bytes": 20}}
// - Error: {"success": false, "error": "Permission denied: path"} or other errors
//
// @param parent Parent talloc context for result allocation
// @param path Path to file to write (required)
// @param content Content to write to file (required)
// @return res_t containing JSON string (owned by parent) or error
res_t ik_tool_exec_file_write(void *parent, const char *path, const char *content);

// Execute bash tool to run a shell command.
//
// Executes the given command via shell and captures stdout and exit code.
// Returns a JSON string in envelope format:
// - Success: {"success": true, "data": {"output": "command output", "exit_code": 0}}
// - Error: {"success": false, "error": "Failed to execute command"} (when popen fails)
//
// Note: A non-zero exit code from the command is NOT an error - it's a successful
// tool execution with the exit code in the data field. Only popen failures result
// in the error envelope.
//
// @param parent Parent talloc context for result allocation
// @param command Shell command to execute (required)
// @return res_t containing JSON string (owned by parent)
res_t ik_tool_exec_bash(void *parent, const char *command);

// Dispatch tool calls by name to appropriate execution function.
//
// Parses the JSON arguments string, extracts tool-specific parameters,
// validates required parameters, and calls the appropriate tool execution
// function. Returns result as JSON string (not res_t error).
//
// For glob tool:
// - Extracts "pattern" (required) and "path" (optional) from arguments
// - Calls ik_tool_exec_glob with typed parameters
// - Returns result from glob execution unchanged
//
// For unimplemented tools (file_read, grep, file_write, bash):
// - Returns error JSON: {"error": "Tool not implemented: <name>"}
//
// Error handling:
// - Invalid JSON arguments: {"error": "Invalid JSON arguments"}
// - Missing required parameter: {"error": "Missing required parameter: <param>"}
// - Unknown tool: {"error": "Unknown tool: <name>"}
// - NULL or empty tool_name: {"error": "Unknown tool: ..."}
//
// @param parent Parent talloc context for result allocation
// @param tool_name Tool function name (e.g., "glob", "file_read")
// @param arguments JSON string of tool arguments
// @return res_t containing JSON string (owned by parent)
res_t ik_tool_dispatch(void *parent, const char *tool_name, const char *arguments);

// Add limit metadata to tool result JSON.
//
// Takes a tool result JSON string and adds two fields:
// - "limit_reached": true
// - "limit_message": "Tool call limit reached (N). Stopping tool loop."
//
// This function is used when the tool loop iteration limit is reached to
// inform the model why the loop is stopping.
//
// @param parent Parent talloc context for result allocation
// @param result_json JSON string of tool result (can be NULL)
// @param max_tool_turns Maximum number of tool turns (used in message)
// @return Modified JSON string with limit metadata (owned by parent), or NULL if result_json is NULL or malformed
char *ik_tool_result_add_limit_metadata(void *parent, const char *result_json, int32_t max_tool_turns);

#endif // IK_TOOL_H
