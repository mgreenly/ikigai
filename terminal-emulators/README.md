# Terminal Enhanced Keyboard Protocol Testing

This repository contains comprehensive testing results and implementation guidance for CSI u / Kitty keyboard protocol support across modern terminal emulators.

## For End Users: Keyboard Support

Applications using this protocol implementation support **CSI u / Kitty keyboard protocol** for enhanced key bindings.

**Available Keys:**
- **Shift+Enter, Ctrl+Enter** - Supported in Kitty, foot, Ghostty, Alacritty 0.15+, and WezTerm (requires config)
- **Alt+Enter, Ctrl+J** - Supported in all terminals (automatic fallback)

**Detection is automatic** - the application will detect your terminal and show you which keys are available. For WezTerm support, add `config.enable_csi_u_key_encoding = true` to `~/.wezterm.lua`.

---

## For Developers: Quick Summary

**Goal:** Enable applications to distinguish previously ambiguous key combinations (Shift+Enter vs Enter, Ctrl+I vs Tab, etc.)

**Solution:** Use **flag 9** (1+8) for universal compatibility across all tested terminals.

**Tested Terminals:**
- ✅ **Kitty** - Perfect, works with flag 1 or 8
- ✅ **foot** - Perfect, works with flag 1 or 8
- ✅ **Ghostty** - Perfect, works with any flag
- ⚠️ **Alacritty** - **Requires flag 8+** (flag 1 doesn't work)
- ⚠️ **WezTerm** - Requires config file, works with flag 1+

**Universal Activation:**
```bash
printf '\x1b[>9u'   # Enable (works on all terminals)
# ... handle enhanced keyboard input ...
printf '\x1b[<u'    # Disable
```

**Key Benefits:**
- Distinguish: Shift+Enter, Ctrl+Enter, Alt+Enter
- Distinguish: Tab vs Ctrl+I, Enter vs Ctrl+M, Escape vs Ctrl+[
- Bind: Ctrl+Shift+letter combinations
- Works: Across 5 major modern terminals

See `IMPLEMENTATION_GUIDE.md` for code examples.

---

## Background

Modern terminals support enhanced keyboard protocols that allow applications to distinguish between key combinations that were previously ambiguous in legacy terminal encoding:

- **Ctrl+I** vs **Tab** (both were `0x09`)
- **Ctrl+M** vs **Enter** (both were `0x0D`)
- **Shift+Enter** vs **Enter**
- **Ctrl+Shift+Key** combinations
- And many more...

The two main protocols are:
- **CSI u** - A simpler protocol for basic disambiguation
- **Kitty Keyboard Protocol** - A more comprehensive protocol with additional features

## Testing Script

Run `./test_keyboard_protocols.sh` to test your terminal's support. The script performs three tests:

1. **Query Detection** - Asks if terminal responds to `ESC [?u` query
2. **Functional Test** - Activates protocol and tests Shift+Enter encoding
3. **Disambiguation Test** - Tests if Tab can be distinguished from Ctrl+I

### Usage

```bash
./test_keyboard_protocols.sh
```

## Test Results

### Ghostty

```
Terminal: xterm-ghostty
Status: ✓✓ FULL SUPPORT
```

**Results:**
- ✓ Responds to query with `ESC [?0u` (flags=0)
- ✓ Shift+Enter produces `ESC [13;130u` (Shift + NumLock)
- ✓ Tab produces `ESC [9;129u` (with NumLock)

**Notes:**
- Reports flags=0, meaning "basic support, features enabled on activation"
- Correctly includes lock keys (NumLock/CapsLock) in modifier reporting
- Full Kitty keyboard protocol implementation
- Works automatically when applications request it

### WezTerm

```
Terminal: xterm-256color
Status: ✓ FUNCTIONAL SUPPORT (Hybrid Mode)
```

**Results:**
- ✗ No query response
- ✓ Shift+Enter produces `ESC [13;2u` (Shift only)
- ⚠ Tab remains plain `0x09` (legacy)

**Configuration Required:**

Create or edit `~/.wezterm.lua`:

```lua
local wezterm = require 'wezterm'
local config = wezterm.config_builder()

-- Enable CSI u keyboard encoding
config.enable_csi_u_key_encoding = true

return config
```

**Notes:**
- Disabled by default - requires configuration
- Uses **hybrid approach**: modified keys use CSI u, unmodified keys stay legacy
- Does not respond to queries even when enabled
- Progressive enhancement: only encodes keys that need disambiguation
- More conservative than full protocol (better compatibility)

**Official Warning:** This changes keyboard behavior in backwards-incompatible ways. Most modern applications handle it correctly, but older applications may have issues.

### Alacritty

```
Terminal: alacritty
Version: 0.15.1
Status: ✓ FUNCTIONAL SUPPORT (Requires Flag 8)
```

**Results:**
- ✓ Responds to query with `ESC [?0u` (flags=0)
- ✗ Flag 1 (disambiguate): Legacy encoding only
- ✓ Flag 8+ (report all keys): Enhanced encoding works!
- ✓ Shift+Enter with flag 8: `ESC [13;2u`
- ✓ Tab with flag 8: `ESC [9u`

**Critical Requirement:**
**Alacritty requires flag 8 ("Report all keys as escape codes") to be set.** Flag 1 alone (disambiguate) does not activate enhanced encoding.

**Activation:**
```
ESC [>8u      # Minimum - report all keys
ESC [>9u      # Recommended - disambiguate + report all (1+8)
ESC [>11u     # With event types (1+2+8)
```

**Important Quirk:**
With flag 8+, Alacritty reports **modifier key presses as separate key events**:
- Pressing Shift alone sends `ESC [57441;2u` (keycode 57441 = Shift key)
- Applications must filter/ignore modifier-only events (keycodes > 50000)
- Wait for actual key combinations, not just modifier presses

**Notes:**
- Kitty keyboard protocol support merged in PR [#7125](https://github.com/alacritty/alacritty/pull/7125)
- Version 0.15.0+ includes full implementation
- No configuration needed - works via protocol activation
- Custom key bindings in config can interfere with protocol encoding

**For Application Developers:**
Always use flag 8 or higher for Alacritty. Flag 1 alone is insufficient.

### Kitty

```
Terminal: kitty
Status: ✓✓ FULL SUPPORT
```

**Results:**
- ✓ Responds to query with feature flags
- ✓ Flag 1 (disambiguate): Works perfectly
- ✓ Flag 8 (report all keys): Works perfectly
- ✓ Shift+Enter: `ESC [13;2u`
- ✓ Tab: Encoded correctly

**Notes:**
- Native Kitty keyboard protocol implementation (gold standard)
- Works with any flag level (1, 8, 9, 15, etc.)
- No configuration needed
- No quirks detected
- Full protocol implementation

### foot

```
Terminal: foot
Status: ✓✓ FULL SUPPORT
```

**Results:**
- ✓ Responds to query with feature flags
- ✓ Flag 1 (disambiguate): Works perfectly
- ✓ Flag 8 (report all keys): Works perfectly
- ✓ Shift+Enter: `ESC [13;2u`
- ✓ Tab: Encoded correctly

**Notes:**
- Full CSI u and Kitty keyboard protocol support
- Works with any flag level (1, 8, 9, 15, etc.)
- No configuration needed
- No quirks detected
- Lightweight and fast

## Protocol Details

### Modifier Encoding

The protocol encodes modifiers as: `modifiers = 1 + modifier_bits`

Where modifier bits are:
- Bit 0 (1): Shift
- Bit 1 (2): Alt
- Bit 2 (4): Ctrl
- Bit 3 (8): Super/Meta
- Bit 4 (16): Hyper
- Bit 5 (32): Meta
- Bit 6 (64): CapsLock
- Bit 7 (128): NumLock

**Example:** `ESC [13;130u` = Enter key
- Keycode: 13 (Enter)
- Modifiers: 130 = 1 + 129 = 1 + (1 + 128) = Shift + NumLock

### Real-World Key Examples

#### Enter Key Variations
```
Plain Enter:         ESC [13;1u   (modifier = 1, no modifiers)
Shift+Enter:         ESC [13;2u   (modifier = 2 = 1 + Shift)
Ctrl+Enter:          ESC [13;5u   (modifier = 5 = 1 + Ctrl)
Alt+Enter:           ESC [13;3u   (modifier = 3 = 1 + Alt)
Ctrl+Shift+Enter:    ESC [13;6u   (modifier = 6 = 1 + Shift + Ctrl)
Ctrl+Alt+Enter:      ESC [13;7u   (modifier = 7 = 1 + Alt + Ctrl)
```

#### Other Common Keys
```
Tab:                 ESC [9;1u    (keycode 9)
Ctrl+I:              ESC [105;5u  (keycode 105 = 'i', distinguishable!)
Shift+Tab:           ESC [9;2u    (backtab, unambiguous)
Ctrl+M:              ESC [109;5u  (keycode 109 = 'm', not Enter!)
Escape:              ESC [27;1u   (keycode 27)
Ctrl+[:              ESC [91;5u   (keycode 91 = '[', not Escape!)
```

**Previously Impossible:** These key combinations were ambiguous or impossible to detect in legacy terminal encoding. Now they're all unambiguous!

### Protocol Activation

Applications activate the protocol with:
```
ESC [>flags u    # Push/enable with flags
ESC [<u          # Pop/disable
```

Common flag values:
- `1` - Basic disambiguation
- `2` - Report key press/release/repeat events
- `4` - Report alternate keys
- `8` - Report all keys as CSI u
- `16` - Report associated text

**Recommended for universal compatibility: Flag 9 (1+8)**

### Key Encoding Format

Enhanced keys are encoded as:
```
ESC [ unicode-key-code ; modifiers u
```

or with Kitty extensions:
```
ESC [ unicode-key-code : alternate-keys ; modifiers : event-type ; text u
```

### Decoding Example (Pseudocode)

```python
# Received: ESC [13;2u
keycode = 13        # Enter
mod_value = 2       # Raw modifier value
mod_bits = mod_value - 1  # Subtract base: 2 - 1 = 1

# Check which modifiers are active
has_shift = bool(mod_bits & 0x01)  # 1 & 1 = True ✓
has_alt   = bool(mod_bits & 0x02)  # 1 & 2 = False
has_ctrl  = bool(mod_bits & 0x04)  # 1 & 4 = False

# Result: Shift+Enter
print(f"Key: Enter, Shift: {has_shift}, Ctrl: {has_ctrl}, Alt: {has_alt}")
# Output: Key: Enter, Shift: True, Ctrl: False, Alt: False
```

## Summary Table

| Terminal | Query | Flag 1 | Flag 8 | Config Required | Quirks |
|----------|-------|--------|--------|-----------------|--------|
| **Kitty** | ✓ | ✓ | ✓ | No | None - gold standard |
| **foot** | ✓ | ✓ | ✓ | No | None - perfect compatibility |
| **Ghostty** | ✓ (0) | ✓ | ✓ | No | None - works with any flag |
| **WezTerm** | ✗ | ✓ | ✓ | **Yes** | Requires config, hybrid mode |
| **Alacritty 0.15.1** | ✓ (0) | **✗** | ✓ | No | **Needs flag 8+**, reports modifier keys |

## Application Support

Modern terminal applications that support enhanced keyboard protocols:

- **Neovim** - Auto-detects and uses protocol
- **Helix** - Full support
- **Zellij** - Full support
- **Vim 9.0+** - Supports Kitty keyboard protocol
- **Kakoune** - Supports enhanced keys
- **tmux 3.2+** - Can pass through to applications

## References

- [Kitty Keyboard Protocol Specification](https://sw.kovidgoyal.net/kitty/keyboard-protocol/)
- [WezTerm Keyboard Encoding](https://wezterm.org/config/key-encoding.html)
- [WezTerm CSI u Configuration](https://wezterm.org/config/lua/config/enable_csi_u_key_encoding.html)
- [Ghostty Documentation](https://ghostty.org/docs/features)

## WezTerm Configuration Deep Dive

WezTerm requires explicit configuration to enable CSI u keyboard encoding. This section documents configuration options and behavior.

### Basic Configuration

Minimal config in `~/.wezterm.lua`:
```lua
local wezterm = require 'wezterm'
local config = wezterm.config_builder()

-- Enable CSI u keyboard encoding
config.enable_csi_u_key_encoding = true

return config
```

### Configuration Details

**Location:** `~/.wezterm.lua` or `~/.config/wezterm/wezterm.lua`

**Setting:** `enable_csi_u_key_encoding`
- Type: Boolean
- Default: `false`
- Effect: Enables CSI u keyboard protocol encoding

**Important Notes:**
1. This setting changes keyboard behavior in **backwards-incompatible ways**
2. Some older terminal applications may not handle it correctly
3. Most modern applications (Neovim, Helix, Zellij) auto-detect and use it
4. WezTerm does NOT respond to protocol queries even when enabled
5. Hybrid mode: Only modified keys are encoded (Tab stays `\t`, Shift+Enter becomes CSI u)

### Full Example Configuration

```lua
local wezterm = require 'wezterm'
local config = {}

if wezterm.config_builder then
  config = wezterm.config_builder()
end

-- Enable CSI u keyboard encoding for enhanced keyboard protocol
config.enable_csi_u_key_encoding = true

-- Optional: Other useful settings
config.font_size = 12.0
config.color_scheme = 'Tokyo Night'

-- Optional: Show which keys are being sent (for debugging)
config.debug_key_events = false  -- Set true to see key events in logs

return config
```

### Verification

After enabling, test with:
```bash
./test_ignore_modifiers.sh 1
```

Should show enhanced encoding for Shift+Enter.

### WezTerm Behavior Summary

| Key | Without Config | With Config |
|-----|---------------|-------------|
| Enter | `\r` | `\r` (legacy) |
| Shift+Enter | `\r` | `ESC [13;2u` (enhanced) ✓ |
| Tab | `\t` | `\t` (legacy) |
| Ctrl+I | `\t` | `ESC [105;5u` (enhanced) ✓ |
| Ctrl+Enter | varies | `ESC [13;5u` (enhanced) ✓ |

**Hybrid Mode:** WezTerm only encodes keys that need disambiguation (modified keys), leaving common unmodified keys in legacy format for better compatibility.

### Troubleshooting WezTerm

**Problem:** Keys not being encoded after config change
- **Solution:** Restart WezTerm or reload config (Ctrl+Shift+R)

**Problem:** Some applications broken after enabling
- **Solution:** Those apps don't support CSI u. Either:
  - Update the application
  - Disable CSI u encoding for those apps
  - Use legacy terminal mode

**Problem:** Want to disable for specific applications
- **Solution:** Not directly supported. Use a different terminal or profile.

## Adding More Terminals to Testing

To test additional terminals in the future, use this workflow:

### 1. Quick Test
```bash
# In the new terminal:
./test_ignore_modifiers.sh 1    # Test flag 1
./test_ignore_modifiers.sh 8    # Test flag 8
```

Record results:
- Flag 1 works? (YES/NO)
- Flag 8 works? (YES/NO)
- Any quirks? (modifier keys reported, etc.)

### 2. Comprehensive Test
```bash
./test_keyboard_protocols.sh
```

This tests:
- Query response
- Shift+Enter encoding
- Tab disambiguation

### 3. Document Findings

Add to Summary Table:
```markdown
| **NewTerminal** | Query | Flag 1 | Flag 8 | Config | Quirks |
|-----------------|-------|--------|--------|--------|--------|
| version X.Y     | ✓/✗   | ✓/✗    | ✓/✗    | Yes/No | Notes  |
```

Add detailed section:
```markdown
### NewTerminal

**Results:**
- Query response: ...
- Flag 1: ...
- Flag 8: ...

**Notes:**
- Configuration requirements
- Special behaviors
- Known issues
```

### 4. Update Strategy Recommendations

If the new terminal has special requirements, update the "Recommended Strategy" section with compatibility notes.

### Terminals to Consider Testing

Potential candidates for future testing:
- **iTerm2** (macOS) - Popular macOS terminal
- **Windows Terminal** - Microsoft's modern terminal
- **Konsole** (KDE) - KDE's terminal emulator
- **GNOME Terminal** - GNOME's default terminal
- **Terminator** - Advanced terminal emulator
- **Hyper** - Electron-based terminal
- **Rio** - Rust-based GPU terminal
- **Contour** - Modern terminal emulator
- **tmux** - Terminal multiplexer (protocol passthrough)
- **screen** - GNU screen (protocol support?)

### Testing Template

When testing a new terminal:
```bash
# 1. Identify terminal
echo $TERM
<terminal-name> --version

# 2. Basic flag test
./test_ignore_modifiers.sh 1
./test_ignore_modifiers.sh 8

# 3. Check for modifier key events (Alacritty-style)
#    If you get keycode > 50000 when pressing Shift alone, note it

# 4. Check for query response
./test_keyboard_protocols.sh | grep "Query"

# 5. Document:
#    - Terminal name and version
#    - Flag requirements (1, 8, both, neither)
#    - Configuration needed (yes/no, what config)
#    - Quirks (modifier keys, hybrid mode, etc.)
```

## Files

- `test_keyboard_protocols.sh` - Comprehensive detection and testing script
- `test_ignore_modifiers.sh <flag>` - Test specific flag, ignore modifier keys
- `test_single_flag.sh <flag>` - Simple single flag test
- `test_csi_u.sh` - Original simpler CSI u detection script
- `.wezterm.lua` - WezTerm configuration with CSI u enabled
- `IMPLEMENTATION_GUIDE.md` - Quick reference for implementing in your application

## Conclusion

All tested terminals support enhanced keyboard protocols, but with different requirements and behaviors:

### ✓✓ Perfect Compatibility (No Quirks)
These terminals work flawlessly with any flag level:
- **Kitty** - Gold standard implementation, works with flag 1 or 8
- **foot** - Perfect compatibility, works with flag 1 or 8
- **Ghostty** - Full support, works with any flag

### ✓ Functional with Quirks

#### Alacritty 0.15.1 - Requires Flag 8
- **Critical limitation**: Flag 1 alone does NOT work
- Must use flag 8 or higher
- Reports modifier key presses as separate events (keycode 57441 for Shift)
- Applications must filter keycodes > 50000

#### WezTerm - Requires Configuration
- Must set `enable_csi_u_key_encoding = true` in config
- Hybrid mode: only encodes modified keys
- Works with flag 1+
- More conservative (better backwards compatibility)

## Recommended Implementation Strategy

### Goal: Support Shift+Enter and Ctrl+Enter Everywhere

Based on comprehensive testing, here's the recommended strategy for universal support:

**Primary Keys (Modern Terminals):**
- Shift+Enter
- Ctrl+Enter

**Fallback Keys (Legacy Terminals):**
- Alt+Enter (intuitive fallback, needs timeout handling)
- Ctrl+J (simple fallback, works instantly)

**Coverage:** 100% of terminals with graceful degradation

### Detection and Transparent Fallback

#### Step 1: Query for Protocol Support

```bash
# Send query
printf '\x1b[?u' >&2

# Wait up to 100ms for response
# If response received: enhanced protocol available
# If timeout: legacy terminal (GNOME Terminal, xterm, etc.)
```

```python
def detect_keyboard_protocol():
    """Returns True if enhanced protocol is available."""
    import sys, select

    # Send query
    sys.stderr.write('\x1b[?u')
    sys.stderr.flush()

    # Wait 100ms for response
    ready, _, _ = select.select([sys.stdin], [], [], 0.1)

    if ready:
        # Read response (should be ESC [?Nu)
        response = sys.stdin.read(10)  # Read response
        if '?' in response and 'u' in response:
            return True  # Protocol supported

    return False  # Legacy terminal
```

#### Step 2: Enable Protocol if Available

```python
enhanced_keys_available = detect_keyboard_protocol()

if enhanced_keys_available:
    # Enable with flag 9 (works on all modern terminals)
    sys.stderr.write('\x1b[>9u')
    sys.stderr.flush()

    # User can now use:
    # - Shift+Enter → ESC [13;2u
    # - Ctrl+Enter  → ESC [13;5u

else:
    # Legacy terminal - use fallbacks:
    # - Alt+Enter  → ESC + \r (needs timeout)
    # - Ctrl+J     → \n (instant)
    pass
```

#### Step 3: Show Appropriate Keys to User

**Transparent UI - Auto-detect and display:**

```python
def get_key_hints():
    """Return key bindings appropriate for current terminal."""
    if enhanced_keys_available:
        return {
            'submit': 'Ctrl+Enter',
            'newline': 'Shift+Enter',
            'info': 'Enhanced keyboard support active'
        }
    else:
        return {
            'submit': 'Ctrl+J or Alt+Enter',
            'newline': 'Alt+Enter',
            'info': 'Using legacy key bindings'
        }

# Display in UI
hints = get_key_hints()
print(f"Submit: {hints['submit']}")
print(f"New line: {hints['newline']}")

# Optional: Show terminal capability
if debug_mode:
    print(f"({hints['info']})")
```

#### Step 4: Handle Input (with Modifier Key Filtering)

**Critical:** Alacritty with flag 8+ sends modifier key presses as separate events. You MUST filter these:

```python
def parse_key_event():
    """Parse key event and return semantic action."""

    if enhanced_keys_available:
        # Parse CSI u sequences
        seq = read_sequence()

        # Extract keycode and modifiers
        # Format: ESC [keycode;modifiers u
        keycode, modifiers = parse_csi_u(seq)

        # CRITICAL: Filter Alacritty modifier-only events
        if keycode > 50000:
            return None  # Ignore: Shift=57441, Ctrl, Alt key presses

        # Now handle real key combinations
        if keycode == 13:  # Enter key
            if modifiers & SHIFT_BIT:
                return 'SHIFT_ENTER'    # Real Shift+Enter combo
            elif modifiers & CTRL_BIT:
                return 'CTRL_ENTER'     # Real Ctrl+Enter combo

    else:
        # Legacy terminal - handle fallbacks

        # Ctrl+J (instant, no ambiguity)
        if char == '\n' and not is_plain_enter:
            return 'CTRL_J_SUBMIT'

        # Alt+Enter (needs timeout handling)
        if char == '\x1b':  # ESC
            # Wait 10-50ms for next char
            next_char = read_with_timeout(0.05)
            if next_char == '\r':
                return 'ALT_ENTER'
            else:
                return 'ESCAPE'

    return None
```

### Complete Example

```python
#!/usr/bin/env python3
"""
Example: Universal Shift+Enter and Ctrl+Enter support
"""

import sys, termios, tty, select

class KeyboardProtocol:
    def __init__(self):
        self.enhanced = self.detect()
        if self.enhanced:
            self.enable()

    def detect(self):
        """Detect if enhanced protocol is available."""
        sys.stderr.write('\x1b[?u')
        sys.stderr.flush()

        ready, _, _ = select.select([sys.stdin], [], [], 0.1)
        if ready:
            response = sys.stdin.read(10)
            return 'u' in response
        return False

    def enable(self):
        """Enable enhanced protocol with flag 9."""
        sys.stderr.write('\x1b[>9u')
        sys.stderr.flush()

    def disable(self):
        """Disable protocol."""
        if self.enhanced:
            sys.stderr.write('\x1b[<u')
            sys.stderr.flush()

    def get_bindings(self):
        """Get key bindings for current terminal."""
        if self.enhanced:
            return {
                'submit': 'Ctrl+Enter',
                'newline': 'Shift+Enter'
            }
        else:
            return {
                'submit': 'Ctrl+J or Alt+Enter',
                'newline': 'Alt+Enter'
            }

# Usage
protocol = KeyboardProtocol()
bindings = protocol.get_bindings()

print(f"Submit message: {bindings['submit']}")
print(f"New line: {bindings['newline']}")

if protocol.enhanced:
    print("✓ Enhanced keyboard support active")
else:
    print("○ Using legacy keyboard mode")

# ... handle input with appropriate parsing ...

# Cleanup on exit
protocol.disable()
```

### User Experience Best Practices

#### 1. Auto-detect and Show Appropriate Keys

**Good:**
```
# Modern terminal detected
Submit: Ctrl+Enter
New line: Shift+Enter
```

**Good:**
```
# Legacy terminal detected
Submit: Ctrl+J or Alt+Enter
New line: Alt+Enter
```

**Bad:**
```
# Shows all keys, confusing
Submit: Ctrl+Enter, Shift+Enter, Alt+Enter, Ctrl+J
```

#### 2. Optional: Inform Advanced Users

```
# In help/settings
Terminal: Kitty (enhanced keyboard support) ✓
  or
Terminal: GNOME Terminal (legacy mode)

[?] Enhanced keyboard protocol enables Ctrl+Enter and Shift+Enter.
    Requires: Kitty, foot, Ghostty, Alacritty 0.15+, or WezTerm.
```

#### 3. Support Both Modes Simultaneously

Even in enhanced mode, still accept Alt+Enter and Ctrl+J:

```python
# In enhanced mode:
if key == 'CTRL_ENTER' or key == 'CTRL_J':
    submit_action()

if key == 'SHIFT_ENTER' or key == 'ALT_ENTER':
    newline_action()
```

**Why?** Users might have muscle memory from other apps or terminals.

### Modifier Key Event Filtering (Critical!)

**What happens when user presses Shift+Enter in Alacritty:**

```
Time    User Action          Terminal Sends              Meaning
────────────────────────────────────────────────────────────────────────
t=0     Press Shift          ESC [57441;2u              "Shift KEY pressed"
                                  ↑                      (redundant)
                                  keycode 57441

t=1     Press Enter          ESC [13;2u                 "Enter WITH Shift"
        (Shift held)              ↑    ↑                (complete info!)
                                  │    └─ modifier=2 (Shift bit set)
                                  └────── keycode 13 (Enter)
```

**Key Understanding:**

You receive **TWO pieces of information**:
1. **Event 1** (`keycode 57441`): "Shift key was pressed" - tells you the modifier state changed
2. **Event 2** (`keycode 13, modifier 2`): "Enter was pressed with Shift held" - tells you the key combination

**You only need Event 2!** The modifier bits on the Enter event already tell you Shift was held. Event 1 is redundant.

**Important:** If user releases Shift before pressing Enter:
```
t=0     Press Shift          ESC [57441;2u   ← Shift pressed
t=1     Release Shift        (nothing)
t=2     Press Enter          ESC [13;1u      ← modifier=1 (NO Shift bit!)
                                      ↑
                                      No Shift = plain Enter
```

The modifier bits reflect **what was held when the key was pressed**, so the distinction happens automatically.

**Your code MUST filter Event 1 (modifier-only events):**

```python
while True:
    keycode, mods = read_key_event()

    # Filter modifier-only events (redundant information)
    if keycode > 50000:
        continue  # Ignore: This is just "Shift was pressed"
                  # We'll see Shift in the modifier bits of the real key

    # Now process real key (has complete information)
    if keycode == 13:  # Enter key
        if mods & SHIFT_BIT:
            handle_shift_enter()    # Shift WAS held (modifier bit set)
        else:
            handle_plain_enter()    # Shift was NOT held (no bit)
```

**Why keycode > 50000?**
- Alacritty uses high keycodes for modifier keys themselves:
  - Shift: 57441
  - Ctrl, Alt: (also > 50000)
- Real keys (Enter=13, Tab=9, letters=97-122) are all < 1000
- Safe threshold: Ignore anything > 50000

**Why does Alacritty do this?**
- With flag 8 ("report all keys"), Alacritty interprets this as:
  - Report modifier keys themselves AS key events
  - AND report modifiers on actual keys (as modifier bits)
- Other terminals just do the second part (modifier bits only)
- Alacritty gives you both → redundant but harmless if filtered

**Other terminals (Kitty, foot, Ghostty):**
- Don't send modifier-only events
- Only send actual key combinations with modifier bits
- Filtering `> 50000` does nothing (no events to filter)

### Terminal Capability Summary

| Terminal | Detection | Keys Available | Modifier Events |
|----------|-----------|----------------|-----------------|
| **Kitty** | Query responds | Shift+Enter, Ctrl+Enter, Alt+Enter, Ctrl+J | No |
| **foot** | Query responds | Shift+Enter, Ctrl+Enter, Alt+Enter, Ctrl+J | No |
| **Ghostty** | Query responds | Shift+Enter, Ctrl+Enter, Alt+Enter, Ctrl+J | No |
| **Alacritty 0.15+** | Query responds | Shift+Enter, Ctrl+Enter, Alt+Enter, Ctrl+J | **Yes - filter >50000** |
| **WezTerm** | No response* | Shift+Enter, Ctrl+Enter (if configured), Alt+Enter, Ctrl+J | No |
| **GNOME Terminal** | No response | Alt+Enter, Ctrl+J only | N/A |
| **xterm** | No response | Alt+Enter, Ctrl+J only | N/A |

*WezTerm doesn't respond to queries but enhanced keys work if configured.

### WezTerm Special Case

WezTerm requires user configuration but doesn't respond to queries. Options:

1. **Try to enable anyway** - If user has it configured, it works
2. **Detect $TERM_PROGRAM** - Check for "WezTerm"
3. **Test a key** - Send test input and check response

**Recommended:** Just enable flag 9 for all terminals. No harm if unsupported.

```python
# Always try to enable (no-op if unsupported)
sys.stderr.write('\x1b[>9u')

# Support both enhanced and legacy keys
# User gets best experience their terminal supports
```

## Cross-Terminal Implementation Strategy

### Recommended Approach

For maximum compatibility across all terminals, use **flag 9**:

```bash
# 1. Query for support
printf '\x1b[?u'
# Wait for response: ESC [?Nu where N = flags

# 2. Activate with flag 1 (works on all except Alacritty)
printf '\x1b[>1u'

# 3. If Alacritty detected or flag 1 fails, upgrade to flag 9
printf '\x1b[>9u'

# 4. Disable when done
printf '\x1b[<u'
```

### Terminal-Specific Strategies

#### Strategy A: Universal (Safest)
Use **flag 9** (1+8) for all terminals:
```
ESC [>9u    # Works on: Kitty, foot, Ghostty, Alacritty, WezTerm
```

**Pros:**
- Single code path for all terminals
- Works with Alacritty (requires flag 8+)
- Works with others (flag 1+ compatible)

**Cons:**
- Slightly more data (reports all keys)
- Alacritty sends modifier key events (need filtering)

#### Strategy B: Detect and Adapt
1. Query terminal type from `$TERM` or terminfo
2. Use flag 1 for Kitty/foot/Ghostty/WezTerm
3. Use flag 9 for Alacritty
4. Filter keycodes > 50000 for Alacritty

**Pros:**
- Optimal for each terminal
- Less data in most cases

**Cons:**
- More complex code
- Fragile (relies on terminal detection)

#### Strategy C: Progressive Enhancement
```bash
# Try flag 1 first
printf '\x1b[>1u'
# Test if Shift+Enter encodes
# If not, upgrade to flag 9
printf '\x1b[>9u'
```

**Pros:**
- Automatically adapts
- No terminal detection needed

**Cons:**
- Requires runtime testing
- Brief delay during detection

### Recommendation: **Use Flag 9 Universally**

For simplicity and reliability:
```
ESC [>9u    # Enable (disambiguate + report all keys)
ESC [<u     # Disable
```

**Implementation requirements:**
1. Filter keycodes > 50000 (modifier-only events from Alacritty)
2. Query first to check support (`ESC [?u`)
3. Fall back to legacy if no response
4. Document WezTerm config requirement

### Key Implementation Details

#### Handling Modifier Keys (Alacritty)
```
If keycode > 50000:
    # This is a modifier key press (Shift, Ctrl, etc.)
    # Ignore and wait for actual key combination
    continue
```

#### Feature Detection
```bash
# Send query
printf '\x1b[?u'

# Wait 100ms for response
# If response: ESC [?Nu where N = flags
#   - Terminal supports protocol
# If no response:
#   - Use legacy encoding
```

#### WezTerm Configuration Check
Inform users that WezTerm requires:
```lua
config.enable_csi_u_key_encoding = true
```

### Testing Your Implementation

Use the provided test scripts:
- `test_ignore_modifiers.sh <flag>` - Test specific flag
- `test_keyboard_protocols.sh` - Comprehensive terminal test

Test on all target terminals to verify behavior.
