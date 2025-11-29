#include "tool.h"

#include "panic.h"

#include <assert.h>

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

    // Create "pattern" property object
    yyjson_mut_val *pattern = yyjson_mut_obj(doc);
    if (pattern == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, pattern, "type", "string")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to pattern"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_str(doc, pattern, "description", "Glob pattern (e.g., 'src/**/*.c')")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add description field to pattern"); // LCOV_EXCL_LINE
    }

    // Create "path" property object
    yyjson_mut_val *path = yyjson_mut_obj(doc);
    if (path == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, path, "type", "string")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add type field to path"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_str(doc, path, "description", "Base directory (default: cwd)")) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add description field to path"); // LCOV_EXCL_LINE
    }

    // Add pattern and path to properties
    if (!yyjson_mut_obj_add_val(doc, properties, "pattern", pattern)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add pattern to properties"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, properties, "path", path)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add path to properties"); // LCOV_EXCL_LINE
    }

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
