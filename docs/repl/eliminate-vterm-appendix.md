# Eliminate libvterm: Appendices

This document contains supplementary analysis and design discussion notes.

## Appendix: Code Size Analysis

**Current render module**:
- `src/render.h`: 86 lines
- `src/render.c`: 222 lines
- `tests/unit/render/render_test.c`: 346 lines
- **Total**: 654 lines

**Key functions in render.c**:
- `encode_utf8()`: 26 lines (manual UTF-8 encoding)
- `ik_render_create()`: 45 lines (vterm initialization)
- `ik_render_clear()`: 8 lines (vterm reset)
- `ik_render_write_text()`: 18 lines (vterm write wrapper)
- `ik_render_set_cursor()`: 18 lines (vterm cursor wrapper)
- `ik_render_blit()`: 64 lines (cell iteration + terminal writes)

**Note**: We manually encode UTF-8 in render.c despite vterm handling it, because we need to convert vterm's codepoint cells back to UTF-8 for the terminal.

**Estimated new module**:
- `src/render_direct.h`: ~35 lines (simpler API)
- `src/render_direct.c`: ~100 lines (cursor calc + blit)
- `tests/unit/render_direct/render_direct_test.c`: ~200 lines
- **Total**: ~335 lines

**Net savings**: ~320 lines + simpler logic + better performance

---

## Design Discussion Summary

### The Unavoidable Complexity

The core challenge with scrollback+viewport rendering is that **wrapping calculations cannot be avoided**:

1. **Storage must be logical lines** (semantic units), not physical screen lines
   - Logical: "user: hello" or "ai: long response..."
   - Physical: Screen rows after wrapping at terminal width
   - Why: Terminal resize must not require reflow logic

2. **Viewport needs physical line counts** to calculate which logical lines are visible
   - Need to know: "Show physical rows 500-523" → "Which logical lines?"
   - Requires mapping logical lines to physical line ranges

3. **This calculation is needed regardless of rendering method**
   - With VTerm: Still need to calculate to know what to feed it
   - With direct: Need to calculate for viewport and cursor positioning

### Why VTerm Doesn't Help

**VTerm calculates wrapping internally**, but:
- Doesn't expose logical→physical line mapping
- We'd need to scan all cells to reverse-engineer the layout
- Memory overhead: ~96 KB for full screen VTerm vs ~16 KB for cache
- Processing overhead: Cell iteration + UTF-8 encode/decode
- Still need our own calculation for viewport management

**Multiple VTerms considered but rejected**:
- One VTerm per logical line: Massive memory overhead (40+ MB for 1000 lines)
- One VTerm for scrollback + one for dynamic: Still need external line mapping
- Can't query VTerm for "which logical line is at physical row N?"

### The Winning Strategy: Cache Physical Line Counts

**Key insight**: Wrapping calculations are expensive but infrequent.

**What to cache**:
- Each scrollback line: `physical_lines` (how many rows at current width)
- Scrollback total: `total_physical_lines` (sum of all)
- Workspace: `physical_lines` (total rows for dynamic zone)

**When to recalculate**:
- Terminal resize: O(n×m) - rare, acceptable
- Text edit: O(m) once per frame - much cheaper than per keystroke
- Scrollback append: O(k) for new line only - cheap
- Rendering: O(1) use cached values - fast

**Performance gain**:
- 500× reduction in processing per second (typical usage)
- 52× reduction in write syscalls (52 writes → 1 write)
- 26× reduction in bytes processed per frame
- ~2.5ms resize time for 1000 lines (imperceptible to user)

### Architecture Decision

**Eliminate libvterm entirely** because:
1. ✅ Wrapping calculation needed either way
2. ✅ Caching makes it efficient
3. ✅ Simpler code (one rendering path)
4. ✅ Better performance (fewer writes, less overhead)
5. ✅ Less memory (96 KB VTerm → 16 KB cache)
6. ✅ Fewer dependencies to maintain

**Implementation is straightforward**:
- `count_wrapped_lines()`: ~50 lines (uses libutf8proc)
- `calculate_cursor_screen_position()`: ~50 lines
- Cache management: ~30 lines per buffer type
- Total: ~150-200 lines to replace 654 lines of VTerm integration
