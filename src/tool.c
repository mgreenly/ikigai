#include "tool.h"

#include "panic.h"

#include <assert.h>

void ik_tool_add_string_param(yyjson_mut_doc *doc,
                              yyjson_mut_val *properties,
                              const char *name,
                              const char *description)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE
    assert(properties != NULL); // LCOV_EXCL_BR_LINE
    assert(name != NULL); // LCOV_EXCL_BR_LINE
    assert(description != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *param = yyjson_mut_obj(doc);
    if (param == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, param, "type", "string")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to parameter"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_str(doc, param, "description", description)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add description field to parameter"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, properties, name, param)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add parameter to properties"); // LCOV_EXCL_LINE
    }
}

yyjson_mut_val *ik_tool_build_glob_schema(yyjson_mut_doc *doc)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE

    // Create root schema object
    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "type": "function"
    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to schema"); // LCOV_EXCL_LINE
    }

    // Create "function" object
    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "function.name": "glob"
    if (!yyjson_mut_obj_add_str(doc, function, "name", "glob")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add name field to function"); // LCOV_EXCL_LINE
    }

    // Add "function.description"
    if (!yyjson_mut_obj_add_str(doc, function, "description", "Find files matching a glob pattern")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add description field to function"); // LCOV_EXCL_LINE
    }

    // Create "function.parameters" object
    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "function.parameters.type": "object"
    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to parameters"); // LCOV_EXCL_LINE
    }

    // Create "function.parameters.properties" object
    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add parameters
    ik_tool_add_string_param(doc, properties, "pattern", "Glob pattern (e.g., 'src/**/*.c')");
    ik_tool_add_string_param(doc, properties, "path", "Base directory (default: cwd)");

    // Add properties to parameters
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add properties to parameters"); // LCOV_EXCL_LINE
    }

    // Create "required" array
    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_arr_add_str(doc, required, "pattern")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add pattern to required array"); // LCOV_EXCL_LINE
    }

    // Add required to parameters
    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add required to parameters"); // LCOV_EXCL_LINE
    }

    // Add parameters to function
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add parameters to function"); // LCOV_EXCL_LINE
    }

    // Add function to schema
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add function to schema"); // LCOV_EXCL_LINE
    }

    return schema;
}

yyjson_mut_val *ik_tool_build_file_read_schema(yyjson_mut_doc *doc)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE

    // Create root schema object
    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "type": "function"
    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to schema"); // LCOV_EXCL_LINE
    }

    // Create "function" object
    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "function.name": "file_read"
    if (!yyjson_mut_obj_add_str(doc, function, "name", "file_read")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add name field to function"); // LCOV_EXCL_LINE
    }

    // Add "function.description"
    if (!yyjson_mut_obj_add_str(doc, function, "description", "Read contents of a file")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add description field to function"); // LCOV_EXCL_LINE
    }

    // Create "function.parameters" object
    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "function.parameters.type": "object"
    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to parameters"); // LCOV_EXCL_LINE
    }

    // Create "function.parameters.properties" object
    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add parameters
    ik_tool_add_string_param(doc, properties, "path", "Path to file");

    // Add properties to parameters
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add properties to parameters"); // LCOV_EXCL_LINE
    }

    // Create "required" array
    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_arr_add_str(doc, required, "path")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add path to required array"); // LCOV_EXCL_LINE
    }

    // Add required to parameters
    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add required to parameters"); // LCOV_EXCL_LINE
    }

    // Add parameters to function
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add parameters to function"); // LCOV_EXCL_LINE
    }

    // Add function to schema
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add function to schema"); // LCOV_EXCL_LINE
    }

    return schema;
}

yyjson_mut_val *ik_tool_build_grep_schema(yyjson_mut_doc *doc)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE

    // Create root schema object
    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "type": "function"
    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to schema"); // LCOV_EXCL_LINE
    }

    // Create "function" object
    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "function.name": "grep"
    if (!yyjson_mut_obj_add_str(doc, function, "name", "grep")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add name field to function"); // LCOV_EXCL_LINE
    }

    // Add "function.description"
    if (!yyjson_mut_obj_add_str(doc, function, "description", "Search file contents for a pattern")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add description field to function"); // LCOV_EXCL_LINE
    }

    // Create "function.parameters" object
    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "function.parameters.type": "object"
    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to parameters"); // LCOV_EXCL_LINE
    }

    // Create "function.parameters.properties" object
    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add parameters
    ik_tool_add_string_param(doc, properties, "pattern", "Search pattern (regex)");
    ik_tool_add_string_param(doc, properties, "path", "File or directory to search");
    ik_tool_add_string_param(doc, properties, "glob", "File pattern filter (e.g., '*.c')");

    // Add properties to parameters
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add properties to parameters"); // LCOV_EXCL_LINE
    }

    // Create "required" array
    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_arr_add_str(doc, required, "pattern")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add pattern to required array"); // LCOV_EXCL_LINE
    }

    // Add required to parameters
    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add required to parameters"); // LCOV_EXCL_LINE
    }

    // Add parameters to function
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add parameters to function"); // LCOV_EXCL_LINE
    }

    // Add function to schema
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add function to schema"); // LCOV_EXCL_LINE
    }

    return schema;
}

yyjson_mut_val *ik_tool_build_file_write_schema(yyjson_mut_doc *doc)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE

    // Create root schema object
    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "type": "function"
    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to schema"); // LCOV_EXCL_LINE
    }

    // Create "function" object
    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "function.name": "file_write"
    if (!yyjson_mut_obj_add_str(doc, function, "name", "file_write")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add name field to function"); // LCOV_EXCL_LINE
    }

    // Add "function.description"
    if (!yyjson_mut_obj_add_str(doc, function, "description", "Write content to a file")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add description field to function"); // LCOV_EXCL_LINE
    }

    // Create "function.parameters" object
    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "function.parameters.type": "object"
    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to parameters"); // LCOV_EXCL_LINE
    }

    // Create "function.parameters.properties" object
    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add parameters
    ik_tool_add_string_param(doc, properties, "path", "Path to file");
    ik_tool_add_string_param(doc, properties, "content", "Content to write");

    // Add properties to parameters
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add properties to parameters"); // LCOV_EXCL_LINE
    }

    // Create "required" array
    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_arr_add_str(doc, required, "path")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add path to required array"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_arr_add_str(doc, required, "content")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add content to required array"); // LCOV_EXCL_LINE
    }

    // Add required to parameters
    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add required to parameters"); // LCOV_EXCL_LINE
    }

    // Add parameters to function
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add parameters to function"); // LCOV_EXCL_LINE
    }

    // Add function to schema
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add function to schema"); // LCOV_EXCL_LINE
    }

    return schema;
}

yyjson_mut_val *ik_tool_build_bash_schema(yyjson_mut_doc *doc)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE

    // Create root schema object
    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "type": "function"
    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to schema"); // LCOV_EXCL_LINE
    }

    // Create "function" object
    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "function.name": "bash"
    if (!yyjson_mut_obj_add_str(doc, function, "name", "bash")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add name field to function"); // LCOV_EXCL_LINE
    }

    // Add "function.description"
    if (!yyjson_mut_obj_add_str(doc, function, "description", "Execute a shell command")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add description field to function"); // LCOV_EXCL_LINE
    }

    // Create "function.parameters" object
    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add "function.parameters.type": "object"
    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to parameters"); // LCOV_EXCL_LINE
    }

    // Create "function.parameters.properties" object
    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add parameters
    ik_tool_add_string_param(doc, properties, "command", "Command to execute");

    // Add properties to parameters
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add properties to parameters"); // LCOV_EXCL_LINE
    }

    // Create "required" array
    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_arr_add_str(doc, required, "command")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add command to required array"); // LCOV_EXCL_LINE
    }

    // Add required to parameters
    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add required to parameters"); // LCOV_EXCL_LINE
    }

    // Add parameters to function
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add parameters to function"); // LCOV_EXCL_LINE
    }

    // Add function to schema
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add function to schema"); // LCOV_EXCL_LINE
    }

    return schema;
}
