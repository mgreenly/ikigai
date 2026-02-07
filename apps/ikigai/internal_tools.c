/**
 * @file internal_tools.c
 * @brief Internal tool implementations (worker thread execution, main thread hooks)
 */

#include "apps/ikigai/internal_tools.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/tool_registry.h"
#include "shared/json_allocator.h"
#include "vendor/yyjson/yyjson.h"

#include "shared/panic.h"
#include "shared/poison.h"

#include <assert.h>
#include <talloc.h>

// No-op verification tool - proves the internal tool lifecycle works end-to-end
static char *internal_noop_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *arguments_json)
{
    assert(ctx != NULL);            // LCOV_EXCL_BR_LINE
    assert(agent != NULL);          // LCOV_EXCL_BR_LINE
    (void)arguments_json;  // Unused - no-op tool ignores arguments

    // Return success JSON
    char *result = talloc_strdup(ctx, "{\"ok\": true}");
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return result;
}

void ik_internal_tools_register(ik_tool_registry_t *registry)
{
    assert(registry != NULL);  // LCOV_EXCL_BR_LINE

    // Create schema document for no-op tool
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);

    // name
    if (!yyjson_mut_obj_add_str(doc, root, "name", "noop")) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // description
    if (!yyjson_mut_obj_add_str(doc, root, "description",
                                "No-op verification tool - proves internal tool infrastructure works"))
        PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // parameters (minimal - no required parameters)
    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, root, "parameters", parameters)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // properties (empty object)
    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // required (empty array)
    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Convert mutable doc to immutable using talloc allocator
    yyjson_alc alc = ik_make_talloc_allocator(registry);
    yyjson_doc *immut_doc = yyjson_mut_doc_imut_copy(doc, &alc);
    if (immut_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_free(doc);

    // Register the no-op tool
    ik_tool_registry_add_internal(registry, "noop", immut_doc, internal_noop_handler, NULL);
}
