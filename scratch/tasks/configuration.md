# Task: Implement Multi-Provider Configuration

**Layer:** 2
**Depends on:** shared-utilities.md

## Pre-Read

**Skills:**
- `/load errors`

**Source Files:**
- `src/config.c`
- `src/config.h`

**Plan Docs:**
- `scratch/plan/configuration.md`

## Objective

Implement config.json and credentials.json loading for multi-provider support.

## Deliverables

1. Update `src/config.c` for new config.json format:
   - `default_provider` field
   - `providers` section with per-provider settings
   - `default_model`, `default_thinking` per provider
   - Backward compatibility with simple format

2. Create credentials loading:
   - `ik_credentials_load()` - Load from credentials.json
   - Environment variable precedence (env > file)
   - Warn on insecure file permissions (not 600)

3. Update existing config loading to use new format

## Reference

- `scratch/plan/configuration.md` - Full specification

## New Config Format

```json
{
  "default_provider": "anthropic",
  "providers": {
    "anthropic": {"default_model": "claude-sonnet-4-5", "default_thinking": "med"},
    "openai": {"default_model": "gpt-4o", "default_thinking": "none"},
    "google": {"default_model": "gemini-2.5-flash", "default_thinking": "med"}
  }
}
```

## Verification

- Config loads with new format
- Credentials load from env and file
- Permissions warning works

## Postconditions

- [ ] Config loads new format
- [ ] Credentials load from env and file
- [ ] Permissions warning works
