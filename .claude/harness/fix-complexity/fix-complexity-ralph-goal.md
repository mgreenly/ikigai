## Objective

All functions must meet complexity thresholds:
- Cyclomatic complexity ≤ 15
- Nesting depth ≤ 5

## Strategy

1. Run `.claude/scripts/check-complexity` to identify functions exceeding thresholds
2. For each function with violations:
   - Use `.claude/scripts/check-complexity --file=<path>` to see details for that file
   - **Analyze the function to understand complexity sources**
     - Count decision points (if, while, for, switch cases, &&, ||)
     - Identify deeply nested blocks
     - Look for repeated patterns or complex conditionals
   - **Refactor to reduce complexity**
     - Extract helper functions for logical subtasks
     - Use early returns (guard clauses) to reduce nesting
     - Simplify conditional expressions (combine conditions, use boolean functions)
     - Replace nested loops with helper functions
     - Break switch statements into dispatch tables or helper functions
   - **Verify compilation after changes**
     - Run `make fmt` to format all code. Ignore the output - this is just an action to format the code. Wait for it to complete before moving to the next step.
     - Run `.claude/scripts/check-compile` to ensure `{"ok": true}`
     - Fix any compilation or linking errors
   - **Maintain existing functionality**
     - Ensure ownership (talloc) patterns remain correct
     - Preserve error handling patterns
     - Keep existing API contracts intact
3. Re-run complexity check after each fix to verify progress

## Refactoring Techniques

### Reducing Cyclomatic Complexity

**Extract helper functions:**
```c
// Before: complexity = 20
Result validate_input(Input *input) {
    if (input == NULL) return ERR("null input");
    if (input->name == NULL) return ERR("null name");
    if (strlen(input->name) == 0) return ERR("empty name");
    if (strlen(input->name) > 100) return ERR("name too long");
    if (input->age < 0) return ERR("negative age");
    if (input->age > 150) return ERR("age too high");
    // ... more validations
    return OK(input);
}

// After: complexity = 3
Result validate_input(Input *input) {
    FORWARD_ERROR(validate_input_not_null(input));
    FORWARD_ERROR(validate_name(input->name));
    FORWARD_ERROR(validate_age(input->age));
    return OK(input);
}
```

**Use early returns (guard clauses):**
```c
// Before: nested complexity
Result process(Data *data) {
    if (data != NULL) {
        if (data->valid) {
            if (data->ready) {
                // actual logic here
            }
        }
    }
    return ERR("invalid");
}

// After: flat complexity
Result process(Data *data) {
    if (data == NULL) return ERR("null data");
    if (!data->valid) return ERR("invalid data");
    if (!data->ready) return ERR("not ready");
    // actual logic here
    return OK(result);
}
```

**Simplify conditionals:**
```c
// Before: multiple decision points
if ((x > 0 && x < 10) || (x > 20 && x < 30)) {
    // ...
}

// After: extracted to named function
if (is_in_valid_range(x)) {
    // ...
}

static bool is_in_valid_range(int x) {
    return (x > 0 && x < 10) || (x > 20 && x < 30);
}
```

### Reducing Nesting Depth

**Invert conditions:**
```c
// Before: depth = 4
void process_items(Item **items, size_t count) {
    if (items != NULL) {
        for (size_t i = 0; i < count; i++) {
            if (items[i] != NULL) {
                if (items[i]->valid) {
                    // process item
                }
            }
        }
    }
}

// After: depth = 2
void process_items(Item **items, size_t count) {
    if (items == NULL) return;

    for (size_t i = 0; i < count; i++) {
        if (items[i] == NULL) continue;
        if (!items[i]->valid) continue;
        // process item
    }
}
```

**Extract nested loops:**
```c
// Before: depth = 3
for (size_t i = 0; i < rows; i++) {
    for (size_t j = 0; j < cols; j++) {
        if (matrix[i][j] > threshold) {
            // complex processing
        }
    }
}

// After: depth = 2
for (size_t i = 0; i < rows; i++) {
    process_row(matrix[i], cols, threshold);
}

static void process_row(int *row, size_t cols, int threshold) {
    for (size_t j = 0; j < cols; j++) {
        if (row[j] > threshold) {
            // complex processing
        }
    }
}
```

## Guidelines

- **One responsibility per function** - if a function does multiple things, split it
- **Prefer readability over cleverness** - clear code with more functions is better than compact complex code
- **Keep helper functions close** - declare static helpers near the function using them
- **Maintain error handling patterns** - use Result types consistently
- **Test incrementally** - verify compilation after each refactoring step

## Validation

**IMPORTANT:** Trust the harness script outputs. Do not re-implement checking logic. The scripts return JSON with `{"ok": true}` or `{"ok": false, "items": [...]}` - trust these results.

**Validation sequence (must run in this exact order):**
1. **First: Format** - Run `make fmt` to format all code. Ignore the output - this is just an action to format the code. Wait for it to complete before moving to the next step.
2. **Then: Compile check** - Run `.claude/scripts/check-compile` and verify it returns `{"ok": true}`
3. **Finally: Complexity check** - Run `.claude/scripts/check-complexity` and verify it returns `{"ok": true}`

## Hints

- For talloc/ownership patterns: `/load memory`
- For Result type patterns: `/load errors`
- For naming conventions: `/load style`
- For understanding module structure: `/load source-code`

## Acceptance

DONE when all validation steps pass in order and both checks return `{"ok": true}`.
