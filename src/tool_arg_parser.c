#include "tool.h"

#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

char *ik_tool_arg_get_string(void *parent, const char *arguments_json, const char *key)
{
    // Defensive: return NULL if inputs are NULL
    if (arguments_json == NULL || key == NULL) {
        return NULL;
    }

    // Parse the JSON string
    yyjson_doc *doc = yyjson_read_(arguments_json, strlen(arguments_json), 0);
    if (doc == NULL) {
        return NULL;
    }

    // Get root object
    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (root == NULL || !yyjson_is_obj(root)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL;
    }

    // Get the value for the specified key
    yyjson_val *value = yyjson_obj_get_(root, key);
    if (value == NULL) {
        yyjson_doc_free(doc);
        return NULL;
    }

    // Verify it's a string
    if (!yyjson_is_str(value)) {
        yyjson_doc_free(doc);
        return NULL;
    }

    // Extract the string value
    const char *str_value = yyjson_get_str_(value);
    if (str_value == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL; // LCOV_EXCL_LINE
    }

    // Allocate and copy the string on the parent context
    char *result = talloc_strdup_(parent, str_value);
    if (result == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // Free the JSON document
    yyjson_doc_free(doc);

    return result;
}

bool ik_tool_arg_get_int(const char *arguments_json, const char *key, int *out_value)
{
    // Defensive: return false if inputs are NULL
    if (arguments_json == NULL || key == NULL || out_value == NULL) {
        return false;
    }

    // Parse the JSON string
    yyjson_doc *doc = yyjson_read_(arguments_json, strlen(arguments_json), 0);
    if (doc == NULL) {
        return false;
    }

    // Get root object
    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (root == NULL || !yyjson_is_obj(root)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return false;
    }

    // Get the value for the specified key
    yyjson_val *value = yyjson_obj_get_(root, key);
    if (value == NULL) {
        yyjson_doc_free(doc);
        return false;
    }

    // Verify it's an integer
    if (!yyjson_is_int(value)) {
        yyjson_doc_free(doc);
        return false;
    }

    // Extract the integer value
    int64_t int_value = yyjson_get_sint_(value);

    // Free the JSON document
    yyjson_doc_free(doc);

    // Set the output value
    *out_value = (int)int_value;

    return true;
}
