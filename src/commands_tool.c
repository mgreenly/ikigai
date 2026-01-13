/**
 * @file commands_tool.c
 * @brief Tool management command implementations (/tool, /refresh)
 */

#include "commands_tool.h"

#include "panic.h"
#include "paths.h"
#include "repl.h"
#include "scrollback.h"
#include "shared.h"
#include "tool_discovery.h"
#include "tool_registry.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <talloc.h>

res_t ik_cmd_tool(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE

    ik_tool_registry_t *registry = repl->shared->tool_registry;
    if (registry == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Tool registry not initialized");  // LCOV_EXCL_LINE
    }

    // If args provided, show schema for specific tool
    if (args != NULL && args[0] != '\0') {
        // Skip leading whitespace
        while (*args == ' ' || *args == '\t') args++;

        if (*args == '\0') {
            // Only whitespace, treat as list all
            args = NULL;
        }
    }

    if (args != NULL) {
        // Show schema for specific tool
        ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, args);
        if (entry == NULL) {
            char *msg = talloc_asprintf(ctx, "Tool not found: %s\n", args);
            if (msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            res_t result = ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
            if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
                talloc_free(msg);  // LCOV_EXCL_LINE
                return result;  // LCOV_EXCL_LINE
            }
            talloc_free(msg);
            return OK(NULL);
        }

        // Format schema as pretty JSON
        char *schema_json = yyjson_val_write(entry->schema_root, YYJSON_WRITE_PRETTY, NULL);
        if (schema_json == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }

        char *output = talloc_asprintf(ctx, "Tool: %s\nPath: %s\nSchema:\n%s\n",
                                       entry->name, entry->path, schema_json);
        free(schema_json);  // yyjson uses malloc, not talloc
        if (output == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        res_t result = ik_scrollback_append_line(repl->current->scrollback, output, strlen(output));
        if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
            talloc_free(output);  // LCOV_EXCL_LINE
            return result;  // LCOV_EXCL_LINE
        }
        talloc_free(output);
        return OK(NULL);
    }

    // List all tools
    if (registry->count == 0) {
        char *msg = talloc_strdup(ctx, "No tools available\n");
        if (msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        res_t result = ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
            talloc_free(msg);  // LCOV_EXCL_LINE
            return result;  // LCOV_EXCL_LINE
        }
        talloc_free(msg);
        return OK(NULL);
    }

    // Build list of tools
    char *list = talloc_strdup(ctx, "Available tools:\n");
    if (list == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < registry->count; i++) {
        ik_tool_registry_entry_t *entry = &registry->entries[i];
        char *line = talloc_asprintf(ctx, "  %s (%s)\n", entry->name, entry->path);
        if (line == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        char *new_list = talloc_asprintf(ctx, "%s%s", list, line);
        if (new_list == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        talloc_free(list);
        talloc_free(line);
        list = new_list;
    }

    res_t result = ik_scrollback_append_line(repl->current->scrollback, list, strlen(list));
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
        talloc_free(list);  // LCOV_EXCL_LINE
        return result;  // LCOV_EXCL_LINE
    }
    talloc_free(list);
    return OK(NULL);
}

res_t ik_cmd_refresh(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)args;  // Unused for /refresh

    ik_tool_registry_t *registry = repl->shared->tool_registry;
    if (registry == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Tool registry not initialized");  // LCOV_EXCL_LINE
    }

    // Clear existing registry
    ik_tool_registry_clear(registry);

    // Get tool directories from paths
    ik_paths_t *paths = repl->shared->paths;
    const char *system_dir = ik_paths_get_tools_system_dir(paths);
    const char *user_dir = ik_paths_get_tools_user_dir(paths);
    const char *project_dir = ik_paths_get_tools_project_dir(paths);

    // Run discovery
    res_t result = ik_tool_discovery_run(ctx, system_dir, user_dir, project_dir, registry);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE - OOM or corruption in discovery
        return result;  // LCOV_EXCL_LINE
    }

    // Report results
    char *msg = talloc_asprintf(ctx, "Tool registry refreshed: %zu tools loaded\n", registry->count);
    if (msg == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    result = ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
        talloc_free(msg);  // LCOV_EXCL_LINE
        return result;  // LCOV_EXCL_LINE
    }
    talloc_free(msg);
    return OK(NULL);
}
