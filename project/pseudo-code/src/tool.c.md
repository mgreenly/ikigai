## Overview

This file manages tool definitions and schemas for the ikigai agent. It defines the structure of available tools (glob, file_read, grep, file_write, bash), generates their JSON schemas for API communication, and provides utility functions for handling tool results (truncation and metadata).

## Code

```
function create_tool_call(memory_context, id, name, arguments):
    validate that id, name, and arguments are not null

    allocate memory for tool call structure
    if allocation fails, panic with out of memory error

    copy id string into the structure
    copy name string into the structure
    copy arguments string into the structure
    if any copy fails, panic with out of memory error

    return tool call structure

function add_string_parameter_to_schema(document, properties_object, parameter_name, parameter_description):
    validate that document, properties_object, parameter_name, and description are not null

    create a new JSON object for the parameter
    if creation fails, panic with out of memory error

    add "type": "string" to the parameter object
    add "description": <parameter_description> to the parameter object
    if either add fails, panic with error message

    add the parameter to the properties object under the parameter_name key
    if addition fails, panic with out of memory error

// Tool schemas are defined declaratively:
// Each tool has:
// - A list of parameter definitions (name, description, required flag)
// - A schema definition (name, description, parameter list)
// - A builder function that constructs the JSON schema

function build_glob_schema(document):
    // glob tool: pattern (required), path (optional)
    delegate to generic schema builder with glob definition

function build_file_read_schema(document):
    // file_read tool: path (required)
    delegate to generic schema builder with file_read definition

function build_grep_schema(document):
    // grep tool: pattern (required), path (optional), glob (optional)
    delegate to generic schema builder with grep definition

function build_file_write_schema(document):
    // file_write tool: path (required), content (required)
    delegate to generic schema builder with file_write definition

function build_bash_schema(document):
    // bash tool: command (required)
    delegate to generic schema builder with bash definition

function build_schema_from_definition(document, schema_definition):
    validate that document and schema_definition are not null

    create root schema JSON object
    if creation fails, panic with out of memory error

    add "type": "function" to root schema

    create function metadata object
    if creation fails, panic with out of memory error

    add tool name and description to function metadata

    create properties object to hold parameter descriptions
    if creation fails, panic with out of memory error

    for each parameter in schema definition:
        add string parameter to properties object

    create parameters wrapper object
    if creation fails, panic with out of memory error

    add "type": "object" to parameters wrapper
    add properties object to parameters wrapper

    create required array
    if creation fails, panic with out of memory error

    for each parameter in schema definition:
        if parameter is marked required, add its name to required array

    add required array to parameters wrapper
    add parameters wrapper to function metadata
    add function metadata to root schema

    return root schema

function build_all_tools_schema(document):
    validate that document is not null

    create array to hold all tool schemas
    if creation fails, panic with out of memory error

    build and add glob schema to array
    build and add file_read schema to array
    build and add grep schema to array
    build and add file_write schema to array
    build and add bash schema to array
    if any addition fails, panic with error message

    return array of all tool schemas

function truncate_output(memory_context, output_string, maximum_size):
    if output_string is null, return null

    calculate length of output_string

    if output length <= maximum_size:
        return a copy of the output string

    allocate memory for truncated result (max_size + space for indicator)
    if allocation fails, panic with out of memory error

    copy first maximum_size bytes of output into truncated buffer
    terminate the buffer with null character

    format indicator message showing how much was shown and total bytes
    if formatting fails, panic with error

    resize truncated buffer to fit both content and indicator
    if resize fails, panic with out of memory error

    append indicator message to truncated content

    return truncated result with indicator

function add_tool_limit_metadata(memory_context, result_json_string, maximum_tool_turns):
    if result_json_string is null, return null

    parse result_json_string to JSON document
    if parsing fails, return null

    get the root object from parsed document
    if root is missing or not an object, free document and return null

    create mutable copy of the JSON document
    if creation fails, panic with out of memory error

    make a mutable copy of the root object
    if copy fails, panic with out of memory error

    add "limit_reached": true to the root object

    format message describing the tool limit and maximum turns
    if formatting fails, panic with error

    add "limit_message": <formatted message> to the root object

    serialize the modified document back to JSON string
    if serialization fails, panic with out of memory error

    copy the JSON string with memory management
    if copy fails, panic with out of memory error

    free temporary JSON documents

    return the result string with limit metadata
```
