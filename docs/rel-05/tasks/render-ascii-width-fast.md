# Task: Fast Path for ASCII Character Width Calculation

## Target
Render Performance Optimization - Avoid library calls for common ASCII characters

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/memory.md (talloc ownership)

## Pre-read Source (patterns)
- src/render_cursor.c (lines 49-72 - utf8proc_charwidth called per character)
- src/ansi.c (ANSI escape sequence handling)

## Pre-read Tests (patterns)
- tests/unit/render/*.c (render test patterns)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `utf8proc_charwidth()` is called for every character during cursor calculation

## Task
Add a fast path for ASCII characters (0x20-0x7E) which are all width 1. This avoids the `utf8proc_charwidth()` library call for ~90% of typical text.

Current approach:
```c
for each codepoint:
    width += utf8proc_charwidth(codepoint);  // Library call every time
```

New approach:
```c
for each byte:
    if (byte >= 0x20 && byte <= 0x7E) {
        width += 1;  // Fast path - no library call
    } else if (byte < 0x80) {
        // Control character - width 0
    } else {
        // UTF-8 multi-byte - decode and call utf8proc_charwidth
    }
```

## TDD Cycle

### Red
1. Create helper function declaration in `src/render_cursor.h` (or appropriate header):
   ```c
   // Calculate display width of text, optimized for ASCII
   // Handles UTF-8 and ANSI escape sequences
   size_t ik_display_width_fast(const char *text, size_t len);
   ```

2. Create `tests/unit/render/display_width_fast_test.c`:
   - Test empty string returns 0
   - Test ASCII string "hello" returns 5
   - Test ASCII with spaces "hello world" returns 11
   - Test control characters (tab, newline) return 0 width
   - Test UTF-8 wide character (e.g., CJK) returns 2
   - Test UTF-8 narrow character (e.g., accented Latin) returns 1
   - Test mixed ASCII and UTF-8
   - Test string with ANSI escape sequences (should skip them)
   - Test emoji width handling

3. Run `make check` - expect failures

### Green
1. Create optimized implementation:
   ```c
   size_t ik_display_width_fast(const char *text, size_t len)
   {
       size_t width = 0;
       size_t i = 0;

       while (i < len) {
           unsigned char c = (unsigned char)text[i];

           // ANSI escape sequence - skip entirely
           if (c == '\x1b' && i + 1 < len && text[i + 1] == '[') {
               size_t skip = ik_ansi_skip_csi(text, len, i);
               if (skip > 0) {
                   i += skip;
                   continue;
               }
           }

           // ASCII printable (0x20-0x7E) - width 1
           if (c >= 0x20 && c <= 0x7E) {
               width += 1;
               i += 1;
               continue;
           }

           // ASCII control (0x00-0x1F, 0x7F) - width 0
           if (c < 0x80) {
               i += 1;
               continue;
           }

           // UTF-8 multi-byte - decode and use utf8proc
           utf8proc_int32_t codepoint;
           utf8proc_ssize_t bytes = utf8proc_iterate(
               (const utf8proc_uint8_t *)text + i, len - i, &codepoint);
           if (bytes > 0) {
               int char_width = utf8proc_charwidth(codepoint);
               if (char_width > 0) {
                   width += (size_t)char_width;
               }
               i += (size_t)bytes;
           } else {
               i += 1;  // Invalid UTF-8 - skip byte
           }
       }

       return width;
   }
   ```

2. Replace existing width calculation in `src/render_cursor.c` with call to `ik_display_width_fast()`

3. Run `make check` - expect pass

### Refactor
1. Verify same results as previous implementation
2. Profile to confirm performance improvement
3. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `ik_display_width_fast()` function exists
- ASCII characters (0x20-0x7E) bypass `utf8proc_charwidth()` call
- UTF-8 and ANSI sequences still handled correctly
- Same width values produced (behavioral equivalence)
- Working tree is clean (all changes committed)
