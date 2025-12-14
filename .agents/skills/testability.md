# Testability

## Description
Refactoring patterns to make code more testable.

## Philosophy

Code that's hard to test is telling you something about its design. Coverage gaps are opportunities to improve architecture, not problems to silence.

## Refactoring Strategies

### 1. Extract Pure Logic from I/O

Separate computation from side effects:

```c
// BEFORE: Hard to test - I/O mixed with logic
res_t process_config(const char *path) {
    char *content = read_file(path);
    // ... 50 lines of parsing logic ...
    return OK_RES;
}

// AFTER: Easy to test - pure logic extracted
res_t parse_config(const char *content, config_t *out);  // Pure, testable
res_t load_config(const char *path, config_t *out) {     // Thin I/O wrapper
    char *content = read_file(path);
    return parse_config(content, out);
}
```

### 2. Make Dependencies Explicit

Inject dependencies instead of using globals or direct calls:

```c
// BEFORE: Hidden dependency
void process(data_t *d) {
    log_message("Processing...");  // Hidden global logger
}

// AFTER: Explicit dependency
void process(data_t *d, logger_t *log) {
    log->write(log, "Processing...");  // Testable with mock logger
}
```

### 3. Split Large Functions

Break complex functions into smaller, testable units:

```c
// BEFORE: Monolithic, many branches
res_t handle_request(req_t *req) {
    // 200 lines with 15 branches
}

// AFTER: Composed of testable pieces
static res_t validate_request(req_t *req);
static res_t authorize_request(req_t *req);
static res_t execute_request(req_t *req);

res_t handle_request(req_t *req) {
    TRY(validate_request(req));
    TRY(authorize_request(req));
    return execute_request(req);
}
```

### 4. Convert Infallible res_t to void

If a function cannot fail, don't pretend it can:

```c
// BEFORE: Fake error path (untestable branch)
res_t init_defaults(config_t *c) {
    c->timeout = 30;
    c->retries = 3;
    return OK_RES;  // Can never fail
}

// AFTER: Honest signature
void init_defaults(config_t *c) {
    c->timeout = 30;
    c->retries = 3;
}
```

### 5. Reduce Conditional Complexity

Flatten nested conditions, use early returns:

```c
// BEFORE: Deep nesting, hard to cover all paths
if (a) {
    if (b) {
        if (c) {
            // action
        }
    }
}

// AFTER: Early returns, linear flow
if (!a) return;
if (!b) return;
if (!c) return;
// action
```

### 6. Parameterize Behavior

Replace hardcoded values with parameters for test control:

```c
// BEFORE: Hardcoded, can't test timeout behavior
void wait_for_response(void) {
    sleep(30);  // Can't test without waiting 30s
}

// AFTER: Parameterized, testable
void wait_for_response(int timeout_sec) {
    sleep(timeout_sec);  // Test with timeout_sec=0
}
```

## When to Refactor vs Mock

| Situation | Action |
|-----------|--------|
| Our code is complex | Refactor to simplify |
| External library call | Mock via wrapper.h |
| System call | Mock via wrapper.h |
| Inline vendor function | Wrap once, test wrapper |

**Rule:** Refactor our code, mock external dependencies.

## References

- `project/error_handling.md` - res_t patterns
- `project/memory.md` - Ownership affects testability
