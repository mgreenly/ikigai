#include "doc_cache.h"

#include "file_utils.h"
#include "panic.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

// Cache entry
typedef struct {
    char *path;      // Canonical path (ik:// translated to filesystem)
    char *content;   // Cached content
} ik_doc_cache_entry_t;

// Document cache structure
struct ik_doc_cache {
    ik_paths_t *paths;                 // Path resolver
    ik_doc_cache_entry_t **entries;    // Array of cache entries
    size_t count;                      // Number of entries
    size_t capacity;                   // Allocated capacity
};

ik_doc_cache_t *ik_doc_cache_create(TALLOC_CTX *ctx, ik_paths_t *paths)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(paths != NULL);  // LCOV_EXCL_BR_LINE

    ik_doc_cache_t *cache = talloc_zero(ctx, ik_doc_cache_t);
    if (cache == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    cache->paths = paths;
    cache->entries = NULL;
    cache->count = 0;
    cache->capacity = 0;

    return cache;
}

res_t ik_doc_cache_get(ik_doc_cache_t *cache, const char *path, char **out_content)
{
    assert(cache != NULL);      // LCOV_EXCL_BR_LINE
    assert(path != NULL);       // LCOV_EXCL_BR_LINE
    assert(out_content != NULL); // LCOV_EXCL_BR_LINE

    // Translate ik:// URIs to filesystem paths
    char *canonical_path = NULL;
    res_t translate_result = ik_paths_translate_ik_uri_to_path(cache, cache->paths, path, &canonical_path);
    if (is_err(&translate_result)) {
        return translate_result;
    }

    // Search for existing entry
    for (size_t i = 0; i < cache->count; i++) {
        if (strcmp(cache->entries[i]->path, canonical_path) == 0) {
            // Cache hit
            talloc_free(canonical_path);
            *out_content = cache->entries[i]->content;
            return OK(NULL);
        }
    }

    // Cache miss - load file
    char *content = NULL;
    res_t read_result = ik_file_read_all(cache, canonical_path, &content, NULL);
    if (is_err(&read_result)) {
        talloc_free(canonical_path);
        return read_result;
    }

    // Add to cache - expand array if needed
    if (cache->count >= cache->capacity) {
        size_t new_capacity = cache->capacity == 0 ? 4 : cache->capacity * 2;
        ik_doc_cache_entry_t **new_entries = talloc_realloc(cache, cache->entries,
                                                             ik_doc_cache_entry_t *,
                                                             (unsigned int)new_capacity);
        if (new_entries == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        cache->entries = new_entries;
        cache->capacity = new_capacity;
    }

    // Create entry
    ik_doc_cache_entry_t *entry = talloc_zero(cache, ik_doc_cache_entry_t);
    if (entry == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    entry->path = talloc_steal(entry, canonical_path);
    entry->content = talloc_steal(entry, content);

    cache->entries[cache->count++] = entry;

    *out_content = entry->content;
    return OK(NULL);
}

void ik_doc_cache_invalidate(ik_doc_cache_t *cache, const char *path)
{
    assert(cache != NULL);  // LCOV_EXCL_BR_LINE
    assert(path != NULL);   // LCOV_EXCL_BR_LINE

    // Translate ik:// URIs to filesystem paths
    char *canonical_path = NULL;
    res_t translate_result = ik_paths_translate_ik_uri_to_path(cache, cache->paths, path, &canonical_path);
    if (is_err(&translate_result)) {
        // If translation fails, can't invalidate - just return
        return;
    }

    // Find and remove entry
    for (size_t i = 0; i < cache->count; i++) {
        if (strcmp(cache->entries[i]->path, canonical_path) == 0) {
            // Free entry
            talloc_free(cache->entries[i]);

            // Shift remaining entries
            for (size_t j = i + 1; j < cache->count; j++) {
                cache->entries[j - 1] = cache->entries[j];
            }
            cache->count--;

            talloc_free(canonical_path);
            return;
        }
    }

    talloc_free(canonical_path);
}

void ik_doc_cache_clear(ik_doc_cache_t *cache)
{
    assert(cache != NULL);  // LCOV_EXCL_BR_LINE

    // Free all entries
    for (size_t i = 0; i < cache->count; i++) {
        talloc_free(cache->entries[i]);
    }

    // Reset array
    cache->count = 0;
}
