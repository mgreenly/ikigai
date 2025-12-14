# Ikigai Return Values

### Return Patterns
- [1. Result Type (res_t)](#1-result-type-res_t)
- [2. Direct Pointer Return](#2-direct-pointer-return)
- [3. Void Return](#3-void-return)
- [4. Primitive Return Values](#4-primitive-return-values)
- [5. Borrowed Pointer Return](#5-borrowed-pointer-return)
- [6. Callback Function Pointers](#6-callback-function-pointers)

---

## 1. Result Type (res_t)

```c
typedef struct {
    union { void *ok; err_t *err; };
    bool is_err;
} res_t;
```

**Compound literal** - C99 syntax for anonymous values: `(res_t){ .ok = value, .is_err = false }`

**Stack-allocated** - No memory allocation. Just a pointer and bool (~16 bytes). Only the `err_t` itallocates memory when there's an error.

### Error Type (err_t)

```c
typedef struct err {
    err_code_t code;
    const char *file;
    int32_t line;
    char msg[256];
} err_t;
```

**Talloc-allocated** - Errors are allocated on the parent context. The error remains valid as long as that context exists. **WARNING:** If you free the context the error is allocated on, the error becomes invalid. See `project/error_handling.md#error-context-lifetime-critical` for the error allocation context lifetime rule.

### Example Function

```c
res_t load_file(TALLOC_CTX *ctx, const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return ERR(ctx, IO, "Failed to open %s", path);
    }

    char *data = talloc_strdup(ctx, "file contents");
    fclose(f);
    return OK(data);
}
```

### Usage Example

```c
res_t res = ik_cfg_load(ctx, path);
if (is_err(&res)) {
    error_fprintf(stderr, res.err);
    return res;
}
ik_cfg_t *config = res.ok;
```

### Error Propagation Macros

**TRY()** - Extract ok value or return error:

```c
char *data = TRY(load_file(ctx, path));
// If error, returns immediately. Otherwise data contains the ok value.
```

**CHECK()** - Validate result, return error if failed:

```c
res_t res = validate_config(config);
CHECK(res);
// If error, returns immediately. Otherwise continues execution.
```

### When to Use res_t

Use `res_t` for operations that can fail at runtime:

- **File I/O** - Opening, reading, writing files
- **Network operations** - HTTP requests, socket operations
- **Parsing** - JSON, user input, external data
- **Resource acquisition** - Creating objects that require validation

Don't use for operations that only fail on out-of-memory (use direct pointer return or PANIC instead) or operations that cannot fail (use void, primitives, or borrowed pointers).

---

## 2. Direct Pointer Return

```c
foo_t *function_name(void *parent, ...params...);
```

**No error handling** - The function PANICs internally on allocation failure, so the returned pointer is guaranteed non-NULL. The object is owned by the parent context.

### Example

```c
ik_request_t *create_request(void *parent) {
    ik_request_t *req = talloc_zero_(parent, sizeof(ik_request_t));
    if (req == NULL) PANIC("Out of memory");
    return req;
}

// Usage
ik_request_t *req = create_request(ctx);
// req is guaranteed non-NULL
```

### When to Use Direct Pointer Return

Use direct pointer return for:

- **Simple object creation** - No validation, only allocation
- **Internal helpers** - Functions where OOM is the only failure mode
- **Constructor-like functions** - Building objects with no error conditions

Don't use when the operation can fail for reasons other than OOM (use res_t instead).

---

## 3. Void Return

```c
void function_name(foo_t *obj, ...params...);
```

**No error possible** - Operations that cannot fail with valid inputs.

### Example

```c
void array_set(ik_array_t *array, size_t index, const void *element) {
    memcpy(array->data + (index * array->element_size), element, array->element_size);
}

// Usage
array_set(arr, 5, &value);
```

### When to Use Void Return

Use void return for:

- **State modifications** - Operations on existing objects
- **Side effects only** - Logging, output operations
- **Cannot fail** - All preconditions enforced by caller

Don't use when operations can fail at runtime (use res_t instead).

---

## 4. Primitive Return Values

```c
size_t function_name(const foo_t *obj);
bool function_name(const foo_t *obj);
```

**Direct data return** - The return value is the actual data, not a status code. Always produces valid results.

### Example

```c
size_t array_size(const ik_array_t *array) {
    return array->size;
}

bool is_empty(const ik_array_t *array) {
    return array->size == 0;
}

// Usage
size_t count = array_size(arr);
if (is_empty(arr)) { ... }
```

### When to Use Primitive Return Values

Use primitive returns for:

- **Queries** - Size, count, capacity
- **Predicates** - Boolean checks (is_empty, is_valid)
- **Calculations** - Operations that cannot fail

The return value is data, not an error code.

---

## 5. Raw Pointer Into Buffer

```c
const char *function_name(const foo_t *obj);
void *function_name(const foo_t *obj, size_t index);
```

**Pointer into internal storage** - Returns a raw pointer to memory inside the object's buffer. This is NOT a talloc handle - it's a direct memory pointer. Cannot be freed or reparented. Use `_ptr` suffix on variables to indicate this.

### Example

```c
void *array_get(const ik_array_t *array, size_t index) {
    return (char *)array->data + (index * array->element_size);
}

// Usage
int *element_ptr = array_get(arr, 5);
// Raw pointer into arr's buffer - valid only while arr exists
```

### When to Use Raw Buffer Pointers

Use raw buffer pointers for:

- **Array element access** - Pointing into array storage
- **String access** - Pointing into internal char buffers
- **Direct memory access** - When you need to read/write internal storage

These are NOT talloc handles. They're raw pointers into another object's memory.

---

## 6. Callback Function Pointers

```c
typedef res_t (*callback_fn)(const char *data, void *ctx);
typedef bool (*predicate_fn)(const foo_t *obj);
typedef void (*render_fn)(foo_t *obj, output_t *out);
```

**Two-category system** - Callbacks fall into two categories: event/data processing (return `res_t`) and pure query/calculation (return value directly), with an exception for side-effect-only callbacks (return `void`).

### Category 1: Event/Data Processing Callbacks

**Always return `res_t`** to allow error propagation and early termination.

Use when:
- Processing async events (streaming, completions)
- Handling data that may trigger errors
- Need ability to abort/signal failure

```c
// Streaming callback
typedef res_t (*ik_stream_cb_t)(const char *chunk, void *ctx);

res_t my_handler(const char *chunk, void *ctx) {
    printf("%s", chunk);
    return OK(NULL);  // or ERR(...) to abort
}

// Completion callback
typedef res_t (*ik_http_completion_cb_t)(const ik_http_completion_t *completion, void *ctx);
```

### Category 2: Pure Query/Calculation Callbacks

**Return the computed value directly** - no error handling needed.

Use when:
- Pure calculations that cannot fail
- Predicates (visibility checks, filters)
- Height/size computations
- Read-only queries with validated inputs

```c
// Layer query callbacks
typedef bool (*ik_layer_visible_fn)(const ik_layer_t *layer);
typedef size_t (*ik_layer_height_fn)(const ik_layer_t *layer, size_t width);

bool is_visible(const ik_layer_t *layer) {
    return layer->visible;
}
```

### Exception: Render/Side-Effect Callbacks

**Return `void`** - side effects only, no computed value or error handling.

```c
typedef void (*ik_render_fn)(const ik_layer_t *layer, ik_output_buffer_t *out);

void render(const ik_layer_t *layer, ik_output_buffer_t *out) {
    output_append(out, layer->text);
}
```

This follows the same pattern as other side-effect-only functions in the codebase.

### Decision Rule

When defining a new callback type:

1. **Does it process events or handle data that could cause errors?**
   - YES → Return `res_t`
   - NO → Continue to #2

2. **Does it compute/return a value?**
   - YES → Return the value directly (`bool`, `size_t`, etc.)
   - NO → Continue to #3

3. **Side effects only (rendering, logging)?**
   - YES → Return `void`

