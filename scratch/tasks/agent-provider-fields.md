# Task: Add Provider Fields to Agent Context

**Model:** sonnet/thinking
**Depends on:** provider-types.md, database-migration.md, configuration.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load memory` - talloc ownership patterns
- `/load ddd` - Domain modeling patterns
- `/load errors` - Result type patterns

**Source:**
- `src/agent.c` - Agent context management
- `src/agent.h` - Agent context struct
- `src/db/agent.c` - Database CRUD operations
- `src/config.h` - Configuration defaults

**Plan:**
- `scratch/plan/architecture.md` - Agent context section
- `scratch/README.md` - Model Assignment section

## Objective

Update `ik_agent_ctx_t` to store provider/model/thinking configuration, including initialization from config defaults and restoration from database. This enables agents to remember their provider settings across sessions and supports lazy-loading of provider instances.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_agent_apply_defaults(ik_agent_ctx_t *agent, ik_config_t *config)` | Apply config defaults to new agent (root or forked) |
| `res_t ik_agent_restore_from_row(ik_agent_ctx_t *agent, ik_db_agent_row_t *row)` | Populate agent from database row |
| `res_t ik_agent_get_provider(ik_agent_ctx_t *agent, ik_provider_t **out)` | Get or create provider instance (lazy-loaded, cached) |

**Note:** This task uses `ik_infer_provider()` from `provider-types.md` to map model names to provider names.

Structs to update:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_agent_ctx_t` | provider (char*), model (char*), thinking_level (ik_thinking_level_t), provider_instance (ik_provider_t*) | Add provider configuration and cached instance |

Enums to use:

| Enum | Source | Purpose |
|------|--------|---------|
| `ik_thinking_level_t` | `src/providers/provider.h` (defined by provider-types.md) | Thinking/reasoning level setting |

Files to update:

- `src/agent.h` - Add new fields to `ik_agent_ctx_t`
- `src/agent.c` - Implement new functions

## Behaviors

### Apply Defaults to New Agent
- For new root agents: use `ik_config_get_default_provider(config)`
- For forked agents: inherit from parent unless explicitly overridden
- Set `agent->provider`, `agent->model`, `agent->thinking_level`
- Allocate strings on agent's talloc context
- Return ERR_INVALID_ARG if config is NULL
- Return OK after setting defaults

### Restore from Database
- Load `provider`, `model`, `thinking_level` from `ik_db_agent_row_t`
- If DB fields are NULL (old agents pre-migration): apply current config defaults
- Allocate strings on agent's talloc context
- Do not load provider_instance (lazy-loaded on first use)
- Return ERR_INVALID_ARG if row is NULL
- Return OK after restoration

### Lazy Provider Loading
- Check if `agent->provider_instance` already cached
- If cached: return existing instance
- If not cached: call `ik_provider_create(agent->provider, &instance)`
- Cache instance in `agent->provider_instance`
- Return ERR_MISSING_CREDENTIALS if provider creation fails
- Return OK with provider instance

### Memory Management
- All strings (provider, model) allocated on agent context
- Provider instance allocated on agent context (cached)
- Provider instance destroyed when agent context destroyed
- NULL-safe cleanup

### Inheritance Rules
- Root agent: uses config defaults
- Forked agent: copies parent's provider/model/thinking
- Explicit override: use provided values instead of parent/config

## Initial Agent Flow

### New Root Agent
1. Create agent: `ik_agent_create(ctx)`
2. Apply defaults: `ik_agent_apply_defaults(agent, config)`
3. Persist: `ik_db_agent_insert(db, agent)`

### Forked Agent
1. Create agent: `ik_agent_fork(parent, ctx)`
2. Apply defaults (inherits from parent): `ik_agent_apply_defaults(agent, config)`
3. Persist: `ik_db_agent_insert(db, agent)`

### Restored Agent
1. Load row: `ik_db_agent_get(db, ctx, uuid, &row)`
2. Create agent: `ik_agent_create(ctx)`
3. Restore: `ik_agent_restore_from_row(agent, row)`

## Test Scenarios

### Apply Defaults
- Root agent: gets default provider from config
- Forked agent: inherits parent's provider/model/thinking
- NULL config: returns ERR_INVALID_ARG
- Config with provider "anthropic": agent gets anthropic defaults

### Restore from Database
- Row with provider values: loads successfully
- Row with NULL provider (old agent): falls back to config defaults
- NULL row: returns ERR_INVALID_ARG
- Restored agent has correct provider/model/thinking

### Lazy Provider Loading
- First call: creates and caches provider instance
- Second call: returns cached instance
- Missing credentials: returns ERR_MISSING_CREDENTIALS
- NULL agent: returns ERR_INVALID_ARG

## Postconditions

- [ ] `ik_agent_ctx_t` has provider, model, thinking_level, provider_instance fields
- [ ] `ik_agent_apply_defaults()` sets initial provider/model/thinking from config
- [ ] `ik_agent_restore_from_row()` loads provider/model/thinking from DB
- [ ] `ik_agent_get_provider()` lazy-loads and caches provider instance
- [ ] Forked agents inherit parent's provider/model/thinking by default
- [ ] All agent tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: agent-provider-fields.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).