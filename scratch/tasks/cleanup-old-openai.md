# Task: Delete Old OpenAI Code

**Phase:** 7 - Cleanup
**Depends on:** 12-openai-native, 09-repl-provider-abstraction

## Objective

Remove all legacy `src/openai/` code and adapter shim after migration is complete.

## Deliverables

1. Delete `src/openai/` directory entirely:
   - `client.c`
   - `client_multi.c`
   - `client_multi_callbacks.c`
   - `client_multi_request.c`
   - `client_msg.c`
   - `client_serialize.c`
   - `http_handler.c` (moved to common/)
   - `sse_parser.c` (moved to common/)
   - `tool_choice.c`

2. Delete adapter shim:
   - Remove `src/providers/openai/adapter_shim.c`

3. Update Makefile:
   - Remove `src/openai/` from build
   - Add `src/providers/` sources

4. Verify no references remain:
   - Grep: no `#include.*openai/` outside `src/providers/`
   - Grep: no `ik_openai_` calls outside `src/providers/`

5. Update documentation:
   - Any docs referencing old paths

## Reference

- `scratch/plan/architecture.md` - Phase 7 Cleanup Checklist

## Verification Checklist

- [ ] `src/openai/` deleted
- [ ] Adapter shim deleted
- [ ] Makefile updated
- [ ] No external openai references
- [ ] Full test suite passes
