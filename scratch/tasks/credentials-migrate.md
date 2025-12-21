# Task: Migrate from Config API Key to Credentials Module

**Layer:** 0
**Model:** sonnet/none
**Depends on:** credentials-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load source-code` - Map of src/*.c files

**Source:**
- `src/config.h` - Current `ik_cfg_t` struct with `openai_api_key`
- `src/config.c` - Current config loading implementation
- `src/credentials.h` - New credentials API (from credentials-core.md)
- `src/openai/client.c` - Uses `cfg->openai_api_key`
- `src/openai/client_multi_request.c` - Uses `cfg->openai_api_key`

## Objective

Remove `openai_api_key` from `ik_cfg_t` and update all production code to use the new credentials API. This task performs the complete migration atomically so the build remains passing throughout.

## Interface

Changes to existing struct (removal):

| Struct | Field to Remove | Reason |
|--------|-----------------|--------|
| `ik_cfg_t` | `char *openai_api_key` | Credentials now loaded via credentials module |

No new interfaces - updates existing code to call:

| Function | Usage Pattern |
|----------|---------------|
| `ik_credentials_load()` | Call at start of each OpenAI send function |
| `ik_credentials_get()` | Retrieve "openai" provider key from loaded credentials |

## Behaviors

### Config Struct Changes

- Remove `openai_api_key` field from `ik_cfg_t` struct definition
- Remove initialization of `openai_api_key` in `create_default_config()`
- Remove JSON parsing of `openai_api_key` in `ik_cfg_load()`
- Remove any validation of `openai_api_key` field

### Configuration File Updates

- Remove `openai_api_key` from `etc/ikigai/config.json`
- Create `etc/ikigai/credentials.example.json` as a reference template

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

### Credential Loading Pattern

Each function that previously used `cfg->openai_api_key` must:

1. Load credentials using `ik_credentials_load(ctx, NULL, &creds)`
2. Get OpenAI key using `ik_credentials_get(creds, "openai")`
3. Check if key is NULL or empty, return ERR if missing
4. Pass key to HTTP functions

### Error Messaging

Use consistent, helpful error messages:

```
"No OpenAI credentials. Set OPENAI_API_KEY or add to ~/.config/ikigai/credentials.json"
```

### Files to Update

| File | Changes |
|------|---------|
| `src/config.h` | Remove `openai_api_key` field from struct |
| `src/config.c` | Remove parsing/initialization of `openai_api_key` |
| `src/openai/client.c` | Add `#include "credentials.h"`, update `ik_openai_send()` and `ik_openai_stream()` |
| `src/openai/client_multi_request.c` | Add `#include "credentials.h"`, update `ik_openai_multi_request()` and `ik_openai_multi_request_stream()` |
| `etc/ikigai/config.json` | Remove `openai_api_key` |
| `etc/ikigai/credentials.example.json` | Create with all three providers |

## Test Scenarios

Not applicable - this task modifies production code. Tests are updated in credential test tasks.

## Postconditions

- [ ] `ik_cfg_t` no longer has `openai_api_key` field
- [ ] `config.c` no longer loads/validates `openai_api_key`
- [ ] `etc/ikigai/config.json` no longer has `openai_api_key`
- [ ] `etc/ikigai/credentials.example.json` exists with all three providers
- [ ] `src/openai/client.c` includes `credentials.h` and uses credentials API
- [ ] `src/openai/client_multi_request.c` includes `credentials.h` and uses credentials API
- [ ] `grep -rn "openai_api_key" src/` returns nothing (no production references)
- [ ] `make src/config.o` succeeds
- [ ] `make src/openai/client.o` succeeds
- [ ] `make src/openai/client_multi_request.o` succeeds

## Verification

```bash
# Verify no references to openai_api_key in production code
grep -rn "openai_api_key" src/
# Should return nothing

# Verify credentials.h is included in openai files
grep -n "#include.*credentials.h" src/openai/client.c
grep -n "#include.*credentials.h" src/openai/client_multi_request.c
# Both should return a line

# Build production code
make src/config.o src/openai/client.o src/openai/client_multi_request.o
# Should succeed

# Verify credentials example exists
cat etc/ikigai/credentials.example.json
# Should show template with all three providers
```

## Note

After this task, tests will still fail because they reference `cfg->openai_api_key`. The credential test tasks fix the test files.
