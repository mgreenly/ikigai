# Task: Event Render Message Styling

## Target
Infrastructure: ANSI color support (Phase 5 - Styling)

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/ansi-color.md

### Pre-read Source (patterns)
- src/ansi.h (color constants, enabled check)
- src/ansi.c (color functions)
- src/event_render.c (current implementation)
- src/event_render.h

### Pre-read Tests (patterns)
- tests/unit/event_render/event_render_test.c

## Pre-conditions
- `make check` passes
- All ANSI infrastructure tasks completed (skip, constants, width calc, input strip, config)
- `ik_ansi_colors_enabled()` and `ik_ansi_fg_256()` available

## Task
Modify `event_render.c` to wrap message content with ANSI color codes based on message kind. Apply colors only when `ik_ansi_colors_enabled()` returns true.

Styling rules:
| Kind | Style |
|------|-------|
| user | Default (no color) |
| mark | Default (no color) - user input |
| rewind | Default (no color) - user input |
| clear | Default (no color) - user input |
| assistant | Gray 249 (slightly subdued) |
| tool_call | Gray 242 (very subdued) |
| tool_result | Gray 242 (very subdued) |
| system | Gray 242 (very subdued) |

Format: `{color_start}{content}{reset}` for colored kinds, just `{content}` for default.

## TDD Cycle

### Red
1. Add tests to `tests/unit/event_render/event_render_test.c`:
   - Test user message has no color codes
   - Test assistant message wrapped with gray 249
   - Test tool_call message wrapped with gray 242
   - Test tool_result message wrapped with gray 242
   - Test system message wrapped with gray 242
   - Test mark renders without color (it's user input)
   - Test colors disabled: no escape sequences in output
   - Verify scrollback line contains expected escape sequences
2. Run `make check` - expect test failures

### Green
1. Add `#include "ansi.h"` to event_render.c
2. Create helper function to wrap content with color:
   ```c
   static char *apply_style(TALLOC_CTX *ctx, const char *content, uint8_t color)
   {
       if (!ik_ansi_colors_enabled() || color == 0) {
           return talloc_strdup(ctx, content);
       }
       char color_seq[16];
       ik_ansi_fg_256(color_seq, sizeof(color_seq), color);
       return talloc_asprintf(ctx, "%s%s%s", color_seq, content, IK_ANSI_RESET);
   }
   ```
3. Update `render_content_event()` to accept kind parameter
4. In `ik_event_render()`, determine color based on kind:
   - assistant: IK_ANSI_GRAY_LIGHT (249)
   - tool_call, tool_result, system: IK_ANSI_GRAY_SUBDUED (242)
   - user, mark, rewind, clear: 0 (no color)
5. Pass color to content renderer, apply before appending to scrollback
6. Run `make check` - expect pass

### Refactor
1. Ensure color logic is clean and maintainable
2. Consider if color mapping should be in ansi.h as constants
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Messages styled with correct colors based on kind
- Colors respect `ik_ansi_colors_enabled()` setting
- User input (user, mark, commands) has no color
- 100% test coverage for new code
