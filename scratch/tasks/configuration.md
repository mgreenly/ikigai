# Task: Implement Multi-Provider Configuration

**Model:** sonnet/none
**Depends on:** credentials-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result type patterns

**Source:**
- `src/config.c` - Existing configuration loading
- `src/config.h` - Configuration struct definition
- `src/credentials.h` - Credentials API (separate from config)

**Plan:**
- `scratch/plan/configuration.md` - Full specification

## Objective

Extend config.json to support multi-provider settings including default provider selection and per-provider defaults (model, thinking level). This task focuses on configuration structure only - credentials loading is handled by the separate credentials API implemented in credentials-core.md.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_config_load(TALLOC_CTX *ctx, const char *path, ik_config_t **out)` | Load and parse config.json with new multi-provider format |
| `const char *ik_config_get_default_provider(ik_config_t *config)` | Get default provider name with env var override support |
| `res_t ik_config_get_provider_defaults(ik_config_t *config, const char *provider, ik_provider_defaults_t **out)` | Get defaults for specific provider |

Structs to update:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_config_t` | db_connection_string, default_provider, providers_map | Main configuration including existing and new fields |

Structs to define:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_provider_defaults_t` | default_model, default_thinking | Per-provider default settings |

Files to update:

- `src/config.c` - Parse new JSON format
- `src/config.h` - Add new struct members

## Behaviors

### Configuration Loading
- Parse existing fields: `db_connection_string`, etc. (unchanged)
- Parse new field: `default_provider` (string)
- Parse new section: `providers` (object with per-provider settings)
- Each provider entry contains: `default_model`, `default_thinking`
- Return ERR_PARSE if JSON is malformed
- Return OK with partial config if optional fields missing

### Backward Compatibility
- Support old config format (no `default_provider` or `providers`)
- Fall back to hardcoded defaults if new fields missing
- Emit warning if using deprecated format (optional)

### Default Provider Selection
- Check `IKIGAI_DEFAULT_PROVIDER` environment variable first
- Use config.json `default_provider` if env var not set
- Fall back to hardcoded default ("openai") if neither present
- Return ERR_INVALID_ARG if provider name is empty string

### Provider Defaults Lookup
- Look up provider name in `providers` map
- Return defaults if found
- Return hardcoded defaults if provider not in config
- Return ERR_INVALID_ARG if provider name is NULL

### Hardcoded Fallback Defaults
- anthropic: model="claude-sonnet-4-5", thinking="med"
- openai: model="gpt-4o", thinking="none"
- google: model="gemini-2.5-flash", thinking="med"

### Memory Management
- Config allocated on provided talloc context
- All strings and nested objects allocated on config context
- Caller owns returned config and provider_defaults

### Environment Variable Override
- `IKIGAI_DEFAULT_PROVIDER` overrides config file setting
- Empty string treated as unset (fall through to config file)
- No other environment variables for config (credentials use separate env vars)

## Configuration Format

### New Format (rel-07)
```json
{
  "db_connection_string": "postgresql://...",
  "default_provider": "anthropic",
  "providers": {
    "anthropic": {
      "default_model": "claude-sonnet-4-5",
      "default_thinking": "med"
    },
    "openai": {
      "default_model": "gpt-4o",
      "default_thinking": "none"
    },
    "google": {
      "default_model": "gemini-2.5-flash",
      "default_thinking": "med"
    }
  }
}
```

### Legacy Format (backward compatible)
```json
{
  "db_connection_string": "postgresql://..."
}
```

### Supported Providers (rel-07 scope)
- `anthropic` - Claude models
- `openai` - GPT and reasoning models
- `google` - Gemini models

**Note:** Plan documents mention xAI and Meta providers, but these are out of scope for rel-07.

## Test Scenarios

### Configuration Loading
- Load config with new format: returns OK with all fields
- Load config with legacy format: returns OK with defaults
- Load config with missing provider section: uses hardcoded defaults
- Load malformed JSON: returns ERR_PARSE

### Default Provider Selection
- Config specifies "anthropic": returns "anthropic"
- Env var set to "google": overrides config, returns "google"
- Neither set: returns hardcoded default "openai"
- Empty string in config: treated as unset, uses hardcoded default

### Provider Defaults Lookup
- Provider in config: returns configured defaults
- Provider not in config: returns hardcoded defaults
- NULL provider name: returns ERR_INVALID_ARG
- Unknown provider: returns hardcoded generic defaults

### Backward Compatibility
- Old config format loads successfully
- Existing fields (db_connection_string) still accessible
- Missing new fields don't cause errors

## Postconditions

- [ ] Config loads new format with `default_provider` and `providers` section
- [ ] `ik_config_get_default_provider()` returns correct value
- [ ] `ik_config_get_provider_defaults()` returns correct values
- [ ] Missing provider config falls back to hardcoded defaults
- [ ] Existing config fields (db_connection_string, etc.) still work
- [ ] `IKIGAI_DEFAULT_PROVIDER` env var overrides config file
- [ ] Legacy config format still loads successfully
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: configuration.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).