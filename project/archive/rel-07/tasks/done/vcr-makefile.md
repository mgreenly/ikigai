# Task: Add VCR Build Rules and Record Targets

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/none
**Depends on:** vcr-mock-integration.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] `tests/helpers/vcr.c` exists (created by vcr-core.md)
- [ ] `tests/helpers/vcr.h` exists (created by vcr-core.md)
- [ ] VCR infrastructure ready (vcr.h/vcr.c exist)

## Pre-Read

**Source:**
- `Makefile` - Current build configuration, test targets, object dependencies

**Plan:**
- `scratch/plan/vcr-cassettes.md` - VCR design (modes, capture, playback)
- `scratch/vcr-tasks.md` - Phase 8 (Build rules and record targets)

## Objective

Update `Makefile` to:
1. Compile `tests/helpers/vcr.c` into `vcr.o`
2. Link `vcr.o` into all test binaries
3. Create convenience targets for recording fixtures with `VCR_RECORD=1`

This enables:
- **Playback mode (default):** `make check` uses fixtures, no network
- **Capture mode:** `make vcr-record-all` re-records all fixtures with real API calls

## Changes Required

### 1. Add vcr.o Compilation Rule

Add to test object build rules (near other test helper objects):

```makefile
$(BUILDDIR)/tests/helpers/vcr.o: tests/helpers/vcr.c tests/helpers/vcr.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(TEST_CFLAGS) -c $< -o $@
```

### 2. Add vcr.o to Test Dependencies

Update test binary linking to include `$(BUILDDIR)/tests/helpers/vcr.o`.

Find existing test binary rules that link helper objects and add vcr.o to the dependency list.

Example pattern (adjust to match existing Makefile structure):

```makefile
# If there's a TEST_HELPER_OBJS variable:
TEST_HELPER_OBJS += $(BUILDDIR)/tests/helpers/vcr.o

# Or if test binaries list dependencies directly, add to each:
$(BUILDDIR)/tests/unit/providers/anthropic: ... $(BUILDDIR)/tests/helpers/vcr.o
```

### 3. Add VCR Record Targets

Add these convenience targets to the Makefile:

```makefile
# VCR recording targets - re-record fixtures with real API calls
# Requires API keys via environment variables

.PHONY: vcr-record-openai vcr-record-anthropic vcr-record-google vcr-record-all

vcr-record-openai: $(BUILDDIR)/tests/unit/providers/openai
	@echo "Recording OpenAI fixtures (requires OPENAI_API_KEY)..."
	VCR_RECORD=1 $(BUILDDIR)/tests/unit/providers/openai

vcr-record-anthropic: $(BUILDDIR)/tests/unit/providers/anthropic
	@echo "Recording Anthropic fixtures (requires ANTHROPIC_API_KEY)..."
	VCR_RECORD=1 $(BUILDDIR)/tests/unit/providers/anthropic

vcr-record-google: $(BUILDDIR)/tests/unit/providers/google
	@echo "Recording Google fixtures (requires GOOGLE_API_KEY)..."
	VCR_RECORD=1 $(BUILDDIR)/tests/unit/providers/google

vcr-record-all: $(BUILDDIR)/tests/unit/providers/openai $(BUILDDIR)/tests/unit/providers/anthropic $(BUILDDIR)/tests/unit/providers/google
	@echo "All fixtures re-recorded"
```

## VCR Environment Variable Handling

The `VCR_RECORD=1` environment variable controls VCR behavior:

- **Not set (default):** Playback mode - tests use fixtures
- **Set to 1:** Capture mode - tests make real HTTP calls and record responses

This is handled in `tests/helpers/vcr.c`, not in the Makefile. The Makefile just passes the env var through.

## Directory Creation

Ensure test helper build directory exists:

```makefile
$(BUILDDIR)/tests/helpers/:
	@mkdir -p $@
```

Or rely on the `@mkdir -p $(dir $@)` in the vcr.o rule.

## Testing

After Makefile changes:

1. **Verify compilation:**
   ```bash
   make clean
   make check
   ```
   Should compile vcr.o and link into test binaries without errors.

2. **Verify playback mode (default):**
   ```bash
   make check
   ```
   Tests should run using fixtures (no network calls).

3. **Verify record targets (dry-run):**
   ```bash
   make -n vcr-record-openai
   ```
   Should show: `VCR_RECORD=1 $(BUILDDIR)/tests/unit/providers/openai`

Note: Do NOT run actual `vcr-record-*` targets unless you have real API keys and intend to overwrite fixtures.

## Postconditions

- [ ] `$(BUILDDIR)/tests/helpers/vcr.o` compilation rule added
- [ ] `vcr.o` linked into all provider test binaries
- [ ] `vcr-record-openai` target defined
- [ ] `vcr-record-anthropic` target defined
- [ ] `vcr-record-google` target defined
- [ ] `vcr-record-all` target defined (runs all three)
- [ ] All targets marked `.PHONY`
- [ ] `make clean && make check` passes
- [ ] `make -n vcr-record-all` shows expected commands
- [ ] Changes committed to git with message: `task: vcr-makefile.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).

## Notes

- VCR recording requires real API credentials - do NOT run in CI
- Fixtures should be committed to git after recording
- The pre-commit hook (created by vcr-precommit-hook.md) will prevent accidental credential leaks
- Record targets are for local development only - CI always runs in playback mode
