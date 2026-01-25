#ifndef IK_DOC_CACHE_H
#define IK_DOC_CACHE_H

#include "error.h"
#include "paths.h"

#include <talloc.h>

// Opaque document cache type
typedef struct ik_doc_cache ik_doc_cache_t;

// Create a new document cache
// ctx: talloc context for allocation
// paths: path resolver for ik:// URI translation
// Returns: new cache instance
ik_doc_cache_t *ik_doc_cache_create(TALLOC_CTX *ctx, ik_paths_t *paths);

// Get document content from cache
// Loads from filesystem if not cached
// Returns: OK(content) on success, ERR on file read failure
// Note: Returned content is owned by cache, do NOT free
res_t ik_doc_cache_get(ik_doc_cache_t *cache, const char *path, char **out_content);

// Invalidate cached content for a specific path
// No-op if path is not cached
void ik_doc_cache_invalidate(ik_doc_cache_t *cache, const char *path);

// Clear entire cache
void ik_doc_cache_clear(ik_doc_cache_t *cache);

#endif // IK_DOC_CACHE_H
