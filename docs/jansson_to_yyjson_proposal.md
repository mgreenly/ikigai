# Decision: Migrate from jansson to yyjson

**Date:** 2025-11-13
**Status:** APPROVED (deferred to LLM integration phase)
**Timing:** Before Phase 3 (LLM Integration)
**Note:** This proposal references `protocol.c` which was removed in Phase 2.5. The migration will apply to `config.c` and future LLM integration code.

---

## Decision

**MIGRATE to yyjson** before implementing LLM integration.

**Primary Reason:** Better talloc integration eliminates the lifecycle mismatch between jansson's reference counting and talloc's hierarchical memory management.

**Secondary Benefit:** 3× faster JSON parsing relevant for streaming LLM responses.

---

## Why Migrate

### Current Problem (jansson)
- Two memory management systems: talloc hierarchy + jansson reference counting
- Every message struct requires destructor boilerplate to bridge models
- 11 manual `json_decref()` calls in error paths
- Ownership bugs: must remember `json_incref()` when storing JSON

```c
// Current: Destructor pattern required
typedef struct {
    json_t *payload;  // jansson-managed
} ik_protocol_msg_t;

static int destructor(ik_protocol_msg_t *msg) {
    if (msg->payload) json_decref(msg->payload);
    return 0;
}
```

### Solution (yyjson)
- Single memory model: talloc-only via custom allocator
- No destructors needed
- Automatic cleanup in error paths
- No reference counting bugs possible

```c
// Future: Direct talloc integration
typedef struct {
    yyjson_doc *payload_doc;  // talloc-managed
} ik_protocol_msg_t;

// No destructor needed - talloc handles everything
```

---

## Migration Details

### Effort Estimate
- **Total: 6-10 hours**
  - Setup + config module: 2-3 hours
  - Protocol module rewrite: 3-4 hours
  - Testing + documentation: 1-3 hours

### Scope
- Rewrite ~400 lines (src/protocol.c, src/config.c)
- Rewrite ~600 lines of tests
- Vendor yyjson source (single header + implementation)
- Update documentation

### Dependencies
- **yyjson not in Debian repos** → vendor source in `src/vendor/yyjson/`
- License: MIT (compatible)
- Size: ~4KB vendored files

---

## Migration Phases

### 1. Preparation (1-2 hours)
- Vendor yyjson in `src/vendor/yyjson/`
- Create talloc allocator wrapper
- Update Makefile

### 2. Config Module (1-2 hours)
- Simpler module, good warm-up
- Update `src/config.c`
- Verify tests pass

### 3. Protocol Module (3-4 hours)
- Update `src/protocol.{c,h}`
- Rewrite parsing/serialization
- Remove destructor pattern
- Update all tests
- Maintain 100% coverage

### 4. Cleanup (1-2 hours)
- Remove jansson from build
- Update documentation
- Final validation

---

## talloc Allocator Implementation

```c
// Wrapper: yyjson → talloc
static void *json_talloc_malloc(void *ctx, size_t size) {
    return talloc_size((TALLOC_CTX *)ctx, size);
}

static void *json_talloc_realloc(void *ctx, void *ptr, size_t size) {
    return talloc_realloc_size((TALLOC_CTX *)ctx, ptr, size);
}

static void json_talloc_free(void *ctx, void *ptr) {
    talloc_free(ptr);
}

yyjson_alc ik_make_talloc_allocator(TALLOC_CTX *ctx) {
    yyjson_alc alc = {
        .malloc = json_talloc_malloc,
        .realloc = json_talloc_realloc,
        .free = json_talloc_free,
        .ctx = ctx
    };
    return alc;
}
```

**Usage:**
```c
yyjson_alc alc = ik_make_talloc_allocator(ctx);
yyjson_doc *doc = yyjson_read_opts(json_str, len, 0, &alc, NULL);
// All JSON memory in talloc hierarchy - no manual cleanup
```

---

## Checklist

**Phase 1: Setup**
- [ ] Vendor `yyjson.{c,h}` in `src/vendor/yyjson/`
- [ ] Create `src/json_allocator.{c,h}`
- [ ] Update Makefile
- [ ] Verify build

**Phase 2: Config**
- [ ] Update `src/config.c`
- [ ] Tests pass
- [ ] Valgrind clean

**Phase 3: Protocol**
- [ ] Update `src/protocol.h` structure
- [ ] Rewrite `ik_protocol_msg_parse()`
- [ ] Rewrite `ik_protocol_msg_serialize()`
- [ ] Remove destructor pattern
- [ ] Update all protocol tests
- [ ] 100% coverage maintained
- [ ] Valgrind clean

**Phase 4: Cleanup**
- [ ] Remove jansson from Makefile
- [ ] Update `docs/memory.md`
- [ ] Update `docs/architecture.md`

**Validation:**
- [ ] All tests pass
- [ ] 100% coverage
- [ ] No memory leaks
- [ ] Clean system build

---

## Key Differences: jansson vs yyjson

### Parsing
```c
// jansson (before)
json_t *root = json_loads(str, 0, &err);
if (!root) return ERR(ctx, PARSE, "...");
if (error) {
    json_decref(root);  // Must remember
    return ERR(...);
}

// yyjson (after)
yyjson_doc *doc = yyjson_read_opts(str, len, 0, &alc, NULL);
if (!doc) return ERR(ctx, PARSE, "...");
if (error) return ERR(...);  // Auto cleanup via talloc
```

### Object Access
```c
// jansson
json_t *val = json_object_get(root, "key");
const char *str = json_string_value(val);

// yyjson
yyjson_val *val = yyjson_obj_get(root, "key");
const char *str = yyjson_get_str(val);
```

---

## References

- **yyjson:** https://github.com/ibireme/yyjson (MIT license)
- **jansson:** https://github.com/akheron/jansson (MIT license)
- **Related docs:** `docs/memory.md`, `docs/architecture.md`

---

**Status:** Approved - Implementation to occur before Phase 3 (LLM Integration)
