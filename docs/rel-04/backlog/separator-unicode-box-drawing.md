# Separator Unicode Box Drawing Character

## Problem

The separator lines use ASCII hyphen (`-`) which looks crude compared to proper box-drawing characters.

**Current:**
```
------------------------------------------------------------------------
```

**Desired:**
```
────────────────────────────────────────────────────────────────────────
```

## Why

- Box-drawing character `─` (U+2500) is purpose-built for horizontal rules
- Visually cleaner - no gaps between characters
- Consistent with modern terminal UI conventions
- The codebase already supports UTF-8 throughout

## Proposed Fix

Change the separator character from `-` to `─` (U+2500 BOX DRAWINGS LIGHT HORIZONTAL) in the separator layer rendering code.

## Scope

- Single file change: `src/layer_separator.c`
- Character constant or string literal change
- No logic changes required
- Update any tests that assert on separator output

## Considerations

- UTF-8 encoding: `─` is 3 bytes (`0xE2 0x94 0x80`)
- Terminal compatibility: Box-drawing characters are universally supported in modern terminals
- Width calculation: `─` is a single-width character, same as `-`
