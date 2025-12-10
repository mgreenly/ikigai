#ifndef IK_LAYER_WRAPPERS_H
#define IK_LAYER_WRAPPERS_H

#include "error.h"
#include "layer.h"
#include "completion.h"
#include <stdbool.h>
#include <stddef.h>
#include <talloc.h>

// Forward declarations for wrapped types
typedef struct ik_scrollback_t ik_scrollback_t;
typedef struct ik_input_buffer_t ik_input_buffer_t;

// Create scrollback layer (wraps existing scrollback buffer)
// The layer delegates to ik_scrollback_t for height calculation and rendering
ik_layer_t *ik_scrollback_layer_create(TALLOC_CTX *ctx, const char *name, ik_scrollback_t *scrollback);

// Create separator layer (renders horizontal line)
// The layer renders a separator line of dashes across the terminal width
ik_layer_t *ik_separator_layer_create(TALLOC_CTX *ctx, const char *name, bool *visible_ptr);

// Set debug info pointers on separator layer (optional - for debugging viewport issues)
void ik_separator_layer_set_debug(ik_layer_t *layer,
                                  size_t *viewport_offset,
                                  size_t *viewport_row,
                                  size_t *viewport_height,
                                  size_t *document_height);

// Create input buffer layer (wraps existing input buffer)
// The layer delegates to ik_input_buffer_t for height calculation and rendering
// visible_ptr and text_ptr/len_ptr are raw pointers that must remain valid
ik_layer_t *ik_input_layer_create(TALLOC_CTX *ctx,
                                  const char *name,
                                  bool *visible_ptr,
                                  const char **text_ptr,
                                  size_t *text_len_ptr);

// Spinner state structure
typedef struct {
    size_t frame_index;  // Current animation frame (0-3)
    bool visible;        // Whether spinner is visible
} ik_spinner_state_t;

// Create spinner layer (renders animated spinner)
// The layer shows an animated spinner with a message while waiting for LLM
ik_layer_t *ik_spinner_layer_create(TALLOC_CTX *ctx, const char *name, ik_spinner_state_t *state);

// Get current spinner frame character
char ik_spinner_get_frame(const ik_spinner_state_t *state);

// Advance to next spinner frame (cycles through animation)
void ik_spinner_advance(ik_spinner_state_t *state);

// Create completion layer (wraps completion context)
// The layer renders tab completion suggestions below input buffer
// visible_ptr and completion_ptr are raw pointers that must remain valid
ik_layer_t *ik_completion_layer_create(TALLOC_CTX *ctx,
                                       const char *name,
                                       ik_completion_t **completion_ptr);

#endif // IK_LAYER_WRAPPERS_H
