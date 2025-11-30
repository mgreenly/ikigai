#include "tool.h"

#include "panic.h"

#include <assert.h>

ik_tool_call_t *ik_tool_call_create(TALLOC_CTX *ctx,
                                    const char *id,
                                    const char *name,
                                    const char *arguments)
{
    assert(id != NULL); // LCOV_EXCL_BR_LINE
    assert(name != NULL); // LCOV_EXCL_BR_LINE
    assert(arguments != NULL); // LCOV_EXCL_BR_LINE

    // Allocate struct on provided context (or talloc root if ctx is NULL)
    ik_tool_call_t *call = talloc(ctx, ik_tool_call_t);
    if (call == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Copy strings as children of the struct
    call->id = talloc_strdup(call, id);
    if (call->id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    call->name = talloc_strdup(call, name);
    if (call->name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    call->arguments = talloc_strdup(call, arguments);
    if (call->arguments == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    return call;
}

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

    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, function, "name", "glob")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, function, "description", "Find files matching a glob pattern")) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_tool_add_string_param(doc, properties, "pattern", "Glob pattern (e.g., 'src/**/*.c')");
    ik_tool_add_string_param(doc, properties, "path", "Base directory (default: cwd)");

    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_arr_add_str(doc, required, "pattern")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    return schema;
}

yyjson_mut_val *ik_tool_build_file_read_schema(yyjson_mut_doc *doc)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, function, "name", "file_read")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, function, "description", "Read contents of a file")) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_tool_add_string_param(doc, properties, "path", "Path to file");

    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_arr_add_str(doc, required, "path")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    return schema;
}

yyjson_mut_val *ik_tool_build_grep_schema(yyjson_mut_doc *doc)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, function, "name", "grep")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, function, "description", "Search file contents for a pattern")) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_tool_add_string_param(doc, properties, "pattern", "Search pattern (regex)");
    ik_tool_add_string_param(doc, properties, "path", "File or directory to search");
    ik_tool_add_string_param(doc, properties, "glob", "File pattern filter (e.g., '*.c')");

    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_arr_add_str(doc, required, "pattern")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    return schema;
}

yyjson_mut_val *ik_tool_build_file_write_schema(yyjson_mut_doc *doc)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, function, "name", "file_write")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, function, "description", "Write content to a file")) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_tool_add_string_param(doc, properties, "path", "Path to file");
    ik_tool_add_string_param(doc, properties, "content", "Content to write");

    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_arr_add_str(doc, required, "path")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_arr_add_str(doc, required, "content")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    return schema;
}

yyjson_mut_val *ik_tool_build_bash_schema(yyjson_mut_doc *doc)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, function, "name", "bash")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, function, "description", "Execute a shell command")) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_tool_add_string_param(doc, properties, "command", "Command to execute");

    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_arr_add_str(doc, required, "command")) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) PANIC("Failed"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) PANIC("Failed"); // LCOV_EXCL_BR_LINE

    return schema;
}

yyjson_mut_val *ik_tool_build_all(yyjson_mut_doc *doc)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE

    // Create array to hold all tools
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    if (arr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Add all 5 tool schemas in order
    yyjson_mut_val *glob_schema = ik_tool_build_glob_schema(doc);
    if (!yyjson_mut_arr_add_val(arr, glob_schema)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add glob schema to array"); // LCOV_EXCL_LINE
    }

    yyjson_mut_val *file_read_schema = ik_tool_build_file_read_schema(doc);
    if (!yyjson_mut_arr_add_val(arr, file_read_schema)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add file_read schema to array"); // LCOV_EXCL_LINE
    }

    yyjson_mut_val *grep_schema = ik_tool_build_grep_schema(doc);
    if (!yyjson_mut_arr_add_val(arr, grep_schema)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add grep schema to array"); // LCOV_EXCL_LINE
    }

    yyjson_mut_val *file_write_schema = ik_tool_build_file_write_schema(doc);
    if (!yyjson_mut_arr_add_val(arr, file_write_schema)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add file_write schema to array"); // LCOV_EXCL_LINE
    }

    yyjson_mut_val *bash_schema = ik_tool_build_bash_schema(doc);
    if (!yyjson_mut_arr_add_val(arr, bash_schema)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add bash schema to array"); // LCOV_EXCL_LINE
    }

    return arr;
}
