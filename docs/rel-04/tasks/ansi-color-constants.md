# Task: ANSI Color Constants and SGR Builders

## Target
Infrastructure: ANSI color support (Phase 1 - Foundation)

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md

### Pre-read Docs
- docs/ansi-color.md
- docs/naming.md

### Pre-read Source (patterns)
- src/ansi.h (from previous task)
- src/ansi.c (from previous task)

### Pre-read Tests (patterns)
- tests/unit/ansi/skip_test.c

## Pre-conditions
- `make check` passes
- `src/ansi.h` and `src/ansi.c` exist with `ik_ansi_skip_csi()`

## Task
Add color constants and SGR sequence builder functions to the ansi module. These will be used by event_render to style messages.

Semantic colors (256-color palette):
- `IK_ANSI_GRAY_SUBDUED` = 242 (very subdued - tools, system)
- `IK_ANSI_GRAY_LIGHT` = 249 (slightly subdued - assistant)

API additions:
```c
// Color constants (256-color indices)
#define IK_ANSI_GRAY_SUBDUED 242
#define IK_ANSI_GRAY_LIGHT   249

// SGR sequence literals
#define IK_ANSI_RESET "\x1b[0m"

// Build foreground color sequence into buffer
// Returns bytes written (0 on error, buffer too small)
// Buffer should be at least 12 bytes for 256-color
size_t ik_ansi_fg_256(char *buf, size_t buf_size, uint8_t color);
```

## TDD Cycle

### Red
1. Update `src/ansi.h`:
   - Add `IK_ANSI_GRAY_SUBDUED`, `IK_ANSI_GRAY_LIGHT` constants
   - Add `IK_ANSI_RESET` macro
   - Add `ik_ansi_fg_256()` declaration
2. Create `tests/unit/ansi/color_test.c`:
   - Test `IK_ANSI_RESET` equals `"\x1b[0m"`
   - Test `ik_ansi_fg_256(buf, 12, 242)` produces `"\x1b[38;5;242m"`
   - Test `ik_ansi_fg_256(buf, 12, 249)` produces `"\x1b[38;5;249m"`
   - Test `ik_ansi_fg_256(buf, 12, 0)` produces `"\x1b[38;5;0m"` (single digit)
   - Test `ik_ansi_fg_256(buf, 12, 255)` produces `"\x1b[38;5;255m"`
   - Test `ik_ansi_fg_256()` returns 0 if buffer too small
3. Add stub implementation returning 0
4. Run `make check` - expect test failures

### Green
1. Implement `ik_ansi_fg_256()`:
   - Use snprintf to format `\x1b[38;5;%dm`
   - Return bytes written (excluding null terminator)
   - Return 0 if buffer too small
2. Run `make check` - expect pass

### Refactor
1. Consider if background color function needed (defer if not used yet)
2. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Color constants defined in `src/ansi.h`
- `ik_ansi_fg_256()` builds correct SGR sequences
- 100% test coverage for new code
