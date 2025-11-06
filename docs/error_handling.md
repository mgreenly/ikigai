# Error Handling

## Philosophy

ikigai uses **Rust-style Result types** with **talloc-managed memory**. All errors are explicitly handled at call sites. No exceptions, no global error state.

Every allocation lives in a talloc context hierarchy. Errors are allocated on the same context as success values, ensuring clean lifecycle management.

## Core Design

### Result Type

```c
typedef struct {
  union {
    void *ok;
    ik_error_t *err;
  };
  bool is_err;
} ik_result_t;
```

Functions that can fail return `ik_result_t`. Check `is_err` to determine success or failure.

### Error Structure

```c
typedef struct ik_error {
  ik_error_code_t code;
  const char *file;
  int32_t line;
  char message[128];
} ik_error_t;
```

Errors are **talloc-allocated** on a provided context. They include:
- Error code enum
- Source file and line number (captured automatically)
- Human-readable message (printf-style formatting)

### Error Codes

```c
typedef enum {
  IK_OK = 0,
  // Add specific codes as needed during implementation
  // Examples: IK_ERR_IO, IK_ERR_PROTOCOL, IK_ERR_NETWORK
} ik_error_code_t;
```

**Strategy:** Start with an empty enum, add codes organically as Phase 1 progresses.

## API Reference

### Creating Results

```c
// Success - zero overhead, just wraps pointer
return OK(value);

// Error - allocates on talloc context
return ERR(ctx, IO, "Cannot open file: %s", path);
```

**Important:** `ERR()` requires a talloc context as first parameter.

### Checking Results

```c
ik_result_t res = config_load(ctx, path);

if (ik_is_err(&res)) {
    // Handle error
    ik_error_fprintf(stderr, res.err);
    talloc_free(ctx);
    return EXIT_FAILURE;
}

// Use success value
ik_cfg_t *config = res.ok;
```

### Propagating Errors

```c
ik_result_t server_init(TALLOC_CTX *ctx) {
    ik_result_t res = config_load(ctx, path);
    CHECK(res);  // Return early if error

    ik_cfg_t *config = res.ok;
    // ... continue with config ...
}
```

`CHECK(expr)` evaluates the expression, and if it's an error, immediately returns that error from the current function.

### Inspecting Errors

```c
ik_error_code_t code = ik_error_code(err);
const char *msg = ik_error_message(err);
ik_error_fprintf(stderr, err);  // Formatted output with file:line
```

## Memory Management Integration

### Ownership Rules

1. **Caller provides context** - Functions receive `TALLOC_CTX *ctx` parameter
2. **Results allocated on context** - Both success values and errors are children of `ctx`
3. **Caller frees context** - Single `talloc_free(ctx)` cleans up everything

### Lifecycle Example

```c
int main(int argc, char *argv[]) {
    // Root context for entire program
    TALLOC_CTX *root = talloc_new(NULL);

    // Load config - result allocated on root
    ik_result_t res = config_load(root, "~/.ikigai/config.json");
    if (ik_is_err(&res)) {
        ik_error_fprintf(stderr, res.err);
        talloc_free(root);  // Frees error
        return EXIT_FAILURE;
    }

    ik_cfg_t *config = res.ok;
    int result = server_run(root, config);

    // Single cleanup - frees config, server state, everything
    talloc_free(root);
    return result;
}
```

### Per-Request Context Pattern

```c
int websocket_callback(request, response, user_data) {
    ws_connection_t *conn = user_data;

    // Create temporary context for this message (child of connection context)
    TALLOC_CTX *msg_ctx = talloc_new(conn->ctx);

    ik_result_t res = message_parse(msg_ctx, json_data);
    if (ik_is_err(&res)) {
        send_error_response(res.err);
        talloc_free(msg_ctx);  // Frees error and any partial allocations
        return -1;
    }

    message_t *msg = res.ok;
    // ... process message ...

    talloc_free(msg_ctx);  // Frees msg and all related allocations
    return 0;
}
```

## Common Patterns

### Pattern 1: Simple function with error

```c
ik_result_t parse_port(TALLOC_CTX *ctx, const char *str) {
    char *endptr;
    long port = strtol(str, &endptr, 10);

    if (*endptr != '\0') {
        return ERR(ctx, INVALID_ARG, "Invalid port number: %s", str);
    }

    if (port < 1024 || port > 65535) {
        return ERR(ctx, OUT_OF_RANGE, "Port out of range: %ld", port);
    }

    int *result = talloc(ctx, int);
    *result = (int)port;
    return OK(result);
}
```

### Pattern 2: Function with multiple error paths

```c
ik_result_t config_load(TALLOC_CTX *ctx, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return ERR(ctx, IO, "Cannot open config: %s", path);
    }

    ik_cfg_t *config = talloc_zero(ctx, ik_cfg_t);

    // Parse config file
    json_error_t jerr;
    json_t *root = json_loadf(f, 0, &jerr);
    fclose(f);

    if (!root) {
        return ERR(ctx, INVALID_ARG, "JSON parse error: %s", jerr.text);
    }

    // Extract fields
    json_t *key = json_object_get(root, "api_key");
    if (!json_is_string(key)) {
        json_decref(root);
        return ERR(ctx, INVALID_ARG, "Missing api_key in config");
    }

    config->api_key = talloc_strdup(config, json_string_value(key));
    json_decref(root);

    return OK(config);
}
```

### Pattern 3: Propagating errors up the stack

```c
ik_result_t server_init(TALLOC_CTX *ctx, const char *config_path) {
    // Load config - if error, CHECK returns early
    ik_result_t res = config_load(ctx, config_path);
    CHECK(res);
    ik_cfg_t *config = res.ok;

    // Validate config
    res = config_validate(ctx, config);
    CHECK(res);

    // Initialize server state
    server_state_t *state = talloc_zero(ctx, server_state_t);
    state->config = config;

    return OK(state);
}
```

### Pattern 4: Temporary context for intermediate work

```c
ik_result_t process_large_file(TALLOC_CTX *ctx, const char *path) {
    // Create temporary context for file processing
    TALLOC_CTX *tmp = talloc_new(ctx);

    ik_result_t res = load_file_contents(tmp, path);
    if (ik_is_err(&res)) {
        talloc_free(tmp);
        return res;  // Propagate error (already on ctx)
    }

    // Process data (all allocations on tmp)
    file_data_t *data = res.ok;
    result_t *result = process_data(tmp, data);

    // Extract what we need and move to parent context
    summary_t *summary = talloc_zero(ctx, summary_t);
    summary->count = result->count;
    summary->digest = talloc_steal(summary, result->digest);

    // Free all temporary allocations
    talloc_free(tmp);

    return OK(summary);
}
```

## Out-of-Memory Handling

If `talloc_zero()` fails when allocating an error, we return a global static OOM error:

```c
ik_error_t *err = talloc_zero(ctx, ik_error_t);
if (!err) {
    return &ik_oom_error;  // Static, read-only error
}
```

**Rationale:** Using a static error allows the program to propagate OOM failures up the stack instead of aborting immediately. Higher-level code can then exit gracefully, log the error, or potentially recover by freeing caches.

The static error is read-only (no file/line information), which avoids race conditions in multi-threaded scenarios. Use `ik_error_is_static(err)` to check if an error is the OOM error before attempting to free it (though in practice, talloc_free of a non-talloc pointer is safe, it just does nothing).

## Thread Safety

All errors are allocated independently on talloc contexts. The only shared static state is the read-only `ik_oom_error`, which is safe to access from multiple threads simultaneously.

libulfius creates a separate thread for each WebSocket connection. Each connection has its own context hierarchy:

```
root (main thread)
└── server_state
    └── connection_1 (thread 1)
        └── msg_ctx (created/freed per message)
    └── connection_2 (thread 2)
        └── msg_ctx (created/freed per message)
```

Errors allocated in one thread's context are isolated from other threads.

## Migration Notes

### Removed from original error.h

- **Static error storage** - All errors now talloc-allocated
- **Thread result types** (`ik_thread_result_t`, `OK_T`, `ERR_T`) - Not needed with talloc
- **DEFER/FINALLY macros** - talloc hierarchy makes manual cleanup unnecessary
- **IK_FAIL/IK_FAIL_IF** - Redundant with `ERR()` and `CHECK()`

### What stayed the same

- **Result type semantics** - Still check `is_err`, access `ok` or `err`
- **Rust-style ergonomics** - `OK()`, `ERR()`, `CHECK()` macros
- **Zero-overhead success** - `OK(value)` is just a struct literal
- **Error inspection** - Same functions for reading error details

## Testing Strategy

### Test Organization

Tests are organized into two categories:

**Unit Tests** (`tests/unit/`)
- One test file per source module (1:1 mapping)
- `src/error.c` → `tests/unit/error_test.c`
- `src/foo.c` → `tests/unit/foo_test.c`
- Each module's test file includes error handling tests

**Integration Tests** (`tests/integration/`)
- Tests that cross module boundaries
- Named descriptively by what they test
- Example: `oom_integration_test.c` for complex OOM scenarios

**Shared Test Infrastructure** (`tests/test_utils.*`)
- OOM injection capabilities for testing allocation failures
- Linked with all tests automatically

### Example Unit Tests

Every module test file should include error handling tests:

```c
START_TEST(test_config_load_missing_file) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_result_t res = config_load(ctx, "/nonexistent/path");

    ck_assert(ik_is_err(&res));
    ck_assert_int_eq(ik_error_code(res.err), IK_ERR_IO);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_config_load_success) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_result_t res = config_load(ctx, "test_fixtures/valid_config.json");

    ck_assert(ik_is_ok(&res));
    ik_cfg_t *config = res.ok;
    ck_assert_str_eq(config->api_key, "test-key-123");

    talloc_free(ctx);  // Frees config and all children
}
END_TEST
```

### Testing OOM Behavior

The test infrastructure provides OOM injection via `test_utils.h`:

```c
#include "../test_utils.h"

START_TEST(test_oom_handling) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Force the next error allocation to fail
    oom_test_fail_next_alloc();

    ik_result_t res = ERR(ctx, INVALID_ARG, "This triggers OOM");

    // Should get static OOM error instead of aborting
    ck_assert(ik_is_err(&res));
    ck_assert(ik_error_is_static(res.err));
    ck_assert_int_eq(res.err->code, IK_ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST
```

OOM test control functions:
- `oom_test_fail_next_alloc()` - Fail the next allocation
- `oom_test_fail_after_n_calls(n)` - Fail after N successful allocations
- `oom_test_reset()` - Reset to normal operation
- `oom_test_get_call_count()` - Get allocation attempt count

### Running Tests

```bash
make check              # Run all tests (unit + integration)
make check-unit         # Run only unit tests
make check-integration  # Run only integration tests
```

## Future Considerations

- **Structured error data** - Phase 3+ may add `void *details` field for complex errors
- **Error chaining** - May add `ik_error_t *cause` for nested error context
- **Named contexts** - Use `talloc_set_name()` for better leak debugging

---

**Principle:** Errors are values, not control flow. Make them explicit, make them cheap, make them impossible to ignore.
