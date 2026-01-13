## Objective

Implement the three TODO items in `.claude/harness/ralph/run` to improve ralph's tracking and flexibility.

## Reference

- `.claude/harness/ralph/run` - Lines 4-20 contain the TODO comments
- `.claude/library/ralph/SKILL.md` - Ralph skill documentation

## Outcomes

1. **Running totals tracking** (lines 4-11)
   - Track across all iterations: iteration count, assistant messages, tool uses, tokens (input, output, cache_read, cache_create), cost, elapsed time
   - Display totals in `display_summary()` at completion
   - Token counts extracted from stream-json events
   - Cost calculated from token counts using model pricing

2. **--no-quotes flag** (line 13)
   - Add `--no-quotes` CLI flag
   - When set, `maybe_ralph_says()` returns immediately without printing quotes
   - Default behavior unchanged (quotes enabled)

3. **Multiple goal files** (lines 15-19)
   - Support `--goal=FILE1,FILE2,FILE3` syntax
   - Process each goal file sequentially
   - Reset progress/summary files when transitioning to next goal
   - Keep running totals (iterations, tokens, cost, time) across all goals
   - Report aggregate stats at completion

## Acceptance

- All three TODOs implemented and working
- TODO comments removed from the script
- `ruby -c .claude/harness/ralph/run` passes (syntax check)
