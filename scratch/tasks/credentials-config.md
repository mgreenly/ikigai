# Task: Remove API Key from Config

**Layer:** 0
**Model:** sonnet/none
**Depends on:** credentials-core.md

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/config.h` - Current `ik_cfg_t` struct with `openai_api_key`
- `src/config.c` - Current config loading implementation
- `src/credentials.h` - New credentials API (from credentials-core.md)

## Objective

Remove `openai_api_key` from `ik_cfg_t` struct and config loading. Update configuration files to reflect the separation of credentials from configuration. This task only modifies the config module; production code that uses `cfg->openai_api_key` is updated in subsequent tasks.

## Interface

Changes to existing struct (removal):

| Struct | Field to Remove | Reason |
|--------|-----------------|--------|
| `ik_cfg_t` | `char *openai_api_key` | Credentials now loaded via credentials module |

## Behaviors

### Config Struct Changes

- Remove `openai_api_key` field from `ik_cfg_t` struct definition
- Remove initialization of `openai_api_key` in `create_default_config()`
- Remove JSON parsing of `openai_api_key` in `ik_cfg_load()`
- Remove any validation of `openai_api_key` field

### Configuration File Updates

- Remove `openai_api_key` from `etc/ikigai/config.json`
- Create `etc/ikigai/credentials.example.json` as a reference template
- Update Makefile install target (if applicable) to include credentials example

### Example Credentials File Format

```json
{
  "openai": {
    "api_key": "sk-proj-YOUR_KEY_HERE"
  },
  "anthropic": {
    "api_key": "sk-ant-api03-YOUR_KEY_HERE"
  },
  "google": {
    "api_key": "YOUR_KEY_HERE"
  }
}
```

## Test Scenarios

Not applicable - this task removes functionality. Existing config tests will be updated in credentials-tests-config.md.

## Postconditions

- [ ] `ik_cfg_t` no longer has `openai_api_key` field
- [ ] `config.c` no longer loads/validates `openai_api_key`
- [ ] `etc/ikigai/config.json` no longer has `openai_api_key`
- [ ] `etc/ikigai/credentials.example.json` exists with all three providers
- [ ] `make src/config.o` compiles successfully
- [ ] `grep -n "openai_api_key" src/config.h` returns nothing
- [ ] `grep -n "openai_api_key" src/config.c` returns nothing
- [ ] `grep "openai_api_key" etc/ikigai/config.json` returns nothing

## Verification

```bash
# Verify field removed from header
grep -n "openai_api_key" src/config.h
# Should return nothing

# Verify field removed from config.c
grep -n "openai_api_key" src/config.c
# Should return nothing

# Verify config.json updated
grep "openai_api_key" etc/ikigai/config.json
# Should return nothing

# Build succeeds (may have errors in other files - that's expected)
make src/config.o
# Should succeed

# Check for compile errors mentioning openai_api_key
make 2>&1 | grep "openai_api_key"
# Will show errors in OTHER files - that's expected, fixed in next task
```

## Note

After this task, the codebase will NOT compile cleanly because:
- `src/openai/client.c` still references `cfg->openai_api_key`
- `src/openai/client_multi_request.c` still references `cfg->openai_api_key`
- Test files still reference `cfg->openai_api_key`

These are fixed in subsequent tasks: `credentials-production.md` and test credential tasks.
