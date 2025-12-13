#include "tool_response.h"
#include "panic.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

res_t ik_tool_response_error(TALLOC_CTX *ctx, const char *error_msg, char **out) {
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(error_msg != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);       // LCOV_EXCL_BR_LINE

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_bool(doc, root, "success", false);
    yyjson_mut_obj_add_str(doc, root, "error", error_msg);

    char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    char *result = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(doc);

    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    *out = result;
    return OK(result);
}

res_t ik_tool_response_success(TALLOC_CTX *ctx, const char *output, char **out) {
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(output != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);   // LCOV_EXCL_BR_LINE

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_bool(doc, root, "success", true);
    yyjson_mut_obj_add_str(doc, root, "output", output);

    char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    char *result = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(doc);

    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    *out = result;
    return OK(result);
}

res_t ik_tool_response_success_ex(TALLOC_CTX *ctx,
                                   const char *output,
                                   ik_tool_field_adder_t add_fields,
                                   void *user_ctx,
                                   char **out) {
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(output != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);    // LCOV_EXCL_BR_LINE

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_bool(doc, root, "success", true);
    yyjson_mut_obj_add_str(doc, root, "output", output);

    // Call the callback to add custom fields
    if (add_fields != NULL) {
        add_fields(doc, root, user_ctx);
    }

    char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    char *result = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(doc);

    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    *out = result;
    return OK(result);
}
