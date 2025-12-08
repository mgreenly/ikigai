# Task: Separator Unicode Box Drawing Character

## Target
Feature: UI Polish - Visual Refinement

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/layer_separator.c (current separator implementation)
- src/layer_wrappers.h (layer API)

### Pre-read Tests (patterns)
- tests/unit/layer/separator_layer_test.c

## Pre-conditions
- `make check` passes
- `make lint` passes

## Task
Replace the ASCII hyphen (`-`) character in the separator layer with the Unicode box-drawing character `─` (U+2500 BOX DRAWINGS LIGHT HORIZONTAL).

**Current:**
```
------------------------------------------------------------------------
```

**After:**
```
────────────────────────────────────────────────────────────────────────
```

The box-drawing character:
- UTF-8 encoding: 3 bytes (`0xE2 0x94 0x80`)
- Display width: 1 column (same as `-`)
- Universally supported in modern terminals

## TDD Cycle

### Red
1. Update test in `tests/unit/layer/separator_layer_test.c`:
   ```c
   START_TEST(test_separator_layer_renders_unicode_box_drawing)
   {
       // Setup separator layer
       // Render to output buffer
       // Verify output contains "─" (U+2500) not "-"
       // Character is 3 bytes in UTF-8
   }
   END_TEST
   ```

2. Run `make check` - expect failure (still renders `-`)

### Green
1. Modify `separator_render()` in `src/layer_separator.c`:
   - Change from appending "-" (1 byte) to appending "─" (3 bytes: `\xE2\x94\x80`)
   - Width loop should still iterate `width` times (1 character per column)

2. Run `make check` - expect pass

### Refactor
1. Consider defining the character as a constant for clarity
2. Run `make lint` - verify passes
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- Separator renders with `─` (U+2500) instead of `-`
- Visual output shows clean horizontal line without gaps
