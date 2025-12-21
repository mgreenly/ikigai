# Task: Delete Old OpenAI Code

**Layer:** 5
**Model:** sonnet/none
**Depends on:** openai-core.md, openai-send-impl.md, repl-provider-abstraction.md, tests-openai.md

## Pre-Read

**Skills:**
- `/load source-code` - Map of source files by functional area

**Source:**
- `src/openai/` - Legacy OpenAI implementation to be removed
- `src/providers/openai/` - New OpenAI provider implementation

**Plan:**
- `scratch/plan/architecture.md` - Phase 7 Cleanup Checklist

## Objective

Remove all legacy `src/openai/` code and old test files after confirming new provider implementation is complete and tested. This cleanup eliminates duplication, reduces maintenance burden, and ensures only the new provider abstraction is used going forward.

## Interface

Files and directories to delete:

| Path | Reason |
|------|--------|
| `src/openai/client.c` | Replaced by `src/providers/openai/` |
| `src/openai/client_multi.c` | Replaced by provider abstraction |
| `src/openai/client_multi_callbacks.c` | Replaced by provider abstraction |
| `src/openai/client_multi_request.c` | Replaced by provider abstraction |
| `src/openai/client_msg.c` | Replaced by request builders |
| `src/openai/client_serialize.c` | Replaced by request builders |
| `src/openai/tool_choice.c` | Replaced by common tool handling |
| `tests/unit/openai/` | Replaced by `tests/unit/providers/openai/` |

Files already migrated to common:

| Old Location | New Location |
|--------------|--------------|
| `src/openai/http_handler.c` | `src/providers/common/http_client.c` |
| `src/openai/sse_parser.c` | `src/providers/common/sse_parser.c` |

## Behaviors

**Code Deletion:**
- Delete entire `src/openai/` directory
- Delete entire `tests/unit/openai/` directory
- Do not delete `src/providers/openai/` (new implementation)
- Do not delete `tests/unit/providers/openai/` (new tests)

**Makefile Updates:**
- Remove `src/openai/` from build sources
- Remove `tests/unit/openai/` from test targets
- Verify `src/providers/` sources are included
- Verify `tests/unit/providers/` test targets exist

**Reference Verification:**
- Search for `#include.*openai/` in codebase
- Verify no includes outside `src/providers/openai/`
- Search for `ik_openai_` function calls
- Verify no calls outside `src/providers/openai/`

**Test Coverage Verification:**
- Confirm new tests cover same functionality as old tests:
  - `client_http_test.c` -> `test_openai_client.c`
  - `client_http_sse_*.c` -> `test_openai_streaming.c`
  - `client_multi_*.c` -> `test_openai_adapter.c`
- Verify no regression in test coverage
- Run `make check` to confirm all tests pass

**Integration Test Updates:**
- Update integration tests referencing old `src/openai/` paths
- Ensure they use provider abstraction instead
- Verify integration tests still pass

**Documentation Updates:**
- Update any documentation referencing old paths
- Update architecture diagrams if needed
- Update developer guides

## Test Scenarios

**Verify No Source References:**
- Run: `grep -r '#include.*openai/' src/ | grep -v 'src/providers/'`
- Expected: No output (all references are in new location)

**Verify No Function Calls:**
- Run: `grep -r 'ik_openai_' src/ | grep -v 'src/providers/openai/'`
- Expected: No output (all calls are internal to provider)

**Verify Old Tests Deleted:**
- Run: `ls tests/unit/openai/`
- Expected: Directory does not exist

**Verify New Tests Pass:**
- Run: `make build/tests/unit/providers/openai/test_openai_adapter`
- Run: `./build/tests/unit/providers/openai/test_openai_adapter`
- Expected: All tests pass

**Verify Full Suite Passes:**
- Run: `make check`
- Expected: All tests pass, no failures

**Verify Coverage Maintained:**
- Run coverage report before and after cleanup
- Expected: Coverage same or better after cleanup

## Postconditions

- [ ] `src/openai/` directory deleted
- [ ] `tests/unit/openai/` directory deleted
- [ ] Makefile updated to remove old sources and tests
- [ ] No external references to old OpenAI code
- [ ] Grep confirms no stray includes or calls
- [ ] New `tests/unit/providers/openai/` tests pass
- [ ] Full test suite passes with `make check`
- [ ] Test coverage maintained or improved
- [ ] Integration tests updated and passing
- [ ] Documentation updated if needed
- [ ] All compilation warnings resolved
