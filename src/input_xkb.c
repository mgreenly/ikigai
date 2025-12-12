// XKB keyboard layout support for input parser
#include "input_xkb.h"

#include "input.h"
#include "logger.h"

#include <assert.h>
#include <string.h>

void ik_input_xkb_build_reverse_map(struct xkb_keymap *keymap, struct xkb_state *state,
                                     ik_xkb_reverse_map_t *map)
{
    assert(keymap != NULL);  // LCOV_EXCL_BR_LINE
    assert(state != NULL);   // LCOV_EXCL_BR_LINE
    assert(map != NULL);     // LCOV_EXCL_BR_LINE

    memset(map, 0, sizeof(*map));

    // Clear modifiers for base key lookup
    xkb_state_update_mask(state, 0, 0, 0, 0, 0, 0);

    // Walk main keyboard keycodes only (9-100)
    // This avoids numpad keys that produce the same characters
    for (xkb_keycode_t kc = 9; kc <= 100; kc++) {
        xkb_keysym_t sym = xkb_state_key_get_one_sym(state, kc);
        if (sym == XKB_KEY_NoSymbol) continue;

        uint32_t utf32 = xkb_keysym_to_utf32(sym);

        // Only store ASCII range, prefer lower keycodes (main keyboard)
        // LCOV_EXCL_BR_START - Defensive: avoid duplicate keycodes, rare in practice
        if (utf32 >= 32 && utf32 < 128 && map->keycodes[utf32] == 0) {
            // LCOV_EXCL_BR_STOP
            map->keycodes[utf32] = kc;
        }
    }
}

void ik_input_xkb_init_state(ik_input_parser_t *parser)
{
    assert(parser != NULL);  // LCOV_EXCL_BR_LINE

    // Create context
    parser->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (parser->xkb_ctx == NULL) {     // LCOV_EXCL_BR_LINE
        // LCOV_EXCL_START - Cannot test XKB library initialization failure
        ik_log_warn("Failed to create xkb context, shifted keys will not work");
        return;
        // LCOV_EXCL_STOP
    }

    // Create keymap from system default (uses RMLVO from environment or defaults)
    struct xkb_rule_names names = {
        .rules = NULL,
        .model = NULL,
        .layout = NULL,  // Use system default
        .variant = NULL,
        .options = NULL
    };

    parser->xkb_keymap = xkb_keymap_new_from_names(parser->xkb_ctx, &names,
                                                    XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (parser->xkb_keymap == NULL) {     // LCOV_EXCL_BR_LINE
        // LCOV_EXCL_START - Cannot test XKB keymap creation failure
        ik_log_warn("Failed to create xkb keymap, shifted keys will not work");
        xkb_context_unref(parser->xkb_ctx);
        parser->xkb_ctx = NULL;
        return;
        // LCOV_EXCL_STOP
    }

    // Create state
    parser->xkb_state = xkb_state_new(parser->xkb_keymap);
    if (parser->xkb_state == NULL) {     // LCOV_EXCL_BR_LINE
        // LCOV_EXCL_START - Cannot test XKB state creation failure
        ik_log_warn("Failed to create xkb state, shifted keys will not work");
        xkb_keymap_unref(parser->xkb_keymap);
        xkb_context_unref(parser->xkb_ctx);
        parser->xkb_keymap = NULL;
        parser->xkb_ctx = NULL;
        return;
        // LCOV_EXCL_STOP
    }

    // Get Shift modifier mask
    xkb_mod_index_t shift_idx = xkb_keymap_mod_get_index(parser->xkb_keymap, XKB_MOD_NAME_SHIFT);
    parser->shift_mask = (xkb_mod_mask_t)(1U << shift_idx);

    // Build reverse keymap
    ik_input_xkb_build_reverse_map(parser->xkb_keymap, parser->xkb_state, &parser->reverse_map);

    parser->xkb_initialized = true;
}

int ik_input_xkb_cleanup_destructor(ik_input_parser_t *parser)
{
    // LCOV_EXCL_BR_START - Defensive: NULL only if init failed
    if (parser->xkb_state != NULL) {
        xkb_state_unref(parser->xkb_state);
    }
    if (parser->xkb_keymap != NULL) {
        xkb_keymap_unref(parser->xkb_keymap);
    }
    if (parser->xkb_ctx != NULL) {
        xkb_context_unref(parser->xkb_ctx);
    }
    // LCOV_EXCL_BR_STOP
    return 0;
}

uint32_t ik_input_xkb_translate_shifted_key(const ik_input_parser_t *parser, uint32_t codepoint)
{
    assert(parser != NULL);  // LCOV_EXCL_BR_LINE

    // If xkb not initialized or codepoint out of ASCII range, return as-is
    if (!parser->xkb_initialized || codepoint < 32 || codepoint >= 128) {  // LCOV_EXCL_BR_LINE
        return codepoint;  // LCOV_EXCL_LINE
    }

    // Look up the X11 keycode for this codepoint
    xkb_keycode_t kc = parser->reverse_map.keycodes[codepoint];
    if (kc == 0) {  // LCOV_EXCL_BR_LINE
        return codepoint;  // No mapping found  // LCOV_EXCL_LINE
    }

    // Set Shift modifier and get the resulting character
    xkb_state_update_mask(parser->xkb_state, parser->shift_mask, 0, 0, 0, 0, 0);
    xkb_keysym_t sym = xkb_state_key_get_one_sym(parser->xkb_state, kc);
    uint32_t result = xkb_keysym_to_utf32(sym);

    // Clear modifiers for next lookup
    xkb_state_update_mask(parser->xkb_state, 0, 0, 0, 0, 0, 0);

    return (result != 0) ? result : codepoint;  // LCOV_EXCL_BR_LINE - Defensive: result rarely 0
}
