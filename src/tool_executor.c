#include "tool_executor.h"

#include "error.h"
#include "panic.h"
#include "paths.h"
#include "tool_external.h"
#include "tool_registry.h"
#include "tool_wrapper.h"

#include <talloc.h>

char *ik_tool_execute_from_registry(TALLOC_CTX *ctx,
                                    ik_tool_registry_t *registry,
                                    ik_paths_t *paths,
                                    const char *agent_id,
                                    const char *tool_name,
                                    const char *arguments)
{
    if (registry == NULL) {
        char *result = ik_tool_wrap_failure(ctx, "Tool registry not initialized", "registry_unavailable");
        if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        return result;
    }

    ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, tool_name);
    if (entry == NULL) {
        char *result = ik_tool_wrap_failure(ctx, "Tool not found in registry", "tool_not_found");
        if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        return result;
    }

    char *translated_args = NULL;
    res_t translate_res = ik_paths_translate_ik_uri_to_path(ctx, paths, arguments, &translated_args);
    if (is_err(&translate_res)) {
        char *result = ik_tool_wrap_failure(ctx, translate_res.err->msg, "translation_failed");
        if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        return result;
    }

    char *raw_result = NULL;
    res_t res = ik_tool_external_exec(ctx, entry->path, agent_id, translated_args, &raw_result);
    if (is_err(&res)) {
        char *result = ik_tool_wrap_failure(ctx, res.err->msg, "execution_failed");
        if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        return result;
    }

    char *translated_result = NULL;
    res_t translate_back_res = ik_paths_translate_path_to_ik_uri(ctx, paths, raw_result, &translated_result);
    if (is_err(&translate_back_res)) {
        char *result = ik_tool_wrap_failure(ctx, translate_back_res.err->msg, "translation_failed");
        if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        return result;
    }

    char *result_json = ik_tool_wrap_success(ctx, translated_result);
    if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return result_json;
}
