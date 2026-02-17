#ifndef IK_FRAMEBUFFER_INSPECT_HELPER_H
#define IK_FRAMEBUFFER_INSPECT_HELPER_H

#include <stdbool.h>
#include <talloc.h>

// Check if framebuffer JSON response contains the given text
// Searches through the rows array for the text substring
bool ik_fb_contains_text(const char *framebuffer_json, const char *text);

// Extract text content from a specific row in framebuffer JSON
// Returns talloc-allocated string on ctx, or NULL if row not found
char *ik_fb_get_row_text(TALLOC_CTX *ctx, const char *framebuffer_json, int row);

// Check if framebuffer JSON is a valid response (has rows array)
bool ik_fb_is_valid(const char *framebuffer_json);

#endif // IK_FRAMEBUFFER_INSPECT_HELPER_H
