## Overview

Routes tool invocations to appropriate handler functions based on tool name. Validates required parameters, ensures JSON arguments are well-formed, and dispatches to specialized execution functions for glob, file_read, grep, file_write, and bash operations. Returns error responses as JSON when parameters are missing or invalid.

## Code

```
function dispatch_tool(parent_context, tool_name, arguments_json):
    validate parent_context is not null

    if tool_name is null or empty:
        return error response: "Unknown tool: "

    if arguments_json is provided:
        validate that arguments_json is well-formed JSON
        if invalid JSON:
            return error response: "Invalid JSON arguments"

    if tool_name is "glob":
        extract required parameter "pattern"
        if pattern is missing:
            return error response: "Missing required parameter: pattern"

        extract optional parameter "path"

        delegate to glob executor with pattern and path

    if tool_name is "file_read":
        extract required parameter "path"
        if path is missing:
            return error response: "Missing required parameter: path"

        delegate to file read executor with path

    if tool_name is "grep":
        extract required parameter "pattern"
        if pattern is missing:
            return error response: "Missing required parameter: pattern"

        extract optional parameters "glob" and "path"

        delegate to grep executor with pattern, glob filter, and path

    if tool_name is "file_write":
        extract required parameter "path"
        if path is missing:
            return error response: "Missing required parameter: path"

        extract required parameter "content"
        if content is missing:
            return error response: "Missing required parameter: content"

        delegate to file write executor with path and content

    if tool_name is "bash":
        extract required parameter "command"
        if command is missing:
            return error response: "Missing required parameter: command"

        delegate to bash executor with command

    tool_name did not match any known tool:
        return error response: "Unknown tool: {tool_name}"

function build_error_response(parent_context, error_message):
    allocate JSON document
    create JSON object as document root
    add string field "error" with error_message
    serialize JSON document to string
    copy string to parent context using talloc
    free temporary allocations
    return copied string
```
