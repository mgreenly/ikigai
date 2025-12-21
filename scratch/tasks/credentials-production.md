# Task: Update Production Code to Use Credentials API

**Layer:** 0
**Model:** sonnet/none
**Depends on:** credentials-config.md

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load source-code` - Map of src/*.c files

**Source:**
- `src/credentials.h` - New credentials API
- `src/openai/client.c` - Uses `cfg->openai_api_key`
- `src/openai/client_multi_request.c` - Uses `cfg->openai_api_key`

## Objective

Update `src/openai/client.c` and `src/openai/client_multi_request.c` to use the new credentials API instead of `cfg->openai_api_key`. This enables OpenAI client code to load credentials from environment variables or the credentials file.

## Interface

No new interfaces - this task updates existing code to call:

| Function | Usage Pattern |
|----------|---------------|
| `ik_credentials_load()` | Call at start of each OpenAI send function to load credentials |
| `ik_credentials_get()` | Retrieve "openai" provider key from loaded credentials |

## Behaviors

### Credential Loading Pattern

Each function that previously used `cfg->openai_api_key` must:

1. Load credentials using `ik_credentials_load(ctx, NULL, &creds)`
2. Get OpenAI key using `ik_credentials_get(creds, "openai")`
3. Check if key is NULL or empty, return ERR if missing
4. Pass key to HTTP functions

### Error Messaging

Use consistent, helpful error messages that guide users:

```
"No OpenAI credentials. Set OPENAI_API_KEY or add to ~/.config/ikigai/credentials.json"
```

This tells the user:
1. What's wrong (no credentials)
2. How to fix it (two options: env var or file)

### Files to Update

| File | Functions | Pattern |
|------|-----------|---------|
| `src/openai/client.c` | `ik_openai_send()`, `ik_openai_stream()` | Add credentials loading before HTTP calls |
| `src/openai/client_multi_request.c` | `ik_openai_multi_request()`, `ik_openai_multi_request_stream()` | Add credentials loading before HTTP calls |

### Include Requirements

Both files need:
```c
#include "credentials.h"
```

### Makefile Dependencies

If dependencies are manually tracked, add:
```makefile
src/openai/client.o: src/credentials.h
src/openai/client_multi_request.o: src/credentials.h
```

## Test Scenarios

Not applicable - this task modifies production code. Tests are updated in credential test tasks.

## Postconditions

- [ ] `src/openai/client.c` includes `credentials.h` and uses credentials API
- [ ] `src/openai/client_multi_request.c` includes `credentials.h` and uses credentials API
- [ ] No references to `cfg->openai_api_key` in `src/openai/*.c` files
- [ ] `make src/openai/client.o` succeeds
- [ ] `make src/openai/client_multi_request.o` succeeds
- [ ] `grep -n "cfg->openai_api_key" src/openai/*.c` returns nothing
- [ ] `grep -n "#include.*credentials.h" src/openai/client.c` returns a line
- [ ] `grep -n "#include.*credentials.h" src/openai/client_multi_request.c` returns a line

## Verification

```bash
# Verify no references to cfg->openai_api_key in openai/
grep -n "cfg->openai_api_key" src/openai/*.c
# Should return nothing

# Verify credentials.h is included
grep -n "#include.*credentials.h" src/openai/client.c
grep -n "#include.*credentials.h" src/openai/client_multi_request.c
# Both should return a line

# Build production code
make src/openai/client.o src/openai/client_multi_request.o
# Should succeed

# Check full build (tests will still fail - that's expected)
make 2>&1 | head -50
# Look for any errors in src/openai/ - should be none
```

## Note

After this task, tests will still fail because they reference `cfg->openai_api_key`. The credential test tasks fix the test files.
