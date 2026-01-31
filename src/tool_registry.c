#include "tool_registry.h"

#include "panic.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>


#include "poison.h"
ik_tool_registry_t *ik_tool_registry_create(TALLOC_CTX *ctx)
{
    ik_tool_registry_t *registry = talloc_zero(ctx, ik_tool_registry_t);
    if (registry == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    registry->capacity = 16;
    registry->entries = talloc_zero_array(registry, ik_tool_registry_entry_t, (unsigned int)registry->capacity);
    if (registry->entries == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    registry->count = 0;
    return registry;
}

ik_tool_registry_entry_t *ik_tool_registry_lookup(ik_tool_registry_t *registry, const char *name)
{
    for (size_t i = 0; i < registry->count; i++) {
        if (strcmp(registry->entries[i].name, name) == 0) {
            return &registry->entries[i];
        }
    }
    return NULL;
}

res_t ik_tool_registry_add(ik_tool_registry_t *registry, const char *name, const char *path, yyjson_doc *schema_doc)
{
    // Check if tool already exists (override)
    ik_tool_registry_entry_t *existing = ik_tool_registry_lookup(registry, name);
    if (existing != NULL) {
        // Replace existing entry
        talloc_free(existing->name);
        talloc_free(existing->path);
        if (existing->schema_doc != NULL) {  // LCOV_EXCL_BR_LINE - defensive: schema_doc never NULL in practice
            talloc_free(existing->schema_doc);
        }

        existing->name = talloc_strdup(registry, name);
        if (existing->name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        existing->path = talloc_strdup(registry, path);
        if (existing->path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        existing->schema_doc = schema_doc;
        talloc_steal(registry, schema_doc);  // LCOV_EXCL_BR_LINE
        existing->schema_root = yyjson_doc_get_root(schema_doc);

        return OK(NULL);
    }

    // Grow array if needed
    if (registry->count >= registry->capacity) {
        registry->capacity *= 2;
        registry->entries = talloc_realloc(registry,
                                           registry->entries,
                                           ik_tool_registry_entry_t,
                                           (unsigned int)registry->capacity);
        if (registry->entries == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }

    // Add new entry
    ik_tool_registry_entry_t *entry = &registry->entries[registry->count];
    registry->count++;

    entry->name = talloc_strdup(registry, name);
    if (entry->name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    entry->path = talloc_strdup(registry, path);
    if (entry->path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    entry->schema_doc = schema_doc;
    talloc_steal(registry, schema_doc);  // LCOV_EXCL_BR_LINE
    entry->schema_root = yyjson_doc_get_root(schema_doc);

    return OK(NULL);
}

void ik_tool_registry_clear(ik_tool_registry_t *registry)
{
    for (size_t i = 0; i < registry->count; i++) {
        talloc_free(registry->entries[i].name);
        talloc_free(registry->entries[i].path);
        if (registry->entries[i].schema_doc != NULL) {  // LCOV_EXCL_BR_LINE - defensive: schema_doc never NULL in practice
            talloc_free(registry->entries[i].schema_doc);
        }
    }
    registry->count = 0;
}

yyjson_mut_val *ik_tool_registry_build_all(ik_tool_registry_t *registry, yyjson_mut_doc *doc)  // LCOV_EXCL_BR_LINE
{
    yyjson_mut_val *tools_array = yyjson_mut_arr(doc);
    if (tools_array == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < registry->count; i++) {
        ik_tool_registry_entry_t *entry = &registry->entries[i];

        // Copy schema root to mutable document
        yyjson_mut_val *tool_schema = yyjson_val_mut_copy(doc, entry->schema_root);
        if (tool_schema == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        if (!yyjson_mut_arr_append(tools_array, tool_schema)) {  // LCOV_EXCL_BR_LINE
            PANIC("Failed to append tool to array");  // LCOV_EXCL_LINE
        }
    }

    return tools_array;
}

// Comparator for qsort - sorts entries alphabetically by name
static int32_t compare_entries(const void *a, const void *b)
{
    const ik_tool_registry_entry_t *entry_a = (const ik_tool_registry_entry_t *)a;
    const ik_tool_registry_entry_t *entry_b = (const ik_tool_registry_entry_t *)b;
    return strcmp(entry_a->name, entry_b->name);
}

void ik_tool_registry_sort(ik_tool_registry_t *registry)
{
    // No-op on empty or single-entry registry
    if (registry->count <= 1) {
        return;
    }

    qsort(registry->entries, registry->count, sizeof(ik_tool_registry_entry_t), compare_entries);
}
