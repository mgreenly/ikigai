# REPL Terminal - Phase 3: Scrollback Buffer Module

[← Back to REPL Terminal Overview](README.md)

**Goal**: Add scrollback buffer storage with layout caching for historical output.

Implement the scrollback buffer module with pre-computed display widths and layout caching. This enables O(1) reflow on terminal resize (1000× faster than naive approach).

## Rationale

**Key insight**: Pre-compute display width once when line is created, then reflow becomes pure arithmetic. This enables O(1) reflow on terminal resize.

**Performance target**:
- 1000 lines × 50 chars average = 50,000 chars total
- Resize time: ~2μs (vs 2.5ms naive approach = 1000× faster)
- Memory overhead: ~32 KB metadata for 1000 lines (plus text content)

## Implementation Tasks

### Task 1: Scrollback Module

**Create**: `src/scrollback.h` and `src/scrollback.c`

**Public API**:
```c
// Layout metadata (hot data for reflow calculations)
typedef struct {
    size_t display_width;    // Pre-computed: sum of all charwidths
    size_t physical_lines;   // Cached: wraps to N lines at cached_width
} ik_line_layout_t;

// Scrollback buffer with separated hot/cold data
typedef struct ik_scrollback_t {
    // Cold data: text content (only accessed during rendering)
    char *text_buffer;           // All line text in one contiguous buffer
    size_t *text_offsets;        // Where each line starts in text_buffer
    size_t *text_lengths;        // Length in bytes of each line

    // Hot data: layout metadata (accessed during reflow and rendering)
    ik_line_layout_t *layouts;   // Parallel array of layout info

    size_t count;                // Number of lines
    size_t capacity;             // Allocated capacity
    int32_t cached_width;        // Terminal width for cached physical_lines
    size_t total_physical_lines; // Cached: sum of all physical_lines
    size_t buffer_used;          // Bytes used in text_buffer
    size_t buffer_capacity;      // Total buffer capacity
} ik_scrollback_t;

// Create scrollback buffer
res_t ik_scrollback_create(void *parent, int32_t terminal_width,
                            ik_scrollback_t **scrollback_out);

// Append line to scrollback (calculates display_width once)
res_t ik_scrollback_append_line(ik_scrollback_t *sb, const char *text, size_t len);

// Ensure layout cache is valid for current terminal width
void ik_scrollback_ensure_layout(ik_scrollback_t *sb, int32_t terminal_width);

// Find which logical line contains a given physical row
size_t ik_scrollback_find_logical_line_at_physical_row(ik_scrollback_t *sb,
                                                        size_t target_row);

// Query functions
size_t ik_scrollback_get_line_count(const ik_scrollback_t *sb);
size_t ik_scrollback_get_total_physical_lines(const ik_scrollback_t *sb);
const char *ik_scrollback_get_line_text(const ik_scrollback_t *sb, size_t index,
                                         size_t *len_out);
```

**Core Logic**:
```c
// Calculate display width of a logical line (sum of all charwidths)
// This is computed ONCE when the line is created
size_t calculate_display_width(const char *text, size_t text_len);

// Calculate how many physical lines a logical line wraps to
// This is O(1) arithmetic using pre-computed display_width
size_t calculate_physical_lines(size_t display_width, int32_t terminal_width);
```

**Implementation Notes**:
- Pre-compute `display_width` once per line using `utf8proc_charwidth()`
- Reflow on resize is just arithmetic: `display_width / terminal_width`
- Separated hot/cold data for cache locality during reflow
- No newline characters stored - implicit in array structure
- Each line is immutable once added (input buffer is mutable, scrollback is not)

**Estimated size**: ~200-250 lines

### Task 2: Comprehensive Unit Tests

**Test Coverage** (`tests/unit/input_buffer/scrollback_test.c`):
- Line append:
  - Single line
  - Multiple lines
  - Empty lines
  - Lines with various UTF-8 content (ASCII, CJK, emoji, combining chars)
- Display width calculation:
  - ASCII text
  - Wide characters (CJK = 2 cells)
  - Emoji
  - Combining characters (0 cells)
  - Mixed content
- Physical lines calculation:
  - Short lines (no wrapping)
  - Exact terminal width boundary
  - Long lines requiring wrapping
  - Empty lines (always 1 physical line)
- Layout cache:
  - Cache valid when width unchanged
  - Cache invalidation on resize
  - Reflow recalculation
  - Running total update
- Find logical line at physical row:
  - First line
  - Middle lines
  - Last line
  - Out of bounds
- OOM injection tests (via MOCKABLE wrappers)

**Coverage requirement**: 100% (lines, functions, branches)

**Estimated size**: ~300-400 lines

### Task 3: Input Buffer Layout Caching

**Update** `src/input_buffer.h`:
```c
typedef struct ik_input_buffer_t {
    ik_byte_array_t *text;
    ik_cursor_t *cursor;

    // Layout cache (NEW - for input buffer wrapping)
    size_t physical_lines;     // Cached: total wrapped lines
    int32_t cached_width;      // Width this is valid for
    bool layout_dirty;         // Need to recalculate
} ik_input_buffer_t;

// Ensure input buffer layout cache is valid
void ik_input_buffer_ensure_layout(ik_input_buffer_t *ws, int32_t terminal_width);

// Invalidate layout cache (call on text edits)
void ik_input_buffer_invalidate_layout(ik_input_buffer_t *ws);

// Query cached physical line count
size_t ik_input_buffer_get_physical_lines(const ik_input_buffer_t *ws);
```

**Update** `src/input_buffer.c`:
- Add layout cache fields to struct
- Implement `ik_input_buffer_ensure_layout()` - full scan (input buffer is mutable, may contain \n)
- Implement `ik_input_buffer_invalidate_layout()` - mark dirty flag
- Call `invalidate_layout()` in all text edit functions
- Input buffer needs full scan on recalculation (unlike scrollback which pre-computes)

**Test Coverage**:
- Layout calculation with various content
- Cache invalidation on text edits
- Cache validation after recalculation
- Terminal resize handling

### Task 4: Manual Verification via client.c

**Demo**: Scrollback buffer with manual line additions
- Create scrollback buffer
- Add lines with various content (ASCII, UTF-8, long lines that wrap)
- Query total physical lines before/after wrapping
- Simulate terminal resize, verify reflow speed
- Print some lines back out
- Test with 1000+ lines to verify performance

**Verification Checklist**:
- [ ] Lines stored correctly in contiguous buffer
- [ ] Display width calculated correctly for various UTF-8 content
- [ ] Physical line counts correct after wrapping
- [ ] Terminal resize updates physical_lines correctly
- [ ] Reflow performance acceptable (1000 lines < 5ms)
- [ ] No memory leaks (talloc hierarchy)
- [ ] Cache locality benefit measurable (if profiling)

## What We Validate

- Scrollback storage with contiguous text buffer
- Pre-computed display width for immutable lines
- O(1) reflow via arithmetic (no UTF-8 re-scanning on resize)
- Layout caching for both scrollback and input buffer
- Separated hot/cold data for cache locality
- Lazy recalculation (only when needed)

## What We Defer

- Viewport calculation (comes in Phase 4)
- Scrollback rendering (comes in Phase 4)
- Scrolling commands (comes in Phase 4)
- Integration with REPL (comes in Phase 4)

## Phase 3 Complete When

- [ ] scrollback module implemented with 100% test coverage
- [ ] input buffer module extended with layout caching
- [ ] Unit tests verify all calculation correctness
- [ ] client.c demo works and passes manual verification
- [ ] Performance target met (1000 lines reflow < 5ms)
- [ ] 100% test coverage maintained
- [ ] `make check && make lint && make coverage` all pass

## Performance Analysis

**Without pre-computed display_width** (naive approach):
```c
// Every resize: O(n * m) where n = lines, m = avg line length
for each resize:
    for each logical line:
        scan UTF-8, decode, call utf8proc_charwidth()  // Expensive
```

**With pre-computed display_width** (our approach):
```c
// On line creation: O(m) - one time
calculate_display_width()  // Scan UTF-8 once

// On resize: O(n) - just arithmetic!
for each logical line:
    physical_lines = display_width / terminal_width  // O(1) division
```

**Typical REPL scenario**:
- 1000 scrollback lines, 50 chars average = 50,000 chars
- 256 bytes input buffer text (typical), up to 4K max
- 24 rows × 80 cols terminal

**Resize performance:**
- **Without pre-computation**: 50,000 chars × 50ns (UTF-8 decode + charwidth) = 2.5ms
- **With pre-computation**: 1000 lines × 2ns (integer division) = 2μs
- **1000× faster resize** (2.5ms → 2μs)

**Memory overhead:**
- Per line: +16 bytes (display_width + physical_lines)
- For 1000 lines: +16 KB (negligible)
- Cache locality: Hot data (layouts) separated from cold data (text)

## Development Approach

Strict TDD with 100% coverage requirement.
