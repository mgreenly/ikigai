# Plan: Fix verify-mocks-providers.md Credential Precondition

## Problem

`verify-mocks-providers.md` (position 17) requires real API calls to record VCR cassettes, but:

1. **Missing precondition:** Task doesn't require `make verify-credentials` to pass first
2. **Soft failure path:** If credentials missing/invalid, task fails late with unclear error
3. **Downstream blocked:** `tests-mock-infrastructure.md` requires cassettes that won't exist

## Existing Infrastructure

The Makefile already has credential validation (lines 373-434):

```makefile
verify-credentials:
    # Reads ~/.config/ikigai/credentials.json
    # Tests OpenAI, Anthropic, Google API keys
    # Exits 1 if any fail
```

## Fix

Update `scratch/tasks/verify-mocks-providers.md` to:

### 1. Add credential validation precondition

**Current preconditions (lines 40-44):**
```markdown
## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] `~/.config/ikigai/credentials.json` exists with valid API keys for all providers
- [ ] `src/credentials.h` exists (from credentials-core.md)
```

**Updated preconditions:**
```markdown
## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] `src/credentials.h` exists (from credentials-core.md)
- [ ] All provider credentials valid (verify: `make verify-credentials` exits 0)
```

### 2. Add early-exit check in Behaviors section

Add after line 132 (after capture_fixture function):

```markdown
### Credential Validation (First Step)

Before running any tests, verify credentials are available:

```c
static void verify_credentials_available(void)
{
    // Check environment variables (set by Makefile from credentials.json)
    const char *anthropic_key = getenv("ANTHROPIC_API_KEY");
    const char *google_key = getenv("GOOGLE_API_KEY");

    if (anthropic_key == NULL || strlen(anthropic_key) == 0) {
        fprintf(stderr, "ANTHROPIC_API_KEY not set. Run: make verify-credentials\n");
        exit(77);  // Skip code for Check framework
    }
    if (google_key == NULL || strlen(google_key) == 0) {
        fprintf(stderr, "GOOGLE_API_KEY not set. Run: make verify-credentials\n");
        exit(77);
    }
}
```

Call this in suite setup before any tests run.
```

### 3. Update postconditions for clarity

Add explicit success/skip criteria:

```markdown
## Postconditions

...existing items...

- [ ] If `make verify-credentials` fails:
  - [ ] Task exits with skip status (not failure)
  - [ ] No partial fixtures created
  - [ ] Clear message: "Run 'make verify-credentials' to diagnose"
```

## Rationale

- **Fail fast:** Credential problems detected before any work starts
- **Clear diagnostics:** User knows exactly what to fix
- **Existing pattern:** Uses established `verify-credentials` target
- **No partial state:** Either all cassettes recorded or none

## Files to Modify

| File | Change |
|------|--------|
| `scratch/tasks/verify-mocks-providers.md` | Add precondition, early-exit check |

## Verification

After fix, this sequence should work:

```bash
# 1. Validate credentials exist and work
make verify-credentials

# 2. Record cassettes (only runs if credentials valid)
make verify-mocks-anthropic CAPTURE_FIXTURES=1
make verify-mocks-google CAPTURE_FIXTURES=1

# 3. Verify cassettes exist
ls tests/fixtures/anthropic/stream_text_basic.txt
ls tests/fixtures/google/stream_text_basic.txt
```
