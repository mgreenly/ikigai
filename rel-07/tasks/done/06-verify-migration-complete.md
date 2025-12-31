# Task: Verify Migration Complete

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** opus/extended
**Depends on:** 05-delete-legacy-openai-code.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Comprehensive verification that legacy OpenAI cleanup is complete. All three providers work correctly, codebase is clean of legacy references, and all tests pass.

## Pre-Read

**Skills:**
- `/load makefile` - Build targets and test execution
- `/load quality` - Testing requirements

## Verification Steps

### 1. File Existence Checks

```bash
# These should NOT exist
! test -d src/openai
! test -f src/providers/openai/shim.c
! test -f src/providers/openai/shim.h
! test -d tests/unit/openai

# These SHOULD exist
test -f src/message.c
test -f src/message.h
test -f src/providers/openai/serialize.c
test -f src/providers/openai/serialize.h
test -f tests/unit/message/creation_test.c
test -f tests/unit/agent/message_management_test.c
```

### 2. Source Code Grep Checks

**No legacy types:**
```bash
# Should return EMPTY
grep -r "ik_openai_conversation_t" src/ --include="*.c" --include="*.h"
grep -r "ik_openai_request_t" src/ --include="*.c" --include="*.h" | grep -v "src/providers/openai"
```

**No legacy functions:**
```bash
# Should return EMPTY
grep -r "ik_openai_conversation_create\|ik_openai_conversation_add_msg\|ik_openai_conversation_clear" src/ --include="*.c" --include="*.h"
grep -r "ik_openai_msg_create" src/ --include="*.c" --include="*.h"
grep -r "ik_openai_shim" src/ --include="*.c" --include="*.h"
```

**No legacy includes:**
```bash
# Should return EMPTY
grep -r "#include.*openai/client\.h" src/ --include="*.c" --include="*.h"
grep -r "#include.*shim\.h" src/ --include="*.c" --include="*.h"
```

**No conversation field:**
```bash
# Should return EMPTY
grep -r "agent->conversation" src/ --include="*.c" --include="*.h"
grep -r "->conversation->" src/ --include="*.c" --include="*.h"
```

### 3. Build Verification

```bash
make clean
make all
```

**Expected:**
- Builds successfully
- No undefined references
- No missing includes
- No linker errors

### 4. Test Suite Execution

```bash
make check
```

**Expected:**
- All unit tests pass
- All integration tests pass
- No test failures

**Run provider tests individually:**
```bash
./build/tests/integration/providers/anthropic/basic_test
./build/tests/integration/providers/openai/basic_test
./build/tests/integration/providers/google/basic_test
```

**Expected:**
- Each provider test passes
- Anthropic works with new message format
- OpenAI works WITHOUT shim layer (direct JSON serialization)
- Google works with new message format

### 5. Agent Struct Verification

```bash
grep -A 10 "Conversation state" src/agent.h
```

**Expected output:**
```c
    // Conversation state (per-agent)
    ik_message_t **messages;      // Array of message pointers
    size_t message_count;         // Number of messages
    size_t message_capacity;      // Allocated capacity
```

**Should NOT contain:**
- `ik_openai_conversation_t *conversation`

### 6. Create Success Criteria Script

```bash
cat > verify_success.sh << 'EOF'
#!/bin/bash
set -e

echo "Verifying migration success..."

# 1. Agent struct uses ik_message_t
grep -q "ik_message_t \*\*messages" src/agent.h && echo "✓ Agent struct migrated"

# 2. No external calls to legacy functions
! grep -r "ik_openai_conversation_" src/ --include="*.c" | grep -v "src/openai/" && echo "✓ No conversation calls"
! grep -r "ik_openai_msg_create" src/ --include="*.c" | grep -v "src/openai/" && echo "✓ No msg_create calls"

# 3. No external includes of openai/*.h
! grep -r "#include.*openai/" src/ --include="*.c" --include="*.h" | grep -v "src/openai/" | grep -v "src/providers/openai/" && echo "✓ No legacy includes"

# 4. src/openai/ deleted
! test -d src/openai && echo "✓ Legacy directory deleted"

# 5. Shim layer deleted
! test -f src/providers/openai/shim.c && echo "✓ Shim layer deleted"

# 6. All tests pass
make check >/dev/null 2>&1 && echo "✓ All tests pass"

# 7. All providers work
./build/tests/integration/providers/anthropic/basic_test >/dev/null 2>&1 && echo "✓ Anthropic works"
./build/tests/integration/providers/openai/basic_test >/dev/null 2>&1 && echo "✓ OpenAI works"
./build/tests/integration/providers/google/basic_test >/dev/null 2>&1 && echo "✓ Google works"

echo ""
echo "All success criteria met! ✓"
EOF

chmod +x verify_success.sh
./verify_success.sh
```

### 7. Create Verification Report

```bash
cat > scratch/verification-report.md << 'EOF'
# Migration Verification Report

**Date:** $(date)
**Status:** SUCCESS ✓

## File Checks

- [ ] src/openai/ deleted
- [ ] Shim files deleted
- [ ] Legacy tests deleted
- [ ] New message.c/h created
- [ ] Agent struct updated

## Grep Checks

- [ ] No ik_openai_conversation_t references
- [ ] No ik_openai_msg_create calls
- [ ] No legacy includes
- [ ] No agent->conversation references

## Build Status

- [ ] Clean build succeeds
- [ ] No compilation errors
- [ ] No linker errors
- [ ] No warnings (or acceptable warnings documented)

## Test Results

- [ ] make check passes
- [ ] Anthropic provider works
- [ ] OpenAI provider works (without shim)
- [ ] Google provider works

## Code Reduction

- Files deleted: 21 (19 from src/openai/, 2 shim files)
- Lines removed: ~6500
- New files created: 4 (message.c, message.h, serialize.c, serialize.h)
- Net reduction: 17 files

## Next Steps

Migration complete. Codebase ready for commit.
EOF
```

### 8. Create Success Marker

**On success, create:**

```bash
cat > scratch/MIGRATION_COMPLETE.md << 'EOF'
# Legacy OpenAI Cleanup - Migration Complete

**Date:** $(date)
**Status:** SUCCESS ✓

## Summary

Successfully migrated from legacy OpenAI-specific conversation storage to
provider-agnostic message format. All three providers (Anthropic, OpenAI, Google)
working correctly with unified message interface.

## Changes

### Deletions
- src/openai/ directory: 19 files
- Shim layer: 2 files
- Legacy tests
- Total: ~6500 lines removed

### Additions
- src/message.c/h: Message creation and DB conversion
- src/providers/openai/serialize.c/h: Shared OpenAI JSON serialization
- Agent message management functions
- Comprehensive test coverage

### Migrations
- REPL code: Uses new ik_message_create_* functions
- Provider code: Reads from agent->messages
- Restore code: Uses ik_message_from_db_msg()
- All tests: Updated to new API

## Verification

All success criteria met:
$(./verify_success.sh 2>&1)

## Architecture

New clean separation:
- REPL → creates ik_message_t → adds to agent->messages
- Providers → read agent->messages → build requests → serialize to JSON
- Database → stores in ik_msg_t → restore converts to ik_message_t

No OpenAI coupling outside src/providers/openai/.

## Next Steps

Ready for commit and release.
EOF
```

## Test Requirements

This task IS a test task. Create verification report and success marker.

## Postconditions

- [ ] All file checks pass
- [ ] All grep checks return empty
- [ ] Clean build succeeds
- [ ] `make check` passes
- [ ] All provider tests pass
- [ ] Success script passes (9/9 checks)
- [ ] Verification report created
- [ ] MIGRATION_COMPLETE.md created

## Success Criteria

This task succeeds when:
1. All 9 automated checks pass
2. No legacy references found
3. All three providers work
4. Full test suite passes
5. Verification report complete
6. Success marker created
