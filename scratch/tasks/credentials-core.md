# Task: Create Credentials Module

**Layer:** 0
**Model:** sonnet/none
**Depends on:** None

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management and ownership rules

**Source:**
- `src/config.c` - Reference for JSON parsing with yyjson
- `src/json_allocator.h` - talloc-based yyjson allocator

## Objective

Create `src/credentials.h` and `src/credentials.c` - a new module for loading API credentials from environment variables and `~/.config/ikigai/credentials.json`. This module provides a unified interface for accessing provider API keys with environment variables taking precedence over file-based configuration.

## Interface

Functions to implement (signatures and contracts):

| Function | Purpose |
|----------|---------|
| `res_t ik_credentials_load(TALLOC_CTX *ctx, const char *path, ik_credentials_t **out_creds)` | Load credentials from file and environment. Returns OK on success, ERR on parse error. Missing file is OK - returns empty credentials. |
| `const char *ik_credentials_get(const ik_credentials_t *creds, const char *provider)` | Get API key for a provider by name ("openai", "anthropic", "google"). Returns API key or NULL if not configured. |
| `bool ik_credentials_insecure_permissions(const char *path)` | Check if credentials file has insecure permissions (not 600). Returns true if permissions are insecure. |

Structs to define (members and purpose):

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_credentials_t` | `char *openai_api_key`, `char *anthropic_api_key`, `char *google_api_key` | Credentials container for all providers. Fields are NULL if not configured. |

## Behaviors

### Credential Loading Precedence

1. Environment variables (highest priority):
   - `OPENAI_API_KEY`
   - `ANTHROPIC_API_KEY`
   - `GOOGLE_API_KEY`
2. credentials.json file (fallback)

### File Handling

- If path is NULL, use default: `~/.config/ikigai/credentials.json`
- If file doesn't exist, return empty credentials (OK result)
- If file has invalid JSON, return ERR with parse message
- If file has insecure permissions (not 600), print warning to stderr but continue loading

### Credentials File Format

```json
{
  "openai": {
    "api_key": "sk-proj-..."
  },
  "anthropic": {
    "api_key": "sk-ant-api03-..."
  },
  "google": {
    "api_key": "..."
  }
}
```

Location: `~/.config/ikigai/credentials.json`
Permissions: Should be mode 600 (warning if not)

### Error Handling

| Scenario | Behavior |
|----------|----------|
| File doesn't exist | OK - use env vars only |
| File has invalid JSON | ERR with parse message |
| File missing provider | OK - that provider's key is NULL |
| Insecure permissions | Warning to stderr, continue loading |
| Provider not in file or env | `ik_credentials_get()` returns NULL |
| Empty environment variable | Ignored - file value used if available |

### Memory Management

- All strings allocated using talloc on the credentials struct
- Credentials struct allocated on provided context
- Caller owns returned credentials struct

## Test Scenarios

- **Empty credentials**: No file, no env - returns empty credentials with all fields NULL
- **Load from environment**: Set env vars, verify they populate credentials
- **Load from file**: Create temp file with JSON, verify parsing
- **Environment precedence**: Env var overrides file value for same provider
- **Provider lookup**: Test `ik_credentials_get()` with various provider names
- **Invalid JSON**: Malformed JSON returns ERR with parse error
- **Insecure permissions**: File with 0644 triggers warning, 0600 does not
- **All providers from file**: JSON with all three providers populates all fields
- **Empty env ignored**: Empty string env var doesn't override file value
- **Unknown provider**: `ik_credentials_get()` with unknown name returns NULL

## Postconditions

- [ ] `src/credentials.h` exists with API declarations
- [ ] `src/credentials.c` implements credential loading
- [ ] `tests/unit/credentials_test.c` exists with comprehensive tests
- [ ] Makefile updated with new source/header in SRC and HEADERS lists
- [ ] `make build/tests/unit/credentials_test` succeeds
- [ ] `./build/tests/unit/credentials_test` passes all tests
- [ ] Environment variables take precedence over file
- [ ] Insecure permissions trigger warning to stderr
- [ ] Missing file is OK (returns empty credentials)
- [ ] Invalid JSON returns ERR with descriptive message

## Verification

```bash
# Build credentials module
make build/tests/unit/credentials_test

# Run tests
./build/tests/unit/credentials_test

# Verify no build warnings
make 2>&1 | grep -i warning | grep credentials
# Should return nothing
```
