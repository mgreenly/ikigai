# CSI u Num Lock Modifier Bug

## Problem
Input appears frozen on ghostty/foot/kitty when Num Lock is enabled. Works on alacritty/wezterm/gnome-terminal.

## Root Cause
CSI u parser in `src/input_escape.c` only handles `modifiers==1` (no modifiers) and `modifiers==2` (Shift). Terminals that include lock state in CSI u sequences send `modifiers=129` when Num Lock is on (1 + 128).

## Terminal Behavior Differences
- **alacritty/wezterm/gnome-terminal**: Don't include lock state bits in CSI u modifiers
- **ghostty/foot/kitty**: Include Num Lock (bit 7 = 128) and Caps Lock (bit 6 = 64) in modifiers

## CSI u Modifier Encoding
```
Bit 0 (1):   Shift
Bit 1 (2):   Alt
Bit 2 (4):   Ctrl
Bit 3 (8):   Super
Bit 4 (16):  Hyper
Bit 5 (32):  Meta
Bit 6 (64):  Caps Lock
Bit 7 (128): Num Lock
```

Base modifier value is 1, so:
- No modifiers: `modifiers = 1`
- Shift only: `modifiers = 2` (1 + 1)
- Num Lock only: `modifiers = 129` (1 + 128)
- Caps Lock only: `modifiers = 65` (1 + 64)

## Debug Evidence
Pressing 'a' with Num Lock on in ghostty sends:
```
ESC [ 97 ; 129 u
```
- keycode = 97 ('a')
- modifiers = 129 (1 + 128 = base + Num Lock)

Parser checked `modifiers == 1` which failed, so it returned `IK_INPUT_UNKNOWN`.

## Fix (Minimal)
Mask out Caps Lock and Num Lock bits before checking modifiers:

```c
// Mask out Caps Lock (bit 6 = 64) and Num Lock (bit 7 = 128)
int32_t effective_mods = modifiers & 0x3F;

// Then use effective_mods instead of modifiers for all checks
if (keycode >= 32 && keycode <= 126 && effective_mods == 1) {
    // ...
}
```

## Recommended Fix (Cleaner)
Separate the base offset from modifier bits for clarity:

```c
int32_t mod_bits = modifiers - 1;           // Remove base offset (CSI u uses base=1)
int32_t action_mods = mod_bits & 0x3F;      // Mask out lock states (bits 6-7)

// Check action modifiers only:
if (action_mods == 0) { /* no modifiers */ }
if (action_mods == 0x01) { /* Shift only */ }
if (action_mods & 0x04) { /* Ctrl pressed (possibly with others) */ }
```

This approach:
1. Explicitly removes the base=1 offset first
2. Cleanly separates "what keys are pressed" from "what lock states are active"
3. Makes the bit manipulation self-documenting
4. Is future-proof if protocol adds more lock state bits

Consider adding constants for readability:
```c
#define CSI_U_MOD_SHIFT  0x01
#define CSI_U_MOD_ALT    0x02
#define CSI_U_MOD_CTRL   0x04
#define CSI_U_MOD_SUPER  0x08
#define CSI_U_MOD_HYPER  0x10
#define CSI_U_MOD_META   0x20
#define CSI_U_ACTION_MASK 0x3F  // Bits 0-5 only
```

## Files to Change
- `src/input_escape.c`: Refactor `parse_csi_u_sequence()` to use cleaner modifier handling

## Testing
1. Build with fix
2. Enable Num Lock
3. Test in ghostty/foot/kitty - should now accept input
4. Test in alacritty/wezterm - should still work
5. Test with Caps Lock enabled
6. Test Shift+letter produces uppercase (action_mods == 0x01)
