#include "tool.h"

#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

ik_tool_call_t *ik_tool_call_create(TALLOC_CTX *ctx,
                                    const char *id,
                                    const char *name,
                                    const char *arguments)
{
    assert(id != NULL); // LCOV_EXCL_BR_LINE
    assert(name != NULL); // LCOV_EXCL_BR_LINE
    assert(arguments != NULL); // LCOV_EXCL_BR_LINE

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

void ik_tool_add_string_parameter(yyjson_mut_doc *doc,
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

// Static definitions for glob tool schema
static const ik_tool_param_def_t glob_params[] = {
    {"pattern", "Glob pattern (e.g., 'src/**/*.c')", true},
    {"path", "Base directory (default: cwd)", false}
};

static const ik_tool_schema_def_t glob_schema_def = {
    .name = "glob",
    .description = "Find files matching a glob pattern",
    .params = glob_params,
    .param_count = 2
};

yyjson_mut_val *ik_tool_build_glob_schema(yyjson_mut_doc *doc)
{
    return ik_tool_build_schema_from_def(doc, &glob_schema_def);
}

// Static definitions for file_read tool schema
static const ik_tool_param_def_t file_read_params[] = {
    {"path", "Path to file", true}
};

static const ik_tool_schema_def_t file_read_schema_def = {
    .name = "file_read",
    .description = "Read contents of a file",
    .params = file_read_params,
    .param_count = 1
};

yyjson_mut_val *ik_tool_build_file_read_schema(yyjson_mut_doc *doc)
{
    return ik_tool_build_schema_from_def(doc, &file_read_schema_def);
}

// Static definitions for grep tool schema
static const ik_tool_param_def_t grep_params[] = {
    {"pattern", "Search pattern (regex)", true},
    {"path", "File or directory to search", false},
    {"glob", "File pattern filter (e.g., '*.c')", false}
};

static const ik_tool_schema_def_t grep_schema_def = {
    .name = "grep",
    .description = "Search file contents for a pattern",
    .params = grep_params,
    .param_count = 3
};

yyjson_mut_val *ik_tool_build_grep_schema(yyjson_mut_doc *doc)
{
    return ik_tool_build_schema_from_def(doc, &grep_schema_def);
}

// Static definitions for file_write tool schema
static const ik_tool_param_def_t file_write_params[] = {
    {"path", "Path to file", true},
    {"content", "Content to write", true}
};

static const ik_tool_schema_def_t file_write_schema_def = {
    .name = "file_write",
    .description = "Write content to a file",
    .params = file_write_params,
    .param_count = 2
};

yyjson_mut_val *ik_tool_build_file_write_schema(yyjson_mut_doc *doc)
{
    return ik_tool_build_schema_from_def(doc, &file_write_schema_def);
}

// Static definitions for bash tool schema
static const ik_tool_param_def_t bash_params[] = {
    {"command", "Command to execute", true}
};

static const ik_tool_schema_def_t bash_schema_def = {
    .name = "bash",
    .description = "Execute a shell command",
    .params = bash_params,
    .param_count = 1
};

yyjson_mut_val *ik_tool_build_bash_schema(yyjson_mut_doc *doc)
{
    return ik_tool_build_schema_from_def(doc, &bash_schema_def);
}

yyjson_mut_val *ik_tool_build_schema_from_def(yyjson_mut_doc *doc,
                                               const ik_tool_schema_def_t *def)
{
    assert(doc != NULL);  // LCOV_EXCL_BR_LINE
    assert(def != NULL);  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) PANIC("Failed");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, function, "name", def->name)) PANIC("Failed");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, function, "description", def->description)) PANIC("Failed");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Add all parameters from definition
    for (size_t i = 0; i < def->param_count; i++) {
        ik_tool_add_string_parameter(doc, properties, def->params[i].name, def->params[i].description);
    }

    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) PANIC("Failed");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) PANIC("Failed");  // LCOV_EXCL_BR_LINE

    // Build required array from params marked as required
    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < def->param_count; i++) {
        if (def->params[i].required) {
            if (!yyjson_mut_arr_add_str(doc, required, def->params[i].name)) PANIC("Failed");  // LCOV_EXCL_BR_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) PANIC("Failed");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) PANIC("Failed");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) PANIC("Failed");  // LCOV_EXCL_BR_LINE

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

char *ik_tool_truncate_output(void *parent, const char *output, size_t max_size)
{
    // If output is NULL, return NULL
    if (output == NULL) {
        return NULL;
    }

    size_t output_len = strlen(output);

    // If output length <= max_size, return talloc_strdup of output
    if (output_len <= max_size) {
        return talloc_strdup(parent, output);
    }

    // Output is over limit, truncate and add indicator
    // Allocate space for truncated content + indicator
    // "[Output truncated: showing first X of Y bytes]"
    char *truncated = talloc_array(parent, char, (unsigned int)(max_size + 200));
    if (truncated == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Copy first max_size bytes
    strncpy(truncated, output, max_size);
    truncated[max_size] = '\0';

    // Append indicator
    char indicator[200];
    int written = snprintf(indicator, sizeof(indicator),
                           "[Output truncated: showing first %zu of %zu bytes]",
                           max_size, output_len);
    if (written < 0 || written >= (int)sizeof(indicator)) { // LCOV_EXCL_BR_LINE
        PANIC("Indicator formatting failed"); // LCOV_EXCL_LINE
    }

    // Resize the result to accommodate both truncated content and indicator
    size_t new_size = max_size + strlen(indicator) + 1;
    truncated = talloc_realloc(parent, truncated, char, (unsigned int)new_size);
    if (truncated == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    strcat(truncated, indicator);

    return truncated;
}

char *ik_tool_result_add_limit_metadata(void *parent, const char *result_json, int32_t max_tool_turns)
{
    if (result_json == NULL) {
        return NULL;
    }

    yyjson_doc *doc = yyjson_read(result_json, strlen(result_json), 0);
    if (doc == NULL) {
        return NULL;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root == NULL || !yyjson_is_obj(root)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL;
    }

    yyjson_mut_doc *mut_doc = yyjson_mut_doc_new(NULL);
    if (mut_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *mut_root = yyjson_val_mut_copy(mut_doc, root);
    if (mut_root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(mut_doc, mut_root);

    if (!yyjson_mut_obj_add_bool(mut_doc, mut_root, "limit_reached", true)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char msg[256];
    int n = snprintf(msg, sizeof(msg), "Tool call limit reached (%d). Stopping tool loop.", (int)max_tool_turns);
    if (n < 0 || n >= (int)sizeof(msg)) PANIC("Format failed");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(mut_doc, mut_root, "limit_message", msg)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t len = 0;
    char *json_str = (char *)yyjson_mut_write(mut_doc, 0, &len);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    char *result = talloc_strdup(parent, json_str);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    free(json_str);  // LCOV_EXCL_BR_LINE

    yyjson_doc_free(doc);
    yyjson_mut_doc_free(mut_doc);

    return result;
}
