# Decision: Eliminate libvterm Dependency

## Status

Accepted

## Context

ikigai's REPL implementation currently uses libvterm for terminal rendering. Analysis shows libvterm provides minimal value in our architecture:

- We manage our own text buffers
- We handle UTF-8/grapheme processing ourselves
- We already use alternate screen buffering
- The only service vterm provides is calculating cursor screen position after text wrapping (~50-100 lines of logic)

We're paying for this minimal functionality with a full external dependency.

## Decision

Remove libvterm and implement direct terminal rendering.

## Consequences

### Positive

- **Code reduction**: Remove 654 lines (render.c + render.h + render_test.c)
- **Simpler implementation**: Replace with ~100-150 lines of direct terminal rendering
- **Performance improvement**: Eliminate double-buffering and cell iteration overhead
- **Reduced complexity**: One fewer dependency to manage across distros
- **Better alignment**: Matches project philosophy of explicit control and minimal dependencies

### Negative

- Need to implement cursor position calculation ourselves
- Need to handle terminal escape sequence generation directly

## Implementation

See detailed design and implementation docs:
- [Analysis](../repl/eliminate-vterm-analysis.md) - Current architecture analysis, impact, performance comparison, risk analysis
- [Design](../repl/eliminate-vterm-design.md) - Proposed solution with data structures and algorithms
- [Implementation](../repl/eliminate-vterm-implementation.md) - Implementation plan and testing strategy
- [Appendix](../repl/eliminate-vterm-appendix.md) - Code size analysis and design discussion

## References

- [REPL Phase 1 Plan](../repl/repl-phase-1.md) - Implementation plan for direct rendering
