# Explicit Context Pattern

## Overview

This document describes the architectural decision to introduce an **Explicit Context** pattern via `ik_env_t` - a struct that aggregates cross-cutting infrastructure dependencies and is passed explicitly through the codebase.

## Problem

Several cross-cutting concerns currently rely on global state or are threaded individually through function signatures:

- **Logger** - Global singleton with static mutex and state
- **Time** - Direct `time()` calls, not mockable for testing
- **Environment variables** - Direct `getenv()` calls scattered throughout
- **Terminal fd** - Hardcoded `/dev/tty` path
- **Signal state** - Global `g_resize_pending` flag
- **Configuration** - Needs to be accessible throughout, will grow over time

This creates testability issues (can't inject mocks) and will become increasingly verbose as more cross-cutting concerns are added.

## Solution

Introduce `ik_env_t` - a runtime environment struct holding all cross-cutting infrastructure services, passed explicitly through the call chain.

### What Belongs in `ik_env_t`

| Concern | Rationale |
|---------|-----------|
| Configuration (`ik_cfg_t`) | Read-only after init, needed throughout codebase |
| Logger | Cross-cutting, used by almost every module |
| Clock/time function | Enables deterministic testing of timestamps |
| Environment variable accessor | Enables testing with controlled environment |
| Terminal file descriptor | Enables testing without real TTY |
| Signal state (resize flag) | Removes need for global state |
| Memory context reference | Convenience back-reference to talloc parent |

### What Stays Explicit (Not in `ik_env_t`)

| Concern | Rationale |
|---------|-----------|
| Database context | Domain-specific, only needed by conversation layer |
| LLM client | Domain-specific, only needed by AI interaction layer |
| Scrollback, input buffer | REPL internals, not cross-cutting |

The distinction: **infrastructure** (used everywhere) goes in `ik_env_t`; **domain services** (used by specific modules) remain explicit parameters.

## Naming

- **Type**: `ik_env_t`
- **Variable**: `env`
- **Pattern name**: Explicit Context Pattern

The name "env" is concise. While "env" has Unix associations with environment variables, context makes meaning clear when it's a struct pointer parameter.

## Relationship to talloc

These serve different purposes:

- **talloc context** - Memory ownership (who frees what)
- **ik_env_t** - Runtime services (what services are available)

The `ik_env_t` struct is allocated ON a talloc context. It holds references to services. The talloc hierarchy handles memory lifetime; `ik_env_t` handles "what infrastructure exists."

```
root_ctx (talloc)
  └─> env (ik_env_t, allocated on root_ctx)
       ├─> env->cfg (reference to config)
       ├─> env->log (logger, child of env)
       └─> env->mem = root_ctx (back-reference)
  └─> repl (ik_repl_ctx_t)
       └─> repl->env = env (reference, not owned)
```

## Benefits

### Testability

Tests can inject mock implementations:
- Fake clock returning fixed time for deterministic timestamp tests
- Mock logger capturing output for assertion
- Controlled environment variables
- Mock terminal fd for headless testing

### Reduced Parameter Proliferation

Instead of threading logger, config, clock, terminal separately through every function, pass single `env` parameter.

### Explicit Dependencies

Unlike global singletons, `ik_env_t` makes dependencies visible in function signatures. A function taking `ik_env_t *env` clearly depends on runtime infrastructure.

### Easy Extension

Adding new cross-cutting concerns means adding a field to `ik_env_t`, not changing function signatures throughout codebase.

### No Global State

Removes the global singleton logger, global signal flag, and hardcoded paths. Everything flows from explicit initialization.

## Architectural Intent

### Initialization

The `main()` function becomes the composition root:

1. Create `ik_env_t` with real implementations (real clock, real getenv, real terminal)
2. Load configuration through env
3. Initialize domain services (database, LLM client) receiving env
4. Initialize REPL receiving env plus domain services
5. Run

### Testing

Test setup creates `ik_env_t` with mock implementations:

1. Create `ik_env_t` with fakes (fixed clock, mock getenv, no terminal)
2. Exercise code under test
3. Assert on deterministic behavior

### Function Signatures

Functions needing infrastructure take `ik_env_t *env` as first parameter (after talloc context where applicable). Functions needing only specific services continue to receive those explicitly.

## Migration Path

This is a refactoring effort to be done incrementally:

1. Define `ik_env_t` struct with initial fields
2. Create `ik_env_init()` that sets up real implementations
3. Migrate logger to use `env->log` instead of global
4. Add `time_()` wrapper and migrate marks/logger to use `env->clock`
5. Move signal state from global to `env->resize_pending`
6. Update function signatures to accept `env` parameter
7. Create test helpers for mock env construction

Each step can be done independently while maintaining working code.

## Related Patterns

- **Dependency Injection** - `ik_env_t` is the mechanism for injecting cross-cutting dependencies
- **Composition Root** - `main()` assembles the env with real implementations
- **Service Locator** (avoided) - Unlike service locator, `ik_env_t` is passed explicitly, not accessed globally

## Related Documentation

- [di.md](../.agents/skills/di.md) - Dependency Injection patterns in ikigai
- [ddd.md](../.agents/skills/ddd.md) - Domain concepts and ubiquitous language
- [memory.md](memory.md) - talloc memory management patterns
