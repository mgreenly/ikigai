// XKB keyboard layout support for input parser
#ifndef IK_INPUT_XKB_H
#define IK_INPUT_XKB_H

#include "apps/ikigai/input.h"

#include <inttypes.h>
#include <xkbcommon/xkbcommon.h>

// Build reverse keymap: Unicode codepoint (ASCII) -> X11 keycode
// Only considers main keyboard keycodes (9-100) to avoid numpad duplicates
void ik_input_xkb_build_reverse_map(struct xkb_keymap *keymap, struct xkb_state *state, ik_xkb_reverse_map_t *map);

// Initialize xkbcommon state for keyboard layout translation
void ik_input_xkb_init_state(ik_input_parser_t *parser);

// Cleanup xkbcommon state (talloc destructor)
int ik_input_xkb_cleanup_destructor(ik_input_parser_t *parser);

// Translate CSI u keycode with Shift modifier using xkbcommon
// Returns the shifted character, or the original codepoint if translation fails
uint32_t ik_input_xkb_translate_shifted_key(const ik_input_parser_t *parser, uint32_t codepoint);

#endif // IK_INPUT_XKB_H
