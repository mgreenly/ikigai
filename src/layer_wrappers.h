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
                                  size_t *document_height,
                                  uint64_t *render_elapsed_us);

// Set navigation context on separator layer (for agent tree navigation)
void ik_separator_layer_set_nav_context(ik_layer_t *layer,
                                        const char *parent_uuid,
                                        const char *prev_sibling_uuid,
                                        const char *current_uuid,
                                        const char *next_sibling_uuid,
                                        size_t child_count);

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
    size_t frame_index;      // Current animation frame (0-9)
    bool visible;            // Whether spinner is visible
    int64_t last_advance_ms; // Timestamp of last advancement (CLOCK_MONOTONIC)
} ik_spinner_state_t;

// Create spinner layer (renders animated spinner)
// The layer shows an animated spinner with a message while waiting for LLM
ik_layer_t *ik_spinner_layer_create(TALLOC_CTX *ctx, const char *name, ik_spinner_state_t *state);

// Get current spinner frame string
const char *ik_spinner_get_frame(const ik_spinner_state_t *state);

// Advance to next spinner frame (cycles through animation)
void ik_spinner_advance(ik_spinner_state_t *state);

// Check elapsed time and advance spinner if >= 80ms since last advance
// Returns true if spinner was advanced, false otherwise
bool ik_spinner_maybe_advance(ik_spinner_state_t *state, int64_t now_ms);

// Create completion layer (wraps completion context)
// The layer renders tab completion suggestions below input buffer
// visible_ptr and completion_ptr are raw pointers that must remain valid
ik_layer_t *ik_completion_layer_create(TALLOC_CTX *ctx, const char *name, ik_completion_t **completion_ptr);

// Create status layer (renders agent model and thinking level)
// The layer shows a separator line and status row with model/thinking info
// visible_ptr, model_ptr, and thinking_level_ptr are raw pointers that must remain valid
ik_layer_t *ik_status_layer_create(TALLOC_CTX *ctx,
                                   const char *name,
                                   bool *visible_ptr,
                                   char **model_ptr,
                                   int *thinking_level_ptr);

// Create banner layer (renders colored ASCII owl face with version info)
// The layer shows a 6-row banner at the top of the terminal
// visible_ptr is a raw pointer that must remain valid
ik_layer_t *ik_banner_layer_create(TALLOC_CTX *ctx,
                                   const char *name,
                                   bool *visible_ptr);

#endif // IK_LAYER_WRAPPERS_H
