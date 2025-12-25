# Task: Verify Legacy OpenAI Cleanup Complete

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** opus/extended
**Depends on:** delete-legacy-files.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Comprehensive verification that legacy OpenAI code removal is complete, all three providers work correctly, and the codebase is clean of legacy references. This is the final quality gate before declaring the migration successful.

## Pre-Read

**Skills:**
- `/load makefile` - Build targets and test execution
- `/load quality` - Testing requirements and quality checks

**Reference Documents:**
- `scratch/problem.md` - Original problem analysis
- `scratch/legacy-openai-removal-checklist.md` - Complete inventory

## Verification Steps

### 1. File Existence Checks

**Verify deletions:**
```bash
# These should NOT exist
! test -d src/openai
! test -f src/providers/openai/shim.c
! test -f src/providers/openai/shim.h
! test -d tests/unit/openai
```

**Verify new files exist:**
```bash
# These SHOULD exist
test -f src/message.c
test -f src/message.h
test -f tests/unit/message/creation_test.c
test -f tests/unit/agent/message_management_test.c
```

### 2. Source Code Grep Checks

Run comprehensive grep searches to find any remaining legacy references.

**No legacy types:**
```bash
# Should return EMPTY
grep -r "ik_openai_conversation_t" src/ --include="*.c" --include="*.h"
grep -r "ik_openai_request_t" src/ --include="*.c" --include="*.h" | grep -v "src/providers/openai"
grep -r "ik_openai_multi_t" src/ --include="*.c" --include="*.h"
```

**No legacy functions:**
```bash
# Should return EMPTY
grep -r "ik_openai_conversation_create\|ik_openai_conversation_add_msg\|ik_openai_conversation_clear" src/ --include="*.c" --include="*.h"
grep -r "ik_openai_msg_create\|ik_openai_msg_create_tool_call\|ik_openai_msg_create_tool_result" src/ --include="*.c" --include="*.h"
grep -r "ik_openai_shim_build_conversation\|ik_openai_shim_map_finish_reason" src/ --include="*.c" --include="*.h"
```

**No legacy includes:**
```bash
# Should return EMPTY (except within src/providers/openai/)
grep -r "#include.*openai/client\.h" src/ --include="*.c" --include="*.h" | grep -v "src/providers/openai"
grep -r "#include.*openai/client_msg\.h" src/ --include="*.c" --include="*.h" | grep -v "src/providers/openai"
grep -r "#include.*shim\.h" src/ --include="*.c" --include="*.h"
```

**No conversation field references:**
```bash
# Should return EMPTY (agent->conversation should not exist)
grep -r "agent->conversation" src/ --include="*.c" --include="*.h"
grep -r "->conversation->" src/ --include="*.c" --include="*.h"
```

### 3. Build Verification

**Clean build:**
```bash
make clean
make all
```

**Expected:**
- Builds successfully
- No undefined references to `ik_openai_*` functions
- No warnings about missing includes
- No linker errors

**Check object files:**
```bash
# Should NOT exist
! test -f build/src/openai/client.o
! test -f build/src/openai/client_msg.o
! test -f build/src/providers/openai/shim.o
```

### 4. Test Suite Execution

**Run all tests:**
```bash
make check
```

**Expected:**
- All unit tests pass
- All integration tests pass
- No test failures related to missing legacy code

**Specific provider tests:**
```bash
# Run individually to verify each provider works
./build/tests/integration/providers/anthropic/basic_test
./build/tests/integration/providers/openai/basic_test
./build/tests/integration/providers/google/basic_test
```

**Expected:**
- Each provider test passes
- Anthropic provider works with new message format
- OpenAI provider works WITHOUT shim layer
- Google provider works with new message format

### 5. Integration Test - Full Conversation Flow

Create and run comprehensive integration test.

**Test Scenario:**
1. Create agent with Anthropic provider
2. Add user message
3. Send to LLM (streaming)
4. Receive assistant response
5. Make tool call
6. Execute tool
7. Send tool result
8. Receive final response
9. Fork agent
10. Clear messages
11. Verify all operations work

**Create `tests/integration/full_conversation_test.c`:**

```c
// Full end-to-end test with all three providers
START_TEST(test_anthropic_full_flow) {
    // Create agent, add messages, send request, verify response
    // Uses only ik_message_t and ik_agent_add_message()
}
END_TEST

START_TEST(test_openai_full_flow) {
    // Same test with OpenAI provider
}
END_TEST

START_TEST(test_google_full_flow) {
    // Same test with Google provider
}
END_TEST
```

**Run:**
```bash
./build/tests/integration/full_conversation_test
```

**Expected:** All three tests pass.

### 6. Code Quality Checks

**Lint:**
```bash
make lint
```

**Expected:**
- No complexity violations
- No file size violations
- Clean codebase

**Coverage (if available):**
```bash
make coverage
```

**Expected:**
- New message code covered by tests
- No coverage regressions

### 7. Agent Struct Verification

**Check agent.h struct definition:**
```bash
grep -A 10 "Conversation state" src/agent.h
```

**Expected output:**
```c
    // Conversation state (per-agent)
    ik_message_t **messages;      // Array of message pointers
    size_t message_count;         // Number of messages
    size_t message_capacity;      // Allocated capacity

    ik_mark_t **marks;
    size_t mark_count;
```

**Should NOT contain:**
- `ik_openai_conversation_t *conversation`
- Any references to old conversation field

### 8. Provider Factory Verification

**Check provider instantiation:**
```bash
grep -A 5 "openai.*create" src/providers/factory.c
```

**Expected:**
- Calls new provider implementation in `src/providers/openai/openai.c`
- Does NOT call old `ik_openai_create()` from deleted `src/openai/client.c`

### 9. Documentation Check

**Verify no legacy documentation:**
```bash
# Check for outdated documentation referencing old API
grep -r "ik_openai_conversation" project/ docs/ README.md 2>/dev/null || true
```

**Expected:**
- Should find nothing OR only historical references
- No current documentation describing old API

### 10. Success Criteria Checklist

From original problem analysis (`scratch/problem.md` line 747-758):

```bash
# Create verification script
cat > verify_success_criteria.sh << 'EOF'
#!/bin/bash
set -e

echo "Checking success criteria..."

# 1. Agent struct uses ik_message_t
grep -q "ik_message_t \*\*messages" src/agent.h && echo "✓ Agent struct migrated"

# 2. No external calls to ik_openai_conversation_*
! grep -r "ik_openai_conversation_create\|ik_openai_conversation_add_msg" src/ --include="*.c" | grep -v "src/openai/" && echo "✓ No conversation calls"

# 3. No external calls to ik_openai_msg_create*
! grep -r "ik_openai_msg_create" src/ --include="*.c" | grep -v "src/openai/" && echo "✓ No msg_create calls"

# 4. No external includes of openai/*.h
! grep -r "#include.*openai/" src/ --include="*.c" --include="*.h" | grep -v "src/openai/" | grep -v "src/providers/openai/" && echo "✓ No legacy includes"

# 5. src/openai/ deleted
! test -d src/openai && echo "✓ Legacy directory deleted"

# 6. Shim layer deleted
! test -f src/providers/openai/shim.c && echo "✓ Shim layer deleted"

# 7. All tests pass
make check >/dev/null 2>&1 && echo "✓ All tests pass"

# 8. No legacy references
! grep -r "ik_openai_" src/ --include="*.c" --include="*.h" | grep -v "src/providers/openai" | grep -v "ik_openai_provider_create" && echo "✓ No legacy references"

# 9. All providers work
./build/tests/integration/providers/anthropic/basic_test >/dev/null 2>&1 && echo "✓ Anthropic works"
./build/tests/integration/providers/openai/basic_test >/dev/null 2>&1 && echo "✓ OpenAI works"
./build/tests/integration/providers/google/basic_test >/dev/null 2>&1 && echo "✓ Google works"

echo ""
echo "All success criteria met! ✓"
EOF

chmod +x verify_success_criteria.sh
./verify_success_criteria.sh
```

## Test Requirements

This task IS a test task. The entire task is verification.

**Create comprehensive verification report:**

Create `scratch/verification-report.md` with:
- Date/time of verification
- All grep check results
- Build output (success/failure)
- Test execution results
- Success criteria checklist status
- Any warnings or issues found

## Postconditions

- [ ] All file existence checks pass
- [ ] All grep searches return empty (no legacy references)
- [ ] Clean build succeeds
- [ ] `make check` passes (all tests)
- [ ] All three provider integration tests pass
- [ ] Full conversation test passes for all providers
- [ ] `make lint` passes
- [ ] Success criteria script passes (10/10 checks)
- [ ] Verification report created in `scratch/verification-report.md`

## Expected Outcome

**On Success:**
Create `scratch/MIGRATION_COMPLETE.md` with:
```markdown
# Legacy OpenAI Cleanup - Migration Complete

**Date:** [timestamp]
**Status:** SUCCESS ✓

## Summary

Successfully migrated from legacy OpenAI-specific conversation storage to
provider-agnostic message format. All three providers (Anthropic, OpenAI, Google)
working correctly with unified message interface.

## Deletions

- Deleted src/openai/ directory: 19 files, ~6000 lines
- Deleted shim layer: 2 files, ~500 lines
- Total reduction: 21 files, ~6500 lines

## Additions

- Created src/message.c: message creation and conversion
- Added dual-mode support to agent struct
- Migrated all REPL/agent code to new API
- Comprehensive test coverage for new functionality

## Verification

All success criteria met:
[paste success_criteria.sh output]

## Next Steps

Migration complete. Codebase ready for release.
```

**On Failure:**
Document what failed and escalate with detailed error report.

## Success Criteria

This task succeeds when:
1. All 10 automated checks pass
2. No legacy code references found anywhere
3. All three providers work correctly
4. Full test suite passes
5. Verification report documents clean state
6. MIGRATION_COMPLETE.md created
