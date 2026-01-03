# Task: Fix top separator missing right half due to ANSI code length

## Target
Bug 2 from gap.md: Top separator missing right half

## Macro Context

**What:** The top separator line doesn't fill the full terminal width when navigation context is displayed.

**Why this matters:**
- Visual inconsistency between top and bottom separators
- Makes the UI look broken/incomplete
- The navigation info appears to "cut off" the separator prematurely

**Observed behavior:**
```
───────────────────────────────────── ↑- ←- [wlT4G-...] →- ↓-
```

**Expected behavior:**
```
──────────────────────────────────────────────────────────────────────────────
                                     (nav info at right edge, filling full width)
```

## Root Cause (from gap.md investigation)

The bug is in `src/layer_separator.c` separator_render():

1. Navigation string is built with ANSI escape codes for dimming (e.g., `ANSI_DIM "\x1b[2m"`, `ANSI_RESET "\x1b[0m"`)
2. `nav_len` is calculated by `snprintf()` return value (line 122) which counts **byte length**
3. ANSI escape codes like `\x1b[2m` are 4 bytes but render as **0 terminal columns**
4. `sep_chars = width - info_len` uses the inflated byte count
5. Result: separator is shorter than needed by the sum of ANSI escape code bytes

**Example:**
- Terminal width: 100 columns
- Navigation string: "↑- [abc...]" with ANSI dim codes
- Actual visual width: 15 columns
- Byte length (snprintf): 30 bytes (includes escape codes)
- `sep_chars = 100 - 30 = 70` (wrong, should be `100 - 15 = 85`)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/git.md
- .agents/skills/makefile.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md

## Pre-read Source
- src/layer_separator.c:66-193 (READ - `separator_render()` function)
- src/layer_separator.c:80-124 (READ - navigation string construction)
- src/ansi.c or src/ansi.h (READ IF EXISTS - check for visual width utilities)
- src/scrollback_utils.c (READ - may have display width calculation functions)

## Source Files to MODIFY
- src/layer_separator.c (MODIFY - fix width calculation)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes

## Task

### Part 1: Create a visual width calculation function

Create a helper function that calculates the visual width of a string, excluding ANSI escape codes:

```c
// Calculate visual (terminal) width of a string, excluding ANSI escape codes
// Returns the number of terminal columns the string will consume
static size_t visual_width(const char *str, size_t len)
{
    size_t width = 0;
    bool in_escape = false;

    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\x1b') {
            in_escape = true;
            continue;
        }
        if (in_escape) {
            // ESC sequences end with a letter (m for SGR codes)
            if ((str[i] >= 'A' && str[i] <= 'Z') || (str[i] >= 'a' && str[i] <= 'z')) {
                in_escape = false;
            }
            continue;
        }
        // Count non-escape characters
        // Note: This doesn't handle multi-byte UTF-8, but the nav string
        // primarily uses ASCII with some known-width Unicode arrows
        width++;
    }

    return width;
}
```

**Important:** This function should also account for UTF-8 characters like the Unicode arrows (↑↓←→) which are 3 bytes each but 1 column wide.

### Part 2: Use visual width for separator calculation

In `separator_render()`, change line 168-174 from:

```c
// Calculate how many separator chars to draw (leave room for nav + debug)
size_t info_len = nav_len + debug_len;
size_t sep_chars = width;
if (info_len > 0 && info_len < width) {
    sep_chars = width - info_len;
}
```

To:

```c
// Calculate VISUAL width of info strings (excluding ANSI escape codes)
size_t nav_visual = visual_width(nav_str, nav_len);
size_t debug_visual = visual_width(debug_str, debug_len);
size_t info_visual = nav_visual + debug_visual;

// Calculate how many separator chars to draw (leave room for visual width)
size_t sep_chars = width;
if (info_visual > 0 && info_visual < width) {
    sep_chars = width - info_visual;
}
```

### Part 3: Handle UTF-8 width correctly

The navigation string contains Unicode arrows (↑↓←→) which are:
- 3 bytes each in UTF-8
- 1 terminal column each when displayed

Options:
1. **Simple approach**: Count any byte sequence starting with 0xE2 as 1 character (covers box-drawing and arrows)
2. **Proper approach**: Use wcwidth() or similar to get actual character width

For this task, use the simple approach since the strings are well-known.

## TDD Cycle

### Red
1. Create a test that verifies separator fills full width with navigation context
2. The test should:
   - Create a separator layer with navigation context set
   - Render to an output buffer with known width (e.g., 80)
   - Count the total visual width of output (excluding ANSI codes)
   - Assert total width equals terminal width
3. Run `make check` - test should fail

### Green
1. Implement `visual_width()` helper function
2. Update `separator_render()` to use visual width
3. Run the test - should pass
4. Run `make check` - all tests pass
5. Run `make lint` - passes

### Refactor
1. Consider moving `visual_width()` to a shared utility if needed elsewhere
2. Add comments explaining the ANSI escape code handling

## Sub-agent Usage
- Use sub-agents to search for existing visual width functions in the codebase
- Use sub-agents to verify the fix doesn't break other separator rendering

## Overcoming Obstacles
- If the simple UTF-8 handling doesn't work, consult wcwidth() documentation
- Test with various navigation states (all enabled, all disabled, mixed)

## Post-conditions
- `make` compiles successfully
- `make check` passes
- `make lint` passes
- Top separator fills full terminal width with navigation context
- Visual appearance matches expected layout
- Working tree is clean (all changes committed)

## Deviations
Document any deviation from this plan with reasoning.
