# Task: Delete Unused conversation.h

## Target

Refactor Issue #5: Remove unused dead code (`conversation.h` and `ik_conversation_t`)

## Context

The `src/conversation.h` file defines `ik_conversation_t` but:
- No implementation file (`conversation.c`) exists
- The type is never instantiated or used anywhere
- It has been superseded by `ik_openai_conversation_t` in `src/openai/client.h`
- The header is never included by any source file

This dead code creates confusion and maintenance burden.

## Pre-read

### Skills
- default
- database
- errors
- git
- log
- makefile
- naming
- quality
- scm
- source-code
- style
- tdd
- align

### Documentation
- docs/README.md

### Source Files to Examine
- src/conversation.h (the file to delete)
- src/openai/client.h (the replacement: `ik_openai_conversation_t`)

### Verification Commands
```bash
# Confirm conversation.h is never included
grep -r '#include.*conversation.h' src/
grep -r '#include.*conversation.h' tests/

# Confirm ik_conversation_t is never used
grep -r 'ik_conversation_t' src/ tests/
```

## Pre-conditions

1. Working tree is clean (`git status --porcelain` returns empty)
2. All tests pass (`make check`)
3. `src/conversation.h` exists and is unused (verify with grep commands above)

## Task

Delete the unused `src/conversation.h` file and verify no breakage.

## TDD Cycle

### Red Phase
Not applicable - this is pure deletion of unused code.

### Green Phase

1. Run verification commands to confirm file is unused:
   ```bash
   grep -r '#include.*conversation.h' src/ tests/
   grep -r 'ik_conversation_t' src/ tests/
   ```
   Both should return no matches (except the file itself).

2. Delete the file:
   ```bash
   rm src/conversation.h
   ```

3. Verify build still works:
   ```bash
   make clean && make check
   ```

### Refactor Phase
Not applicable - simple deletion.

## Post-conditions

1. `src/conversation.h` no longer exists
2. All tests pass (`make check`)
3. Lint passes (`make lint`)
4. Coverage unchanged (`make coverage`)
5. Working tree is clean (changes committed)

## Commit Message Format

```
refactor: remove unused conversation.h

- ik_conversation_t was never instantiated or used
- Superseded by ik_openai_conversation_t in openai/client.h
- Eliminates dead code and reduces maintenance burden
```

## Risk Assessment

**Risk: Low**
- File is confirmed unused before deletion
- Build and tests verify no breakage
- Easy to revert if needed

## Estimated Complexity

**Trivial** - Simple file deletion with verification
