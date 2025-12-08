# Task: Add Second Horizontal Rule Below Input

## Target
Feature: Autocompletion - UI Layout

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/layer_separator.c (existing separator layer)
- src/layer_wrappers.h (layer creation API)
- src/repl_init.c (layer cake setup)
- src/repl.h (repl context)

### Pre-read Tests (patterns)
- tests/unit/layer/separator_layer_test.c

## Pre-conditions
- `make check` passes
- First HR bug fixed (completion-hr-bug-fix task)

## Task
Add a second horizontal rule below the input area. Completion suggestions will appear below this second HR.

**Current layout:**
```
scrollback...
-----------  (separator)
input area
completion   (below input, no separator)
```

**Required layout:**
```
scrollback...
-----------  (upper separator)
input area
-----------  (lower separator)
completion
```

## TDD Cycle

### Red
1. Add to `src/repl.h`:
   ```c
   ik_layer_t *lower_separator_layer;  // Separator below input
   bool lower_separator_visible;       // Visibility flag
   ```

2. Add tests:
   ```c
   START_TEST(test_lower_separator_renders)
   {
       // Render frame with completion active
       // Output should contain two separator lines
   }
   END_TEST

   START_TEST(test_lower_separator_only_with_completion)
   {
       // Lower separator visible only when completion active
       // (or always visible - TBD based on design preference)
   }
   END_TEST
   ```

3. Run `make check` - expect failure

### Green
1. Create lower separator layer in `repl_init.c`:
   ```c
   // Initialize lower separator
   repl->lower_separator_visible = true;

   // Create lower separator layer
   repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator",
                                                            &repl->lower_separator_visible);

   // Add to layer cake after input layer, before completion layer
   // Order: scrollback, spinner, separator, input, lower_separator, completion
   result = ik_layer_cake_add_layer(repl->layer_cake, repl->lower_separator_layer);
   if (is_err(&result)) PANIC("allocation failed");
   ```

2. Update layer order in `repl_init.c`:
   ```c
   // Add layers to cake (in order)
   ik_layer_cake_add_layer(repl->layer_cake, repl->scrollback_layer);
   ik_layer_cake_add_layer(repl->layer_cake, repl->spinner_layer);
   ik_layer_cake_add_layer(repl->layer_cake, repl->separator_layer);       // Upper HR
   ik_layer_cake_add_layer(repl->layer_cake, repl->input_layer);
   ik_layer_cake_add_layer(repl->layer_cake, repl->lower_separator_layer); // Lower HR
   ik_layer_cake_add_layer(repl->layer_cake, repl->completion_layer);
   ```

3. Run `make check` - expect pass

### Refactor
1. Consider: should lower separator only appear when completion is active?
   - Option A: Always visible (current approach)
   - Option B: Only visible when completion active (tie to completion visibility)
2. Run `make lint` - verify passes
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- Two horizontal rules render (above and below input)
- Completion appears below lower horizontal rule
- Layer order correct: scrollback, spinner, upper_separator, input, lower_separator, completion
