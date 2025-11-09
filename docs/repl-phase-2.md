# REPL Terminal - Phase 2: Add Scrollback and Scrolling

[← Back to REPL Terminal Overview](repl-terminal.md)

**Goal**: Implement the full continuous buffer model.

Once Phase 1 validates the fundamentals, add the continuous buffer with scrollback.

## New features

1. Scrollback buffer (`ik_line_array_t`)
2. Separator line
3. Continuous buffer model (scrollback + separator + dynamic zone)
4. Viewport scrolling (mouse wheel, Page Up/Down)
5. Snap-back behavior (typing when scrolled up)
6. Enter submits line to scrollback
7. Dynamic zone scrolling (when taller than screen)

## Split-Buffer REPL Terminal Design

**Goal**: A working REPL with scrollback buffer and dynamic input zone.

### Features

1. **Alternate Screen Mode**
   - App uses alternate screen buffer on launch
   - Clean terminal on exit (no history pollution)

2. **Split Buffer Layout**
   ```
   [scrollback line 1 - oldest]
   [scrollback line 2]
   [scrollback line 3]
   ...
   [scrollback line N - most recent]
   ─────────────────────────────────
   > user input here█
   ```
   - **Scrollback zone** (top): Immutable conversation history
   - **ASCII separator**: Visual boundary (e.g., `─────────`)
   - **Dynamic zone** (bottom): Editable prompt line

3. **REPL Behavior**
   - Type text in prompt
   - Press Enter → text moves to scrollback buffer
   - New empty prompt appears in dynamic zone

4. **Scrolling**
   - **Mouse wheel**: Scroll up/down through buffer
   - **Page Up/Down**: Scroll by larger increments (e.g., screen height)
   - **Bounds**:
     - Can't scroll above first/oldest line
     - Can't scroll past bottom (last line of dynamic zone)
   - **Dynamic zone behavior**:
     - As you scroll up, dynamic zone disappears off bottom line-by-line
     - Can scroll to any position where dynamic zone is partially or fully visible
     - Separator line scrolls with dynamic zone (not fixed)

5. **Config Integration**
   - Load config via existing `ik_cfg_load()`
   - Initial config only needs basic settings (API key path for later)

### Architecture

**Mental Model**: The terminal is a viewport into one continuous buffer:

```
┌─────────────────────────────────────┐
│  Terminal Screen (80x24 viewport)   │
│                                      │
│  ╔═══════════════════════════════╗  │
│  ║ Scrollback line 500           ║  │ ← Viewport showing
│  ║ Scrollback line 501           ║  │   lines 500-523 of
│  ║ ...                           ║  │   continuous buffer
│  ║ Scrollback line 520           ║  │
│  ║ ───────────────────────────── ║  │ ← Separator (part of buffer)
│  ║ Dynamic zone line 1           ║  │
│  ║ Dynamic zone line 2           ║  │ ← Multi-line input area
│  ║ Dynamic zone line 3█          ║  │   (variable height)
│  ╚═══════════════════════════════╝  │
└─────────────────────────────────────┘

        Continuous Buffer:
        [scrollback lines...]
        [separator line]
        [dynamic zone lines...]
```

**Key behaviors**:
- One continuous buffer: scrollback + separator + dynamic zone
- Viewport scrolls through this buffer via mouse wheel or Page Up/Down
- Dynamic zone height varies based on text content (wrapping)
- As you scroll up, dynamic zone + separator disappear off bottom line-by-line
- Cursor always positioned within dynamic zone (even when scrolled off-screen)
- When you type with cursor off-screen, viewport snaps to show cursor at bottom

**Implementation Strategy**: Use libvterm for ALL rendering (one coherent system), but maintain our own scrollback buffer for control.

### Detailed Design Decisions

See [repl-phase-2-details.md](repl-phase-2-details.md) for complete implementation details including:
- Input Flow
- Progressive Input Implementation
- Cursor Management
- Snap-back behavior
- Scrollback Line Format
- Line Processing Pipeline
- Module Organization
- Memory Strategy
- Data Structures
- Rendering Pipeline
- Implementation Components

### What This Validates

- Alternate screen terminal behavior
- vterm as unified rendering system
- Continuous buffer model (scrollback + separator + dynamic zone)
- Viewport scrolling through variable-height content
- Multi-line dynamic zone with cursor movement
- Snap-back behavior when typing while scrolled
- Mouse wheel and Page Up/Down scrolling
- Scroll bounds with partially visible dynamic zone
- Memory management patterns for dynamic buffers
- Single-write blit pattern (no flicker)
- Foundation for adding streaming AI responses
