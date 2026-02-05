/**
 * @file json_allocator.h
 * @brief talloc-based allocator for yyjson
 *
 * Provides a yyjson allocator that uses talloc, enabling automatic memory
 * management for JSON parsing. All JSON memory becomes part of the talloc
 * hierarchy, eliminating the need for manual cleanup.
 */

#ifndef IK_JSON_ALLOCATOR_H
#define IK_JSON_ALLOCATOR_H

#include <talloc.h>
#include "vendor/yyjson/yyjson.h"

/**
 * Create a yyjson allocator that uses talloc
 *
 * @param ctx The talloc context to allocate under
 * @return A yyjson allocator configured to use talloc
 *
 * Usage:
 *   yyjson_alc allocator = ik_make_talloc_allocator(ctx);
 *   yyjson_doc *doc = yyjson_read_opts(json_str, len, 0, &allocator, NULL);
 *   // All JSON memory is now in talloc hierarchy - no manual cleanup needed
 */
yyjson_alc ik_make_talloc_allocator(TALLOC_CTX *ctx);

#endif /* IK_JSON_ALLOCATOR_H */
