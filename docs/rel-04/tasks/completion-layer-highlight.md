# Task: Completion Layer Highlight Current Selection

## Target
Feature: Autocompletion - UI Display

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/layer_completion.c (completion layer rendering)
- src/layer_wrappers.h (layer creation API)
- src/completion.h (completion data structure with current selection)

### Pre-read Tests (patterns)
- tests/unit/layer/completion_layer_test.c

## Pre-conditions
- `make check` passes
- Completion state machine complete (completion-state-machine task)

## Task
The completion layer already highlights the current selection with ANSI reverse video. Verify this works correctly with the new Tab cycling behavior and ensure the visual feedback is correct.

**Current behavior:**
- Layer renders all candidates
- Current selection (`comp->current`) shown in reverse video

**Required behavior:**
- Same, but verify it works with Tab cycling
- Bold (or reverse video) clearly shows which option is selected

## TDD Cycle

### Red
1. Add/update tests in `tests/unit/layer/completion_layer_test.c`:
   ```c
   START_TEST(test_highlight_follows_current)
   {
       // Setup completion with 2 candidates, current = 0
       // Render should have reverse video on first candidate

       // Move to next (current = 1)
       ik_completion_next(comp);

       // Render again - reverse video should be on second candidate
   }
   END_TEST

   START_TEST(test_highlight_cycles_correctly)
   {
       // Setup completion with 3 candidates
       // Tab through all 3, verify highlight moves correctly
       // Tab again wraps to first
   }
   END_TEST
   ```

2. Run `make check` - verify existing tests still pass

### Green
1. Review `completion_render()` in `src/layer_completion.c`:
   ```c
   static void completion_render(const ik_layer_t *layer,
                                 ik_output_buffer_t *output,
                                 size_t width,
                                 size_t start_row,
                                 size_t row_count)
   {
       // ... existing code ...

       for (size_t i = 0; i < candidates_to_render; i++) {
           const char *candidate = comp->candidates[i];
           bool is_current = (i == comp->current);  // This tracks selection

           // Apply ANSI reverse video if current
           if (is_current) {
               ik_output_buffer_append(output, "\x1b[7m", 4);  // Reverse video
           }

           // ... render candidate ...

           if (is_current) {
               ik_output_buffer_append(output, "\x1b[0m", 4);  // Reset
           }
       }
   }
   ```

2. Verify the highlight moves correctly when `comp->current` changes
3. Run `make check` - expect pass

### Refactor
1. Consider using bold (`\x1b[1m`) instead of or in addition to reverse video
2. Ensure ANSI codes don't affect width calculations
3. Run `make lint` - verify passes
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- Current selection visually highlighted (reverse video or bold)
- Highlight follows Tab cycling correctly
- Highlight wraps around correctly
