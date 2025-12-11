# Enhanced Keyboard Protocol - Implementation Guide

Quick reference for implementing universal terminal keyboard protocol support.

## TL;DR - Universal Strategy

**Use flag 9 everywhere:**

```c
// Enable
write(STDOUT, "\x1b[>9u", 6);

// Handle input
while (reading_input) {
    keycode = parse_csi_u_sequence();

    // Filter Alacritty modifier keys
    if (keycode > 50000) {
        continue;  // Skip modifier-only events
    }

    // Process actual key
    handle_key(keycode, modifiers);
}

// Disable
write(STDOUT, "\x1b[<u", 5);
```

## Why Flag 9?

| Terminal | Flag 1 | Flag 9 | Notes |
|----------|--------|--------|-------|
| Kitty | ✓ | ✓ | Works with both |
| foot | ✓ | ✓ | Works with both |
| Ghostty | ✓ | ✓ | Works with both |
| WezTerm | ✓ | ✓ | Requires config |
| Alacritty | **✗** | ✓ | **Only works with 8+** |

**Flag 9 = 1 + 8:**
- Bit 1: Disambiguate escape codes
- Bit 8: Report all keys as escape codes

## Protocol Flow

```
1. Query Support
   ┌─────────────────┐
   │ Send: ESC [?u   │
   └─────────────────┘
           │
           ├─ Response: ESC [?Nu  → Supported
           └─ No response (100ms) → Not supported, use legacy

2. Enable Protocol
   ┌─────────────────┐
   │ Send: ESC [>9u  │
   └─────────────────┘

3. Receive Keys
   ┌──────────────────────────────────┐
   │ Format: ESC [keycode;modifiers u │
   │ Example: ESC [13;2u              │
   │          ↑       ↑                │
   │          │       └─ Shift (2)    │
   │          └───────── Enter (13)   │
   └──────────────────────────────────┘

   Filter: if keycode > 50000 → Skip (Alacritty modifier key)

4. Disable Protocol
   ┌─────────────────┐
   │ Send: ESC [<u   │
   └─────────────────┘
```

## Key Format

```
ESC [ keycode ; modifiers u

keycode:    Unicode codepoint or special key code
modifiers:  1 + (Shift=1 | Alt=2 | Ctrl=4 | Super=8 | ... | NumLock=128)

Examples:
  ESC [13;2u    = Shift+Enter (keycode 13, modifier 2 = 1 + Shift)
  ESC [9;1u     = Tab (keycode 9, modifier 1 = no modifiers)
  ESC [13;6u    = Ctrl+Alt+Enter (modifier 6 = 1 + Alt + Ctrl)
  ESC [57441;2u = Shift key press alone (Alacritty only - FILTER THIS)
```

## Modifier Bits

```c
#define MOD_SHIFT    (1 << 0)  // 1
#define MOD_ALT      (1 << 1)  // 2
#define MOD_CTRL     (1 << 2)  // 4
#define MOD_SUPER    (1 << 3)  // 8
#define MOD_HYPER    (1 << 4)  // 16
#define MOD_META     (1 << 5)  // 32
#define MOD_CAPS     (1 << 6)  // 64
#define MOD_NUM      (1 << 7)  // 128

// Decode
int mod_value = parse_modifier();
int mod_bits = mod_value - 1;  // Subtract base value of 1

bool has_shift = mod_bits & MOD_SHIFT;
bool has_ctrl  = mod_bits & MOD_CTRL;
// etc.
```

## Special Keycodes

```
Enter:     13
Tab:       9
Backspace: 127 (or 8)
Escape:    27
Space:     32

Letters:   a=97, b=98, ... z=122
Numbers:   0=48, 1=49, ... 9=57

Function keys: F1-F12 (varies)
Arrow keys: Up/Down/Left/Right (varies)

Modifier keys (Alacritty only - FILTER THESE):
  Shift:     57441
  Ctrl:      (unknown)
  Alt:       (unknown)

  Rule: if keycode > 50000, skip it
```

## Parsing Example (C)

```c
typedef struct {
    int keycode;
    int modifiers;
    bool valid;
} Key;

Key parse_kitty_key(const char *seq, size_t len) {
    Key key = {0};

    // Check for ESC [
    if (len < 4 || seq[0] != '\x1b' || seq[1] != '[') {
        return key;
    }

    // Parse: ESC [ keycode ; modifiers u
    int keycode = 0, modifiers = 0;
    int i = 2;

    // Read keycode
    while (i < len && isdigit(seq[i])) {
        keycode = keycode * 10 + (seq[i] - '0');
        i++;
    }

    // Check for semicolon
    if (i < len && seq[i] == ';') {
        i++;

        // Read modifiers
        while (i < len && isdigit(seq[i])) {
            modifiers = modifiers * 10 + (seq[i] - '0');
            i++;
        }
    } else {
        modifiers = 1;  // No modifiers
    }

    // Check for 'u' terminator
    if (i < len && seq[i] == 'u') {
        // Filter Alacritty modifier keys
        if (keycode > 50000) {
            return key;  // Invalid - skip modifier key
        }

        key.keycode = keycode;
        key.modifiers = modifiers - 1;  // Subtract base value
        key.valid = true;
    }

    return key;
}
```

## Terminal-Specific Notes

### Alacritty
- **MUST use flag 8+** (flag 1 doesn't work)
- Sends modifier key events (keycode > 50000)
- Filter these out: `if (keycode > 50000) continue;`

### WezTerm
- Requires user config: `enable_csi_u_key_encoding = true`
- Hybrid mode: only modified keys encoded
- Plain Tab stays as `\t` (0x09)
- Works with flag 1+

### Kitty, foot, Ghostty
- Perfect compatibility
- No configuration needed
- Work with any flag (1, 8, 9, etc.)
- No quirks

## Testing

Test your implementation with the provided scripts:

```bash
# Test in each terminal
./test_ignore_modifiers.sh 9

# Comprehensive test
./test_keyboard_protocols.sh
```

## Fallback Strategy

```c
// 1. Try to enable protocol
send_query();
if (wait_for_response(100ms)) {
    // Supported
    enable_protocol_flag_9();
    use_enhanced_mode = true;
} else {
    // Not supported
    use_legacy_mode = true;
}

// 2. Handle input
if (use_enhanced_mode) {
    key = parse_kitty_key(buffer);
    if (key.valid) {
        handle_enhanced_key(key);
    } else {
        // Fall back to legacy for this key
        handle_legacy_key(buffer);
    }
} else {
    handle_legacy_key(buffer);
}
```

## Common Pitfalls

1. **Not filtering Alacritty modifier keys**
   - Symptom: App responds to Shift press alone
   - Fix: `if (keycode > 50000) continue;`

2. **Not handling legacy fallback**
   - Symptom: Breaks on old terminals
   - Fix: Query first, fall back if no response

3. **Forgetting to disable protocol**
   - Symptom: Terminal stays in weird mode after exit
   - Fix: Always send `ESC [<u` before exit (use atexit handler)

4. **Using flag 1 only**
   - Symptom: Works everywhere except Alacritty
   - Fix: Use flag 8 or 9 instead

5. **Not documenting WezTerm config**
   - Symptom: Users complain "doesn't work in WezTerm"
   - Fix: Document config requirement in your README

## Summary

**For universal compatibility: Use flag 9 + filter keycodes > 50000**

This gives you enhanced keyboard protocol support across:
- ✅ Kitty
- ✅ foot
- ✅ Ghostty
- ✅ Alacritty
- ✅ WezTerm (with user config)

**Total compatibility: ~95% of modern terminal users**
