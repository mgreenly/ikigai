# Refactor: Code Quality Improvements

Technical debt reduction and architectural improvements identified through codebase analysis.

## Scope

This refactoring addresses the top code quality issues identified:

1. **Unused Dead Code** - Remove `conversation.h` and `ik_conversation_t` (superseded by `ik_openai_conversation_t`)
2. **File Reading Duplication** - Extract shared file reading utility from 3 files (~120 lines)
3. **JSON Response Duplication** - Extract tool response builders from 5 tool files (~200 lines)
4. **Circular Dependency** - Fix `msg.h` ↔ `db/replay.h` layering violation
5. **Logger Global State** - Add `ik_logger_ctx_t` with dependency injection
6. **Shared Context DI** - Add logger and ikigai_path dependency injection to `ik_shared_ctx_init()`

## Principles

- **TDD** - All changes driven by tests
- **100% Coverage** - Maintain coverage requirement
- **Small Commits** - One logical change per commit
- **Clean Working Tree** - Always leave tree clean

## Task Order

Tasks ordered by dependencies and complexity (simplest first):

1. `delete-unused-conversation.md` - Quick win, removes confusion
2. `extract-file-reader.md` - Simple utility extraction
3. `extract-tool-response-builders.md` - Similar utility extraction
4. `fix-msg-db-circular-dep.md` - Architectural layering fix
5. `logger-dependency-injection.md` - Largest change, DI pattern
6. `shared-ctx-signature.md` - Update shared_ctx_init signature with DI
7. `shared-ctx-callsites.md` - Update all call sites to use new signature
8. `shared-ctx-coverage.md` - Fix coverage test with new DI pattern

## Success Criteria

- All tests pass (`make check`)
- Lint passes (`make lint`)
- Coverage maintained (`make coverage`)
- No new warnings
- Cleaner architecture verified by code review
