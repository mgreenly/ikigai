---
name: di
description: Dependency Injection (DI) skill for the ikigai project
---

# Dependency Injection (DI)

## Core Concept

Pass dependencies from outside rather than creating internally.

```c
// BAD - hidden dependencies
void repl_init() {
    config = load_config();
    db = connect_database();
}

// GOOD - explicit dependencies
void repl_init(config_t *cfg, db_t *db, llm_t *llm) {
    // Dependencies passed from caller
}
```

## ikigai Patterns

### Pattern 1: Constructor Injection

Main creates all dependencies, passes them down:

```c
int main(void) {
    void *root_ctx = talloc_new(NULL);
    ik_cfg_t *cfg = NULL;
    TRY(ik_cfg_load(root_ctx, "~/.ikigai/config.json", &cfg));

    ik_db_ctx_t *db = NULL;
    TRY(ik_db_init(root_ctx, cfg->db_connection_string, &db));

    ik_llm_client_t *llm = NULL;
    TRY(ik_llm_init(root_ctx, cfg->openai_api_key, &llm));

    ik_repl_ctx_t *repl = NULL;
    TRY(ik_repl_init(root_ctx, cfg, db, llm, &repl));
    ik_repl_run(repl);

    talloc_free(root_ctx);
}
```

### Pattern 2: Context Parameter

First parameter is always talloc context:

```c
res_t ik_cfg_load(TALLOC_CTX *ctx, const char *path, ik_cfg_t **out_cfg);
res_t ik_db_init(TALLOC_CTX *ctx, const char *conn_str, ik_db_ctx_t **out_db);
res_t ik_repl_init(TALLOC_CTX *ctx, ik_cfg_t *cfg, ik_db_ctx_t *db,
                   ik_llm_client_t *llm, ik_repl_ctx_t **out_repl);
```

### Pattern 3: Struct Composition

```c
struct ik_repl_ctx_t {
    ik_cfg_t *cfg;              // Injected (external)
    ik_db_ctx_t *db;            // Injected (external)
    ik_llm_client_t *llm;       // Injected (external)
    ik_term_ctx_t *term;        // Created internally
    ik_scrollback_t *scrollback;// Created internally
};
```

## DI for Testing

```c
// Production
ik_llm_client_t *llm = ik_llm_init(ctx, real_api_key);
ik_repl_init(ctx, cfg, db, llm, &repl);

// Test - same init, mock dependency
ik_llm_client_t *mock_llm = create_mock_llm_that_fails();
ik_repl_init(ctx, cfg, db, mock_llm, &repl);
```

## Anti-Patterns

```c
// BAD: Service Locator - hidden dependency
llm_client_t *llm = service_locator_get("llm");

// BAD: Singleton - global state
static config_t *g_config = NULL;

// BAD: Constructor I/O - hidden file access
config = load_config_from_disk();

// BAD: Defensive null checks on required deps
if (!cfg || !db) PANIC("Missing");

// GOOD: Assert required dependencies
assert(cfg != NULL);
assert(db != NULL);
```

## DI in C

- **No interfaces** - Use function pointers for polymorphism:
```c
typedef struct {
    res_t (*send)(void *ctx, message_t *msg);
    res_t (*stream)(void *ctx, message_t *msg, chunk_callback_t cb);
} llm_provider_vtable_t;
```
- **No frameworks** - Composition in `main()`, simple and explicit
- **Manual wiring** - More code, but transparent

## Rules

1. `main()` is composition root
2. First param is `TALLOC_CTX`
3. Pass dependencies as parameters, never create internally
4. Modules don't know about config files
5. No global state - everything in talloc hierarchy
6. Assert required dependencies, don't defensively null-check

## Summary

- **Explicit** - Function signature reveals dependencies
- **Testable** - Pass mock implementations
- **Composable** - Wire differently per context
- **No globals** - Multiple instances coexist
- **Clear ownership** - Caller controls lifecycle
