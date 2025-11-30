#include "tool.h"

#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <talloc.h>

// Build error JSON response
static char *ik_tool_dispatch_build_error(void *parent, const char *error_msg)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_str(doc, root, "error", error_msg);
    char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    char *result = talloc_strdup(parent, json);
    free(json);
    yyjson_mut_doc_free(doc);
    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return result;
}

res_t ik_tool_dispatch(void *parent, const char *tool_name, const char *arguments)
{
    assert(parent != NULL); // LCOV_EXCL_BR_LINE

    // Check for NULL or empty tool_name
    if (tool_name == NULL || tool_name[0] == '\0') {
        char *error_json = ik_tool_dispatch_build_error(parent, "Unknown tool: ");
        return OK(error_json);
    }

    // First, validate JSON arguments (unless we know the tool doesn't use arguments)
    // This check happens before tool-specific handling to detect invalid JSON early
    if (arguments != NULL) {
        yyjson_doc *json_doc = yyjson_read_(arguments, strlen(arguments), 0);
        if (json_doc == NULL) {
            // Invalid JSON arguments
            char *error_json = ik_tool_dispatch_build_error(parent, "Invalid JSON arguments");
            return OK(error_json);
        }
        yyjson_doc_free(json_doc);
    }

    // Handle glob tool
    if (strcmp(tool_name, "glob") == 0) {
        // Extract required "pattern" parameter
        char *pattern = ik_tool_arg_get_string(parent, arguments, "pattern");
        if (pattern == NULL) {
            char *error_json = ik_tool_dispatch_build_error(parent, "Missing required parameter: pattern");
            return OK(error_json);
        }

        // Extract optional "path" parameter
        char *path = ik_tool_arg_get_string(parent, arguments, "path");

        // Call ik_tool_exec_glob and return its result
        return ik_tool_exec_glob(parent, pattern, path);
    }

    // Handle file_read tool
    if (strcmp(tool_name, "file_read") == 0) {
        // Extract required "path" parameter
        char *path = ik_tool_arg_get_string(parent, arguments, "path");
        if (path == NULL) {
            char *error_json = ik_tool_dispatch_build_error(parent, "Missing required parameter: path");
            return OK(error_json);
        }

        // Call ik_tool_exec_file_read and return its result
        return ik_tool_exec_file_read(parent, path);
    }

    // Handle grep tool
    if (strcmp(tool_name, "grep") == 0) {
        // Extract required "pattern" parameter
        char *pattern = ik_tool_arg_get_string(parent, arguments, "pattern");
        if (pattern == NULL) {
            char *error_json = ik_tool_dispatch_build_error(parent, "Missing required parameter: pattern");
            return OK(error_json);
        }

        // Extract optional "glob" and "path" parameters
        char *glob = ik_tool_arg_get_string(parent, arguments, "glob");
        char *path = ik_tool_arg_get_string(parent, arguments, "path");

        // Call ik_tool_exec_grep and return its result
        return ik_tool_exec_grep(parent, pattern, glob, path);
    }

    // Handle file_write tool
    if (strcmp(tool_name, "file_write") == 0) {
        // Extract required "path" parameter
        char *path = ik_tool_arg_get_string(parent, arguments, "path");
        if (path == NULL) {
            char *error_json = ik_tool_dispatch_build_error(parent, "Missing required parameter: path");
            return OK(error_json);
        }

        // Extract required "content" parameter
        char *content = ik_tool_arg_get_string(parent, arguments, "content");
        if (content == NULL) {
            char *error_json = ik_tool_dispatch_build_error(parent, "Missing required parameter: content");
            return OK(error_json);
        }

        // Call ik_tool_exec_file_write and return its result
        return ik_tool_exec_file_write(parent, path, content);
    }

    // Handle bash tool
    if (strcmp(tool_name, "bash") == 0) {
        // Extract required "command" parameter
        char *command = ik_tool_arg_get_string(parent, arguments, "command");
        if (command == NULL) {
            char *error_json = ik_tool_dispatch_build_error(parent, "Missing required parameter: command");
            return OK(error_json);
        }

        // Call ik_tool_exec_bash and return its result
        return ik_tool_exec_bash(parent, command);
    }

    // Unknown tool name
    char error_msg[256];
    int written = snprintf(error_msg, sizeof(error_msg), "Unknown tool: %s", tool_name);
    if (written < 0 || written >= (int)sizeof(error_msg)) { // LCOV_EXCL_BR_LINE
        PANIC("Tool name too long"); // LCOV_EXCL_LINE
    }

    char *error_json = ik_tool_dispatch_build_error(parent, error_msg);
    return OK(error_json);
}
