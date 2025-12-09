# Task: Implement Synchronized Output Mode

## Target
Render Performance Optimization - Prevent screen tearing on supported terminals

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/memory.md (talloc ownership)

## Pre-read Source (patterns)
- src/repl_viewport.c (render frame function)
- src/terminal.c (terminal initialization and capabilities)
- src/terminal.h (terminal structure)

## Pre-read Tests (patterns)
- tests/unit/repl/*.c (repl test patterns)
- tests/unit/terminal/*.c (terminal test patterns)

## Pre-conditions
- `make check` passes
- No synchronized output support exists

## Task
Implement synchronized output mode using the DEC private mode sequences:
- `\x1b[?2026h` - Begin synchronized update (terminal buffers output)
- `\x1b[?2026l` - End synchronized update (terminal flushes buffer atomically)

This prevents screen tearing by telling the terminal to buffer all output until the frame is complete, then display it atomically. Supported by modern terminals including:
- kitty
- iTerm2
- WezTerm
- foot
- Contour
- mintty

Terminals that don't support it will simply ignore the sequences.

## TDD Cycle

### Red
1. Add synchronized output flag to terminal config in `src/terminal.h`:
   ```c
   typedef struct ik_terminal {
       // ... existing fields ...
       bool sync_output_enabled;  // Whether to use synchronized output mode
   } ik_terminal_t;
   ```

2. Add escape sequence constants to `src/escape.h`:
   ```c
   // Synchronized output (DEC private mode 2026)
   #define IK_ESC_SYNC_START "\x1b[?2026h"
   #define IK_ESC_SYNC_END   "\x1b[?2026l"
   #define IK_ESC_SYNC_START_LEN 8
   #define IK_ESC_SYNC_END_LEN   8
   ```

3. Create `tests/unit/terminal/sync_output_test.c`:
   - Test sync output can be enabled
   - Test sync output sequences are included in frame when enabled
   - Test frame starts with sync start, ends with sync end
   - Test sync output disabled by default (for compatibility)
   - Test frame works correctly when sync output disabled

4. Run `make check` - expect failures

### Green
1. In `src/terminal.c`:
   - Add `sync_output_enabled = false` as default in terminal init
   - Consider detecting terminal type to auto-enable (optional)

2. Modify `ik_repl_render_frame()` in `src/repl_viewport.c`:
   ```c
   // Build framebuffer
   size_t offset = 0;

   // Start synchronized output if enabled
   if (repl->shared->term->sync_output_enabled) {
       memcpy(framebuffer + offset, IK_ESC_SYNC_START, IK_ESC_SYNC_START_LEN);
       offset += IK_ESC_SYNC_START_LEN;
   }

   // ... existing clear screen, content, cursor positioning ...

   // End synchronized output if enabled
   if (repl->shared->term->sync_output_enabled) {
       memcpy(framebuffer + offset, IK_ESC_SYNC_END, IK_ESC_SYNC_END_LEN);
       offset += IK_ESC_SYNC_END_LEN;
   }

   // Single atomic write
   write(tty_fd, framebuffer, offset);
   ```

3. (Optional) Add environment variable or config to enable:
   ```c
   // In terminal init
   const char *sync_env = getenv("IKIGAI_SYNC_OUTPUT");
   if (sync_env && strcmp(sync_env, "1") == 0) {
       term->sync_output_enabled = true;
   }
   ```

4. Run `make check` - expect pass

### Refactor
1. Consider auto-detection based on $TERM or terminal query
2. Ensure framebuffer capacity accounts for sync sequences
3. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `IK_ESC_SYNC_START` and `IK_ESC_SYNC_END` constants exist
- `ik_terminal_t` has `sync_output_enabled` field
- When enabled, frames are wrapped in synchronized output sequences
- Disabled by default for compatibility with older terminals
- Can be enabled via environment variable or config

## References
- https://gist.github.com/christianparpart/d8a62cc1ab659194337d73e399004f65 (synchronized output spec)
- Terminal support: kitty, iTerm2, WezTerm, foot, Contour, mintty
